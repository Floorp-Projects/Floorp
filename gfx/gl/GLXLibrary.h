/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GLXLIBRARY_H
#define GFX_GLXLIBRARY_H

#include "GLContextTypes.h"
typedef realGLboolean GLboolean;

// stuff from glx.h
#include "X11/Xlib.h"
typedef struct __GLXcontextRec* GLXContext;
typedef XID GLXPixmap;
typedef XID GLXDrawable;
/* GLX 1.3 and later */
typedef struct __GLXFBConfigRec* GLXFBConfig;
typedef XID GLXFBConfigID;
typedef XID GLXContextID;
typedef XID GLXWindow;
typedef XID GLXPbuffer;
// end of stuff from glx.h
#include "prenv.h"

struct PRLibrary;
class gfxASurface;

namespace mozilla {
namespace gl {

class GLXLibrary
{
public:
    constexpr GLXLibrary()
    : xDestroyContextInternal(nullptr)
    , xMakeCurrentInternal(nullptr)
    , xGetCurrentContextInternal(nullptr)
    , xGetProcAddressInternal(nullptr)
    , xChooseFBConfigInternal(nullptr)
    , xGetFBConfigsInternal(nullptr)
    , xCreateNewContextInternal(nullptr)
    , xGetFBConfigAttribInternal(nullptr)
    , xSwapBuffersInternal(nullptr)
    , xQueryExtensionsStringInternal(nullptr)
    , xGetClientStringInternal(nullptr)
    , xQueryServerStringInternal(nullptr)
    , xCreatePixmapInternal(nullptr)
    , xCreateGLXPixmapWithConfigInternal(nullptr)
    , xDestroyPixmapInternal(nullptr)
    , xQueryVersionInternal(nullptr)
    , xBindTexImageInternal(nullptr)
    , xReleaseTexImageInternal(nullptr)
    , xWaitGLInternal(nullptr)
    , xWaitXInternal(nullptr)
    , xCreateContextAttribsInternal(nullptr)
    , xGetVideoSyncInternal(nullptr)
    , xWaitVideoSyncInternal(nullptr)
    , xSwapIntervalInternal(nullptr)
    , mInitialized(false), mTriedInitializing(false)
    , mUseTextureFromPixmap(false), mDebug(false)
    , mHasRobustness(false), mHasCreateContextAttribs(false)
    , mHasVideoSync(false)
    , mIsATI(false), mIsNVIDIA(false)
    , mClientIsMesa(false), mGLXMajorVersion(0)
    , mGLXMinorVersion(0)
    , mOGLLibrary(nullptr)
    {}

    void xDestroyContext(Display* display, GLXContext context);
    Bool xMakeCurrent(Display* display,
                      GLXDrawable drawable,
                      GLXContext context);

    GLXContext xGetCurrentContext();
    static void* xGetProcAddress(const char* procName);
    GLXFBConfig* xChooseFBConfig(Display* display,
                                 int screen,
                                 const int* attrib_list,
                                 int* nelements);
    GLXFBConfig* xGetFBConfigs(Display* display,
                               int screen,
                               int* nelements);
    GLXContext xCreateNewContext(Display* display,
                                 GLXFBConfig config,
                                 int render_type,
                                 GLXContext share_list,
                                 Bool direct);
    int xGetFBConfigAttrib(Display* display,
                           GLXFBConfig config,
                           int attribute,
                           int* value);
    void xSwapBuffers(Display* display, GLXDrawable drawable);
    const char* xQueryExtensionsString(Display* display,
                                       int screen);
    const char* xGetClientString(Display* display,
                                 int screen);
    const char* xQueryServerString(Display* display,
                                   int screen, int name);
    GLXPixmap xCreatePixmap(Display* display,
                            GLXFBConfig config,
                            Pixmap pixmap,
                            const int* attrib_list);
    GLXPixmap xCreateGLXPixmapWithConfig(Display* display,
                                         GLXFBConfig config,
                                         Pixmap pixmap);
    void xDestroyPixmap(Display* display, GLXPixmap pixmap);
    Bool xQueryVersion(Display* display,
                       int* major,
                       int* minor);
    void xBindTexImage(Display* display,
                       GLXDrawable drawable,
                       int buffer,
                       const int* attrib_list);
    void xReleaseTexImage(Display* display,
                          GLXDrawable drawable,
                          int buffer);
    void xWaitGL();
    void xWaitX();

    GLXContext xCreateContextAttribs(Display* display,
                                     GLXFBConfig config,
                                     GLXContext share_list,
                                     Bool direct,
                                     const int* attrib_list);

    int xGetVideoSync(unsigned int* count);
    int xWaitVideoSync(int divisor, int remainder, unsigned int* count);
    void xSwapInterval(Display* dpy, GLXDrawable drawable, int interval);

    bool EnsureInitialized();

    GLXPixmap CreatePixmap(gfxASurface* aSurface);
    void DestroyPixmap(Display* aDisplay, GLXPixmap aPixmap);
    void BindTexImage(Display* aDisplay, GLXPixmap aPixmap);
    void ReleaseTexImage(Display* aDisplay, GLXPixmap aPixmap);
    void UpdateTexImage(Display* aDisplay, GLXPixmap aPixmap);

    bool UseTextureFromPixmap() { return mUseTextureFromPixmap; }
    bool HasRobustness() { return mHasRobustness; }
    bool HasCreateContextAttribs() { return mHasCreateContextAttribs; }
    bool SupportsTextureFromPixmap(gfxASurface* aSurface);
    bool SupportsVideoSync();
    bool SupportsSwapControl() const { return bool(xSwapIntervalInternal); }
    bool IsATI() { return mIsATI; }
    bool IsMesa() { return mClientIsMesa; }
    bool GLXVersionCheck(int aMajor, int aMinor);

private:

    typedef void (GLAPIENTRY * PFNGLXDESTROYCONTEXTPROC) (Display*,
                                                          GLXContext);
    PFNGLXDESTROYCONTEXTPROC xDestroyContextInternal;
    typedef Bool (GLAPIENTRY * PFNGLXMAKECURRENTPROC) (Display*,
                                                       GLXDrawable,
                                                       GLXContext);
    PFNGLXMAKECURRENTPROC xMakeCurrentInternal;
    typedef GLXContext (GLAPIENTRY * PFNGLXGETCURRENTCONTEXT) ();
    PFNGLXGETCURRENTCONTEXT xGetCurrentContextInternal;
    typedef void* (GLAPIENTRY * PFNGLXGETPROCADDRESSPROC) (const char*);
    PFNGLXGETPROCADDRESSPROC xGetProcAddressInternal;
    typedef GLXFBConfig* (GLAPIENTRY * PFNGLXCHOOSEFBCONFIG) (Display*,
                                                              int,
                                                              const int*,
                                                              int*);
    PFNGLXCHOOSEFBCONFIG xChooseFBConfigInternal;
    typedef GLXFBConfig* (GLAPIENTRY * PFNGLXGETFBCONFIGS) (Display*,
                                                            int,
                                                            int*);
    PFNGLXGETFBCONFIGS xGetFBConfigsInternal;
    typedef GLXContext (GLAPIENTRY * PFNGLXCREATENEWCONTEXT) (Display*,
                                                              GLXFBConfig,
                                                              int,
                                                              GLXContext,
                                                              Bool);
    PFNGLXCREATENEWCONTEXT xCreateNewContextInternal;
    typedef int (GLAPIENTRY * PFNGLXGETFBCONFIGATTRIB) (Display*,
                                                        GLXFBConfig,
                                                        int,
                                                        int*);
    PFNGLXGETFBCONFIGATTRIB xGetFBConfigAttribInternal;

    typedef void (GLAPIENTRY * PFNGLXSWAPBUFFERS) (Display*,
                                                   GLXDrawable);
    PFNGLXSWAPBUFFERS xSwapBuffersInternal;
    typedef const char* (GLAPIENTRY * PFNGLXQUERYEXTENSIONSSTRING) (Display*,
                                                                    int);
    PFNGLXQUERYEXTENSIONSSTRING xQueryExtensionsStringInternal;
    typedef const char* (GLAPIENTRY * PFNGLXGETCLIENTSTRING) (Display*,
                                                              int);
    PFNGLXGETCLIENTSTRING xGetClientStringInternal;
    typedef const char* (GLAPIENTRY * PFNGLXQUERYSERVERSTRING) (Display*,
                                                                int,
                                                                int);
    PFNGLXQUERYSERVERSTRING xQueryServerStringInternal;

    typedef GLXPixmap (GLAPIENTRY * PFNGLXCREATEPIXMAP) (Display*,
                                                         GLXFBConfig,
                                                         Pixmap,
                                                         const int*);
    PFNGLXCREATEPIXMAP xCreatePixmapInternal;
    typedef GLXPixmap (GLAPIENTRY * PFNGLXCREATEGLXPIXMAPWITHCONFIG)
                                                        (Display*,
                                                         GLXFBConfig,
                                                         Pixmap);
    PFNGLXCREATEGLXPIXMAPWITHCONFIG xCreateGLXPixmapWithConfigInternal;
    typedef void (GLAPIENTRY * PFNGLXDESTROYPIXMAP) (Display*,
                                                     GLXPixmap);
    PFNGLXDESTROYPIXMAP xDestroyPixmapInternal;
    typedef Bool (GLAPIENTRY * PFNGLXQUERYVERSION) (Display*,
                                                    int*,
                                                    int*);
    PFNGLXQUERYVERSION xQueryVersionInternal;

    typedef void (GLAPIENTRY * PFNGLXBINDTEXIMAGE) (Display*,
                                                    GLXDrawable,
                                                    int,
                                                    const int*);
    PFNGLXBINDTEXIMAGE xBindTexImageInternal;

    typedef void (GLAPIENTRY * PFNGLXRELEASETEXIMAGE) (Display*,
                                                       GLXDrawable,
                                                       int);
    PFNGLXRELEASETEXIMAGE xReleaseTexImageInternal;

    typedef void (GLAPIENTRY * PFNGLXWAITGL) ();
    PFNGLXWAITGL xWaitGLInternal;

    typedef void (GLAPIENTRY * PFNGLXWAITX) ();
    PFNGLXWAITGL xWaitXInternal;

    typedef GLXContext (GLAPIENTRY * PFNGLXCREATECONTEXTATTRIBS) (Display*,
                                                                  GLXFBConfig,
                                                                  GLXContext,
                                                                  Bool,
                                                                  const int*);
    PFNGLXCREATECONTEXTATTRIBS xCreateContextAttribsInternal;

    typedef int (GLAPIENTRY * PFNGLXGETVIDEOSYNCSGI) (unsigned int* count);
    PFNGLXGETVIDEOSYNCSGI xGetVideoSyncInternal;

    typedef int (GLAPIENTRY * PFNGLXWAITVIDEOSYNCSGI) (int divisor, int remainder, unsigned int* count);
    PFNGLXWAITVIDEOSYNCSGI xWaitVideoSyncInternal;

    typedef void (GLAPIENTRY * PFNGLXSWAPINTERVALEXT) (Display* dpy, GLXDrawable drawable, int interval);
    PFNGLXSWAPINTERVALEXT xSwapIntervalInternal;

#ifdef DEBUG
    void BeforeGLXCall();
    void AfterGLXCall();
#endif

    bool mInitialized;
    bool mTriedInitializing;
    bool mUseTextureFromPixmap;
    bool mDebug;
    bool mHasRobustness;
    bool mHasCreateContextAttribs;
    bool mHasVideoSync;
    bool mIsATI;
    bool mIsNVIDIA;
    bool mClientIsMesa;
    int mGLXMajorVersion;
    int mGLXMinorVersion;
    PRLibrary* mOGLLibrary;
};

// a global GLXLibrary instance
extern GLXLibrary sGLXLibrary;

} /* namespace gl */
} /* namespace mozilla */
#endif /* GFX_GLXLIBRARY_H */

