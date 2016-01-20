/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFVPXRuntimeLinker.h"
#include "FFmpegRuntimeLinker.h"
#include "FFmpegLog.h"
#include "mozilla/Types.h"
#include "nsIFile.h"
#include "nsXPCOMPrivate.h" // for XUL_DLL
#include "prmem.h"
#include "prlink.h"

#if defined(XP_WIN)
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#endif

namespace mozilla
{

template <int V> class FFmpegDecoderModule
{
public:
  static already_AddRefed<PlatformDecoderModule> Create();
};

namespace ffvpx
{

FFVPXRuntimeLinker::LinkStatus FFVPXRuntimeLinker::sLinkStatus =
  LinkStatus_INIT;

PRLibrary* FFVPXRuntimeLinker::sLinkedLib = nullptr;
PRLibrary* FFVPXRuntimeLinker::sLinkedUtilLib = nullptr;
static unsigned (*avcodec_version)() = nullptr;

#ifdef __GNUC__
#define AV_FUNC(func, ver) void (*func)();
#define LIBAVCODEC_ALLVERSION
#else
#define AV_FUNC(func, ver) decltype(func)* func;
#endif
#include "FFmpegFunctionList.h"
#undef AV_FUNC

static PRLibrary*
MozAVLink(const char* aName)
{
  PRLibSpec lspec;
  lspec.type = PR_LibSpec_Pathname;
  lspec.value.pathname = aName;
  return PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
}

/* static */ bool
FFVPXRuntimeLinker::Link()
{
  if (sLinkStatus) {
    return sLinkStatus == LinkStatus_SUCCEEDED;
  }

  MOZ_ASSERT(NS_IsMainThread());

  // We retrieve the path of the XUL library as this is where mozavcodec and
  // mozavutil libs are located.
  char* path =
    PR_GetLibraryFilePathname(XUL_DLL, (PRFuncPtr)&FFVPXRuntimeLinker::Link);
  if (!path) {
    return false;
  }
  nsCOMPtr<nsIFile> xulFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  if (!xulFile ||
      NS_FAILED(xulFile->InitWithNativePath(nsDependentCString(path)))) {
    PR_Free(path);
    return false;
  }
  PR_Free(path);

  nsCOMPtr<nsIFile> rootDir;
  if (NS_FAILED(xulFile->GetParent(getter_AddRefs(rootDir))) || !rootDir) {
    return false;
  }
  nsAutoCString rootPath;
  if (NS_FAILED(rootDir->GetNativePath(rootPath))) {
    return false;
  }

  char* libname = NULL;
  /* Get the platform-dependent library name of the module */
  libname = PR_GetLibraryName(rootPath.get(), "mozavutil");
  if (!libname) {
    return false;
  }
  sLinkedUtilLib = MozAVLink(libname);
  PR_FreeLibraryName(libname);
  libname = PR_GetLibraryName(rootPath.get(), "mozavcodec");
  if (libname) {
    sLinkedLib = MozAVLink(libname);
    PR_FreeLibraryName(libname);
    if (sLinkedLib && sLinkedUtilLib) {
      if (Bind("mozavcodec")) {
        sLinkStatus = LinkStatus_SUCCEEDED;
        return true;
      }
    }
  }

  Unlink();

  sLinkStatus = LinkStatus_FAILED;
  return false;
}

/* static */ bool
FFVPXRuntimeLinker::Bind(const char* aLibName)
{
  int version = AV_FUNC_57;

#define AV_FUNC(func, ver)                                                     \
  if ((ver) & version) {                                                       \
    if (!(func = (decltype(func))PR_FindSymbol(((ver) & AV_FUNC_AVUTIL_MASK) ? sLinkedUtilLib : sLinkedLib, #func))) { \
      FFMPEG_LOG("Couldn't load function " #func " from %s.", aLibName);       \
      return false;                                                            \
    }                                                                          \
  } else {                                                                     \
    func = (decltype(func))nullptr;                                            \
  }
#include "FFmpegFunctionList.h"
#undef AV_FUNC
  return true;
}

/* static */ already_AddRefed<PlatformDecoderModule>
FFVPXRuntimeLinker::CreateDecoderModule()
{
  if (!Link()) {
    return nullptr;
  }
  return FFmpegDecoderModule<FFVPX_VERSION>::Create();
}

/* static */ void
FFVPXRuntimeLinker::Unlink()
{
  if (sLinkedUtilLib && sLinkedUtilLib != sLinkedLib) {
    PR_UnloadLibrary(sLinkedUtilLib);
  }
  if (sLinkedLib) {
    PR_UnloadLibrary(sLinkedLib);
    sLinkedLib = nullptr;
    sLinkStatus = LinkStatus_INIT;
    avcodec_version = nullptr;
  }
  sLinkedUtilLib = nullptr;
}

#undef LIBAVCODEC_ALLVERSION
} // namespace ffvpx
} // namespace mozilla
