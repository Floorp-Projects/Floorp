/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileInfo.h"
#include "FileInfoImpl.h"
#include "FileInfoManager.h"

#include "gtest/gtest.h"

#include "mozilla/ArrayAlgorithm.h"
#include "mozilla/StaticMutex.h"
#include "nsTArray.h"

#include <array>

using namespace mozilla;
using namespace mozilla::dom::indexedDB;

class SimpleFileManager;

using SimpleFileInfo = FileInfo<SimpleFileManager>;

struct SimpleFileManagerStats final {
  // XXX We don't keep track of the specific aFileId parameters here, should we?

  size_t mAsyncDeleteFileCalls = 0;
  size_t mSyncDeleteFileCalls = 0;
};

class SimpleFileManager final : public FileInfoManager<SimpleFileManager>,
                                public AtomicSafeRefCounted<SimpleFileManager> {
 public:
  using FileInfoManager<SimpleFileManager>::MutexType;

  MOZ_DECLARE_REFCOUNTED_TYPENAME(SimpleFileManager)

  // SimpleFileManager functions that are used by SimpleFileInfo

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
  explicit SimpleFileManager(SimpleFileManagerStats* aStats = nullptr)
      : mStats{aStats} {}

  void CreateDBOnlyFileInfos() {
    for (const auto id : kDBOnlyFileInfoIds) {
      // Copied from within DatabaseFileManager::Init.

      mFileInfos.InsertOrUpdate(
          id, MakeNotNull<SimpleFileInfo*>(FileInfoManagerGuard{},
                                           SafeRefPtrFromThis(), id,
                                           static_cast<nsrefcnt>(1)));

      mLastFileId = std::max(id, mLastFileId);
    }
  }

  static MutexType& Mutex() { return sMutex; }

  static constexpr auto kDBOnlyFileInfoIds =
      std::array<int64_t, 3>{{10, 20, 30}};

 private:
  inline static MutexType sMutex;

  SimpleFileManagerStats* const mStats;
};

// These tests test the SimpleFileManager itself, to ensure the SimpleFileInfo
// tests below are valid.

TEST(DOM_IndexedDB_SimpleFileManager, Invalidate)
{
  const auto fileManager = MakeSafeRefPtr<SimpleFileManager>();

  fileManager->Invalidate();

  ASSERT_TRUE(fileManager->Invalidated());
}

// These tests mainly test SimpleFileInfo, which is a simplified version of
// DatabaseFileInfo (SimpleFileInfo doesn't work with real files stored on
// disk). The actual objects, DatabaseFileInfo and DatabaseFileManager are not
// tested here.

TEST(DOM_IndexedDB_SimpleFileInfo, Create)
{
  auto stats = SimpleFileManagerStats{};

  {
    const auto fileManager = MakeSafeRefPtr<SimpleFileManager>(&stats);
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

TEST(DOM_IndexedDB_SimpleFileInfo, CreateWithInitialDBRefCnt)
{
  auto stats = SimpleFileManagerStats{};

  {
    const auto fileManager = MakeSafeRefPtr<SimpleFileManager>(&stats);
    fileManager->CreateDBOnlyFileInfos();

    for (const auto id : SimpleFileManager::kDBOnlyFileInfoIds) {
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

TEST(DOM_IndexedDB_SimpleFileInfo, CreateWithInitialDBRefCnt_Invalidate)
{
  auto stats = SimpleFileManagerStats{};

  {
    const auto fileManager = MakeSafeRefPtr<SimpleFileManager>(&stats);
    fileManager->CreateDBOnlyFileInfos();

    const auto fileInfos = TransformIntoNewArray(
        SimpleFileManager::kDBOnlyFileInfoIds,
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

TEST(DOM_IndexedDB_SimpleFileInfo, CreateWithInitialDBRefCnt_UpdateDBRefsToZero)
{
  auto stats = SimpleFileManagerStats{};

  {
    const auto fileManager = MakeSafeRefPtr<SimpleFileManager>(&stats);
    fileManager->CreateDBOnlyFileInfos();

    const auto fileInfo =
        fileManager->GetFileInfo(SimpleFileManager::kDBOnlyFileInfoIds[0]);
    fileInfo->UpdateDBRefs(-1);

    int32_t memRefCnt, dbRefCnt;
    fileInfo->GetReferences(&memRefCnt, &dbRefCnt);

    ASSERT_EQ(1, memRefCnt);  // we hold one in fileInfo ourselves
    ASSERT_EQ(0, dbRefCnt);
  }

  ASSERT_EQ(0u, stats.mSyncDeleteFileCalls);
  ASSERT_EQ(1u, stats.mAsyncDeleteFileCalls);
}

TEST(DOM_IndexedDB_SimpleFileInfo, ReleaseWithFileManagerCleanup)
{
  auto stats = SimpleFileManagerStats{};
  {
    const auto fileManager = MakeSafeRefPtr<SimpleFileManager>(&stats);
    fileManager->CreateDBOnlyFileInfos();

    auto* fileInfo = fileManager->CreateFileInfo().forget().take();
    fileInfo->Release(/* aSyncDeleteFile */ true);

    // This was the only reference and SimpleFileManager was not invalidated,
    // so SimpleFileManager::Cleanup should have been called.
    ASSERT_EQ(1u, stats.mSyncDeleteFileCalls);
  }
  ASSERT_EQ(0u, stats.mAsyncDeleteFileCalls);
}

#ifndef DEBUG
// These tests cause assertion failures in DEBUG builds.

TEST(DOM_IndexedDB_SimpleFileInfo, Invalidate_CreateFileInfo)
{
  auto stats = SimpleFileManagerStats{};
  {
    const auto fileManager = MakeSafeRefPtr<SimpleFileManager>(&stats);

    fileManager->Invalidate();

    const auto fileInfo = fileManager->CreateFileInfo();
    Unused << fileInfo;

    ASSERT_EQ(nullptr, fileInfo);
  }

  ASSERT_EQ(0u, stats.mSyncDeleteFileCalls);
  ASSERT_EQ(0u, stats.mAsyncDeleteFileCalls);
}
#endif

TEST(DOM_IndexedDB_SimpleFileInfo, Invalidate_Release)
{
  auto stats = SimpleFileManagerStats{};
  {
    const auto fileManager = MakeSafeRefPtr<SimpleFileManager>(&stats);

    const auto fileInfo = fileManager->CreateFileInfo();
    Unused << fileInfo;

    fileManager->Invalidate();

    // SimpleFileManager was invalidated, so Release does not do any cleanup.
  }

  ASSERT_EQ(0u, stats.mSyncDeleteFileCalls);
  ASSERT_EQ(0u, stats.mAsyncDeleteFileCalls);
}

TEST(DOM_IndexedDB_SimpleFileInfo, Invalidate_ReleaseWithFileManagerCleanup)
{
  auto stats = SimpleFileManagerStats{};
  {
    const auto fileManager = MakeSafeRefPtr<SimpleFileManager>(&stats);

    auto* fileInfo = fileManager->CreateFileInfo().forget().take();

    fileManager->Invalidate();

    // SimpleFileManager was invalidated, so Release does not do any cleanup.
    fileInfo->Release(/* aSyncDeleteFile */ true);
  }

  ASSERT_EQ(0u, stats.mSyncDeleteFileCalls);
  ASSERT_EQ(0u, stats.mAsyncDeleteFileCalls);
}

// XXX Add test for GetFileForFileInfo
