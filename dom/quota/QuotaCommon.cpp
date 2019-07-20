/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaCommon.h"

namespace mozilla {
namespace dom {
namespace quota {

namespace {

LazyLogModule gLogger("QuotaManager");

}  // namespace

#ifdef NIGHTLY_BUILD
NS_NAMED_LITERAL_CSTRING(kQuotaInternalError, "internal");
NS_NAMED_LITERAL_CSTRING(kQuotaExternalError, "external");
#endif

LogModule* GetQuotaManagerLogger() { return gLogger; }

void SanitizeCString(nsACString& aString) {
  char* iter = aString.BeginWriting();
  char* end = aString.EndWriting();

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

void SanitizeOrigin(nsACString& aOrigin) {
  int32_t colonPos = aOrigin.FindChar(':');
  if (colonPos >= 0) {
    const nsACString& prefix(Substring(aOrigin, 0, colonPos));

    nsCString normSuffix(Substring(aOrigin, colonPos));
    SanitizeCString(normSuffix);

    aOrigin = prefix + normSuffix;
  } else {
    nsCString origin(aOrigin);
    SanitizeCString(origin);

    aOrigin = origin;
  }
}

}  // namespace quota
}  // namespace dom
}  // namespace mozilla
