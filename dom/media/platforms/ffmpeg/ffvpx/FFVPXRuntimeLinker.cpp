/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFVPXRuntimeLinker.h"
#include "FFmpegLibWrapper.h"
#include "FFmpegLog.h"
#include "mozilla/FileUtils.h"
#include "nsLocalFile.h"
#include "prmem.h"
#include "prlink.h"
#ifdef XP_WIN
#  include <windows.h>
#endif

// We use a known symbol located in lgpllibs to determine its location.
// soundtouch happens to be always included in lgpllibs
// Use abort() instead of exception in SoundTouch.
#define ST_NO_EXCEPTION_HANDLING 1
#include "soundtouch/SoundTouch.h"

namespace mozilla {

template <int V>
class FFmpegDecoderModule {
 public:
  static already_AddRefed<PlatformDecoderModule> Create(FFmpegLibWrapper*);
};

static FFmpegLibWrapper sFFVPXLib;

FFVPXRuntimeLinker::LinkStatus FFVPXRuntimeLinker::sLinkStatus =
    LinkStatus_INIT;

static PRLibrary* MozAVLink(nsIFile* aFile) {
  PRLibSpec lspec;
  PathString path = aFile->NativePath();
#ifdef XP_WIN
  lspec.type = PR_LibSpec_PathnameU;
  lspec.value.pathname_u = path.get();
#else
  lspec.type = PR_LibSpec_Pathname;
  lspec.value.pathname = path.get();
#endif
#ifdef MOZ_WIDGET_ANDROID
  PRLibrary* lib = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_GLOBAL);
#else
  PRLibrary* lib = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
#endif
  if (!lib) {
    FFMPEG_LOG("unable to load library %s", aFile->HumanReadablePath().get());
  }
  return lib;
}

/* static */
bool FFVPXRuntimeLinker::Init() {
  if (sLinkStatus) {
    return sLinkStatus == LinkStatus_SUCCEEDED;
  }

  MOZ_ASSERT(NS_IsMainThread());
  sLinkStatus = LinkStatus_FAILED;

#ifdef MOZ_WAYLAND
  sFFVPXLib.LinkVAAPILibs();
#endif

  // We retrieve the path of the lgpllibs library as this is where mozavcodec
  // and mozavutil libs are located.
  PathString lgpllibsname = GetLibraryName(nullptr, "lgpllibs");
  if (lgpllibsname.IsEmpty()) {
    return false;
  }
  PathString path = GetLibraryFilePathname(
      lgpllibsname.get(), (PRFuncPtr)&soundtouch::SoundTouch::getVersionId);
  if (path.IsEmpty()) {
    return false;
  }
  RefPtr<nsLocalFile> xulFile = new nsLocalFile(path);
  if (xulFile->NativePath().IsEmpty()) {
    return false;
  }

  nsCOMPtr<nsIFile> rootDir;
  if (NS_FAILED(xulFile->GetParent(getter_AddRefs(rootDir))) || !rootDir) {
    return false;
  }
  PathString rootPath = rootDir->NativePath();

  /* Get the platform-dependent library name of the module */
  PathString libname = GetLibraryName(rootPath.get(), "mozavutil");
  if (libname.IsEmpty()) {
    return false;
  }
  RefPtr<nsLocalFile> libFile = new nsLocalFile(libname);
  if (libFile->NativePath().IsEmpty()) {
    return false;
  }
  sFFVPXLib.mAVUtilLib = MozAVLink(libFile);
  libname = GetLibraryName(rootPath.get(), "mozavcodec");
  if (!libname.IsEmpty()) {
    libFile = new nsLocalFile(libname);
    if (!libFile->NativePath().IsEmpty()) {
      sFFVPXLib.mAVCodecLib = MozAVLink(libFile);
    }
  }
  if (sFFVPXLib.Link() == FFmpegLibWrapper::LinkResult::Success) {
    sLinkStatus = LinkStatus_SUCCEEDED;
    return true;
  }
  return false;
}

/* static */
already_AddRefed<PlatformDecoderModule> FFVPXRuntimeLinker::Create() {
  if (!Init()) {
    return nullptr;
  }
  return FFmpegDecoderModule<FFVPX_VERSION>::Create(&sFFVPXLib);
}

/* static */
void FFVPXRuntimeLinker::GetRDFTFuncs(FFmpegRDFTFuncs* aOutFuncs) {
  MOZ_ASSERT(sLinkStatus != LinkStatus_INIT);
  if (sFFVPXLib.av_rdft_init && sFFVPXLib.av_rdft_calc &&
      sFFVPXLib.av_rdft_end) {
    aOutFuncs->init = sFFVPXLib.av_rdft_init;
    aOutFuncs->calc = sFFVPXLib.av_rdft_calc;
    aOutFuncs->end = sFFVPXLib.av_rdft_end;
  } else {
    NS_WARNING("RDFT functions expected but not found");
    *aOutFuncs = FFmpegRDFTFuncs();  // zero
  }
}

}  // namespace mozilla
