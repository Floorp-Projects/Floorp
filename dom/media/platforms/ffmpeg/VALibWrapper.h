/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_FFMPEG_VALIBWRAPPER_H_
#define DOM_MEDIA_PLATFORMS_FFMPEG_VALIBWRAPPER_H_

#include "mozilla/Attributes.h"
#include "mozilla/Types.h"

struct PRLibrary;

#ifdef MOZ_WIDGET_GTK
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
  int (*vaInitialize)(void* dpy, int* major_version, int* minor_version);
  int (*vaTerminate)(void* dpy);
  void* (*vaGetDisplayDRM)(int fd);

 private:
  PRLibrary* mVALib;
  PRLibrary* mVALibDrm;
};

}  // namespace mozilla
#endif  // MOZ_WIDGET_GTK

#endif  // DOM_MEDIA_PLATFORMS_FFMPEG_VALIBWRAPPER_H_
