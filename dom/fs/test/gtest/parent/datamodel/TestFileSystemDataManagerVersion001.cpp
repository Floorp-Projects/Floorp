/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ErrorList.h"
#include "FileSystemDataManager.h"
#include "FileSystemDatabaseManagerVersion001.h"
#include "FileSystemFileManager.h"
#include "FileSystemHashSource.h"
#include "ResultStatement.h"
#include "SchemaVersion001.h"
#include "TestHelpers.h"
#include "gtest/gtest.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "mozilla/Array.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Result.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/PFileSystemManager.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsContentUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsLiteralString.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTHashSet.h"

namespace mozilla::dom::fs::test {

using data::FileSystemDatabaseManagerVersion001;
using data::FileSystemFileManager;

static const Origin& getTestOrigin() {
  static const Origin orig = "testOrigin"_ns;
  return orig;
}

static void MakeDatabaseManagerVersion001(
    FileSystemDatabaseManagerVersion001*& aResult) {
  TEST_TRY_UNWRAP(auto storageService,
                  MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<mozIStorageService>,
                                          MOZ_SELECT_OVERLOAD(do_GetService),
                                          MOZ_STORAGE_SERVICE_CONTRACTID));

  const auto flags = mozIStorageService::CONNECTION_DEFAULT;
  ResultConnection connection;

  nsresult rv = storageService->OpenSpecialDatabase(kMozStorageMemoryStorageKey,
                                                    VoidCString(), flags,
                                                    getter_AddRefs(connection));
  ASSERT_NSEQ(NS_OK, rv);

  const Origin& testOrigin = getTestOrigin();

  TEST_TRY_UNWRAP(
      DatabaseVersion version,
      SchemaVersion001::InitializeConnection(connection, testOrigin));
  ASSERT_EQ(1, version);

  nsCOMPtr<nsIFile> testPath;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(testPath));
  ASSERT_NSEQ(NS_OK, rv);

  auto fmRes =
      FileSystemFileManager::CreateFileSystemFileManager(std::move(testPath));
  ASSERT_FALSE(fmRes.isErr());

  aResult = new FileSystemDatabaseManagerVersion001(
      std::move(connection), MakeUnique<FileSystemFileManager>(fmRes.unwrap()));
}

TEST(TestFileSystemDatabaseManagerVersion001, smokeTestCreateRemoveDirectories)
{
  nsresult rv = NS_OK;
  FileSystemDatabaseManagerVersion001* rdm = nullptr;
  ASSERT_NO_FATAL_FAILURE(MakeDatabaseManagerVersion001(rdm));
  UniquePtr<FileSystemDatabaseManagerVersion001> dm(rdm);

  TEST_TRY_UNWRAP(EntryId rootId, data::GetRootHandle(getTestOrigin()));

  FileSystemChildMetadata firstChildMeta(rootId, u"First"_ns);
  TEST_TRY_UNWRAP_ERR(
      rv, dm->GetOrCreateDirectory(firstChildMeta, /* create */ false));
  ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

  TEST_TRY_UNWRAP(EntryId firstChild,
                  dm->GetOrCreateDirectory(firstChildMeta, /* create */ true));

  int32_t dbVersion = 0;
  TEST_TRY_UNWRAP(FileSystemDirectoryListing entries,
                  dm->GetDirectoryEntries(rootId, dbVersion));
  ASSERT_EQ(1u, entries.directories().Length());
  ASSERT_EQ(0u, entries.files().Length());

  const auto& firstItemRef = entries.directories()[0];
  ASSERT_TRUE(u"First"_ns == firstItemRef.entryName())
  << firstItemRef.entryName();
  ASSERT_EQ(firstChild, firstItemRef.entryId());

  TEST_TRY_UNWRAP(EntryId firstChildClone,
                  dm->GetOrCreateDirectory(firstChildMeta, /* create */ true));
  ASSERT_EQ(firstChild, firstChildClone);

  FileSystemChildMetadata secondChildMeta(firstChild, u"Second"_ns);
  TEST_TRY_UNWRAP(EntryId secondChild,
                  dm->GetOrCreateDirectory(secondChildMeta, /* create */ true));

  FileSystemEntryPair shortPair(firstChild, secondChild);
  TEST_TRY_UNWRAP(Path shortPath, dm->Resolve(shortPair));
  ASSERT_EQ(1u, shortPath.Length());
  ASSERT_EQ(u"Second"_ns, shortPath[0]);

  FileSystemEntryPair longPair(rootId, secondChild);
  TEST_TRY_UNWRAP(Path longPath, dm->Resolve(longPair));
  ASSERT_EQ(2u, longPath.Length());
  ASSERT_EQ(u"First"_ns, longPath[0]);
  ASSERT_EQ(u"Second"_ns, longPath[1]);

  FileSystemEntryPair wrongPair(secondChild, rootId);
  TEST_TRY_UNWRAP_ERR(rv, dm->Resolve(wrongPair));
  ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

  PageNumber page = 0;
  TEST_TRY_UNWRAP(FileSystemDirectoryListing fEntries,
                  dm->GetDirectoryEntries(firstChild, page));
  ASSERT_EQ(1u, fEntries.directories().Length());
  ASSERT_EQ(0u, fEntries.files().Length());

  const auto& secItemRef = fEntries.directories()[0];
  ASSERT_TRUE(u"Second"_ns == secItemRef.entryName())
  << secItemRef.entryName();
  ASSERT_EQ(secondChild, secItemRef.entryId());

  TEST_TRY_UNWRAP_ERR(
      rv, dm->RemoveDirectory(firstChildMeta, /* recursive */ false));
  ASSERT_NSEQ(NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR,
              rv);  // Is this a good error?

  TEST_TRY_UNWRAP(bool isDeleted,
                  dm->RemoveDirectory(firstChildMeta, /* recursive */ true));
  ASSERT_TRUE(isDeleted);

  FileSystemChildMetadata thirdChildMeta(secondChild, u"Second"_ns);
  TEST_TRY_UNWRAP_ERR(
      rv, dm->GetOrCreateDirectory(thirdChildMeta, /* create */ true));
  ASSERT_NSEQ(NS_ERROR_STORAGE_CONSTRAINT, rv);  // Is this a good error?

  dm->Close();
}

TEST(TestFileSystemDatabaseManagerVersion001, smokeTestCreateRemoveFiles)
{
  nsresult rv = NS_OK;
  // Create data manager
  FileSystemDatabaseManagerVersion001* rdm = nullptr;
  ASSERT_NO_FATAL_FAILURE(MakeDatabaseManagerVersion001(rdm));
  UniquePtr<FileSystemDatabaseManagerVersion001> dm(rdm);

  TEST_TRY_UNWRAP(EntryId rootId, data::GetRootHandle(getTestOrigin()));

  FileSystemChildMetadata firstChildMeta(rootId, u"First"_ns);
  // If creating is not allowed, getting a file from empty root fails
  TEST_TRY_UNWRAP_ERR(rv,
                      dm->GetOrCreateFile(firstChildMeta, /* create */ false));
  ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

  // Creating a file under empty root succeeds
  TEST_TRY_UNWRAP(EntryId firstChild,
                  dm->GetOrCreateFile(firstChildMeta, /* create */ true));

  // Second time, the same file is returned
  TEST_TRY_UNWRAP(EntryId firstChildClone,
                  dm->GetOrCreateFile(firstChildMeta, /* create */ true));
  ASSERT_STREQ(firstChild.get(), firstChildClone.get());

  // Directory listing returns the created file
  PageNumber page = 0;
  TEST_TRY_UNWRAP(FileSystemDirectoryListing entries,
                  dm->GetDirectoryEntries(rootId, page));
  ASSERT_EQ(0u, entries.directories().Length());
  ASSERT_EQ(1u, entries.files().Length());

  const auto& firstItemRef = entries.files()[0];
  ASSERT_TRUE(u"First"_ns == firstItemRef.entryName())
  << firstItemRef.entryName();
  ASSERT_STREQ(firstChild.get(), firstItemRef.entryId().get());

  // Getting the file entry as directory fails
  TEST_TRY_UNWRAP_ERR(
      rv, dm->GetOrCreateDirectory(firstChildMeta, /* create */ false));
  ASSERT_NSEQ(NS_ERROR_DOM_TYPE_MISMATCH_ERR, rv);

  // Getting or creating the file entry as directory also fails
  TEST_TRY_UNWRAP_ERR(
      rv, dm->GetOrCreateDirectory(firstChildMeta, /* create */ true));
  ASSERT_NSEQ(NS_ERROR_DOM_TYPE_MISMATCH_ERR, rv);

  // Creating a file with non existing parent hash fails

  EntryId notAChildHash = "0123456789abcdef0123456789abcdef"_ns;
  FileSystemChildMetadata notAChildMeta(notAChildHash, u"Dummy"_ns);
  TEST_TRY_UNWRAP_ERR(rv,
                      dm->GetOrCreateFile(notAChildMeta, /* create */ true));
  ASSERT_NSEQ(NS_ERROR_STORAGE_CONSTRAINT, rv);  // Is this a good error?

  // We create a directory under root
  FileSystemChildMetadata secondChildMeta(rootId, u"Second"_ns);
  TEST_TRY_UNWRAP(EntryId secondChild,
                  dm->GetOrCreateDirectory(secondChildMeta, /* create */ true));

  // The root should now contain the existing file and the new directory
  TEST_TRY_UNWRAP(FileSystemDirectoryListing fEntries,
                  dm->GetDirectoryEntries(rootId, page));
  ASSERT_EQ(1u, fEntries.directories().Length());
  ASSERT_EQ(1u, fEntries.files().Length());

  const auto& secItemRef = fEntries.directories()[0];
  ASSERT_TRUE(u"Second"_ns == secItemRef.entryName())
  << secItemRef.entryName();
  ASSERT_EQ(secondChild, secItemRef.entryId());

  // Create a file under the new directory
  FileSystemChildMetadata thirdChildMeta(secondChild, u"Third"_ns);
  TEST_TRY_UNWRAP(EntryId thirdChild,
                  dm->GetOrCreateFile(thirdChildMeta, /* create */ true));

  FileSystemEntryPair entryPair(rootId, thirdChild);
  TEST_TRY_UNWRAP(Path entryPath, dm->Resolve(entryPair));
  ASSERT_EQ(2u, entryPath.Length());
  ASSERT_EQ(u"Second"_ns, entryPath[0]);
  ASSERT_EQ(u"Third"_ns, entryPath[1]);

  // If recursion is not allowed, the non-empty new directory may not be removed
  TEST_TRY_UNWRAP_ERR(
      rv, dm->RemoveDirectory(secondChildMeta, /* recursive */ false));
  ASSERT_NSEQ(NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR,
              rv);  // Is this a good error?

  // If recursion is allowed, the new directory goes away.
  TEST_TRY_UNWRAP(bool isDeleted,
                  dm->RemoveDirectory(secondChildMeta, /* recursive */ true));
  ASSERT_TRUE(isDeleted);

  // The file under the removed directory is no longer accessible.
  TEST_TRY_UNWRAP_ERR(rv,
                      dm->GetOrCreateFile(thirdChildMeta, /* create */ true));
  ASSERT_NSEQ(NS_ERROR_STORAGE_CONSTRAINT, rv);  // Is this a good error?

  // The deletion is reflected by the root directory listing
  TEST_TRY_UNWRAP(FileSystemDirectoryListing nEntries,
                  dm->GetDirectoryEntries(rootId, 0));
  ASSERT_EQ(0u, nEntries.directories().Length());
  ASSERT_EQ(1u, nEntries.files().Length());

  const auto& fileItemRef = nEntries.files()[0];
  ASSERT_TRUE(u"First"_ns == fileItemRef.entryName())
  << fileItemRef.entryName();
  ASSERT_EQ(firstChild, fileItemRef.entryId());

  dm->Close();
}

}  // namespace mozilla::dom::fs::test
