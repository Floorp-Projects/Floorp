/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemContentTypeGuess.h"

#include "ErrorList.h"
#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/mime_guess_ffi_generated.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsString.h"

namespace mozilla::dom::fs {

Result<ContentType, QMResult> FileSystemContentTypeGuess::FromPath(
    const Name& aPath) {
  NS_ConvertUTF16toUTF8 path(aPath);
  ContentType contentType;
  nsresult rv = mimeGuessFromPath(&path, &contentType);

  // QM_TRY is too verbose.
  if (NS_FAILED(rv)) {
    return Err(QMResult(rv));
  }

  return contentType;
}

}  // namespace mozilla::dom::fs
