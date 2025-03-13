/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VALibWrapper.h"

#include "FFmpegLog.h"
#include "PlatformDecoderModule.h"
#include "prlink.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/widget/DMABufLibWrapper.h"

namespace mozilla {

VALibWrapper VALibWrapper::sFuncs;
static int (*vaInitialize)(void* dpy, int* major_version, int* minor_version);
static int (*vaTerminate)(void* dpy);
static void* (*vaGetDisplayDRM)(int fd);

static VADisplayHolder* sDisplayHolder;
static StaticMutex sDisplayHolderMutex;

void VALibWrapper::Link() {
#define VA_FUNC_OPTION_SILENT(func)                               \
  if (!((func) = (decltype(func))PR_FindSymbol(mVALib, #func))) { \
    (func) = (decltype(func))nullptr;                             \
  }

  // mVALib is optional and may not be present.
  if (mVALib) {
    VA_FUNC_OPTION_SILENT(vaExportSurfaceHandle)
    VA_FUNC_OPTION_SILENT(vaSyncSurface)
    VA_FUNC_OPTION_SILENT(vaInitialize)
    VA_FUNC_OPTION_SILENT(vaTerminate)
  }
#undef VA_FUNC_OPTION_SILENT

#define VAD_FUNC_OPTION_SILENT(func)                                 \
  if (!((func) = (decltype(func))PR_FindSymbol(mVALibDrm, #func))) { \
    FFMPEGP_LOG("Couldn't load function " #func);                    \
  }

  // mVALibDrm is optional and may not be present.
  if (mVALibDrm) {
    VAD_FUNC_OPTION_SILENT(vaGetDisplayDRM)
  }
#undef VAD_FUNC_OPTION_SILENT
}

bool VALibWrapper::LinkVAAPILibs() {
  if (!gfx::gfxVars::CanUseHardwareVideoDecoding() || !XRE_IsRDDProcess()) {
    return false;
  }

  PRLibSpec lspec;
  lspec.type = PR_LibSpec_Pathname;
  const char* libDrm = "libva-drm.so.2";
  lspec.value.pathname = libDrm;
  mVALibDrm = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
  if (!mVALibDrm) {
    FFMPEGP_LOG("VA-API support: Missing or old %s library.\n", libDrm);
    return false;
  }

  const char* lib = "libva.so.2";
  lspec.value.pathname = lib;
  mVALib = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
  // Don't use libva when it's missing vaExportSurfaceHandle.
  if (mVALib && !PR_FindSymbol(mVALib, "vaExportSurfaceHandle")) {
    PR_UnloadLibrary(mVALib);
    mVALib = nullptr;
  }
  if (!mVALib) {
    FFMPEGP_LOG("VA-API support: Missing or old %s library.\n", lib);
    return false;
  }

  Link();
  return true;
}

bool VALibWrapper::AreVAAPIFuncsAvailable() {
#define VA_FUNC_LOADED(func) ((func) != nullptr)
  return VA_FUNC_LOADED(vaExportSurfaceHandle) &&
         VA_FUNC_LOADED(vaSyncSurface) && VA_FUNC_LOADED(vaInitialize) &&
         VA_FUNC_LOADED(vaTerminate) && VA_FUNC_LOADED(vaGetDisplayDRM);
}

/* static */
bool VALibWrapper::IsVAAPIAvailable() {
  static bool once = sFuncs.LinkVAAPILibs();
  (void)once;

  return sFuncs.AreVAAPIFuncsAvailable();
}

/* static */
RefPtr<VADisplayHolder> VADisplayHolder::GetSingleton() {
  StaticMutexAutoLock lock(sDisplayHolderMutex);

  if (sDisplayHolder) {
    return RefPtr{sDisplayHolder};
  }

  int drmFd = widget::GetDMABufDevice()->OpenDRMFd();
  VADisplay display = vaGetDisplayDRM(drmFd);
  if (!display) {
    FFMPEGP_LOG("  Can't get DRM VA-API display.");
    return nullptr;
  }

  RefPtr displayHolder = new VADisplayHolder(display, drmFd);

  int major, minor;
  VAStatus status = vaInitialize(display, &major, &minor);
  if (status != VA_STATUS_SUCCESS) {
    FFMPEGP_LOG("  vaInitialize failed.");
    return nullptr;
  }

  sDisplayHolder = displayHolder;

  return displayHolder;
}

void VADisplayHolder::MaybeDestroy() {
  StaticMutexAutoLock lock(sDisplayHolderMutex);
  MOZ_ASSERT(int32_t(mRefCnt) >= 0, "dup release");
  if (mRefCnt == 0) {
    // No new reference added before the lock was taken.
    sDisplayHolder = nullptr;
    delete this;
  }
}

VADisplayHolder::~VADisplayHolder() {
  vaTerminate(mDisplay);
  close(mDRMFd);
}

}  // namespace mozilla
