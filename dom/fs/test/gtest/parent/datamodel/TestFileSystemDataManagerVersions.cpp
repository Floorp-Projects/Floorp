/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "ErrorList.h"
#include "FileSystemDataManager.h"
#include "FileSystemDatabaseManagerVersion001.h"
#include "FileSystemDatabaseManagerVersion002.h"
#include "FileSystemFileManager.h"
#include "FileSystemHashSource.h"
#include "ResultStatement.h"
#include "SchemaVersion001.h"
#include "SchemaVersion002.h"
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
#include "mozilla/dom/quota/CommonMetadata.h"
#include "mozilla/dom/quota/test/QuotaManagerDependencyFixture.h"
#include "nsContentUtils.h"
#include "nsIFile.h"
#include "nsLiteralString.h"
#include "nsNetCID.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTHashSet.h"

namespace mozilla::dom::fs::test {

using data::FileSystemDatabaseManagerVersion001;
using data::FileSystemDatabaseManagerVersion002;
using data::FileSystemFileManager;

quota::OriginMetadata GetOriginMetadataSample() {
  return quota::OriginMetadata{""_ns,
                               "firefox.com"_ns,
                               "http://firefox.com"_ns,
                               "http://firefox.com"_ns,
                               /* aIsPrivate */ false,
                               quota::PERSISTENCE_TYPE_DEFAULT};
}

class TestFileSystemDatabaseManagerVersionsBase
    : public quota::test::QuotaManagerDependencyFixture {
 public:
  void SetUp() override { ASSERT_NO_FATAL_FAILURE(InitializeFixture()); }

  void TearDown() override {
    EXPECT_NO_FATAL_FAILURE(ClearStoragesForOrigin(GetOriginMetadataSample()));
    ASSERT_NO_FATAL_FAILURE(ShutdownFixture());
  }
};

class TestFileSystemDatabaseManagerVersions
    : public TestFileSystemDatabaseManagerVersionsBase,
      public ::testing::WithParamInterface<DatabaseVersion> {
 public:
  static void AssertEntryIdMoved(const EntryId& aOriginal,
                                 const EntryId& aMoved) {
    switch (sVersion) {
      case 1: {
        ASSERT_EQ(aOriginal, aMoved);
        break;
      }
      case 2: {
        ASSERT_NE(aOriginal, aMoved);
        break;
      }
      default: {
        ASSERT_FALSE(false)
        << "Unknown database version";
      }
    }
  }

  static void AssertEntryIdCollision(const EntryId& aOriginal,
                                     const EntryId& aMoved) {
    switch (sVersion) {
      case 1: {
        // We generated a new entryId
        ASSERT_NE(aOriginal, aMoved);
        break;
      }
      case 2: {
        // We get the same entryId for the same input
        ASSERT_EQ(aOriginal, aMoved);
        break;
      }
      default: {
        ASSERT_FALSE(false)
        << "Unknown database version";
      }
    }
  }

  static DatabaseVersion sVersion;
};

// This is a minimal mock  to allow us to safely call the lock methods
// while avoiding assertions
class MockFileSystemDataManager final : public data::FileSystemDataManager {
 public:
  MockFileSystemDataManager(const quota::OriginMetadata& aOriginMetadata,
                            MovingNotNull<nsCOMPtr<nsIEventTarget>> aIOTarget,
                            MovingNotNull<RefPtr<TaskQueue>> aIOTaskQueue)
      : FileSystemDataManager(aOriginMetadata, nullptr, std::move(aIOTarget),
                              std::move(aIOTaskQueue)) {}

  void SetDatabaseManager(data::FileSystemDatabaseManager* aDatabaseManager) {
    mDatabaseManager =
        UniquePtr<data::FileSystemDatabaseManager>(aDatabaseManager);
  }

  virtual ~MockFileSystemDataManager() {
    mDatabaseManager->Close();
    mDatabaseManager = nullptr;

    // Need to avoid assertions
    mState = State::Closed;
  }
};

static void MakeDatabaseManagerVersions(
    const DatabaseVersion aVersion,
    RefPtr<MockFileSystemDataManager>& aDataManager,
    FileSystemDatabaseManagerVersion001*& aDatabaseManager) {
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

  auto fmRes = FileSystemFileManager::CreateFileSystemFileManager(
      GetOriginMetadataSample());
  ASSERT_FALSE(fmRes.isErr());

  const Origin& testOrigin = GetTestOrigin();

  if (1 == aVersion) {
    TEST_TRY_UNWRAP(
        TestFileSystemDatabaseManagerVersions::sVersion,
        SchemaVersion001::InitializeConnection(connection, testOrigin));
  } else {
    ASSERT_EQ(2, aVersion);

    TEST_TRY_UNWRAP(TestFileSystemDatabaseManagerVersions::sVersion,
                    SchemaVersion002::InitializeConnection(
                        connection, *fmRes.inspect(), testOrigin));
  }
  ASSERT_NE(0, TestFileSystemDatabaseManagerVersions::sVersion);

  TEST_TRY_UNWRAP(EntryId rootId, data::GetRootHandle(GetTestOrigin()));

  QM_TRY_UNWRAP(auto streamTransportService,
                MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<nsIEventTarget>,
                                        MOZ_SELECT_OVERLOAD(do_GetService),
                                        NS_STREAMTRANSPORTSERVICE_CONTRACTID),
                QM_VOID);

  quota::OriginMetadata originMetadata = GetOriginMetadataSample();

  nsCString taskQueueName("OPFS "_ns + originMetadata.mOrigin);

  RefPtr<TaskQueue> ioTaskQueue =
      TaskQueue::Create(do_AddRef(streamTransportService), taskQueueName.get());

  aDataManager = MakeRefPtr<MockFileSystemDataManager>(
      originMetadata, WrapMovingNotNull(streamTransportService),
      WrapMovingNotNull(ioTaskQueue));

  if (1 == aVersion) {
    aDatabaseManager = new FileSystemDatabaseManagerVersion001(
        aDataManager, std::move(connection), fmRes.unwrap(), rootId);
  } else {
    ASSERT_EQ(2, aVersion);

    aDatabaseManager = new FileSystemDatabaseManagerVersion002(
        aDataManager, std::move(connection), fmRes.unwrap(), rootId);
  }

  aDataManager->SetDatabaseManager(aDatabaseManager);
}

DatabaseVersion TestFileSystemDatabaseManagerVersions::sVersion = 0;

TEST_P(TestFileSystemDatabaseManagerVersions,
       smokeTestCreateRemoveDirectories) {
  const DatabaseVersion version = GetParam();

  auto ioTask = [version]() {
    nsresult rv = NS_OK;
    // Ensure that FileSystemDataManager lives for the lifetime of the test
    RefPtr<MockFileSystemDataManager> dataManager;
    FileSystemDatabaseManagerVersion001* dm = nullptr;
    ASSERT_NO_FATAL_FAILURE(
        MakeDatabaseManagerVersions(version, dataManager, dm));
    ASSERT_TRUE(dm);
    // if any of these exit early, we have to close
    auto autoClose = MakeScopeExit([dm] { dm->Close(); });

    TEST_TRY_UNWRAP(EntryId rootId, data::GetRootHandle(GetTestOrigin()));

    FileSystemChildMetadata firstChildMeta(rootId, u"First"_ns);
    TEST_TRY_UNWRAP_ERR(
        rv, dm->GetOrCreateDirectory(firstChildMeta, /* create */ false));
    ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

    TEST_TRY_UNWRAP(EntryId firstChild, dm->GetOrCreateDirectory(
                                            firstChildMeta, /* create */ true));

    int32_t dbVersion = 0;
    TEST_TRY_UNWRAP(FileSystemDirectoryListing entries,
                    dm->GetDirectoryEntries(rootId, dbVersion));
    ASSERT_EQ(1u, entries.directories().Length());
    ASSERT_EQ(0u, entries.files().Length());

    const auto& firstItemRef = entries.directories()[0];
    ASSERT_TRUE(u"First"_ns == firstItemRef.entryName())
    << firstItemRef.entryName();
    ASSERT_EQ(firstChild, firstItemRef.entryId());

    TEST_TRY_UNWRAP(
        EntryId firstChildClone,
        dm->GetOrCreateDirectory(firstChildMeta, /* create */ true));
    ASSERT_EQ(firstChild, firstChildClone);

    FileSystemChildMetadata secondChildMeta(firstChild, u"Second"_ns);
    TEST_TRY_UNWRAP(
        EntryId secondChild,
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
    TEST_TRY_UNWRAP(Path emptyPath, dm->Resolve(wrongPair));
    ASSERT_TRUE(emptyPath.IsEmpty());

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
    ASSERT_NSEQ(NS_ERROR_DOM_INVALID_MODIFICATION_ERR, rv);

    TEST_TRY_UNWRAP(bool isDeleted,
                    dm->RemoveDirectory(firstChildMeta, /* recursive */ true));
    ASSERT_TRUE(isDeleted);

    FileSystemChildMetadata thirdChildMeta(secondChild, u"Second"_ns);
    TEST_TRY_UNWRAP_ERR(
        rv, dm->GetOrCreateDirectory(thirdChildMeta, /* create */ true));
    ASSERT_NSEQ(NS_ERROR_STORAGE_CONSTRAINT, rv);  // Is this a good error?

    dm->Close();
  };

  PerformOnIOThread(std::move(ioTask));
}

TEST_P(TestFileSystemDatabaseManagerVersions, smokeTestCreateRemoveFiles) {
  const DatabaseVersion version = GetParam();

  auto ioTask = [version]() {
    nsresult rv = NS_OK;
    // Ensure that FileSystemDataManager lives for the lifetime of the test
    RefPtr<MockFileSystemDataManager> datamanager;
    FileSystemDatabaseManagerVersion001* dm = nullptr;
    ASSERT_NO_FATAL_FAILURE(
        MakeDatabaseManagerVersions(version, datamanager, dm));

    TEST_TRY_UNWRAP(EntryId rootId, data::GetRootHandle(GetTestOrigin()));

    FileSystemChildMetadata firstChildMeta(rootId, u"First"_ns);
    // If creating is not allowed, getting a file from empty root fails
    TEST_TRY_UNWRAP_ERR(
        rv, dm->GetOrCreateFile(firstChildMeta, /* create */ false));
    ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

    // Creating a file under empty root succeeds
    TEST_TRY_UNWRAP(EntryId firstChild,
                    dm->GetOrCreateFile(firstChildMeta, /* create */ true));

    // Second time, the same file is returned
    TEST_TRY_UNWRAP(EntryId firstChildClone,
                    dm->GetOrCreateFile(firstChildMeta, /* create */ true));
    ASSERT_EQ(firstChild, firstChildClone);

    // Directory listing returns the created file
    PageNumber page = 0;
    TEST_TRY_UNWRAP(FileSystemDirectoryListing entries,
                    dm->GetDirectoryEntries(rootId, page));
    ASSERT_EQ(0u, entries.directories().Length());
    ASSERT_EQ(1u, entries.files().Length());

    auto& firstItemRef = entries.files()[0];
    ASSERT_TRUE(u"First"_ns == firstItemRef.entryName())
    << firstItemRef.entryName();
    ASSERT_EQ(firstChild, firstItemRef.entryId());

    FileId fileId = FileId(firstItemRef.entryId());  // Default

    ContentType type;
    TimeStamp lastModifiedMilliSeconds;
    Path path;
    nsCOMPtr<nsIFile> file;
    rv = dm->GetFile(firstItemRef.entryId(), fileId, FileMode::EXCLUSIVE, type,
                     lastModifiedMilliSeconds, path, file);
    ASSERT_NSEQ(NS_OK, rv);

    const int64_t nowMilliSeconds = PR_Now() / 1000;
    ASSERT_GE(nowMilliSeconds, lastModifiedMilliSeconds);
    const int64_t expectedMaxDelayMilliSeconds = 100;
    const int64_t actualDelay = nowMilliSeconds - lastModifiedMilliSeconds;
    ASSERT_LT(actualDelay, expectedMaxDelayMilliSeconds);

    ASSERT_EQ(1u, path.Length());
    ASSERT_STREQ(u"First"_ns, path[0]);

    ASSERT_NE(nullptr, file);

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
    TEST_TRY_UNWRAP(
        EntryId secondChild,
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

    // If recursion is not allowed, the non-empty new directory may not be
    // removed
    TEST_TRY_UNWRAP_ERR(
        rv, dm->RemoveDirectory(secondChildMeta, /* recursive */ false));
    ASSERT_NSEQ(NS_ERROR_DOM_INVALID_MODIFICATION_ERR, rv);

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
  };

  PerformOnIOThread(std::move(ioTask));
}

TEST_P(TestFileSystemDatabaseManagerVersions, smokeTestCreateMoveDirectories) {
  const DatabaseVersion version = GetParam();

  auto ioTask = [version]() {
    // Ensure that FileSystemDataManager lives for the lifetime of the test
    RefPtr<MockFileSystemDataManager> datamanager;
    FileSystemDatabaseManagerVersion001* dm = nullptr;
    ASSERT_NO_FATAL_FAILURE(
        MakeDatabaseManagerVersions(version, datamanager, dm));
    auto closeAtExit = MakeScopeExit([&dm]() { dm->Close(); });

    TEST_TRY_UNWRAP(EntryId rootId, data::GetRootHandle(GetTestOrigin()));

    FileSystemEntryMetadata rootMeta{rootId, u"root"_ns,
                                     /* is directory */ true};

    {
      // Sanity check: no existing items should be found
      TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                      dm->GetDirectoryEntries(rootId, /* page */ 0u));
      ASSERT_TRUE(contents.directories().IsEmpty());
      ASSERT_TRUE(contents.files().IsEmpty());
    }

    // Create subdirectory
    FileSystemChildMetadata firstChildMeta(rootId, u"First"_ns);
    TEST_TRY_UNWRAP(
        EntryId firstChildDir,
        dm->GetOrCreateDirectory(firstChildMeta, /* create */ true));

    {
      // Check that directory listing is as expected
      TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                      dm->GetDirectoryEntries(rootId, /* page */ 0u));
      ASSERT_TRUE(contents.files().IsEmpty());
      ASSERT_EQ(1u, contents.directories().Length());
      ASSERT_STREQ(firstChildMeta.childName(),
                   contents.directories()[0].entryName());
    }

    {
      // Try to move subdirectory to its current location
      FileSystemEntryMetadata src{firstChildDir, firstChildMeta.childName(),
                                  /* is directory */ true};
      FileSystemChildMetadata dest{rootId, src.entryName()};
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      firstChildDir = moved;
    }

    {
      // Try to move subdirectory under itself
      FileSystemEntryMetadata src{firstChildDir, firstChildMeta.childName(),
                                  /* is directory */ true};
      FileSystemChildMetadata dest{src.entryId(), src.entryName()};
      TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
      ASSERT_NSEQ(NS_ERROR_DOM_INVALID_MODIFICATION_ERR, rv);
    }

    {
      // Try to move root under its subdirectory
      FileSystemEntryMetadata src{rootId, rootMeta.entryName(),
                                  /* is directory */ true};
      FileSystemChildMetadata dest{firstChildDir, src.entryName()};
      TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
      ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);
    }

    // Create subsubdirectory
    FileSystemChildMetadata firstChildDescendantMeta(firstChildDir,
                                                     u"Descendant"_ns);
    TEST_TRY_UNWRAP(EntryId firstChildDescendant,
                    dm->GetOrCreateDirectory(firstChildDescendantMeta,
                                             /* create */ true));

    {
      TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                      dm->GetDirectoryEntries(firstChildDir, /* page */ 0u));
      ASSERT_TRUE(contents.files().IsEmpty());
      ASSERT_EQ(1u, contents.directories().Length());
      ASSERT_STREQ(firstChildDescendantMeta.childName(),
                   contents.directories()[0].entryName());

      TEST_TRY_UNWRAP(
          Path subSubDirPath,
          dm->Resolve({rootId, contents.directories()[0].entryId()}));
      ASSERT_EQ(2u, subSubDirPath.Length());
      ASSERT_STREQ(firstChildMeta.childName(), subSubDirPath[0]);
      ASSERT_STREQ(firstChildDescendantMeta.childName(), subSubDirPath[1]);
    }

    {
      // Try to move subsubdirectory to its current location
      FileSystemEntryMetadata src{firstChildDescendant,
                                  firstChildDescendantMeta.childName(),
                                  /* is directory */ true};
      FileSystemChildMetadata dest{firstChildDir, src.entryName()};
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      firstChildDescendant = moved;
    }

    {
      // Try to move subsubdirectory under itself
      FileSystemEntryMetadata src{firstChildDescendant,
                                  firstChildDescendantMeta.childName(),
                                  /* is directory */ true};
      FileSystemChildMetadata dest{src.entryId(), src.entryName()};
      TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
      ASSERT_NSEQ(NS_ERROR_DOM_INVALID_MODIFICATION_ERR, rv);
    }

    {
      // Try to move subdirectory under its descendant
      FileSystemEntryMetadata src{firstChildDir, firstChildMeta.childName(),
                                  /* is directory */ true};
      FileSystemChildMetadata dest{firstChildDescendant, src.entryName()};
      TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
      ASSERT_NSEQ(NS_ERROR_DOM_INVALID_MODIFICATION_ERR, rv);
    }

    {
      // Try to move root under its subsubdirectory
      FileSystemEntryMetadata src{rootId, rootMeta.entryName(),
                                  /* is directory */ true};
      FileSystemChildMetadata dest{firstChildDescendant, src.entryName()};
      TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
      ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);
    }

    // Create file in the subdirectory with already existing subsubdirectory
    FileSystemChildMetadata testFileMeta(firstChildDir, u"Subfile"_ns);
    TEST_TRY_UNWRAP(EntryId testFile,
                    dm->GetOrCreateFile(testFileMeta, /* create */ true));

    // Get handles to the original locations of the entries
    FileSystemEntryMetadata subSubDir;
    FileSystemEntryMetadata subSubFile;

    {
      TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                      dm->GetDirectoryEntries(firstChildDir, /* page */ 0u));
      ASSERT_EQ(1u, contents.files().Length());
      ASSERT_EQ(1u, contents.directories().Length());

      subSubDir = contents.directories()[0];
      ASSERT_STREQ(firstChildDescendantMeta.childName(), subSubDir.entryName());

      subSubFile = contents.files()[0];
      ASSERT_STREQ(testFileMeta.childName(), subSubFile.entryName());
      ASSERT_EQ(testFile, subSubFile.entryId());
    }

    {
      TEST_TRY_UNWRAP(Path entryPath,
                      dm->Resolve({rootId, subSubFile.entryId()}));
      ASSERT_EQ(2u, entryPath.Length());
      ASSERT_STREQ(firstChildMeta.childName(), entryPath[0]);
      ASSERT_STREQ(testFileMeta.childName(), entryPath[1]);
    }

    {
      // Try to move file to its current location
      FileSystemEntryMetadata src{testFile, testFileMeta.childName(),
                                  /* is directory */ false};
      FileSystemChildMetadata dest{firstChildDir, src.entryName()};
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      testFile = moved;
    }

    {
      // Try to move subsubdirectory under a file
      FileSystemEntryMetadata src{firstChildDescendant,
                                  firstChildDescendantMeta.childName(),
                                  /* is directory */ true};
      FileSystemChildMetadata dest{testFile,
                                   firstChildDescendantMeta.childName()};
      TEST_TRY_UNWRAP_ERR(nsresult rv, dm->MoveEntry(src, dest));
      ASSERT_NSEQ(NS_ERROR_STORAGE_CONSTRAINT, rv);
    }

    {
      // Silently overwrite a directory with a file using rename
      FileSystemEntryMetadata src{testFile, testFileMeta.childName(),
                                  /* is directory */ false};
      const FileSystemChildMetadata& dest = firstChildDescendantMeta;
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      testFile = moved;
    }

    {
      // Move file back and recreate the directory
      FileSystemEntryMetadata src{testFile,
                                  firstChildDescendantMeta.childName(),
                                  /* is directory */ false};

      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, testFileMeta));
      ASSERT_FALSE(moved.IsEmpty());
      testFile = moved;

      TEST_TRY_UNWRAP(EntryId firstChildDescendantCheck,
                      dm->GetOrCreateDirectory(firstChildDescendantMeta,
                                               /* create */ true));
      ASSERT_EQ(firstChildDescendant, firstChildDescendantCheck);
    }

    {
      // Try to rename directory to quietly overwrite a file
      FileSystemEntryMetadata src{firstChildDescendant,
                                  firstChildDescendantMeta.childName(),
                                  /* is directory */ true};
      const FileSystemChildMetadata& dest = testFileMeta;
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      firstChildDescendant = moved;
    }

    {
      // Move directory back and recreate the file
      FileSystemEntryMetadata src{firstChildDescendant,
                                  testFileMeta.childName(),
                                  /* is directory */ true};

      FileSystemChildMetadata dest{firstChildDir,
                                   firstChildDescendantMeta.childName()};

      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      firstChildDescendant = moved;

      TEST_TRY_UNWRAP(EntryId testFileCheck,
                      dm->GetOrCreateFile(testFileMeta, /* create */ true));
      ASSERT_EQ(testFile, testFileCheck);
    }

    {
      // Move file one level up
      FileSystemEntryMetadata src{testFile, testFileMeta.childName(),
                                  /* is directory */ false};
      FileSystemChildMetadata dest{rootId, src.entryName()};
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      testFile = moved;
    }

    {
      // Check that listings are as expected
      TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                      dm->GetDirectoryEntries(firstChildDir, 0u));
      ASSERT_TRUE(contents.files().IsEmpty());
      ASSERT_EQ(1u, contents.directories().Length());
      ASSERT_STREQ(firstChildDescendantMeta.childName(),
                   contents.directories()[0].entryName());
    }

    {
      TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                      dm->GetDirectoryEntries(rootId, 0u));
      ASSERT_EQ(1u, contents.files().Length());
      ASSERT_EQ(1u, contents.directories().Length());
      ASSERT_STREQ(testFileMeta.childName(), contents.files()[0].entryName());
      ASSERT_STREQ(firstChildMeta.childName(),
                   contents.directories()[0].entryName());
    }

    {
      ASSERT_NO_FATAL_FAILURE(
          AssertEntryIdMoved(subSubFile.entryId(), testFile));
      TEST_TRY_UNWRAP(Path entryPath,
                      dm->Resolve({rootId, subSubFile.entryId()}));
      if (1 == sVersion) {
        ASSERT_EQ(1u, entryPath.Length());
        ASSERT_STREQ(testFileMeta.childName(), entryPath[0]);
      } else {
        ASSERT_EQ(2, sVersion);
        // Per spec, path result is empty when no path exists.
        ASSERT_TRUE(entryPath.IsEmpty());
      }
    }

    {
      // Try to get a handle to the old item
      TEST_TRY_UNWRAP_ERR(
          nsresult rv, dm->GetOrCreateFile(testFileMeta, /* create */ false));
      ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);
    }

    {
      // Try to move + rename file one level down to collide with a
      // subSubDirectory, silently overwriting it
      FileSystemEntryMetadata src{testFile, testFileMeta.childName(),
                                  /* is directory */ false};
      FileSystemChildMetadata dest{firstChildDir,
                                   firstChildDescendantMeta.childName()};
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      testFile = moved;
    }

    {
      // Restore filename, move file to its original location and recreate
      // the overwritten directory
      FileSystemEntryMetadata src{testFile,
                                  firstChildDescendantMeta.childName(),
                                  /* is directory */ false};
      FileSystemChildMetadata dest{rootId, testFileMeta.childName()};
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      testFile = moved;

      FileSystemChildMetadata oldLocation{firstChildDir,
                                          firstChildDescendantMeta.childName()};

      // Is there still something out there?
      TEST_TRY_UNWRAP_ERR(nsresult rv,
                          dm->GetOrCreateFile(oldLocation, /* create */ false));
      ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

      TEST_TRY_UNWRAP(EntryId firstChildDescendantCheck,
                      dm->GetOrCreateDirectory(oldLocation, /* create */ true));
      ASSERT_EQ(firstChildDescendant, firstChildDescendantCheck);
    }

    // Rename file first and then try to move it to collide with
    // subSubDirectory, silently overwriting it
    {
      // Rename
      FileSystemEntryMetadata src{testFile, testFileMeta.childName(),
                                  /* is directory */ false};
      FileSystemChildMetadata dest{rootId,
                                   firstChildDescendantMeta.childName()};
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      testFile = moved;
    }

    {
      // Try to move one level down
      FileSystemEntryMetadata src{testFile,
                                  firstChildDescendantMeta.childName(),
                                  /* is directory */ false};
      FileSystemChildMetadata dest{firstChildDir,
                                   firstChildDescendantMeta.childName()};
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      testFile = moved;
    }

    {
      // Move the file back and recreate the directory
      FileSystemEntryMetadata src{testFile,
                                  firstChildDescendantMeta.childName(),
                                  /* is directory */ false};
      FileSystemChildMetadata dest{rootId,
                                   firstChildDescendantMeta.childName()};
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      testFile = moved;

      FileSystemChildMetadata oldLocation{firstChildDir,
                                          firstChildDescendantMeta.childName()};

      // Is there still something out there?
      TEST_TRY_UNWRAP_ERR(nsresult rv,
                          dm->GetOrCreateFile(oldLocation, /* create */ false));
      ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

      TEST_TRY_UNWRAP(EntryId firstChildDescendantCheck,
                      dm->GetOrCreateDirectory(oldLocation, /* create */ true));
      ASSERT_EQ(firstChildDescendant, firstChildDescendantCheck);
    }

    {
      // Try to move subSubDirectory one level up to quietly overwrite a file
      FileSystemEntryMetadata src{firstChildDescendant,
                                  firstChildDescendantMeta.childName(),
                                  /* is directory */ true};
      FileSystemChildMetadata dest{rootId,
                                   firstChildDescendantMeta.childName()};
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      firstChildDescendant = moved;
    }

    {
      // Move subSubDirectory back one level down and recreate the file
      FileSystemEntryMetadata src{firstChildDescendant,
                                  firstChildDescendantMeta.childName(),
                                  /* is directory */ true};
      FileSystemChildMetadata dest{firstChildDir,
                                   firstChildDescendantMeta.childName()};
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      firstChildDescendant = moved;

      FileSystemChildMetadata oldLocation{rootId,
                                          firstChildDescendantMeta.childName()};

      // We should no longer find anything there
      TEST_TRY_UNWRAP_ERR(nsresult rv, dm->GetOrCreateDirectory(
                                           oldLocation, /* create */ false));
      ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

      TEST_TRY_UNWRAP(EntryId testFileCheck,
                      dm->GetOrCreateFile(oldLocation, /* create */ true));
      ASSERT_NO_FATAL_FAILURE(AssertEntryIdCollision(testFile, testFileCheck));
      testFile = testFileCheck;
    }

    // Create a new file in the subsubdirectory
    FileSystemChildMetadata newFileMeta{firstChildDescendant,
                                        testFileMeta.childName()};
    EntryId oldFirstChildDescendant = firstChildDescendant;

    TEST_TRY_UNWRAP(EntryId newFile,
                    dm->GetOrCreateFile(newFileMeta, /* create */ true));

    {
      TEST_TRY_UNWRAP(Path entryPath, dm->Resolve({rootId, newFile}));
      ASSERT_EQ(3u, entryPath.Length());
      ASSERT_STREQ(firstChildMeta.childName(), entryPath[0]);
      ASSERT_STREQ(firstChildDescendantMeta.childName(), entryPath[1]);
      ASSERT_STREQ(testFileMeta.childName(), entryPath[2]);
    }

    {
      // Move subSubDirectory one level up and rename it to testFile's old name
      FileSystemEntryMetadata src{firstChildDescendant,
                                  firstChildDescendantMeta.childName(),
                                  /* is directory */ true};
      FileSystemChildMetadata dest{rootId, testFileMeta.childName()};
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      firstChildDescendant = moved;
    }

    {
      // Try to get handles to the moved items
      TEST_TRY_UNWRAP_ERR(nsresult rv,
                          dm->GetOrCreateDirectory(firstChildDescendantMeta,
                                                   /* create */ false));
      ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

      //  Still under the same parent which was moved
      if (1 == sVersion) {
        TEST_TRY_UNWRAP(EntryId handle,
                        dm->GetOrCreateFile(newFileMeta, /* create */ false));
        ASSERT_EQ(handle, newFile);

        TEST_TRY_UNWRAP(
            handle, dm->GetOrCreateDirectory({rootId, testFileMeta.childName()},
                                             /* create */ false));
        ASSERT_EQ(handle, firstChildDescendant);
      } else if (2 == sVersion) {
        TEST_TRY_UNWRAP_ERR(
            rv, dm->GetOrCreateFile(newFileMeta, /* create */ false));
        ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

        TEST_TRY_UNWRAP(
            EntryId newFileCheck,
            dm->GetOrCreateFile({firstChildDescendant, newFileMeta.childName()},
                                /* create */ false));
        ASSERT_FALSE(newFileCheck.IsEmpty());
      } else {
        ASSERT_FALSE(false)
        << "Unknown database version";
      }
    }

    {
      // Check that new file path is as expected
      TEST_TRY_UNWRAP(
          EntryId newFileCheck,
          dm->GetOrCreateFile({firstChildDescendant, newFileMeta.childName()},
                              /* create */ false));
      ASSERT_NO_FATAL_FAILURE(AssertEntryIdMoved(newFileCheck, newFile));
      newFile = newFileCheck;

      TEST_TRY_UNWRAP(Path entryPath, dm->Resolve({rootId, newFile}));
      ASSERT_EQ(2u, entryPath.Length());
      ASSERT_STREQ(testFileMeta.childName(), entryPath[0]);
      ASSERT_STREQ(testFileMeta.childName(), entryPath[1]);
    }

    // Move first file and subSubDirectory back one level down keeping the names
    {
      FileSystemEntryMetadata src{testFile,
                                  firstChildDescendantMeta.childName(),
                                  /* is directory */ false};
      FileSystemChildMetadata dest{firstChildDir,
                                   firstChildDescendantMeta.childName()};

      // Flag is ignored
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      testFile = moved;
    }

    {
      // Then move the directory
      FileSystemEntryMetadata src{firstChildDescendant,
                                  testFileMeta.childName(),
                                  /* is directory */ true};
      FileSystemChildMetadata dest{firstChildDir, testFileMeta.childName()};

      // Flag is ignored
      TEST_TRY_UNWRAP(EntryId moved, dm->MoveEntry(src, dest));
      ASSERT_FALSE(moved.IsEmpty());
      firstChildDescendant = moved;
    }

    // Check that listings are as expected
    {
      TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                      dm->GetDirectoryEntries(rootId, 0u));
      ASSERT_TRUE(contents.files().IsEmpty());
      ASSERT_EQ(1u, contents.directories().Length());
      ASSERT_STREQ(firstChildMeta.childName(),
                   contents.directories()[0].entryName());
    }

    {
      TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                      dm->GetDirectoryEntries(firstChildDir, 0u));
      ASSERT_EQ(1u, contents.files().Length());
      ASSERT_EQ(1u, contents.directories().Length());
      ASSERT_STREQ(firstChildDescendantMeta.childName(),
                   contents.files()[0].entryName());
      ASSERT_STREQ(testFileMeta.childName(),
                   contents.directories()[0].entryName());
    }

    {
      TEST_TRY_UNWRAP(FileSystemDirectoryListing contents,
                      dm->GetDirectoryEntries(firstChildDescendant, 0u));
      ASSERT_EQ(1u, contents.files().Length());
      ASSERT_TRUE(contents.directories().IsEmpty());
      ASSERT_STREQ(testFileMeta.childName(), contents.files()[0].entryName());
    }

    // Names are swapped
    {
      TEST_TRY_UNWRAP(Path entryPath, dm->Resolve({rootId, testFile}));
      ASSERT_EQ(2u, entryPath.Length());
      ASSERT_STREQ(firstChildMeta.childName(), entryPath[0]);
      ASSERT_STREQ(firstChildDescendantMeta.childName(), entryPath[1]);
    }

    {
      TEST_TRY_UNWRAP(Path entryPath,
                      dm->Resolve({rootId, firstChildDescendant}));
      ASSERT_EQ(2u, entryPath.Length());
      ASSERT_STREQ(firstChildMeta.childName(), entryPath[0]);
      ASSERT_STREQ(testFileMeta.childName(), entryPath[1]);
    }

    {
      // Check that new file path is also as expected
      TEST_TRY_UNWRAP(
          EntryId newFileCheck,
          dm->GetOrCreateFile({firstChildDescendant, newFileMeta.childName()},
                              /* create */ false));
      ASSERT_NO_FATAL_FAILURE(AssertEntryIdMoved(newFileCheck, newFile));
      newFile = newFileCheck;

      TEST_TRY_UNWRAP(Path entryPath, dm->Resolve({rootId, newFile}));
      ASSERT_EQ(3u, entryPath.Length());
      ASSERT_STREQ(firstChildMeta.childName(), entryPath[0]);
      ASSERT_STREQ(testFileMeta.childName(), entryPath[1]);
      ASSERT_STREQ(testFileMeta.childName(), entryPath[2]);
    }

    {
      // Try to get handles to the old items
      TEST_TRY_UNWRAP_ERR(
          nsresult rv, dm->GetOrCreateFile({rootId, testFileMeta.childName()},
                                           /* create */ false));
      ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

      TEST_TRY_UNWRAP_ERR(
          rv,
          dm->GetOrCreateFile({rootId, firstChildDescendantMeta.childName()},
                              /* create */ false));
      ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

      TEST_TRY_UNWRAP_ERR(
          rv, dm->GetOrCreateDirectory({rootId, testFileMeta.childName()},
                                       /* create */ false));
      ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

      TEST_TRY_UNWRAP_ERR(rv,
                          dm->GetOrCreateDirectory(
                              {rootId, firstChildDescendantMeta.childName()},
                              /* create */ false));
      ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);

      TEST_TRY_UNWRAP_ERR(
          rv, dm->GetOrCreateFile({firstChildDir, testFileMeta.childName()},
                                  /* create */ false));
      ASSERT_NSEQ(NS_ERROR_DOM_TYPE_MISMATCH_ERR, rv);

      TEST_TRY_UNWRAP_ERR(
          rv, dm->GetOrCreateDirectory(
                  {firstChildDir, firstChildDescendantMeta.childName()},
                  /* create */ false));
      ASSERT_NSEQ(NS_ERROR_DOM_TYPE_MISMATCH_ERR, rv);

      TEST_TRY_UNWRAP_ERR(
          rv, dm->GetOrCreateFile({testFile, newFileMeta.childName()},
                                  /* create */ false));
      ASSERT_NSEQ(NS_ERROR_DOM_NOT_FOUND_ERR, rv);
    }
  };

  PerformOnIOThread(std::move(ioTask));
}

INSTANTIATE_TEST_SUITE_P(TestDatabaseManagerVersions,
                         TestFileSystemDatabaseManagerVersions,
                         testing::Values(1, 2));

}  // namespace mozilla::dom::fs::test
