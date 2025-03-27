/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_FFMPEG_VALIBWRAPPER_H_
#define DOM_MEDIA_PLATFORMS_FFMPEG_VALIBWRAPPER_H_

#include "mozilla/Attributes.h"
#include "mozilla/Types.h"
#include "nsISupportsImpl.h"

struct PRLibrary;

#ifdef MOZ_WIDGET_GTK

// Forward declare from va.h
typedef void* VADisplay;
typedef int VAStatus;
#  define VA_EXPORT_SURFACE_READ_ONLY 0x0001
#  define VA_EXPORT_SURFACE_SEPARATE_LAYERS 0x0004
#  define VA_STATUS_SUCCESS 0x00000000

namespace mozilla {

class MOZ_ONLY_USED_TO_AVOID_STATIC_CONSTRUCTORS VALibWrapper {
 public:
  // The class is used only in static storage and so is zero initialized.
  VALibWrapper() = default;
  // The libraries are not unloaded in the destructor, because doing so would
  // require a static constructor to register the static destructor.  As the
  // class is in static storage, the destructor would only run on shutdown
  // anyway.
  ~VALibWrapper() = default;

  // Check if sVALib and sVALibDrm are available with necessary symbols and
  // initialize sFuncs;
  static bool IsVAAPIAvailable();
  static VALibWrapper sFuncs;

 private:
  void Link();
  bool AreVAAPIFuncsAvailable();
  // Attempt to load libva-drm and libva and resolve necessary symbols.
  // Upon failure, the entire object will be reset and any attached libraries
  // will be unlinked.  Do not invoke more than once.
  bool LinkVAAPILibs();

 public:
  int (*vaExportSurfaceHandle)(void*, unsigned int, uint32_t, uint32_t, void*);
  int (*vaSyncSurface)(void*, unsigned int);

 private:
  PRLibrary* mVALib;
  PRLibrary* mVALibDrm;
};

class VADisplayHolder {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DESTROY(VADisplayHolder,
                                                     MaybeDestroy())

  static RefPtr<VADisplayHolder> GetSingleton();

  const VADisplay mDisplay;

 private:
  VADisplayHolder(VADisplay aDisplay, int aDRMFd)
      : mDisplay(aDisplay), mDRMFd(aDRMFd) {};
  ~VADisplayHolder();

  void MaybeDestroy();

  const int mDRMFd;
};

}  // namespace mozilla
#endif  // MOZ_WIDGET_GTK

#endif  // DOM_MEDIA_PLATFORMS_FFMPEG_VALIBWRAPPER_H_
