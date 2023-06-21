/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SanitizationUtils.h"

#include <string>

#include "mozilla/dom/quota/QuotaManager.h"
#include "nsString.h"

namespace mozilla::dom::quota {

nsAutoCString MakeSanitizedOriginCString(const nsACString& aOrigin) {
#ifdef XP_WIN
  NS_ASSERTION(!strcmp(QuotaManager::kReplaceChars,
                       FILE_ILLEGAL_CHARACTERS FILE_PATH_SEPARATOR),
               "Illegal file characters have changed!");
#endif

  nsAutoCString res{aOrigin};

  res.ReplaceChar(QuotaManager::kReplaceChars, '+');

  return res;
}

nsAutoString MakeSanitizedOriginString(const nsACString& aOrigin) {
  // An origin string is ASCII-only, since it is obtained via
  // nsIPrincipal::GetOrigin, which returns an ACString.
  return NS_ConvertASCIItoUTF16(MakeSanitizedOriginCString(aOrigin));
}

}  // namespace mozilla::dom::quota
