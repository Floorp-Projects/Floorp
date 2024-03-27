/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFVPXRuntimeLinker.h"
#include "FFmpegLibWrapper.h"
#include "FFmpegLog.h"
#include "nsXREDirProvider.h"
#include "prlink.h"

namespace mozilla {

template <int V>
class FFmpegDecoderModule {
 public:
  static already_AddRefed<PlatformDecoderModule> Create(FFmpegLibWrapper*);
};

template <int V>
class FFmpegEncoderModule {
 public:
  static already_AddRefed<PlatformEncoderModule> Create(FFmpegLibWrapper*);
};

static FFmpegLibWrapper sFFVPXLib;

StaticMutex FFVPXRuntimeLinker::sMutex;

FFVPXRuntimeLinker::LinkStatus FFVPXRuntimeLinker::sLinkStatus =
    LinkStatus_INIT;

static PRLibrary* MozAVLink(nsIFile* aFile) {
  PRLibSpec lspec;
  PathString path = aFile->NativePath();
#ifdef XP_WIN
  lspec.type = PR_LibSpec_PathnameU;
  lspec.value.pathname_u = path.get();
#else
#  if defined(XP_OPENBSD)
  /* on OpenBSD, libmozavcodec.so and libmozavutil.so are preloaded before
   * sandboxing so make sure only the filename is passed to
   * PR_LoadLibraryWithFlags(), dlopen() will return the preloaded library
   * handle instead of failing to find it due to sandboxing */
  nsAutoCString leaf;
  nsresult rv = aFile->GetNativeLeafName(leaf);
  if (NS_SUCCEEDED(rv)) {
    path = PathString(leaf);
  }
#  endif  // defined(XP_OPENBSD)
  lspec.type = PR_LibSpec_Pathname;
  lspec.value.pathname = path.get();
#endif
#ifdef MOZ_WIDGET_ANDROID
  PRLibrary* lib = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_GLOBAL);
#else
  PRLibrary* lib = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
#endif
  if (!lib) {
    FFMPEGV_LOG("unable to load library %s", aFile->HumanReadablePath().get());
  }
  return lib;
}

/* static */
bool FFVPXRuntimeLinker::Init() {
  // Enter critical section to set up ffvpx.
  StaticMutexAutoLock lock(sMutex);

  if (sLinkStatus) {
    return sLinkStatus == LinkStatus_SUCCEEDED;
  }

  sLinkStatus = LinkStatus_FAILED;

#ifdef MOZ_WIDGET_GTK
  sFFVPXLib.LinkVAAPILibs();
#endif

  nsCOMPtr<nsIFile> libFile;
  RefPtr<nsXREDirProvider> provider = nsXREDirProvider::GetSingleton();
  if (NS_FAILED(provider->GetGREDir()->Clone(getter_AddRefs(libFile)))) {
    return false;
  }

  if (NS_FAILED(libFile->AppendNative(MOZ_DLL_PREFIX "mozavutil" MOZ_DLL_SUFFIX
                                                     ""_ns))) {
    return false;
  }
  sFFVPXLib.mAVUtilLib = MozAVLink(libFile);

  if (NS_FAILED(libFile->SetNativeLeafName(
          MOZ_DLL_PREFIX "mozavcodec" MOZ_DLL_SUFFIX ""_ns))) {
    return false;
  }
  sFFVPXLib.mAVCodecLib = MozAVLink(libFile);
  FFmpegLibWrapper::LinkResult res = sFFVPXLib.Link();
  FFMPEGP_LOG("Link result: %s", FFmpegLibWrapper::LinkResultToString(res));
  if (res == FFmpegLibWrapper::LinkResult::Success) {
    sLinkStatus = LinkStatus_SUCCEEDED;
    return true;
  }
  return false;
}

/* static */
already_AddRefed<PlatformDecoderModule> FFVPXRuntimeLinker::CreateDecoder() {
  if (!Init()) {
    return nullptr;
  }
  return FFmpegDecoderModule<FFVPX_VERSION>::Create(&sFFVPXLib);
}

/* static */
already_AddRefed<PlatformEncoderModule> FFVPXRuntimeLinker::CreateEncoder() {
  if (!Init()) {
    return nullptr;
  }
  return FFmpegEncoderModule<FFVPX_VERSION>::Create(&sFFVPXLib);
}

/* static */
void FFVPXRuntimeLinker::GetFFTFuncs(FFmpegFFTFuncs* aOutFuncs) {
  []() MOZ_NO_THREAD_SAFETY_ANALYSIS {
    MOZ_ASSERT(sLinkStatus != LinkStatus_INIT);
  }();
  MOZ_ASSERT(sFFVPXLib.av_tx_init && sFFVPXLib.av_tx_uninit);
  aOutFuncs->init = sFFVPXLib.av_tx_init;
  aOutFuncs->uninit = sFFVPXLib.av_tx_uninit;
}

}  // namespace mozilla
