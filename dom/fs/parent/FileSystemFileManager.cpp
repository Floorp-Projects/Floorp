/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemFileManager.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "nsIFile.h"
#include "nsCOMPtr.h"
#include "nsXPCOM.h"

namespace mozilla::dom::fs {

Result<nsCOMPtr<nsIFile>, QMResult> getOrCreateFile(
    const nsAString& aDatabaseFilePath, bool& aExists) {
  MOZ_ASSERT(!aDatabaseFilePath.IsEmpty());

  nsCOMPtr<nsIFile> result;
  QM_TRY(QM_TO_RESULT(NS_NewLocalFile(aDatabaseFilePath,
                                      /* aFollowLinks */ false,
                                      getter_AddRefs(result))));

  QM_TRY(QM_TO_RESULT(result->Exists(&aExists)));

  if (aExists) {
    return result;
  }

  QM_TRY(QM_TO_RESULT(aFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644)));

  return result;
}

Result<nsString, QMResult> getFileSystemDirectory(const Origin& aOrigin) {
  static nsString databaseFilePath;

  if (databaseFilePath.IsEmpty()) {
    QM_TRY_UNWRAP(RefPtr<quota::QuotaManager> quotaManager,
                  quota::QuotaManager::GetOrCreate());

    QM_TRY_UNWRAP(nsCOMPtr<nsIFile> directoryEntry,
                  quotaManager->GetDirectoryForOrigin(
                      quota::PERSISTENCE_TYPE_DEFAULT, aOrigin));

    QM_TRY(QM_TO_RESULT(directoryEntry->Append(u"filesystem"_ns)));

    QM_TRY(QM_TO_RESULT(directoryEntry->Append(u"metadata.sqlite"_ns)));

    QM_TRY(QM_TO_RESULT(directoryEntry->GetPath(databaseFilePath)));
  }

  return databaseFilePath;
}

}  // namespace mozilla::dom::fs
