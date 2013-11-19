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
typedef struct __GLXcontextRec *GLXContext;
typedef XID GLXPixmap;
typedef XID GLXDrawable;
/* GLX 1.3 and later */
typedef struct __GLXFBConfigRec *GLXFBConfig;
typedef XID GLXFBConfigID;
typedef XID GLXContextID;
typedef XID GLXWindow;
typedef XID GLXPbuffer;
// end of stuff from glx.h

struct PRLibrary;

namespace mozilla {
namespace gl {

class GLXLibrary
{
public:
    GLXLibrary() : mInitialized(false), mTriedInitializing(false),
                   mUseTextureFromPixmap(false), mDebug(false),
                   mHasRobustness(false), mIsATI(false), mIsNVIDIA(false),
                   mClientIsMesa(false), mGLXMajorVersion(0),
                   mGLXMinorVersion(0), mLibType(OPENGL_LIB),
                   mOGLLibrary(nullptr) {}

    void xDestroyContext(Display* display, GLXContext context);
    Bool xMakeCurrent(Display* display, 
                      GLXDrawable drawable, 
                      GLXContext context);

    enum LibraryType
    {
      OPENGL_LIB = 0,
      MESA_LLVMPIPE_LIB = 1,
      LIBS_MAX
    };

    GLXContext xGetCurrentContext();
    static void* xGetProcAddress(const char *procName);
    GLXFBConfig* xChooseFBConfig(Display* display, 
                                 int screen, 
                                 const int *attrib_list, 
                                 int *nelements);
    GLXFBConfig* xGetFBConfigs(Display* display, 
                               int screen, 
                               int *nelements);
    GLXContext xCreateNewContext(Display* display, 
                                 GLXFBConfig config, 
                                 int render_type, 
                                 GLXContext share_list, 
                                 Bool direct);
    int xGetFBConfigAttrib(Display *display,
                           GLXFBConfig config,
                           int attribute,
                           int *value);
    void xSwapBuffers(Display *display, GLXDrawable drawable);
    const char * xQueryExtensionsString(Display *display,
                                        int screen);
    const char * xGetClientString(Display *display,
                                  int screen);
    const char * xQueryServerString(Display *display,
                                    int screen, int name);
    GLXPixmap xCreatePixmap(Display *display, 
                            GLXFBConfig config,
                            Pixmap pixmap,
                            const int *attrib_list);
    GLXPixmap xCreateGLXPixmapWithConfig(Display *display,
                                         GLXFBConfig config,
                                         Pixmap pixmap);
    void xDestroyPixmap(Display *display, GLXPixmap pixmap);
    Bool xQueryVersion(Display *display,
                       int *major,
                       int *minor);
    void xBindTexImage(Display *display,
                       GLXDrawable drawable,
                       int buffer,
                       const int *attrib_list);
    void xReleaseTexImage(Display *display,
                          GLXDrawable drawable,
                          int buffer);
    void xWaitGL();
    void xWaitX();

    GLXContext xCreateContextAttribs(Display* display, 
                                     GLXFBConfig config, 
                                     GLXContext share_list, 
                                     Bool direct,
                                     const int* attrib_list);

    bool EnsureInitialized(LibraryType libType);

    GLXPixmap CreatePixmap(gfxASurface* aSurface);
    void DestroyPixmap(GLXPixmap aPixmap);
    void BindTexImage(GLXPixmap aPixmap);
    void ReleaseTexImage(GLXPixmap aPixmap);

    bool UseTextureFromPixmap() { return mUseTextureFromPixmap; }
    bool HasRobustness() { return mHasRobustness; }
    bool SupportsTextureFromPixmap(gfxASurface* aSurface);
    bool IsATI() { return mIsATI; }
    bool GLXVersionCheck(int aMajor, int aMinor);
    static LibraryType SelectLibrary(const ContextFlags& aFlags);

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
    typedef void* (GLAPIENTRY * PFNGLXGETPROCADDRESSPROC) (const char *);
    PFNGLXGETPROCADDRESSPROC xGetProcAddressInternal;
    typedef GLXFBConfig* (GLAPIENTRY * PFNGLXCHOOSEFBCONFIG) (Display *,
                                                              int,
                                                              const int *,
                                                              int *);
    PFNGLXCHOOSEFBCONFIG xChooseFBConfigInternal;
    typedef GLXFBConfig* (GLAPIENTRY * PFNGLXGETFBCONFIGS) (Display *,
                                                            int,
                                                            int *);
    PFNGLXGETFBCONFIGS xGetFBConfigsInternal;
    typedef GLXContext (GLAPIENTRY * PFNGLXCREATENEWCONTEXT) (Display *,
                                                              GLXFBConfig,
                                                              int,
                                                              GLXContext,
                                                              Bool);
    PFNGLXCREATENEWCONTEXT xCreateNewContextInternal;
    typedef int (GLAPIENTRY * PFNGLXGETFBCONFIGATTRIB) (Display *, 
                                                        GLXFBConfig,
                                                        int,
                                                        int *);
    PFNGLXGETFBCONFIGATTRIB xGetFBConfigAttribInternal;

    typedef void (GLAPIENTRY * PFNGLXSWAPBUFFERS) (Display *,
                                                   GLXDrawable);
    PFNGLXSWAPBUFFERS xSwapBuffersInternal;
    typedef const char * (GLAPIENTRY * PFNGLXQUERYEXTENSIONSSTRING) (Display *,
                                                                     int);
    PFNGLXQUERYEXTENSIONSSTRING xQueryExtensionsStringInternal;
    typedef const char * (GLAPIENTRY * PFNGLXGETCLIENTSTRING) (Display *,
                                                               int);
    PFNGLXGETCLIENTSTRING xGetClientStringInternal;
    typedef const char * (GLAPIENTRY * PFNGLXQUERYSERVERSTRING) (Display *,
                                                                 int,
                                                                 int);
    PFNGLXQUERYSERVERSTRING xQueryServerStringInternal;

    typedef GLXPixmap (GLAPIENTRY * PFNGLXCREATEPIXMAP) (Display *,
                                                         GLXFBConfig,
                                                         Pixmap,
                                                         const int *);
    PFNGLXCREATEPIXMAP xCreatePixmapInternal;
    typedef GLXPixmap (GLAPIENTRY * PFNGLXCREATEGLXPIXMAPWITHCONFIG)
                                                        (Display *,
                                                         GLXFBConfig,
                                                         Pixmap);
    PFNGLXCREATEGLXPIXMAPWITHCONFIG xCreateGLXPixmapWithConfigInternal;
    typedef void (GLAPIENTRY * PFNGLXDESTROYPIXMAP) (Display *,
                                                     GLXPixmap);
    PFNGLXDESTROYPIXMAP xDestroyPixmapInternal;
    typedef Bool (GLAPIENTRY * PFNGLXQUERYVERSION) (Display *,
                                                    int *,
                                                    int *);
    PFNGLXQUERYVERSION xQueryVersionInternal;

    typedef void (GLAPIENTRY * PFNGLXBINDTEXIMAGE) (Display *,
                                                    GLXDrawable,
                                                    int,
                                                    const int *);
    PFNGLXBINDTEXIMAGE xBindTexImageInternal;

    typedef void (GLAPIENTRY * PFNGLXRELEASETEXIMAGE) (Display *,
                                                       GLXDrawable,
                                                       int);
    PFNGLXRELEASETEXIMAGE xReleaseTexImageInternal;

    typedef void (GLAPIENTRY * PFNGLXWAITGL) ();
    PFNGLXWAITGL xWaitGLInternal;
    
    typedef void (GLAPIENTRY * PFNGLXWAITX) ();
    PFNGLXWAITGL xWaitXInternal;

    typedef GLXContext (GLAPIENTRY * PFNGLXCREATECONTEXTATTRIBS) (Display *,
                                                                  GLXFBConfig,
                                                                  GLXContext,
                                                                  Bool,
                                                                  const int *);
    PFNGLXCREATECONTEXTATTRIBS xCreateContextAttribsInternal;

#ifdef DEBUG
    void BeforeGLXCall();
    void AfterGLXCall();
#endif

    bool mInitialized;
    bool mTriedInitializing;
    bool mUseTextureFromPixmap;
    bool mDebug;
    bool mHasRobustness;
    bool mIsATI;
    bool mIsNVIDIA;
    bool mClientIsMesa;
    int mGLXMajorVersion;
    int mGLXMinorVersion;
    LibraryType mLibType;
    PRLibrary *mOGLLibrary;
};

// a global GLXLibrary instance
extern GLXLibrary sGLXLibrary[GLXLibrary::LIBS_MAX];
extern GLXLibrary& sDefGLXLib;

} /* namespace gl */
} /* namespace mozilla */
#endif /* GFX_GLXLIBRARY_H */

