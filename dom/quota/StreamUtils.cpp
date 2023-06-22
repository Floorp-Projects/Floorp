/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamUtils.h"

#include "mozilla/Result.h"
#include "mozilla/dom/quota/Assertions.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIOutputStream.h"
#include "nsIRandomAccessStream.h"
#include "nsNetUtil.h"

namespace mozilla::dom::quota {

Result<nsCOMPtr<nsIOutputStream>, nsresult> GetOutputStream(
    nsIFile& aFile, FileFlag aFileFlag) {
  AssertIsOnIOThread();

  switch (aFileFlag) {
    case FileFlag::Truncate:
      QM_TRY_RETURN(NS_NewLocalFileOutputStream(&aFile));

    case FileFlag::Update: {
      QM_TRY_INSPECT(const bool& exists,
                     MOZ_TO_RESULT_INVOKE_MEMBER(&aFile, Exists));

      if (!exists) {
        return nsCOMPtr<nsIOutputStream>();
      }

      QM_TRY_INSPECT(const auto& stream,
                     NS_NewLocalFileRandomAccessStream(&aFile));

      nsCOMPtr<nsIOutputStream> outputStream = do_QueryInterface(stream);
      QM_TRY(OkIf(outputStream), Err(NS_ERROR_FAILURE));

      return outputStream;
    }

    case FileFlag::Append:
      QM_TRY_RETURN(NS_NewLocalFileOutputStream(
          &aFile, PR_WRONLY | PR_CREATE_FILE | PR_APPEND));

    default:
      MOZ_CRASH("Should never get here!");
  }
}

Result<nsCOMPtr<nsIBinaryOutputStream>, nsresult> GetBinaryOutputStream(
    nsIFile& aFile, FileFlag aFileFlag) {
  QM_TRY_UNWRAP(auto outputStream, GetOutputStream(aFile, aFileFlag));

  QM_TRY(OkIf(outputStream), Err(NS_ERROR_UNEXPECTED));

  return nsCOMPtr<nsIBinaryOutputStream>(
      NS_NewObjectOutputStream(outputStream));
}

Result<nsCOMPtr<nsIBinaryInputStream>, nsresult> GetBinaryInputStream(
    nsIFile& aDirectory, const nsAString& aFilename) {
  MOZ_ASSERT(!NS_IsMainThread());

  QM_TRY_INSPECT(const auto& file, MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
                                       nsCOMPtr<nsIFile>, aDirectory, Clone));

  QM_TRY(MOZ_TO_RESULT(file->Append(aFilename)));

  QM_TRY_UNWRAP(auto stream, NS_NewLocalFileInputStream(file));

  QM_TRY_INSPECT(const auto& bufferedStream,
                 NS_NewBufferedInputStream(stream.forget(), 512));

  QM_TRY(OkIf(bufferedStream), Err(NS_ERROR_FAILURE));

  return nsCOMPtr<nsIBinaryInputStream>(
      NS_NewObjectInputStream(bufferedStream));
}

}  // namespace mozilla::dom::quota
