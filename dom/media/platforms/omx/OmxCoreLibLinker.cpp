/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxCoreLibLinker.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Preferences.h"
#include "MainThreadUtils.h"
#include "prlink.h"
#include "PlatformDecoderModule.h"

#ifdef LOG
#undef LOG
#endif

#define LOG(arg, ...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("OmxCoreLibLinker::%s: " arg, __func__, ##__VA_ARGS__))

namespace mozilla
{

OmxCoreLibLinker::LinkStatus OmxCoreLibLinker::sLinkStatus =
  LinkStatus_INIT;

const char* OmxCoreLibLinker::sLibNames[] = {
  "libopenmaxil.so",        // Raspberry Pi
  "libomxr_core.so",        // Renesas R-Car, RZ/G
  "libomxil-bellagio.so.0", // Bellagio: An OSS implementation of OpenMAX IL
};

PRLibrary* OmxCoreLibLinker::sLinkedLib = nullptr;
const char* OmxCoreLibLinker::sLibName = nullptr;

#define OMX_FUNC(func) void (*func)();
#include "OmxFunctionList.h"
#undef OMX_FUNC

bool
OmxCoreLibLinker::TryLinkingLibrary(const char *libName)
{
  PRLibSpec lspec;
  lspec.type = PR_LibSpec_Pathname;
  lspec.value.pathname = libName;
  sLinkedLib = PR_LoadLibraryWithFlags(lspec, PR_LD_NOW | PR_LD_LOCAL);
  if (sLinkedLib) {
    if (Bind(libName)) {
      sLibName = libName;
      LOG("Succeeded to load %s", libName);
      return true;
    } else {
      LOG("Failed to link %s", libName);
    }
    Unlink();
  }
  return false;
}

/* static */ bool
OmxCoreLibLinker::Link()
{
  LOG("");

  if (sLinkStatus) {
    return sLinkStatus == LinkStatus_SUCCEEDED;
  }

  MOZ_ASSERT(NS_IsMainThread());

  nsAutoCString libPath;
  nsresult rv = Preferences::GetCString("media.omx.core-lib-path", libPath);
  if (NS_SUCCEEDED(rv) && !libPath.IsEmpty()) {
    if (TryLinkingLibrary(libPath.Data())) {
      sLinkStatus = LinkStatus_SUCCEEDED;
      return true;
    }
  }

  // try known paths
  for (size_t i = 0; i < ArrayLength(sLibNames); i++) {
    if (TryLinkingLibrary(sLibNames[i])) {
      sLinkStatus = LinkStatus_SUCCEEDED;
      return true;
    }
  }
  sLinkStatus = LinkStatus_FAILED;
  return false;
}

/* static */ bool
OmxCoreLibLinker::Bind(const char* aLibName)
{
#define OMX_FUNC(func)                                                  \
  {                                                                     \
    if (!(func = (typeof(func))PR_FindSymbol(sLinkedLib, #func))) {     \
      LOG("Couldn't load function " #func " from %s.", aLibName);       \
      return false;                                                     \
    }                                                                   \
  }
#include "OmxFunctionList.h"
#undef OMX_FUNC
  return true;
}

/* static */ void
OmxCoreLibLinker::Unlink()
{
  LOG("");

  if (sLinkedLib) {
    PR_UnloadLibrary(sLinkedLib);
    sLinkedLib = nullptr;
    sLibName = nullptr;
    sLinkStatus = LinkStatus_INIT;
  }
}

} // namespace mozilla
