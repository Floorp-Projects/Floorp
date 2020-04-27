/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaCommon.h"

#include "mozilla/Logging.h"  // LazyLogModule
#include "nsIFile.h"
#include "nsXPCOM.h"
#include "nsXULAppAPI.h"
#ifdef XP_WIN
#  include "mozilla/ipc/BackgroundParent.h"
#  include "mozilla/StaticPrefs_dom.h"
#  include "nsILocalFileWin.h"
#endif
#include "nsXPCOM.h"

namespace mozilla {
namespace dom {
namespace quota {

namespace {

#ifdef XP_WIN
Atomic<int32_t> gUseDOSDevicePathSyntax(-1);
#endif

LazyLogModule gLogger("QuotaManager");

void AnonymizeCString(nsACString& aCString, uint32_t aStart) {
  MOZ_ASSERT(!aCString.IsEmpty());
  MOZ_ASSERT(aStart < aCString.Length());

  char* iter = aCString.BeginWriting() + aStart;
  char* end = aCString.EndWriting();

  while (iter != end) {
    char c = *iter;

    if (IsAsciiAlpha(c)) {
      *iter = 'a';
    } else if (IsAsciiDigit(c)) {
      *iter = 'D';
    }

    ++iter;
  }
}

}  // namespace

const char kQuotaGenericDelimiter = '|';

#ifdef NIGHTLY_BUILD
NS_NAMED_LITERAL_CSTRING(kQuotaInternalError, "internal");
NS_NAMED_LITERAL_CSTRING(kQuotaExternalError, "external");
#endif

LogModule* GetQuotaManagerLogger() { return gLogger; }

void AnonymizeCString(nsACString& aCString) {
  if (aCString.IsEmpty()) {
    return;
  }
  AnonymizeCString(aCString, /* aStart */ 0);
}

void AnonymizeOriginString(nsACString& aOriginString) {
  if (aOriginString.IsEmpty()) {
    return;
  }

  int32_t start = aOriginString.FindChar(':');
  if (start < 0) {
    start = 0;
  }

  AnonymizeCString(aOriginString, start);
}

#ifdef XP_WIN
void CacheUseDOSDevicePathSyntaxPrefValue() {
  MOZ_ASSERT(XRE_IsParentProcess());
  AssertIsOnBackgroundThread();

  if (gUseDOSDevicePathSyntax == -1) {
    bool useDOSDevicePathSyntax =
        StaticPrefs::dom_quotaManager_useDOSDevicePathSyntax_DoNotUseDirectly();
    gUseDOSDevicePathSyntax = useDOSDevicePathSyntax ? 1 : 0;
  }
}
#endif

Result<nsCOMPtr<nsIFile>, nsresult> QM_NewLocalFile(const nsAString& aPath) {
  nsCOMPtr<nsIFile> file;
  nsresult rv =
      NS_NewLocalFile(aPath, /* aFollowLinks */ false, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    QM_WARNING("Failed to construct a file for path (%s)",
               NS_ConvertUTF16toUTF8(aPath).get());
    return Err(rv);
  }

#ifdef XP_WIN
  MOZ_ASSERT(gUseDOSDevicePathSyntax != -1);

  if (gUseDOSDevicePathSyntax) {
    nsCOMPtr<nsILocalFileWin> winFile = do_QueryInterface(file, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return Err(rv);
    }

    MOZ_ASSERT(winFile);
    winFile->SetUseDOSDevicePathSyntax(true);
  }
#endif

  return file;
}

}  // namespace quota
}  // namespace dom
}  // namespace mozilla
