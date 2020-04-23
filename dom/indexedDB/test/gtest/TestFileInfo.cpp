/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileInfoTImpl.h"
#include "FileManagerBase.h"

#include "gtest/gtest.h"

#include "mozilla/ArrayAlgorithm.h"
#include "mozilla/StaticMutex.h"
#include "nsTArray.h"

#include <array>

using namespace mozilla;
using namespace mozilla::dom::indexedDB;

struct TestFileManagerStats final {
  // XXX We don't keep track of the specific aFileId parameters here, should we?

  size_t mAsyncDeleteFileCalls = 0;
  size_t mSyncDeleteFileCalls = 0;
};

class TestFileManager final : public FileManagerBase<TestFileManager> {
 public:
  using FileManagerBase<TestFileManager>::MutexType;

  // FileManager functions that are used by FileInfo

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestFileManager)

  [[nodiscard]] nsresult AsyncDeleteFile(const int64_t aFileId) {
    MOZ_RELEASE_ASSERT(!mFileInfos.Contains(aFileId));

    if (mStats) {
      ++mStats->mAsyncDeleteFileCalls;
    }

    return NS_OK;
  }

  [[nodiscard]] nsresult SyncDeleteFile(const int64_t aFileId) {
    MOZ_RELEASE_ASSERT(!mFileInfos.Contains(aFileId));

    if (mStats) {
      ++mStats->mSyncDeleteFileCalls;
    }
    return NS_OK;
  }

  // Test-specific functions
  explicit TestFileManager(TestFileManagerStats* aStats = nullptr)
      : mStats{aStats} {}

  void CreateDBOnlyFileInfos() {
    for (const auto id : kDBOnlyFileInfoIds) {
      // Copied from within FileManager::Init.

      mFileInfos.Put(
          id, new FileInfo(FileManagerGuard{},
                           SafeRefPtr{this, AcquireStrongRefFromRawPtr{}}, id,
                           static_cast<nsrefcnt>(1)));

      mLastFileId = std::max(id, mLastFileId);
    }
  }

  static MutexType& Mutex() { return sMutex; }

  static constexpr auto kDBOnlyFileInfoIds =
      std::array<int64_t, 3>{{10, 20, 30}};

 private:
  inline static MutexType sMutex;

  TestFileManagerStats* const mStats;

  ~TestFileManager() = default;
};

using TestFileInfo = FileInfoT<TestFileManager>;

// These tests test the TestFileManager itself, to ensure the FileInfo tests
// below are valid.

TEST(DOM_IndexedDB_TestFileManager, Invalidate)
{
  const auto fileManager = MakeRefPtr<TestFileManager>();

  fileManager->Invalidate();

  ASSERT_TRUE(fileManager->Invalidated());
}

// These tests mainly test FileInfo, but it is not completely independent from
// the FileManager concept, including the functionality of FileManagerBase.
// The actual FileManager is not tested here.

TEST(DOM_IndexedDB_FileInfo, Create)
{
  auto stats = TestFileManagerStats{};

  {
    const auto fileManager = MakeRefPtr<TestFileManager>(&stats);
    auto fileInfo = fileManager->CreateFileInfo();

    int32_t memRefCnt, dbRefCnt;
    fileInfo->GetReferences(&memRefCnt, &dbRefCnt);

    ASSERT_EQ(fileManager, &fileInfo->Manager());

    ASSERT_EQ(1, memRefCnt);
    ASSERT_EQ(0, dbRefCnt);
  }

  ASSERT_EQ(0u, stats.mSyncDeleteFileCalls);
  ASSERT_EQ(1u, stats.mAsyncDeleteFileCalls);
}

TEST(DOM_IndexedDB_FileInfo, CreateWithInitialDBRefCnt)
{
  auto stats = TestFileManagerStats{};

  {
    const auto fileManager = MakeRefPtr<TestFileManager>(&stats);
    fileManager->CreateDBOnlyFileInfos();

    for (const auto id : TestFileManager::kDBOnlyFileInfoIds) {
      const auto fileInfo = fileManager->GetFileInfo(id);
      ASSERT_NE(nullptr, fileInfo);

      int32_t memRefCnt, dbRefCnt;
      fileInfo->GetReferences(&memRefCnt, &dbRefCnt);

      ASSERT_EQ(fileManager, &fileInfo->Manager());

      ASSERT_EQ(1, memRefCnt);  // we hold one in fileInfo ourselves
      ASSERT_EQ(1, dbRefCnt);
    }
  }

  ASSERT_EQ(0u, stats.mSyncDeleteFileCalls);
  // Since the files have still non-zero dbRefCnt, nothing must be deleted.
  ASSERT_EQ(0u, stats.mAsyncDeleteFileCalls);
}

TEST(DOM_IndexedDB_FileInfo, CreateWithInitialDBRefCnt_Invalidate)
{
  auto stats = TestFileManagerStats{};

  {
    const auto fileManager = MakeRefPtr<TestFileManager>(&stats);
    fileManager->CreateDBOnlyFileInfos();

    const auto fileInfos = TransformIntoNewArray(
        TestFileManager::kDBOnlyFileInfoIds,
        [&fileManager](const auto id) { return fileManager->GetFileInfo(id); });

    fileManager->Invalidate();

    for (const auto& fileInfo : fileInfos) {
      int32_t memRefCnt, dbRefCnt;
      fileInfo->GetReferences(&memRefCnt, &dbRefCnt);

      ASSERT_EQ(1, memRefCnt);  // we hold one in fileInfo ourselves
      ASSERT_EQ(0, dbRefCnt);   // dbRefCnt was cleared by Invalidate
    }
  }

  ASSERT_EQ(0u, stats.mSyncDeleteFileCalls);
  // Since the files have still non-zero dbRefCnt, nothing must be deleted.
  ASSERT_EQ(0u, stats.mAsyncDeleteFileCalls);
}

TEST(DOM_IndexedDB_FileInfo, CreateWithInitialDBRefCnt_UpdateDBRefsToZero)
{
  auto stats = TestFileManagerStats{};

  {
    const auto fileManager = MakeRefPtr<TestFileManager>(&stats);
    fileManager->CreateDBOnlyFileInfos();

    const auto fileInfo =
        fileManager->GetFileInfo(TestFileManager::kDBOnlyFileInfoIds[0]);
    fileInfo->UpdateDBRefs(-1);

    int32_t memRefCnt, dbRefCnt;
    fileInfo->GetReferences(&memRefCnt, &dbRefCnt);

    ASSERT_EQ(1, memRefCnt);  // we hold one in fileInfo ourselves
    ASSERT_EQ(0, dbRefCnt);
  }

  ASSERT_EQ(0u, stats.mSyncDeleteFileCalls);
  ASSERT_EQ(1u, stats.mAsyncDeleteFileCalls);
}

TEST(DOM_IndexedDB_FileInfo, ReleaseWithFileManagerCleanup)
{
  auto stats = TestFileManagerStats{};
  {
    const auto fileManager = MakeRefPtr<TestFileManager>(&stats);
    fileManager->CreateDBOnlyFileInfos();

    auto* fileInfo = fileManager->CreateFileInfo().forget().take();
    fileInfo->Release(/* aSyncDeleteFile */ true);

    // This was the only reference and FileManager was not invalidated, to
    // FileManager::Cleanup should have been called.
    ASSERT_EQ(1u, stats.mSyncDeleteFileCalls);
  }
  ASSERT_EQ(0u, stats.mAsyncDeleteFileCalls);
}

#ifndef DEBUG
// These tests cause assertion failures in DEBUG builds.

TEST(DOM_IndexedDB_FileInfo, Invalidate_CreateFileInfo)
{
  auto stats = TestFileManagerStats{};
  {
    const auto fileManager = MakeRefPtr<TestFileManager>(&stats);

    fileManager->Invalidate();

    const auto fileInfo = fileManager->CreateFileInfo();
    Unused << fileInfo;

    ASSERT_EQ(nullptr, fileInfo);
  }

  ASSERT_EQ(0u, stats.mSyncDeleteFileCalls);
  ASSERT_EQ(0u, stats.mAsyncDeleteFileCalls);
}
#endif

TEST(DOM_IndexedDB_FileInfo, Invalidate_Release)
{
  auto stats = TestFileManagerStats{};
  {
    const auto fileManager = MakeRefPtr<TestFileManager>(&stats);

    const auto fileInfo = fileManager->CreateFileInfo();
    Unused << fileInfo;

    fileManager->Invalidate();

    // FileManager was invalidated, so Release does not do any cleanup.
  }

  ASSERT_EQ(0u, stats.mSyncDeleteFileCalls);
  ASSERT_EQ(0u, stats.mAsyncDeleteFileCalls);
}

TEST(DOM_IndexedDB_FileInfo, Invalidate_ReleaseWithFileManagerCleanup)
{
  auto stats = TestFileManagerStats{};
  {
    const auto fileManager = MakeRefPtr<TestFileManager>(&stats);

    auto* fileInfo = fileManager->CreateFileInfo().forget().take();

    fileManager->Invalidate();

    // FileManager was invalidated, so Release does not do any cleanup.
    fileInfo->Release(/* aSyncDeleteFile */ true);
  }

  ASSERT_EQ(0u, stats.mSyncDeleteFileCalls);
  ASSERT_EQ(0u, stats.mAsyncDeleteFileCalls);
}

// XXX Add test for GetFileForFileInfo
