/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GetDirectoryForOrigin.h"

#include "FileSystemHashSource.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"

namespace mozilla::dom::fs {

Result<nsCOMPtr<nsIFile>, QMResult> GetDirectoryForOrigin(
    const quota::QuotaManager& aQuotaManager, const Origin& aOrigin) {
  QM_TRY_UNWRAP(auto directory, QM_TO_RESULT_TRANSFORM(quota::QM_NewLocalFile(
                                    aQuotaManager.GetBasePath())));

  QM_TRY(QM_TO_RESULT(directory->Append(u"opfs-storage"_ns)));

  QM_TRY_UNWRAP(auto originHash,
                data::FileSystemHashSource::GenerateHash(
                    "parent"_ns, NS_ConvertUTF8toUTF16(aOrigin)));

  QM_TRY_UNWRAP(auto encodedOrigin,
                data::FileSystemHashSource::EncodeHash(originHash));

  QM_TRY(QM_TO_RESULT(directory->Append(encodedOrigin)));

  return directory;
}

}  // namespace mozilla::dom::fs
