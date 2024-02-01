/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GLXLIBRARY_H
#define GFX_GLXLIBRARY_H

#include "GLContext.h"
#include "mozilla/Assertions.h"
#include "mozilla/DataMutex.h"
#include "mozilla/gfx/XlibDisplay.h"
#include "prlink.h"
typedef realGLboolean GLboolean;

// stuff from glx.h
#include "X11/Xlib.h"
#include "X11/Xutil.h"  // for XVisualInfo
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

class GLContextGLX;

class GLXLibrary final {
 public:
  bool EnsureInitialized(Display* aDisplay);

 private:
  class WrapperScope final {
    const GLXLibrary& mGlx;
    const char* const mFuncName;
    Display* const mDisplay;

   public:
    WrapperScope(const GLXLibrary& glx, const char* const funcName,
                 Display* aDisplay);
    ~WrapperScope();
  };

 public:
#ifdef DEBUG
#  define DECL_WRAPPER_SCOPE(display) \
    const WrapperScope wrapperScope(*this, __func__, display);
#else
#  define DECL_WRAPPER_SCOPE(display)
#endif

  void fDestroyContext(Display* display, GLXContext context) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fDestroyContext(display, context);
  }

  Bool fMakeCurrent(Display* display, GLXDrawable drawable,
                    GLXContext context) const {
    DECL_WRAPPER_SCOPE(display)
    GLContext::ResetTLSCurrentContext();
    return mSymbols.fMakeCurrent(display, drawable, context);
  }

  XVisualInfo* fGetConfig(Display* display, XVisualInfo* info, int attrib,
                          int* value) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fGetConfig(display, info, attrib, value);
  }

  GLXContext fGetCurrentContext() const {
    DECL_WRAPPER_SCOPE(nullptr)
    return mSymbols.fGetCurrentContext();
  }

  GLXFBConfig* fChooseFBConfig(Display* display, int screen,
                               const int* attrib_list, int* nelements) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fChooseFBConfig(display, screen, attrib_list, nelements);
  }

  XVisualInfo* fChooseVisual(Display* display, int screen,
                             int* attrib_list) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fChooseVisual(display, screen, attrib_list);
  }

  GLXFBConfig* fGetFBConfigs(Display* display, int screen,
                             int* nelements) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fGetFBConfigs(display, screen, nelements);
  }

  GLXContext fCreateNewContext(Display* display, GLXFBConfig config,
                               int render_type, GLXContext share_list,
                               Bool direct) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fCreateNewContext(display, config, render_type, share_list,
                                      direct);
  }

  int fGetFBConfigAttrib(Display* display, GLXFBConfig config, int attribute,
                         int* value) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fGetFBConfigAttrib(display, config, attribute, value);
  }

  void fSwapBuffers(Display* display, GLXDrawable drawable) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fSwapBuffers(display, drawable);
  }

  const char* fQueryExtensionsString(Display* display, int screen) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fQueryExtensionsString(display, screen);
  }

  const char* fGetClientString(Display* display, int screen) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fGetClientString(display, screen);
  }

  const char* fQueryServerString(Display* display, int screen, int name) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fQueryServerString(display, screen, name);
  }

  GLXPixmap fCreatePixmap(Display* display, GLXFBConfig config, Pixmap pixmap,
                          const int* attrib_list) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fCreatePixmap(display, config, pixmap, attrib_list);
  }

  GLXPixmap fCreateGLXPixmapWithConfig(Display* display, GLXFBConfig config,
                                       Pixmap pixmap) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fCreateGLXPixmapWithConfig(display, config, pixmap);
  }

  void fDestroyPixmap(Display* display, GLXPixmap pixmap) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fDestroyPixmap(display, pixmap);
  }

  Bool fQueryVersion(Display* display, int* major, int* minor) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fQueryVersion(display, major, minor);
  }

  void fBindTexImage(Display* display, GLXDrawable drawable, int buffer,
                     const int* attrib_list) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fBindTexImageEXT(display, drawable, buffer, attrib_list);
  }

  void fReleaseTexImage(Display* display, GLXDrawable drawable,
                        int buffer) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fReleaseTexImageEXT(display, drawable, buffer);
  }

  void fWaitGL() const {
    DECL_WRAPPER_SCOPE(nullptr)
    return mSymbols.fWaitGL();
  }

  void fWaitX() const {
    DECL_WRAPPER_SCOPE(nullptr)
    return mSymbols.fWaitX();
  }

  GLXContext fCreateContextAttribs(Display* display, GLXFBConfig config,
                                   GLXContext share_list, Bool direct,
                                   const int* attrib_list) const {
    DECL_WRAPPER_SCOPE(display)
    return mSymbols.fCreateContextAttribsARB(display, config, share_list,
                                             direct, attrib_list);
  }

  int fGetVideoSync(unsigned int* count) const {
    DECL_WRAPPER_SCOPE(nullptr)
    return mSymbols.fGetVideoSyncSGI(count);
  }

  int fWaitVideoSync(int divisor, int remainder, unsigned int* count) const {
    DECL_WRAPPER_SCOPE(nullptr)
    return mSymbols.fWaitVideoSyncSGI(divisor, remainder, count);
  }

  void fSwapInterval(Display* dpy, GLXDrawable drawable, int interval) const {
    DECL_WRAPPER_SCOPE(dpy)
    return mSymbols.fSwapIntervalEXT(dpy, drawable, interval);
  }

  int fQueryDrawable(Display* dpy, GLXDrawable drawable, int attribute,
                     unsigned int* value) const {
    DECL_WRAPPER_SCOPE(dpy)
    return mSymbols.fQueryDrawable(dpy, drawable, attribute, value);
  }
#undef DECL_WRAPPER_SCOPE

  ////

  bool HasRobustness() { return mHasRobustness; }
  bool HasVideoMemoryPurge() { return mHasVideoMemoryPurge; }
  bool HasCreateContextAttribs() { return mHasCreateContextAttribs; }
  bool SupportsTextureFromPixmap(gfxASurface* aSurface);
  bool SupportsVideoSync(Display* aDisplay);
  bool SupportsSwapControl() const { return bool(mSymbols.fSwapIntervalEXT); }
  bool SupportsBufferAge() const {
    MOZ_ASSERT(mInitialized);
    return mHasBufferAge;
  }
  bool IsATI() { return mIsATI; }
  bool IsMesa() { return mClientIsMesa; }

  auto GetGetProcAddress() const { return mSymbols.fGetProcAddress; }

  std::shared_ptr<gfx::XlibDisplay> GetDisplay();

 private:
  struct {
    void(GLAPIENTRY* fDestroyContext)(Display*, GLXContext);
    Bool(GLAPIENTRY* fMakeCurrent)(Display*, GLXDrawable, GLXContext);
    XVisualInfo*(GLAPIENTRY* fGetConfig)(Display*, XVisualInfo*, int, int*);
    GLXContext(GLAPIENTRY* fGetCurrentContext)();
    void*(GLAPIENTRY* fGetProcAddress)(const char*);
    GLXFBConfig*(GLAPIENTRY* fChooseFBConfig)(Display*, int, const int*, int*);
    XVisualInfo*(GLAPIENTRY* fChooseVisual)(Display*, int, const int*);
    GLXFBConfig*(GLAPIENTRY* fGetFBConfigs)(Display*, int, int*);
    GLXContext(GLAPIENTRY* fCreateNewContext)(Display*, GLXFBConfig, int,
                                              GLXContext, Bool);
    int(GLAPIENTRY* fGetFBConfigAttrib)(Display*, GLXFBConfig, int, int*);
    void(GLAPIENTRY* fSwapBuffers)(Display*, GLXDrawable);
    const char*(GLAPIENTRY* fQueryExtensionsString)(Display*, int);
    const char*(GLAPIENTRY* fGetClientString)(Display*, int);
    const char*(GLAPIENTRY* fQueryServerString)(Display*, int, int);
    GLXPixmap(GLAPIENTRY* fCreatePixmap)(Display*, GLXFBConfig, Pixmap,
                                         const int*);
    GLXPixmap(GLAPIENTRY* fCreateGLXPixmapWithConfig)(Display*, GLXFBConfig,
                                                      Pixmap);
    void(GLAPIENTRY* fDestroyPixmap)(Display*, GLXPixmap);
    Bool(GLAPIENTRY* fQueryVersion)(Display*, int*, int*);
    void(GLAPIENTRY* fWaitGL)();
    void(GLAPIENTRY* fWaitX)();
    void(GLAPIENTRY* fBindTexImageEXT)(Display*, GLXDrawable, int, const int*);
    void(GLAPIENTRY* fReleaseTexImageEXT)(Display*, GLXDrawable, int);
    GLXContext(GLAPIENTRY* fCreateContextAttribsARB)(Display*, GLXFBConfig,
                                                     GLXContext, Bool,
                                                     const int*);
    int(GLAPIENTRY* fGetVideoSyncSGI)(unsigned int*);
    int(GLAPIENTRY* fWaitVideoSyncSGI)(int, int, unsigned int*);
    void(GLAPIENTRY* fSwapIntervalEXT)(Display*, GLXDrawable, int);
    int(GLAPIENTRY* fQueryDrawable)(Display*, GLXDrawable, int, unsigned int*);
  } mSymbols = {};

  bool mInitialized = false;
  bool mTriedInitializing = false;
  bool mDebug = false;
  bool mHasRobustness = false;
  bool mHasVideoMemoryPurge = false;
  bool mHasCreateContextAttribs = false;
  bool mHasVideoSync = false;
  bool mHasBufferAge = false;
  bool mIsATI = false;
  bool mIsNVIDIA = false;
  bool mClientIsMesa = false;
  PRLibrary* mOGLLibrary = nullptr;
  StaticDataMutex<std::weak_ptr<gfx::XlibDisplay>> mOwnDisplay{
      "GLXLibrary::mOwnDisplay"};
};

// a global GLXLibrary instance
extern GLXLibrary sGLXLibrary;

} /* namespace gl */
} /* namespace mozilla */
#endif /* GFX_GLXLIBRARY_H */
