//========================================================================
// GLFW - An OpenGL framework
// Platform:    X11/GLX
// API version: 3.0
// WWW:         http://www.glfw.org/
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2010 Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include "internal.h"

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


/* Define GLX 1.4 FSAA tokens if not already defined */
#ifndef GLX_VERSION_1_4

#define GLX_SAMPLE_BUFFERS  100000
#define GLX_SAMPLES         100001

#endif /*GLX_VERSION_1_4*/

// Action for EWMH client messages
#define _NET_WM_STATE_REMOVE        0
#define _NET_WM_STATE_ADD           1
#define _NET_WM_STATE_TOGGLE        2


//************************************************************************
//****                  GLFW internal functions                       ****
//************************************************************************

//========================================================================
// Checks whether the event is a MapNotify for the specified window
//========================================================================

static Bool isMapNotify(Display* d, XEvent* e, char* arg)
{
    return (e->type == MapNotify) && (e->xmap.window == (Window)arg);
}


//========================================================================
// Retrieve a single window property of the specified type
// Inspired by fghGetWindowProperty from freeglut
//========================================================================

static unsigned long getWindowProperty(Window window,
                                       Atom property,
                                       Atom type,
                                       unsigned char** value)
{
    Atom actualType;
    int actualFormat;
    unsigned long itemCount, bytesAfter;

    XGetWindowProperty(_glfwLibrary.X11.display,
                       window,
                       property,
                       0,
                       LONG_MAX,
                       False,
                       type,
                       &actualType,
                       &actualFormat,
                       &itemCount,
                       &bytesAfter,
                       value);

    if (actualType != type)
        return 0;

    return itemCount;
}


//========================================================================
// Check whether the specified atom is supported
//========================================================================

static Atom getSupportedAtom(Atom* supportedAtoms,
                             unsigned long atomCount,
                             const char* atomName)
{
    Atom atom = XInternAtom(_glfwLibrary.X11.display, atomName, True);
    if (atom != None)
    {
        unsigned long i;

        for (i = 0;  i < atomCount;  i++)
        {
            if (supportedAtoms[i] == atom)
                return atom;
        }
    }

    return None;
}


//========================================================================
// Check whether the running window manager is EWMH-compliant
//========================================================================

static GLboolean checkForEWMH(_GLFWwindow* window)
{
    Window* windowFromRoot = NULL;
    Window* windowFromChild = NULL;

    // Hey kids; let's see if the window manager supports EWMH!

    // First we need a couple of atoms, which should already be there
    Atom supportingWmCheck =
        XInternAtom(_glfwLibrary.X11.display, "_NET_SUPPORTING_WM_CHECK", True);
    Atom wmSupported =
        XInternAtom(_glfwLibrary.X11.display, "_NET_SUPPORTED", True);
    if (supportingWmCheck == None || wmSupported == None)
        return GL_FALSE;

    // Then we look for the _NET_SUPPORTING_WM_CHECK property of the root window
    if (getWindowProperty(_glfwLibrary.X11.root,
                          supportingWmCheck,
                          XA_WINDOW,
                          (unsigned char**) &windowFromRoot) != 1)
    {
        XFree(windowFromRoot);
        return GL_FALSE;
    }

    // It should be the ID of a child window (of the root)
    // Then we look for the same property on the child window
    if (getWindowProperty(*windowFromRoot,
                          supportingWmCheck,
                          XA_WINDOW,
                          (unsigned char**) &windowFromChild) != 1)
    {
        XFree(windowFromRoot);
        XFree(windowFromChild);
        return GL_FALSE;
    }

    // It should be the ID of that same child window
    if (*windowFromRoot != *windowFromChild)
    {
        XFree(windowFromRoot);
        XFree(windowFromChild);
        return GL_FALSE;
    }

    XFree(windowFromRoot);
    XFree(windowFromChild);

    // We are now fairly sure that an EWMH-compliant window manager is running

    Atom* supportedAtoms;
    unsigned long atomCount;

    // Now we need to check the _NET_SUPPORTED property of the root window
    atomCount = getWindowProperty(_glfwLibrary.X11.root,
                                  wmSupported,
                                  XA_ATOM,
                                  (unsigned char**) &supportedAtoms);

    // See which of the atoms we support that are supported by the WM

    window->X11.wmState =
        getSupportedAtom(supportedAtoms, atomCount, "_NET_WM_STATE");

    window->X11.wmStateFullscreen =
        getSupportedAtom(supportedAtoms, atomCount, "_NET_WM_STATE_FULLSCREEN");

    window->X11.wmPing =
        getSupportedAtom(supportedAtoms, atomCount, "_NET_WM_PING");

    window->X11.wmActiveWindow =
        getSupportedAtom(supportedAtoms, atomCount, "_NET_ACTIVE_WINDOW");

    XFree(supportedAtoms);

    return GL_TRUE;
}

//========================================================================
// Translates an X Window key to internal coding
//========================================================================

static int translateKey(int keycode)
{
    KeySym key, key_lc, key_uc;

    // Try secondary keysym, for numeric keypad keys
    // Note: This way we always force "NumLock = ON", which at least
    // enables GLFW users to detect numeric keypad keys
    key = XKeycodeToKeysym(_glfwLibrary.X11.display, keycode, 1);
    switch (key)
    {
        // Numeric keypad
        case XK_KP_0:         return GLFW_KEY_KP_0;
        case XK_KP_1:         return GLFW_KEY_KP_1;
        case XK_KP_2:         return GLFW_KEY_KP_2;
        case XK_KP_3:         return GLFW_KEY_KP_3;
        case XK_KP_4:         return GLFW_KEY_KP_4;
        case XK_KP_5:         return GLFW_KEY_KP_5;
        case XK_KP_6:         return GLFW_KEY_KP_6;
        case XK_KP_7:         return GLFW_KEY_KP_7;
        case XK_KP_8:         return GLFW_KEY_KP_8;
        case XK_KP_9:         return GLFW_KEY_KP_9;
        case XK_KP_Separator:
        case XK_KP_Decimal:   return GLFW_KEY_KP_DECIMAL;
        case XK_KP_Equal:     return GLFW_KEY_KP_EQUAL;
        case XK_KP_Enter:     return GLFW_KEY_KP_ENTER;
        default:              break;
    }

    // Now try pimary keysym
    key = XKeycodeToKeysym(_glfwLibrary.X11.display, keycode, 0);
    switch (key)
    {
        // Special keys (non character keys)
        case XK_Escape:       return GLFW_KEY_ESC;
        case XK_Tab:          return GLFW_KEY_TAB;
        case XK_Shift_L:      return GLFW_KEY_LSHIFT;
        case XK_Shift_R:      return GLFW_KEY_RSHIFT;
        case XK_Control_L:    return GLFW_KEY_LCTRL;
        case XK_Control_R:    return GLFW_KEY_RCTRL;
        case XK_Meta_L:
        case XK_Alt_L:        return GLFW_KEY_LALT;
        case XK_Mode_switch:  // Mapped to Alt_R on many keyboards
        case XK_Meta_R:
        case XK_ISO_Level3_Shift: // AltGr on at least some machines
        case XK_Alt_R:        return GLFW_KEY_RALT;
        case XK_Super_L:      return GLFW_KEY_LSUPER;
        case XK_Super_R:      return GLFW_KEY_RSUPER;
        case XK_Menu:         return GLFW_KEY_MENU;
        case XK_Num_Lock:     return GLFW_KEY_KP_NUM_LOCK;
        case XK_Caps_Lock:    return GLFW_KEY_CAPS_LOCK;
        case XK_Scroll_Lock:  return GLFW_KEY_SCROLL_LOCK;
        case XK_Pause:        return GLFW_KEY_PAUSE;
        case XK_KP_Delete:
        case XK_Delete:       return GLFW_KEY_DEL;
        case XK_BackSpace:    return GLFW_KEY_BACKSPACE;
        case XK_Return:       return GLFW_KEY_ENTER;
        case XK_KP_Home:
        case XK_Home:         return GLFW_KEY_HOME;
        case XK_KP_End:
        case XK_End:          return GLFW_KEY_END;
        case XK_KP_Page_Up:
        case XK_Page_Up:      return GLFW_KEY_PAGEUP;
        case XK_KP_Page_Down:
        case XK_Page_Down:    return GLFW_KEY_PAGEDOWN;
        case XK_KP_Insert:
        case XK_Insert:       return GLFW_KEY_INSERT;
        case XK_KP_Left:
        case XK_Left:         return GLFW_KEY_LEFT;
        case XK_KP_Right:
        case XK_Right:        return GLFW_KEY_RIGHT;
        case XK_KP_Down:
        case XK_Down:         return GLFW_KEY_DOWN;
        case XK_KP_Up:
        case XK_Up:           return GLFW_KEY_UP;
        case XK_F1:           return GLFW_KEY_F1;
        case XK_F2:           return GLFW_KEY_F2;
        case XK_F3:           return GLFW_KEY_F3;
        case XK_F4:           return GLFW_KEY_F4;
        case XK_F5:           return GLFW_KEY_F5;
        case XK_F6:           return GLFW_KEY_F6;
        case XK_F7:           return GLFW_KEY_F7;
        case XK_F8:           return GLFW_KEY_F8;
        case XK_F9:           return GLFW_KEY_F9;
        case XK_F10:          return GLFW_KEY_F10;
        case XK_F11:          return GLFW_KEY_F11;
        case XK_F12:          return GLFW_KEY_F12;
        case XK_F13:          return GLFW_KEY_F13;
        case XK_F14:          return GLFW_KEY_F14;
        case XK_F15:          return GLFW_KEY_F15;
        case XK_F16:          return GLFW_KEY_F16;
        case XK_F17:          return GLFW_KEY_F17;
        case XK_F18:          return GLFW_KEY_F18;
        case XK_F19:          return GLFW_KEY_F19;
        case XK_F20:          return GLFW_KEY_F20;
        case XK_F21:          return GLFW_KEY_F21;
        case XK_F22:          return GLFW_KEY_F22;
        case XK_F23:          return GLFW_KEY_F23;
        case XK_F24:          return GLFW_KEY_F24;
        case XK_F25:          return GLFW_KEY_F25;

        // Numeric keypad (should have been detected in secondary keysym!)
        case XK_KP_Divide:    return GLFW_KEY_KP_DIVIDE;
        case XK_KP_Multiply:  return GLFW_KEY_KP_MULTIPLY;
        case XK_KP_Subtract:  return GLFW_KEY_KP_SUBTRACT;
        case XK_KP_Add:       return GLFW_KEY_KP_ADD;
        case XK_KP_Equal:     return GLFW_KEY_KP_EQUAL;
        case XK_KP_Enter:     return GLFW_KEY_KP_ENTER;

        // The rest (should be printable keys)
        default:
            // Make uppercase
            XConvertCase(key, &key_lc, &key_uc);
            key = key_uc;

            // Valid ISO 8859-1 character?
            if ((key >=  32 && key <= 126) || (key >= 160 && key <= 255))
                return (int) key;

            return GLFW_KEY_UNKNOWN;
    }
}


//========================================================================
// Translates an X Window event to Unicode
//========================================================================

static int translateChar(XKeyEvent* event)
{
    KeySym keysym;

    // Get X11 keysym
    XLookupString(event, NULL, 0, &keysym, NULL);

    // Convert to Unicode (see x11_keysym2unicode.c)
    return (int) _glfwKeySym2Unicode(keysym);
}


//========================================================================
// Create a blank cursor (for locked mouse mode)
//========================================================================

static Cursor createNULLCursor(Display* display, Window root)
{
    Pixmap    cursormask;
    XGCValues xgc;
    GC        gc;
    XColor    col;
    Cursor    cursor;

    // TODO: Add error checks

    cursormask = XCreatePixmap(display, root, 1, 1, 1);
    xgc.function = GXclear;
    gc = XCreateGC(display, cursormask, GCFunction, &xgc);
    XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
    col.pixel = 0;
    col.red = 0;
    col.flags = 4;
    cursor = XCreatePixmapCursor(display, cursormask, cursormask,
                                 &col,&col, 0,0);
    XFreePixmap(display, cursormask);
    XFreeGC(display, gc);

    return cursor;
}


//========================================================================
// Returns the specified attribute of the specified GLXFBConfig
// NOTE: Do not call this unless we have found GLX 1.3+ or GLX_SGIX_fbconfig
//========================================================================

static int getFBConfigAttrib(_GLFWwindow* window, GLXFBConfig fbconfig, int attrib)
{
    int value;

    if (window->GLX.has_GLX_SGIX_fbconfig)
    {
        window->GLX.GetFBConfigAttribSGIX(_glfwLibrary.X11.display,
                                          fbconfig, attrib, &value);
    }
    else
        glXGetFBConfigAttrib(_glfwLibrary.X11.display, fbconfig, attrib, &value);

    return value;
}


//========================================================================
// Return a list of available and usable framebuffer configs
//========================================================================

static _GLFWfbconfig* getFBConfigs(_GLFWwindow* window, unsigned int* found)
{
    GLXFBConfig* fbconfigs;
    _GLFWfbconfig* result;
    int i, count = 0;

    *found = 0;

    if (_glfwLibrary.X11.glxMajor == 1 && _glfwLibrary.X11.glxMinor < 3)
    {
        if (!window->GLX.has_GLX_SGIX_fbconfig)
        {
            fprintf(stderr, "GLXFBConfigs are not supported by the X server\n");
            _glfwSetError(GLFW_NO_PIXEL_FORMAT);
            return NULL;
        }
    }

    if (window->GLX.has_GLX_SGIX_fbconfig)
    {
        fbconfigs = window->GLX.ChooseFBConfigSGIX(_glfwLibrary.X11.display,
                                                   _glfwLibrary.X11.screen,
                                                   NULL,
                                                   &count);
        if (!count)
        {
            fprintf(stderr, "No GLXFBConfigs returned\n");
            _glfwSetError(GLFW_NO_PIXEL_FORMAT);
            return NULL;
        }
    }
    else
    {
        fbconfigs = glXGetFBConfigs(_glfwLibrary.X11.display, _glfwLibrary.X11.screen, &count);
        if (!count)
        {
            fprintf(stderr, "No GLXFBConfigs returned\n");
            _glfwSetError(GLFW_NO_PIXEL_FORMAT);
            return NULL;
        }
    }

    result = (_GLFWfbconfig*) malloc(sizeof(_GLFWfbconfig) * count);
    if (!result)
    {
        _glfwSetError(GLFW_OUT_OF_MEMORY);
        return NULL;
    }

    for (i = 0;  i < count;  i++)
    {
        if (!getFBConfigAttrib(window, fbconfigs[i], GLX_DOUBLEBUFFER) ||
            !getFBConfigAttrib(window, fbconfigs[i], GLX_VISUAL_ID))
        {
            // Only consider double-buffered GLXFBConfigs with associated visuals
            continue;
        }

        if (!(getFBConfigAttrib(window, fbconfigs[i], GLX_RENDER_TYPE) & GLX_RGBA_BIT))
        {
            // Only consider RGBA GLXFBConfigs
            continue;
        }

        if (!(getFBConfigAttrib(window, fbconfigs[i], GLX_DRAWABLE_TYPE) & GLX_WINDOW_BIT))
        {
            // Only consider window GLXFBConfigs
            continue;
        }

        result[*found].redBits = getFBConfigAttrib(window, fbconfigs[i], GLX_RED_SIZE);
        result[*found].greenBits = getFBConfigAttrib(window, fbconfigs[i], GLX_GREEN_SIZE);
        result[*found].blueBits = getFBConfigAttrib(window, fbconfigs[i], GLX_BLUE_SIZE);

        result[*found].alphaBits = getFBConfigAttrib(window, fbconfigs[i], GLX_ALPHA_SIZE);
        result[*found].depthBits = getFBConfigAttrib(window, fbconfigs[i], GLX_DEPTH_SIZE);
        result[*found].stencilBits = getFBConfigAttrib(window, fbconfigs[i], GLX_STENCIL_SIZE);

        result[*found].accumRedBits = getFBConfigAttrib(window, fbconfigs[i], GLX_ACCUM_RED_SIZE);
        result[*found].accumGreenBits = getFBConfigAttrib(window, fbconfigs[i], GLX_ACCUM_GREEN_SIZE);
        result[*found].accumBlueBits = getFBConfigAttrib(window, fbconfigs[i], GLX_ACCUM_BLUE_SIZE);
        result[*found].accumAlphaBits = getFBConfigAttrib(window, fbconfigs[i], GLX_ACCUM_ALPHA_SIZE);

        result[*found].auxBuffers = getFBConfigAttrib(window, fbconfigs[i], GLX_AUX_BUFFERS);
        result[*found].stereo = getFBConfigAttrib(window, fbconfigs[i], GLX_STEREO);

        if (window->GLX.has_GLX_ARB_multisample)
            result[*found].samples = getFBConfigAttrib(window, fbconfigs[i], GLX_SAMPLES);
        else
            result[*found].samples = 0;

        result[*found].platformID = (GLFWintptr) getFBConfigAttrib(window, fbconfigs[i], GLX_FBCONFIG_ID);

        (*found)++;
    }

    XFree(fbconfigs);

    return result;
}


//========================================================================
// Create the OpenGL context
//========================================================================

#define setGLXattrib(attribs, index, attribName, attribValue) \
    attribs[index++] = attribName; \
    attribs[index++] = attribValue;

static int createContext(_GLFWwindow* window, const _GLFWwndconfig* wndconfig, GLXFBConfigID fbconfigID)
{
    int attribs[40];
    int flags, dummy, index;
    GLXFBConfig* fbconfig;

    // Retrieve the previously selected GLXFBConfig
    {
        index = 0;

        setGLXattrib(attribs, index, GLX_FBCONFIG_ID, (int) fbconfigID);
        setGLXattrib(attribs, index, None, None);

        if (window->GLX.has_GLX_SGIX_fbconfig)
        {
            fbconfig = window->GLX.ChooseFBConfigSGIX(_glfwLibrary.X11.display,
                                                      _glfwLibrary.X11.screen,
                                                      attribs,
                                                      &dummy);
        }
        else
        {
            fbconfig = glXChooseFBConfig(_glfwLibrary.X11.display,
                                         _glfwLibrary.X11.screen,
                                         attribs,
                                         &dummy);
        }

        if (fbconfig == NULL)
        {
            fprintf(stderr, "Unable to retrieve the selected GLXFBConfig\n");
            _glfwSetError(GLFW_INTERNAL_ERROR);
            return GL_FALSE;
        }
    }

    // Retrieve the corresponding visual
    if (window->GLX.has_GLX_SGIX_fbconfig)
    {
        window->GLX.visual = window->GLX.GetVisualFromFBConfigSGIX(_glfwLibrary.X11.display,
                                                                   *fbconfig);
    }
    else
    {
        window->GLX.visual = glXGetVisualFromFBConfig(_glfwLibrary.X11.display,
                                                      *fbconfig);
    }

    if (window->GLX.visual == NULL)
    {
        XFree(fbconfig);

        fprintf(stderr, "Unable to retrieve visual for GLXFBconfig\n");
        _glfwSetError(GLFW_INTERNAL_ERROR);
        return GL_FALSE;
    }

    if (window->GLX.has_GLX_ARB_create_context)
    {
        index = 0;

        if (wndconfig->glMajor != 1 || wndconfig->glMinor != 0)
        {
            // Request an explicitly versioned context

            setGLXattrib(attribs, index, GLX_CONTEXT_MAJOR_VERSION_ARB, wndconfig->glMajor);
            setGLXattrib(attribs, index, GLX_CONTEXT_MINOR_VERSION_ARB, wndconfig->glMinor);
        }

        if (wndconfig->glForward || wndconfig->glDebug)
        {
            flags = 0;

            if (wndconfig->glForward)
                flags |= GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;

            if (wndconfig->glDebug)
                flags |= GLX_CONTEXT_DEBUG_BIT_ARB;

            setGLXattrib(attribs, index, GLX_CONTEXT_FLAGS_ARB, flags);
        }

        if (wndconfig->glProfile)
        {
            if (!window->GLX.has_GLX_ARB_create_context_profile)
            {
                fprintf(stderr, "OpenGL profile requested but GLX_ARB_create_context_profile "
                                "is unavailable\n");
                _glfwSetError(GLFW_UNAVAILABLE_VERSION);
                return GL_FALSE;
            }

            if (wndconfig->glProfile == GLFW_OPENGL_CORE_PROFILE)
                flags = GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
            else
                flags = GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;

            setGLXattrib(attribs, index, GLX_CONTEXT_PROFILE_MASK_ARB, flags);
        }

        setGLXattrib(attribs, index, None, None);

        window->GLX.context = window->GLX.CreateContextAttribsARB(_glfwLibrary.X11.display,
                                                                  *fbconfig,
                                                                  NULL,
                                                                  True,
                                                                  attribs);
    }
    else
    {
        if (window->GLX.has_GLX_SGIX_fbconfig)
        {
            window->GLX.context = window->GLX.CreateContextWithConfigSGIX(_glfwLibrary.X11.display,
                                                                          *fbconfig,
                                                                          GLX_RGBA_TYPE,
                                                                          NULL,
                                                                          True);
        }
        else
        {
            window->GLX.context = glXCreateNewContext(_glfwLibrary.X11.display,
                                                      *fbconfig,
                                                      GLX_RGBA_TYPE,
                                                      NULL,
                                                      True);
        }
    }

    XFree(fbconfig);

    if (window->GLX.context == NULL)
    {
        fprintf(stderr, "Unable to create OpenGL context\n");
        // TODO: Handle all the various error codes here
        _glfwSetError(GLFW_INTERNAL_ERROR);
        return GL_FALSE;
    }

    window->GLX.fbconfigID = fbconfigID;

    return GL_TRUE;
}

#undef setGLXattrib


//========================================================================
// Initialize GLX-specific extensions
//========================================================================

static void initGLXExtensions(_GLFWwindow* window)
{
    if (_glfwPlatformExtensionSupported("GLX_SGI_swap_control"))
    {
        window->GLX.SwapIntervalSGI = (PFNGLXSWAPINTERVALSGIPROC)
            _glfwPlatformGetProcAddress("glXSwapIntervalSGI");

        if (window->GLX.SwapIntervalSGI)
            window->GLX.has_GLX_SGI_swap_control = GL_TRUE;
    }

    if (_glfwPlatformExtensionSupported("GLX_SGIX_fbconfig"))
    {
        window->GLX.GetFBConfigAttribSGIX = (PFNGLXGETFBCONFIGATTRIBSGIXPROC)
            _glfwPlatformGetProcAddress("glXGetFBConfigAttribSGIX");
        window->GLX.ChooseFBConfigSGIX = (PFNGLXCHOOSEFBCONFIGSGIXPROC)
            _glfwPlatformGetProcAddress("glXChooseFBConfigSGIX");
        window->GLX.CreateContextWithConfigSGIX = (PFNGLXCREATECONTEXTWITHCONFIGSGIXPROC)
            _glfwPlatformGetProcAddress("glXCreateContextWithConfigSGIX");
        window->GLX.GetVisualFromFBConfigSGIX = (PFNGLXGETVISUALFROMFBCONFIGSGIXPROC)
            _glfwPlatformGetProcAddress("glXGetVisualFromFBConfigSGIX");

        if (window->GLX.GetFBConfigAttribSGIX &&
            window->GLX.ChooseFBConfigSGIX &&
            window->GLX.CreateContextWithConfigSGIX &&
            window->GLX.GetVisualFromFBConfigSGIX)
        {
            window->GLX.has_GLX_SGIX_fbconfig = GL_TRUE;
        }
    }

    if (_glfwPlatformExtensionSupported("GLX_ARB_multisample"))
        window->GLX.has_GLX_ARB_multisample = GL_TRUE;

    if (_glfwPlatformExtensionSupported("GLX_ARB_create_context"))
    {
        window->GLX.CreateContextAttribsARB = (PFNGLXCREATECONTEXTATTRIBSARBPROC)
            _glfwPlatformGetProcAddress("glXCreateContextAttribsARB");

        if (window->GLX.CreateContextAttribsARB)
            window->GLX.has_GLX_ARB_create_context = GL_TRUE;
    }

    if (_glfwPlatformExtensionSupported("GLX_ARB_create_context_profile"))
        window->GLX.has_GLX_ARB_create_context_profile = GL_TRUE;
}


//========================================================================
// Create the X11 window (and its colormap)
//========================================================================

static GLboolean createWindow(_GLFWwindow* window,
                              int width, int height,
                              const _GLFWwndconfig* wndconfig)
{
    XEvent event;
    unsigned long wamask;
    XSetWindowAttributes wa;

    // Every window needs a colormap
    // Create one based on the visual used by the current context

    window->X11.colormap = XCreateColormap(_glfwLibrary.X11.display,
                                        _glfwLibrary.X11.root,
                                        window->GLX.visual->visual,
                                        AllocNone);

    // Create the actual window
    {
        wamask = CWBorderPixel | CWColormap | CWEventMask;

        wa.colormap = window->X11.colormap;
        wa.border_pixel = 0;
        wa.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask |
            PointerMotionMask | ButtonPressMask | ButtonReleaseMask |
            ExposureMask | FocusChangeMask | VisibilityChangeMask;

        if (wndconfig->mode == GLFW_WINDOWED)
        {
            // The /only/ reason we are setting the background pixel here is
            // that otherwise our window wont get any decorations on systems
            // using Compiz on Intel hardware
            wa.background_pixel = BlackPixel(_glfwLibrary.X11.display, _glfwLibrary.X11.screen);
            wamask |= CWBackPixel;
        }

        window->X11.window = XCreateWindow(
            _glfwLibrary.X11.display,
            _glfwLibrary.X11.root,
            0, 0,                            // Upper left corner of this window on root
            window->width, window->height,
            0,                               // Border width
            window->GLX.visual->depth,          // Color depth
            InputOutput,
            window->GLX.visual->visual,
            wamask,
            &wa
        );

        if (!window->X11.window)
        {
            // TODO: Handle all the various error codes here
            _glfwSetError(GLFW_INTERNAL_ERROR);
            return GL_FALSE;
        }
    }

    // Check whether an EWMH-compliant window manager is running
    window->X11.hasEWMH = checkForEWMH(window);

    if (window->mode == GLFW_FULLSCREEN && !window->X11.hasEWMH)
    {
        // This is the butcher's way of removing window decorations
        // Setting the override-redirect attribute on a window makes the window
        // manager ignore the window completely (ICCCM, section 4)
        // The good thing is that this makes undecorated fullscreen windows
        // easy to do; the bad thing is that we have to do everything manually
        // and some things (like iconify/restore) won't work at all, as they're
        // usually performed by the window manager

        XSetWindowAttributes attributes;
        attributes.override_redirect = True;
        XChangeWindowAttributes(_glfwLibrary.X11.display,
                                window->X11.window,
                                CWOverrideRedirect,
                                &attributes);

        window->X11.overrideRedirect = GL_TRUE;
    }

    // Find or create the protocol atom for window close notifications
    window->X11.wmDeleteWindow = XInternAtom(_glfwLibrary.X11.display,
                                          "WM_DELETE_WINDOW",
                                          False);

    // Declare the WM protocols we support
    {
        int count = 0;
        Atom protocols[2];

        // The WM_DELETE_WINDOW ICCCM protocol
        // Basic window close notification protocol
        if (window->X11.wmDeleteWindow != None)
            protocols[count++] = window->X11.wmDeleteWindow;

        // The _NET_WM_PING EWMH protocol
        // Tells the WM to ping our window and flag us as unresponsive if we
        // don't reply within a few seconds
        if (window->X11.wmPing != None)
            protocols[count++] = window->X11.wmPing;

        if (count > 0)
        {
            XSetWMProtocols(_glfwLibrary.X11.display, window->X11.window,
                            protocols, count);
        }
    }

    // Set ICCCM WM_HINTS property
    {
        XWMHints* hints = XAllocWMHints();
        if (!hints)
        {
            _glfwSetError(GLFW_OUT_OF_MEMORY);
            return GL_FALSE;
        }

        hints->flags = StateHint;
        hints->initial_state = NormalState;

        XSetWMHints(_glfwLibrary.X11.display, window->X11.window, hints);
        XFree(hints);
    }

    // Set ICCCM WM_NORMAL_HINTS property (even if no parts are set)
    {
        XSizeHints* hints = XAllocSizeHints();
        if (!hints)
        {
            _glfwSetError(GLFW_OUT_OF_MEMORY);
            return GL_FALSE;
        }

        hints->flags = 0;

        if (wndconfig->windowNoResize)
        {
            hints->flags |= (PMinSize | PMaxSize);
            hints->min_width  = hints->max_width  = window->width;
            hints->min_height = hints->max_height = window->height;
        }

        XSetWMNormalHints(_glfwLibrary.X11.display, window->X11.window, hints);
        XFree(hints);
    }

    _glfwPlatformSetWindowTitle(window, "GLFW Window");

    // Make sure the window is mapped before proceeding
    XMapWindow(_glfwLibrary.X11.display, window->X11.window);
    XPeekIfEvent(_glfwLibrary.X11.display, &event, isMapNotify,
                 (char*) window->X11.window);

    return GL_TRUE;
}


//========================================================================
// Enter fullscreen mode
//========================================================================

static void enterFullscreenMode(_GLFWwindow* window)
{
    if (!_glfwLibrary.X11.saver.changed)
    {
        // Remember old screen saver settings
        XGetScreenSaver(_glfwLibrary.X11.display,
                        &_glfwLibrary.X11.saver.timeout,
                        &_glfwLibrary.X11.saver.interval,
                        &_glfwLibrary.X11.saver.blanking,
                        &_glfwLibrary.X11.saver.exposure);

        // Disable screen saver
        XSetScreenSaver(_glfwLibrary.X11.display, 0, 0, DontPreferBlanking,
                        DefaultExposures);

        _glfwLibrary.X11.saver.changed = GL_TRUE;
    }

    _glfwSetVideoMode(_glfwLibrary.X11.screen,
                      &window->width, &window->height,
                      &window->refreshRate);

    if (window->X11.hasEWMH &&
        window->X11.wmState != None &&
        window->X11.wmStateFullscreen != None)
    {
        if (window->X11.wmActiveWindow != None)
        {
            // Ask the window manager to raise and focus the GLFW window
            // Only focused windows with the _NET_WM_STATE_FULLSCREEN state end
            // up on top of all other windows ("Stacking order" in EWMH spec)

            XEvent event;
            memset(&event, 0, sizeof(event));

            event.type = ClientMessage;
            event.xclient.window = window->X11.window;
            event.xclient.format = 32; // Data is 32-bit longs
            event.xclient.message_type = window->X11.wmActiveWindow;
            event.xclient.data.l[0] = 1; // Sender is a normal application
            event.xclient.data.l[1] = 0; // We don't really know the timestamp

            XSendEvent(_glfwLibrary.X11.display,
                       _glfwLibrary.X11.root,
                       False,
                       SubstructureNotifyMask | SubstructureRedirectMask,
                       &event);
        }

        // Ask the window manager to make the GLFW window a fullscreen window
        // Fullscreen windows are undecorated and, when focused, are kept
        // on top of all other windows

        XEvent event;
        memset(&event, 0, sizeof(event));

        event.type = ClientMessage;
        event.xclient.window = window->X11.window;
        event.xclient.format = 32; // Data is 32-bit longs
        event.xclient.message_type = window->X11.wmState;
        event.xclient.data.l[0] = _NET_WM_STATE_ADD;
        event.xclient.data.l[1] = window->X11.wmStateFullscreen;
        event.xclient.data.l[2] = 0; // No secondary property
        event.xclient.data.l[3] = 1; // Sender is a normal application

        XSendEvent(_glfwLibrary.X11.display,
                   _glfwLibrary.X11.root,
                   False,
                   SubstructureNotifyMask | SubstructureRedirectMask,
                   &event);
    }
    else if (window->X11.overrideRedirect)
    {
        // In override-redirect mode, we have divorced ourselves from the
        // window manager, so we need to do everything manually

        XRaiseWindow(_glfwLibrary.X11.display, window->X11.window);
        XSetInputFocus(_glfwLibrary.X11.display, window->X11.window,
                        RevertToParent, CurrentTime);
        XMoveWindow(_glfwLibrary.X11.display, window->X11.window, 0, 0);
        XResizeWindow(_glfwLibrary.X11.display, window->X11.window,
                      window->width, window->height);
    }

    if (_glfwLibrary.cursorLockWindow == window)
        _glfwPlatformHideMouseCursor(window);

    // HACK: Try to get window inside viewport (for virtual displays) by moving
    // the mouse cursor to the upper left corner (and then to the center)
    // This hack should be harmless on saner systems as well
    XWarpPointer(_glfwLibrary.X11.display, None, window->X11.window, 0,0,0,0, 0,0);
    XWarpPointer(_glfwLibrary.X11.display, None, window->X11.window, 0,0,0,0,
                 window->width / 2, window->height / 2);
}

//========================================================================
// Leave fullscreen mode
//========================================================================

static void leaveFullscreenMode(_GLFWwindow* window)
{
    _glfwRestoreVideoMode(_glfwLibrary.X11.screen);

    // Did we change the screen saver setting?
    if (_glfwLibrary.X11.saver.changed)
    {
        // Restore old screen saver settings
        XSetScreenSaver(_glfwLibrary.X11.display,
                        _glfwLibrary.X11.saver.timeout,
                        _glfwLibrary.X11.saver.interval,
                        _glfwLibrary.X11.saver.blanking,
                        _glfwLibrary.X11.saver.exposure);

        _glfwLibrary.X11.saver.changed = GL_FALSE;
    }

    if (window->X11.hasEWMH &&
        window->X11.wmState != None &&
        window->X11.wmStateFullscreen != None)
    {
        // Ask the window manager to make the GLFW window a normal window
        // Normal windows usually have frames and other decorations

        XEvent event;
        memset(&event, 0, sizeof(event));

        event.type = ClientMessage;
        event.xclient.window = window->X11.window;
        event.xclient.format = 32; // Data is 32-bit longs
        event.xclient.message_type = window->X11.wmState;
        event.xclient.data.l[0] = _NET_WM_STATE_REMOVE;
        event.xclient.data.l[1] = window->X11.wmStateFullscreen;
        event.xclient.data.l[2] = 0; // No secondary property
        event.xclient.data.l[3] = 1; // Sender is a normal application

        XSendEvent(_glfwLibrary.X11.display,
                   _glfwLibrary.X11.root,
                   False,
                   SubstructureNotifyMask | SubstructureRedirectMask,
                   &event);
    }

    if (_glfwLibrary.cursorLockWindow == window)
        _glfwPlatformShowMouseCursor(window);
}


//========================================================================
// Return the GLFW window corresponding to the specified X11 window
//========================================================================
static _GLFWwindow* findWindow(Window handle)
{
    _GLFWwindow* window;

    for (window = _glfwLibrary.windowListHead;  window;  window = window->next)
    {
        if (window->X11.window == handle)
            return window;
    }

    return NULL;
}


//========================================================================
// Get and process next X event (called by _glfwPlatformPollEvents)
//========================================================================

static void processSingleEvent(void)
{
    _GLFWwindow* window;

    XEvent event;
    XNextEvent(_glfwLibrary.X11.display, &event);

    switch (event.type)
    {
        case KeyPress:
        {
            // A keyboard key was pressed
            window = findWindow(event.xkey.window);
            if (window == NULL)
            {
                fprintf(stderr, "Cannot find GLFW window structure for KeyPress event\n");
                return;
            }

            // Translate and report key press
            _glfwInputKey(window, translateKey(event.xkey.keycode), GLFW_PRESS);

            // Translate and report character input
            _glfwInputChar(window, translateChar(&event.xkey));

            break;
        }

        case KeyRelease:
        {
            // A keyboard key was released
            window = findWindow(event.xkey.window);
            if (window == NULL)
            {
                fprintf(stderr, "Cannot find GLFW window structure for KeyRelease event\n");
                return;
            }

            // Do not report key releases for key repeats. For key repeats we
            // will get KeyRelease/KeyPress pairs with similar or identical
            // time stamps. User selected key repeat filtering is handled in
            // _glfwInputKey()/_glfwInputChar().
            if (XEventsQueued(_glfwLibrary.X11.display, QueuedAfterReading))
            {
                XEvent nextEvent;
                XPeekEvent(_glfwLibrary.X11.display, &nextEvent);

                if (nextEvent.type == KeyPress &&
                    nextEvent.xkey.window == event.xkey.window &&
                    nextEvent.xkey.keycode == event.xkey.keycode)
                {
                    // This last check is a hack to work around key repeats
                    // leaking through due to some sort of time drift
                    // Toshiyuki Takahashi can press a button 16 times per
                    // second so it's fairly safe to assume that no human is
                    // pressing the key 50 times per second (value is ms)
                    if ((nextEvent.xkey.time - event.xkey.time) < 20)
                    {
                        // Do not report anything for this event
                        break;
                    }
                }
            }

            // Translate and report key release
            _glfwInputKey(window, translateKey(event.xkey.keycode), GLFW_RELEASE);

            break;
        }

        case ButtonPress:
        {
            // A mouse button was pressed or a scrolling event occurred
            window = findWindow(event.xbutton.window);
            if (window == NULL)
            {
                fprintf(stderr, "Cannot find GLFW window structure for ButtonPress event\n");
                return;
            }

            if (event.xbutton.button == Button1)
                _glfwInputMouseClick(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS);
            else if (event.xbutton.button == Button2)
                _glfwInputMouseClick(window, GLFW_MOUSE_BUTTON_MIDDLE, GLFW_PRESS);
            else if (event.xbutton.button == Button3)
                _glfwInputMouseClick(window, GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS);

            // XFree86 3.3.2 and later translates mouse wheel up/down into
            // mouse button 4 & 5 presses
            else if (event.xbutton.button == Button4)
            {
                window->wheelPos++;  // To verify: is this up or down?
                if (window->mouseWheelCallback)
                    window->mouseWheelCallback(window, window->wheelPos);
            }
            else if (event.xbutton.button == Button5)
            {
                window->wheelPos--;
                if (window->mouseWheelCallback)
                    window->mouseWheelCallback(window, window->wheelPos);
            }
            break;
        }

        case ButtonRelease:
        {
            // A mouse button was released
            window = findWindow(event.xbutton.window);
            if (window == NULL)
            {
                fprintf(stderr, "Cannot find GLFW window structure for ButtonRelease event\n");
                return;
            }

            if (event.xbutton.button == Button1)
            {
                _glfwInputMouseClick(window,
                                     GLFW_MOUSE_BUTTON_LEFT,
                                     GLFW_RELEASE);
            }
            else if (event.xbutton.button == Button2)
            {
                _glfwInputMouseClick(window,
                                     GLFW_MOUSE_BUTTON_MIDDLE,
                                     GLFW_RELEASE);
            }
            else if (event.xbutton.button == Button3)
            {
                _glfwInputMouseClick(window,
                                     GLFW_MOUSE_BUTTON_RIGHT,
                                     GLFW_RELEASE);
            }
            break;
        }

        case MotionNotify:
        {
            // The mouse cursor was moved
            window = findWindow(event.xmotion.window);
            if (window == NULL)
            {
                fprintf(stderr, "Cannot find GLFW window structure for MotionNotify event\n");
                return;
            }

            if (event.xmotion.x != window->X11.cursorPosX ||
                event.xmotion.y != window->X11.cursorPosY)
            {
                // The mouse cursor was moved and we didn't do it

                if (_glfwLibrary.cursorLockWindow == window)
                {
                    if (window->X11.pointerHidden)
                    {
                        window->mousePosX += event.xmotion.x -
                                             window->X11.cursorPosX;
                        window->mousePosY += event.xmotion.y -
                                             window->X11.cursorPosY;
                    }
                }
                else
                {
                    window->mousePosX = event.xmotion.x;
                    window->mousePosY = event.xmotion.y;
                }

                window->X11.cursorPosX = event.xmotion.x;
                window->X11.cursorPosY = event.xmotion.y;
                window->X11.mouseMoved = GL_TRUE;

                if (window->mousePosCallback)
                {
                    window->mousePosCallback(window,
                                             window->mousePosX,
                                             window->mousePosY);
                }
            }
            break;
        }

        case ConfigureNotify:
        {
            // The window configuration changed somehow
            window = findWindow(event.xconfigure.window);
            if (window == NULL)
            {
                fprintf(stderr, "Cannot find GLFW window structure for ConfigureNotify event\n");
                return;
            }

            if (event.xconfigure.width != window->width ||
                event.xconfigure.height != window->height)
            {
                // The window was resized

                window->width = event.xconfigure.width;
                window->height = event.xconfigure.height;
                if (window->windowSizeCallback)
                {
                    window->windowSizeCallback(window,
                                               window->width,
                                               window->height);
                }
            }
            break;
        }

        case ClientMessage:
        {
            // Custom client message, probably from the window manager
            window = findWindow(event.xclient.window);
            if (window == NULL)
            {
                fprintf(stderr, "Cannot find GLFW window structure for ClientMessage event\n");
                return;
            }

            if ((Atom) event.xclient.data.l[ 0 ] == window->X11.wmDeleteWindow)
            {
                // The window manager was asked to close the window, for example by
                // the user pressing a 'close' window decoration button

                window->closed = GL_TRUE;
            }
            else if (window->X11.wmPing != None &&
                     (Atom) event.xclient.data.l[ 0 ] == window->X11.wmPing)
            {
                // The window manager is pinging us to make sure we are still
                // responding to events

                event.xclient.window = _glfwLibrary.X11.root;
                XSendEvent(_glfwLibrary.X11.display,
                           event.xclient.window,
                           False,
                           SubstructureNotifyMask | SubstructureRedirectMask,
                           &event);
            }

            break;
        }

        case MapNotify:
        {
            // The window was mapped
            window = findWindow(event.xmap.window);
            if (window == NULL)
            {
                fprintf(stderr, "Cannot find GLFW window structure for MapNotify event\n");
                return;
            }

            window->iconified = GL_FALSE;
            break;
        }

        case UnmapNotify:
        {
            // The window was unmapped
            window = findWindow(event.xmap.window);
            if (window == NULL)
            {
                fprintf(stderr, "Cannot find GLFW window structure for UnmapNotify event\n");
                return;
            }

            window->iconified = GL_TRUE;
            break;
        }

        case FocusIn:
        {
            // The window gained focus
            window = findWindow(event.xfocus.window);
            if (window == NULL)
            {
                fprintf(stderr, "Cannot find GLFW window structure for FocusIn event\n");
                return;
            }

            window->active = GL_TRUE;

            if (_glfwLibrary.cursorLockWindow == window)
                _glfwPlatformHideMouseCursor(window);

            break;
        }

        case FocusOut:
        {
            // The window lost focus
            window = findWindow(event.xfocus.window);
            if (window == NULL)
            {
                fprintf(stderr, "Cannot find GLFW window structure for FocusOut event\n");
                return;
            }

            window->active = GL_FALSE;
            _glfwInputDeactivation(window);

            if (_glfwLibrary.cursorLockWindow == window)
                _glfwPlatformShowMouseCursor(window);

            break;
        }

        case Expose:
        {
            // The window's contents was damaged
            window = findWindow(event.xexpose.window);
            if (window == NULL)
            {
                fprintf(stderr, "Cannot find GLFW window structure for Expose event\n");
                return;
            }

            if (window->windowRefreshCallback)
                window->windowRefreshCallback(window);

            break;
        }

        // Was the window destroyed?
        case DestroyNotify:
            return;

        default:
        {
#if defined(_GLFW_HAS_XRANDR)
            switch (event.type - _glfwLibrary.X11.XRandR.eventBase)
            {
                case RRScreenChangeNotify:
                {
                    // Show XRandR that we really care
                    XRRUpdateConfiguration(&event);
                    break;
                }
            }
#endif
            break;
        }
    }
}



//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Here is where the window is created, and
// the OpenGL rendering context is created
//========================================================================

int _glfwPlatformOpenWindow(_GLFWwindow* window,
                            int width, int height,
                            const _GLFWwndconfig* wndconfig,
                            const _GLFWfbconfig* fbconfig)
{
    _GLFWfbconfig closest;

    window->refreshRate    = wndconfig->refreshRate;
    window->windowNoResize = wndconfig->windowNoResize;

    // Create the invisible cursor for hidden cursor mode
    window->X11.cursor = createNULLCursor(_glfwLibrary.X11.display, _glfwLibrary.X11.root);

    initGLXExtensions(window);

    // Choose the best available fbconfig
    {
        unsigned int fbcount;
        _GLFWfbconfig* fbconfigs;
        const _GLFWfbconfig* result;

        fbconfigs = getFBConfigs(window, &fbcount);
        if (!fbconfigs)
            return GL_FALSE;

        result = _glfwChooseFBConfig(fbconfig, fbconfigs, fbcount);
        if (!result)
        {
            free(fbconfigs);
            return GL_FALSE;
        }

        closest = *result;
        free(fbconfigs);
    }

    if (!createContext(window, wndconfig, (GLXFBConfigID) closest.platformID))
        return GL_FALSE;

    if (!createWindow(window, width, height, wndconfig))
        return GL_FALSE;

    if (wndconfig->mode == GLFW_FULLSCREEN)
    {
#if defined(_GLFW_HAS_XRANDR)
        // Request screen change notifications
        if (_glfwLibrary.X11.XRandR.available)
        {
            XRRSelectInput(_glfwLibrary.X11.display,
                           window->X11.window,
                           RRScreenChangeNotifyMask);
        }
#endif
        enterFullscreenMode(window);
    }

    // Process the window map event and any other that may have arrived
    _glfwPlatformPollEvents();

    // Retrieve and set initial cursor position
    {
        Window cursorWindow, cursorRoot;
        int windowX, windowY, rootX, rootY;
        unsigned int mask;

        XQueryPointer(_glfwLibrary.X11.display,
                      window->X11.window,
                      &cursorRoot,
                      &cursorWindow,
                      &rootX, &rootY,
                      &windowX, &windowY,
                      &mask);

        // TODO: Probably check for some corner cases here.

        window->mousePosX = windowX;
        window->mousePosY = windowY;
    }

    return GL_TRUE;
}


//========================================================================
// Make the OpenGL context associated with the specified window current
//========================================================================

int _glfwPlatformMakeWindowCurrent(_GLFWwindow* window)
{
    if (window)
    {
        glXMakeCurrent(_glfwLibrary.X11.display,
                       window->X11.window,
                       window->GLX.context);
    }
    else
        glXMakeCurrent(_glfwLibrary.X11.display, None, NULL);
}


//========================================================================
// Properly kill the window/video display
//========================================================================

void _glfwPlatformCloseWindow(_GLFWwindow* window)
{
    if (window->mode == GLFW_FULLSCREEN)
        leaveFullscreenMode(window);

    if (window->GLX.context)
    {
        // Release and destroy the context
        glXMakeCurrent(_glfwLibrary.X11.display, None, NULL);
        glXDestroyContext(_glfwLibrary.X11.display, window->GLX.context);
        window->GLX.context = NULL;
    }

    if (window->GLX.visual)
    {
        XFree(window->GLX.visual);
        window->GLX.visual = NULL;
    }

    if (window->X11.window)
    {
        XUnmapWindow(_glfwLibrary.X11.display, window->X11.window);
        XDestroyWindow(_glfwLibrary.X11.display, window->X11.window);
        window->X11.window = (Window) 0;
    }

    if (window->X11.colormap)
    {
        XFreeColormap(_glfwLibrary.X11.display, window->X11.colormap);
        window->X11.colormap = (Colormap) 0;
    }

    if (window->X11.cursor)
    {
        XFreeCursor(_glfwLibrary.X11.display, window->X11.cursor);
        window->X11.cursor = (Cursor) 0;
    }
}


//========================================================================
// Set the window title
//========================================================================

void _glfwPlatformSetWindowTitle(_GLFWwindow* window, const char* title)
{
    // Set window & icon title
    XStoreName(_glfwLibrary.X11.display, window->X11.window, title);
    XSetIconName(_glfwLibrary.X11.display, window->X11.window, title);
}


//========================================================================
// Set the window size
//========================================================================

void _glfwPlatformSetWindowSize(_GLFWwindow* window, int width, int height)
{
    int mode = 0, rate, sizeChanged = GL_FALSE;
    XSizeHints* sizehints;

    rate = window->refreshRate;

    if (window->mode == GLFW_FULLSCREEN)
    {
        // Get the closest matching video mode for the specified window size
        mode = _glfwGetClosestVideoMode(_glfwLibrary.X11.screen, &width, &height, &rate);
    }

    if (window->windowNoResize)
    {
        // Update window size restrictions to match new window size

        sizehints = XAllocSizeHints();
        sizehints->flags = 0;

        sizehints->min_width  = sizehints->max_width  = width;
        sizehints->min_height = sizehints->max_height = height;

        XSetWMNormalHints(_glfwLibrary.X11.display, window->X11.window, sizehints);
        XFree(sizehints);
    }

    // Change window size before changing fullscreen mode?
    if (window->mode == GLFW_FULLSCREEN && (width > window->width))
    {
        XResizeWindow(_glfwLibrary.X11.display, window->X11.window, width, height);
        sizeChanged = GL_TRUE;
    }

    if (window->mode == GLFW_FULLSCREEN)
    {
        // Change video mode, keeping current refresh rate
        _glfwSetVideoModeMODE(_glfwLibrary.X11.screen, mode, window->refreshRate);
    }

    // Set window size (if not already changed)
    if (!sizeChanged)
        XResizeWindow(_glfwLibrary.X11.display, window->X11.window, width, height);
}


//========================================================================
// Set the window position.
//========================================================================

void _glfwPlatformSetWindowPos(_GLFWwindow* window, int x, int y)
{
    XMoveWindow(_glfwLibrary.X11.display, window->X11.window, x, y);
}


//========================================================================
// Window iconification
//========================================================================

void _glfwPlatformIconifyWindow(_GLFWwindow* window)
{
    if (window->X11.overrideRedirect)
    {
        // We can't iconify/restore override-redirect windows, as that's
        // performed by the window manager
        return;
    }

    XIconifyWindow(_glfwLibrary.X11.display, window->X11.window, _glfwLibrary.X11.screen);
}


//========================================================================
// Window un-iconification
//========================================================================

void _glfwPlatformRestoreWindow(_GLFWwindow* window)
{
    if (window->X11.overrideRedirect)
    {
        // We can't iconify/restore override-redirect windows, as that's
        // performed by the window manager
        return;
    }

    XMapWindow(_glfwLibrary.X11.display, window->X11.window);
}


//========================================================================
// Swap OpenGL buffers
//========================================================================

void _glfwPlatformSwapBuffers(void)
{
    glXSwapBuffers(_glfwLibrary.X11.display,
                   _glfwLibrary.currentWindow->X11.window);
}


//========================================================================
// Set double buffering swap interval
//========================================================================

void _glfwPlatformSwapInterval(int interval)
{
    _GLFWwindow* window = _glfwLibrary.currentWindow;

    if (window->GLX.has_GLX_SGI_swap_control)
        window->GLX.SwapIntervalSGI(interval);
}


//========================================================================
// Read back framebuffer parameters from the context
//========================================================================

void _glfwPlatformRefreshWindowParams(void)
{
    int dummy;
    GLXFBConfig* fbconfig;
#if defined(_GLFW_HAS_XRANDR)
    XRRScreenConfiguration* sc;
#elif defined(_GLFW_HAS_XF86VIDMODE)
    XF86VidModeModeLine modeline;
    int dotclock;
    float pixels_per_second, pixels_per_frame;
#endif
    _GLFWwindow* window = _glfwLibrary.currentWindow;

    int attribs[] = { GLX_FBCONFIG_ID, window->GLX.fbconfigID, None };

    if (window->GLX.has_GLX_SGIX_fbconfig)
    {
        fbconfig = window->GLX.ChooseFBConfigSGIX(_glfwLibrary.X11.display,
                                                  _glfwLibrary.X11.screen,
                                                  attribs,
                                                  &dummy);
    }
    else
    {
        fbconfig = glXChooseFBConfig(_glfwLibrary.X11.display,
                                     _glfwLibrary.X11.screen,
                                     attribs,
                                     &dummy);
    }

    if (fbconfig == NULL)
    {
        // This should never ever happen
        // TODO: Flag this as an error and propagate up
        fprintf(stderr, "Cannot find known GLXFBConfig by ID. "
                        "This cannot happen. Have a nice day.\n");
        abort();
    }

    // There is no clear definition of an "accelerated" context on X11/GLX, and
    // true sounds better than false, so we hardcode true here
    window->accelerated = GL_TRUE;

    window->redBits = getFBConfigAttrib(window, *fbconfig, GLX_RED_SIZE);
    window->greenBits = getFBConfigAttrib(window, *fbconfig, GLX_GREEN_SIZE);
    window->blueBits = getFBConfigAttrib(window, *fbconfig, GLX_BLUE_SIZE);

    window->alphaBits = getFBConfigAttrib(window, *fbconfig, GLX_ALPHA_SIZE);
    window->depthBits = getFBConfigAttrib(window, *fbconfig, GLX_DEPTH_SIZE);
    window->stencilBits = getFBConfigAttrib(window, *fbconfig, GLX_STENCIL_SIZE);

    window->accumRedBits = getFBConfigAttrib(window, *fbconfig, GLX_ACCUM_RED_SIZE);
    window->accumGreenBits = getFBConfigAttrib(window, *fbconfig, GLX_ACCUM_GREEN_SIZE);
    window->accumBlueBits = getFBConfigAttrib(window, *fbconfig, GLX_ACCUM_BLUE_SIZE);
    window->accumAlphaBits = getFBConfigAttrib(window, *fbconfig, GLX_ACCUM_ALPHA_SIZE);

    window->auxBuffers = getFBConfigAttrib(window, *fbconfig, GLX_AUX_BUFFERS);
    window->stereo = getFBConfigAttrib(window, *fbconfig, GLX_STEREO) ? GL_TRUE : GL_FALSE;

    // Get FSAA buffer sample count
    if (window->GLX.has_GLX_ARB_multisample)
        window->samples = getFBConfigAttrib(window, *fbconfig, GLX_SAMPLES);
    else
        window->samples = 0;

    // Default to refresh rate unknown (=0 according to GLFW spec)
    window->refreshRate = 0;

    // Retrieve refresh rate if possible
#if defined(_GLFW_HAS_XRANDR)
    if (_glfwLibrary.X11.XRandR.available)
    {
        sc = XRRGetScreenInfo(_glfwLibrary.X11.display, _glfwLibrary.X11.root);
        window->refreshRate = XRRConfigCurrentRate(sc);
        XRRFreeScreenConfigInfo(sc);
    }
#elif defined(_GLFW_HAS_XF86VIDMODE)
    if (_glfwLibrary.X11.XF86VidMode.available)
    {
        // Use the XF86VidMode extension to get current video mode
        XF86VidModeGetModeLine(_glfwLibrary.X11.display, _glfwLibrary.X11.screen,
                               &dotclock, &modeline);
        pixels_per_second = 1000.0f * (float) dotclock;
        pixels_per_frame  = (float) modeline.htotal * modeline.vtotal;
        window->refreshRate = (int)(pixels_per_second/pixels_per_frame+0.5);
    }
#endif

    XFree(fbconfig);
}


//========================================================================
// Poll for new window and input events
//========================================================================

void _glfwPlatformPollEvents(void)
{
    _GLFWwindow* window;

    // Flag that the cursor has not moved
    if (window = _glfwLibrary.cursorLockWindow)
        window->X11.mouseMoved = GL_FALSE;

    // Process all pending events
    while (XPending(_glfwLibrary.X11.display))
        processSingleEvent();

    // Did we get mouse movement in fully enabled hidden cursor mode?
    if (window = _glfwLibrary.cursorLockWindow)
    {
        if (window->X11.mouseMoved && window->X11.pointerHidden)
        {
            _glfwPlatformSetMouseCursorPos(window,
                                           window->width / 2,
                                           window->height / 2);
        }
    }

    for (window = _glfwLibrary.windowListHead;  window; )
    {
        if (window->closed && window->windowCloseCallback)
            window->closed = window->windowCloseCallback(window);

        if (window->closed)
        {
            _GLFWwindow* next = window->next;
            glfwCloseWindow(window);
            window = next;
        }
        else
            window = window->next;
    }
}


//========================================================================
// Wait for new window and input events
//========================================================================

void _glfwPlatformWaitEvents(void)
{
    XEvent event;

    // Block waiting for an event to arrive
    XNextEvent(_glfwLibrary.X11.display, &event);
    XPutBackEvent(_glfwLibrary.X11.display, &event);

    _glfwPlatformPollEvents();
}


//========================================================================
// Hide mouse cursor (lock it)
//========================================================================

void _glfwPlatformHideMouseCursor(_GLFWwindow* window)
{
    // Hide cursor
    if (!window->X11.pointerHidden)
    {
        XDefineCursor(_glfwLibrary.X11.display, window->X11.window, window->X11.cursor);
        window->X11.pointerHidden = GL_TRUE;
    }

    // Grab cursor to user window
    if (!window->X11.pointerGrabbed)
    {
        if (XGrabPointer(_glfwLibrary.X11.display, window->X11.window, True,
                         ButtonPressMask | ButtonReleaseMask |
                         PointerMotionMask, GrabModeAsync, GrabModeAsync,
                         window->X11.window, None, CurrentTime) ==
            GrabSuccess)
        {
            window->X11.pointerGrabbed = GL_TRUE;
        }
    }
}


//========================================================================
// Show mouse cursor (unlock it)
//========================================================================

void _glfwPlatformShowMouseCursor(_GLFWwindow* window)
{
    // Un-grab cursor (only in windowed mode: in fullscreen mode we still
    // want the mouse grabbed in order to confine the cursor to the window
    // area)
    if (window->X11.pointerGrabbed)
    {
        XUngrabPointer(_glfwLibrary.X11.display, CurrentTime);
        window->X11.pointerGrabbed = GL_FALSE;
    }

    // Show cursor
    if (window->X11.pointerHidden)
    {
        XUndefineCursor(_glfwLibrary.X11.display, window->X11.window);
        window->X11.pointerHidden = GL_FALSE;
    }
}


//========================================================================
// Set physical mouse cursor position
//========================================================================

void _glfwPlatformSetMouseCursorPos(_GLFWwindow* window, int x, int y)
{
    // Store the new position so we can recognise it later
    window->X11.cursorPosX = x;
    window->X11.cursorPosY = y;

    XWarpPointer(_glfwLibrary.X11.display, None, window->X11.window, 0,0,0,0, x, y);
}

