/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GLXLIBRARY_H
#define GFX_GLXLIBRARY_H

#include "GLContext.h"
typedef realGLboolean GLboolean;
#include <GL/glx.h>

namespace mozilla {
namespace gl {

class GLXLibrary
{
public:
    GLXLibrary() : mInitialized(false), mTriedInitializing(false),
                   mUseTextureFromPixmap(false), mDebug(false),
                   mHasRobustness(false), mOGLLibrary(nullptr) {}

    void xDestroyContext(Display* display, GLXContext context);
    Bool xMakeCurrent(Display* display, 
                      GLXDrawable drawable, 
                      GLXContext context);
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
    XVisualInfo* xGetVisualFromFBConfig(Display* display, 
                                        GLXFBConfig config);
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
    GLXContext xCreateContext(Display *display,
                              XVisualInfo *vis,
                              GLXContext shareList,
                              Bool direct);
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

    bool EnsureInitialized();

    GLXPixmap CreatePixmap(gfxASurface* aSurface);
    void DestroyPixmap(GLXPixmap aPixmap);
    void BindTexImage(GLXPixmap aPixmap);
    void ReleaseTexImage(GLXPixmap aPixmap);

    bool UseTextureFromPixmap() { return mUseTextureFromPixmap; }
    bool HasRobustness() { return mHasRobustness; }
    bool SupportsTextureFromPixmap(gfxASurface* aSurface);

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
    typedef XVisualInfo* (GLAPIENTRY * PFNGLXGETVISUALFROMFBCONFIG) (Display *,
                                                                     GLXFBConfig);
    PFNGLXGETVISUALFROMFBCONFIG xGetVisualFromFBConfigInternal;
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
    typedef GLXContext (GLAPIENTRY * PFNGLXCREATECONTEXT) (Display *,
                                                           XVisualInfo *,
                                                           GLXContext,
                                                           Bool);
    PFNGLXCREATECONTEXT xCreateContextInternal;
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
    PRLibrary *mOGLLibrary;
};

// a global GLXLibrary instance
extern GLXLibrary sGLXLibrary;

} /* namespace gl */
} /* namespace mozilla */
#endif /* GFX_GLXLIBRARY_H */

