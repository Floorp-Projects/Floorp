/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GLXLIBRARY_H
#define GFX_GLXLIBRARY_H

#include "GLContextTypes.h"
#include "prlink.h"
typedef realGLboolean GLboolean;

// stuff from glx.h
#include "X11/Xlib.h"
#include "X11/Xutil.h" // for XVisualInfo
#include "X11UndefineNone.h"
typedef struct __GLXcontextRec* GLXContext;
typedef XID GLXPixmap;
typedef XID GLXDrawable;
/* GLX 1.3 and later */
typedef struct __GLXFBConfigRec* GLXFBConfig;
// end of stuff from glx.h
#include "prenv.h"

struct PRLibrary;
class gfxASurface;

namespace mozilla {
namespace gl {

class GLXLibrary
{
public:
    GLXLibrary()
        : mSymbols{nullptr}
        , mInitialized(false)
        , mTriedInitializing(false)
        , mUseTextureFromPixmap(false)
        , mDebug(false)
        , mHasRobustness(false)
        , mHasCreateContextAttribs(false)
        , mHasVideoSync(false)
        , mIsATI(false), mIsNVIDIA(false)
        , mClientIsMesa(false)
        , mOGLLibrary(nullptr)
    {}

    bool EnsureInitialized();

private:
    void BeforeGLXCall() const;
    void AfterGLXCall() const;

public:

#ifdef DEBUG
#define BEFORE_CALL BeforeGLXCall();
#define AFTER_CALL AfterGLXCall();
#else
#define BEFORE_CALL
#define AFTER_CALL
#endif

#define WRAP(X) \
    { \
        BEFORE_CALL \
        const auto ret = mSymbols. X ; \
        AFTER_CALL \
        return ret; \
    }
#define VOID_WRAP(X) \
    { \
        BEFORE_CALL \
        mSymbols. X ; \
        AFTER_CALL \
    }

    void           fDestroyContext(Display* display, GLXContext context) const
        VOID_WRAP( fDestroyContext(display, context) )

    Bool      fMakeCurrent(Display* display, GLXDrawable drawable, GLXContext context) const
        WRAP( fMakeCurrent(display, drawable, context) )

    GLXContext fGetCurrentContext() const
        WRAP(  fGetCurrentContext() )

    GLXFBConfig* fChooseFBConfig(Display* display, int screen, const int* attrib_list, int* nelements) const
        WRAP(    fChooseFBConfig(display, screen, attrib_list, nelements) )

    XVisualInfo* fChooseVisual(Display* display, int screen, int* attrib_list) const
        WRAP(    fChooseVisual(display, screen, attrib_list) )

    GLXFBConfig* fGetFBConfigs(Display* display, int screen, int* nelements) const
        WRAP(    fGetFBConfigs(display, screen, nelements) )

    GLXContext fCreateNewContext(Display* display, GLXFBConfig config, int render_type, GLXContext share_list, Bool direct) const
        WRAP(  fCreateNewContext(display, config, render_type, share_list, direct) )

    int       fGetFBConfigAttrib(Display* display, GLXFBConfig config, int attribute, int* value) const
        WRAP( fGetFBConfigAttrib(display, config, attribute, value) )

    void           fSwapBuffers(Display* display, GLXDrawable drawable) const
        VOID_WRAP( fSwapBuffers(display, drawable) )

    const char* fQueryExtensionsString(Display* display, int screen) const
        WRAP(   fQueryExtensionsString(display, screen) )

    const char* fGetClientString(Display* display, int screen) const
        WRAP(   fGetClientString(display, screen) )

    const char* fQueryServerString(Display* display, int screen, int name) const
        WRAP(   fQueryServerString(display, screen, name) )

    GLXPixmap fCreatePixmap(Display* display, GLXFBConfig config, Pixmap pixmap, const int* attrib_list) const
        WRAP( fCreatePixmap(display, config, pixmap, attrib_list) )

    GLXPixmap fCreateGLXPixmapWithConfig(Display* display, GLXFBConfig config, Pixmap pixmap) const
        WRAP( fCreateGLXPixmapWithConfig(display, config, pixmap) )

    void           fDestroyPixmap(Display* display, GLXPixmap pixmap) const
        VOID_WRAP( fDestroyPixmap(display, pixmap) )

    Bool      fQueryVersion(Display* display, int* major, int* minor) const
        WRAP( fQueryVersion(display, major, minor) )

    void           fBindTexImage(Display* display, GLXDrawable drawable, int buffer, const int* attrib_list) const
        VOID_WRAP( fBindTexImageEXT(display, drawable, buffer, attrib_list) )

    void           fReleaseTexImage(Display* display, GLXDrawable drawable, int buffer) const
        VOID_WRAP( fReleaseTexImageEXT(display, drawable, buffer) )

    void           fWaitGL() const
        VOID_WRAP( fWaitGL() )

    void           fWaitX() const
        VOID_WRAP( fWaitX() )

    GLXContext fCreateContextAttribs(Display* display, GLXFBConfig config, GLXContext share_list, Bool direct, const int* attrib_list) const
        WRAP(  fCreateContextAttribsARB(display, config, share_list, direct, attrib_list) )

    int       fGetVideoSync(unsigned int* count) const
        WRAP( fGetVideoSyncSGI(count) )

    int       fWaitVideoSync(int divisor, int remainder, unsigned int* count) const
        WRAP( fWaitVideoSyncSGI(divisor, remainder, count) )

    void           fSwapInterval(Display* dpy, GLXDrawable drawable, int interval) const
        VOID_WRAP( fSwapIntervalEXT(dpy, drawable, interval) )

#undef WRAP
#undef VOID_WRAP
#undef BEFORE_CALL
#undef AFTER_CALL

    ////

    GLXPixmap CreatePixmap(gfxASurface* aSurface);
    void DestroyPixmap(Display* aDisplay, GLXPixmap aPixmap);
    void BindTexImage(Display* aDisplay, GLXPixmap aPixmap);
    void ReleaseTexImage(Display* aDisplay, GLXPixmap aPixmap);
    void UpdateTexImage(Display* aDisplay, GLXPixmap aPixmap);

    ////

    bool UseTextureFromPixmap() { return mUseTextureFromPixmap; }
    bool HasRobustness() { return mHasRobustness; }
    bool HasCreateContextAttribs() { return mHasCreateContextAttribs; }
    bool SupportsTextureFromPixmap(gfxASurface* aSurface);
    bool SupportsVideoSync();
    bool SupportsSwapControl() const { return bool(mSymbols.fSwapIntervalEXT); }
    bool IsATI() { return mIsATI; }
    bool IsMesa() { return mClientIsMesa; }

    PRFuncPtr GetGetProcAddress() const {
        return (PRFuncPtr)mSymbols.fGetProcAddress;
    }

private:
    struct {
        void         (GLAPIENTRY *fDestroyContext) (Display*, GLXContext);
        Bool         (GLAPIENTRY *fMakeCurrent) (Display*, GLXDrawable, GLXContext);
        GLXContext   (GLAPIENTRY *fGetCurrentContext) ();
        void*        (GLAPIENTRY *fGetProcAddress) (const char*);
        GLXFBConfig* (GLAPIENTRY *fChooseFBConfig) (Display*, int, const int*, int*);
        XVisualInfo* (GLAPIENTRY *fChooseVisual) (Display*, int, const int*);
        GLXFBConfig* (GLAPIENTRY *fGetFBConfigs) (Display*, int, int*);
        GLXContext   (GLAPIENTRY *fCreateNewContext) (Display*, GLXFBConfig, int,
                                                      GLXContext, Bool);
        int          (GLAPIENTRY *fGetFBConfigAttrib) (Display*, GLXFBConfig, int, int*);
        void         (GLAPIENTRY *fSwapBuffers) (Display*, GLXDrawable);
        const char*  (GLAPIENTRY *fQueryExtensionsString) (Display*, int);
        const char*  (GLAPIENTRY *fGetClientString) (Display*, int);
        const char*  (GLAPIENTRY *fQueryServerString) (Display*, int, int);
        GLXPixmap    (GLAPIENTRY *fCreatePixmap) (Display*, GLXFBConfig, Pixmap,
                                                  const int*);
        GLXPixmap    (GLAPIENTRY *fCreateGLXPixmapWithConfig) (Display*, GLXFBConfig,
                                                              Pixmap);
        void         (GLAPIENTRY *fDestroyPixmap) (Display*, GLXPixmap);
        Bool         (GLAPIENTRY *fQueryVersion) (Display*, int*, int*);
        void         (GLAPIENTRY *fWaitGL) ();
        void         (GLAPIENTRY *fWaitX) ();
        void         (GLAPIENTRY *fBindTexImageEXT) (Display*, GLXDrawable, int,
                                                     const int*);
        void         (GLAPIENTRY *fReleaseTexImageEXT) (Display*, GLXDrawable, int);
        GLXContext   (GLAPIENTRY *fCreateContextAttribsARB) (Display*, GLXFBConfig,
                                                             GLXContext, Bool,
                                                             const int*);
        int          (GLAPIENTRY *fGetVideoSyncSGI) (unsigned int*);
        int          (GLAPIENTRY *fWaitVideoSyncSGI) (int, int, unsigned int*);
        void         (GLAPIENTRY *fSwapIntervalEXT) (Display*, GLXDrawable, int);
    } mSymbols;

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
    PRLibrary* mOGLLibrary;
};

// a global GLXLibrary instance
extern GLXLibrary sGLXLibrary;

} /* namespace gl */
} /* namespace mozilla */
#endif /* GFX_GLXLIBRARY_H */

