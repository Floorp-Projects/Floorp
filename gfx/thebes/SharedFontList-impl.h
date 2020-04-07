/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SharedFontList_impl_h
#define SharedFontList_impl_h

#include "SharedFontList.h"

#include "base/shared_memory.h"

#include "gfxFontUtils.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"

// This is split out from SharedFontList.h because that header is included
// quite widely (via gfxPlatformFontList.h, gfxTextRun.h, etc), and other code
// such as the generated DOM bindings code gets upset at (indirect) inclusion
// of <windows.h> via SharedMemoryBasic.h. So this header, which defines the
// actual shared-memory FontList class, is included only by the .cpp files that
// implement or directly interface with the font list, to avoid polluting other
// headers.

namespace mozilla {
namespace fontlist {

/**
 * Data used to initialize a font family alias (a "virtual" family that refers
 * to some or all of the faces of another family, used when alternate family
 * names are found in the font resource for localization or for styled
 * subfamilies). AliasData records are collected incrementally while scanning
 * the fonts, and then used to set up the Aliases list in the shared font list.
 */
struct AliasData {
  nsTArray<Pointer> mFaces;
  uint32_t mIndex = 0;
  FontVisibility mVisibility = FontVisibility::Unknown;
  bool mBundled = false;
  bool mBadUnderline = false;
  bool mForceClassic = false;

  void InitFromFamily(const Family* aFamily) {
    mIndex = aFamily->Index();
    mVisibility = aFamily->Visibility();
    mBundled = aFamily->IsBundled();
    mBadUnderline = aFamily->IsBadUnderlineFamily();
    mForceClassic = aFamily->IsForceClassic();
  }
};

/**
 * The Shared Font List is a collection of data that lives in shared memory
 * so that all processes can use it, rather than maintaining their own copies,
 * and provides the metadata needed for CSS font-matching (a list of all the
 * available font families and their faces, style properties, etc, as well as
 * character coverage).
 *
 * An important assumption is that all processes see the same collection of
 * installed fonts; therefore it is valid for them all to share the same set
 * of font metadata. The data is updated only by the parent process; content
 * processes have read-only access to it.
 *
 * The total size of this data varies greatly depending on the user's installed
 * fonts; and it is not known at startup because we load a lot of the font data
 * on first use rather than preloading during initialization (because that's
 * too expensive/slow).
 *
 * Therefore, the shared memory area needs to be able to grow during the
 * session; we can't predict how much space will be needed, and we can't afford
 * to pre-allocate such a huge block that it could never overflow. To handle
 * this, we maintain a (generally short) array of blocks of shared memory,
 * and then allocate our Family, Face, etc. objects within these. Because we
 * only ever add data (never delete what we've already stored), we can use a
 * simplified allocator that doesn't ever need to free blocks; the only time
 * the memory is released during a session is the (rare) case where a font is
 * installed or deleted while the browser is running, and in this case we just
 * delete the entire shared font list and start afresh.
 */
class FontList {
 public:
  friend struct Pointer;

  explicit FontList(uint32_t aGeneration);
  ~FontList();

  /**
   * Initialize the master list of installed font families. This must be
   * set during font-list creation, before the list is shared with any
   * content processes. All installed font families known to the browser
   * appear in this list, although some may be marked as "hidden" so that
   * they are not exposed to the font-family property.
   *
   * Once initialized, the master family list is immutable; in the (rare)
   * event that the system's collection of installed fonts changes, we discard
   * the FontList and create a new one.
   *
   * In some cases, a font family may be known by multiple names (e.g.
   * localizations in multiple languages, or there may be legacy family names
   * that correspond to specific styled faces like "Arial Black"). Such names
   * do not appear in this master list, but are referred to as aliases (see
   * SetAliases below); the alias list need not be populated before the font
   * list is shared to content processes and used.
   *
   * Only used in the parent process.
   */
  void SetFamilyNames(const nsTArray<Family::InitData>& aFamilies);

  /**
   * Aliases are Family records whose Face entries are already part of another
   * family (either because the family has multiple localized names, or because
   * the alias family is a legacy name like "Arial Narrow" that is a subset of
   * the faces in the main "Arial" family). The table of aliases is initialized
   * from a hash of alias family name -> array of Face records.
   *
   * Like the master family list, the list of family aliases is immutable once
   * initialized.
   *
   * Only used in the parent process.
   */
  void SetAliases(nsClassHashtable<nsCStringHashKey, AliasData>& aAliasTable);

  /**
   * Local names are PostScript or Full font names of individual faces, used
   * to look up faces for @font-face { src: local(...) } rules. Some platforms
   * (e.g. macOS) can look up local names directly using platform font APIs,
   * in which case the local names table here is unused.
   *
   * The list of local names is immutable once initialized. Local font name
   * lookups may occur before this list has been set up, in which case they
   * will use the SearchForLocalFace method.
   *
   * Only used in the parent process.
   */
  void SetLocalNames(nsDataHashtable<nsCStringHashKey, LocalFaceRec::InitData>&
                         aLocalNameTable);

  /**
   * Look up a Family record by name, typically to satisfy the font-family
   * property or a font family listed in preferences.
   * If aAllowHidden is true, "system" font families normally not exposed
   * to users may be found.
   */
  Family* FindFamily(const nsCString& aName, bool aAllowHidden = false);

  /**
   * Look up an individual Face by PostScript or Full name, for @font-face
   * rules using src:local(...). This requires the local names list to have
   * been initialized.
   */
  LocalFaceRec* FindLocalFace(const nsCString& aName);

  /**
   * Search families for a face with local name aName; should only be used if
   * the mLocalFaces array has not yet been set up, as this will be a more
   * expensive search than FindLocalFace.
   */
  void SearchForLocalFace(const nsACString& aName, Family** aFamily,
                          Face** aFace);

  bool Initialized() { return mBlocks.Length() > 0 && NumFamilies() > 0; }

  uint32_t NumFamilies() { return GetHeader().mFamilyCount; }
  Family* Families() {
    return static_cast<Family*>(GetHeader().mFamilies.ToPtr(this));
  }

  uint32_t NumAliases() { return GetHeader().mAliasCount; }
  Family* AliasFamilies() {
    return static_cast<Family*>(GetHeader().mAliases.ToPtr(this));
  }

  uint32_t NumLocalFaces() { return GetHeader().mLocalFaceCount; }
  LocalFaceRec* LocalFaces() {
    return static_cast<LocalFaceRec*>(GetHeader().mLocalFaces.ToPtr(this));
  }

  /**
   * Ask the font list to initialize the character map for a given face.
   */
  void LoadCharMapFor(Face& aFace, const Family* aFamily);

  /**
   * Allocate shared-memory space for a record of aSize bytes. The returned
   * pointer will be 32-bit aligned. (This method may trigger the allocation of
   * a new shared memory block, if required.)
   *
   * Only used in the parent process.
   */
  Pointer Alloc(uint32_t aSize);

  /**
   * Convert a native pointer to a shared-memory Pointer record that can be
   * passed between processes.
   */
  Pointer ToSharedPointer(const void* aPtr);

  uint32_t GetGeneration() { return GetHeader().mGeneration; }

  /**
   * Header info that is stored at the beginning of the first shared-memory
   * block for the font list.
   * (Subsequent blocks have only the mAllocated field, accessed via the
   * Block::Allocated() method.)
   * The mGeneration and mFamilyCount fields are set by the parent process
   * during font-list construction, before the list has been shared with any
   * other process, and subsequently never change; therefore, we don't need
   * to use std::atomic<> for these.
   */
  struct Header {
    std::atomic<uint32_t> mAllocated;   // Space allocated from this block;
                                        // must be first field in Header
    uint32_t mGeneration;               // Font-list generation ID
    uint32_t mFamilyCount;              // Number of font families in the list
    std::atomic<uint32_t> mBlockCount;  // Total number of blocks that exist
    std::atomic<uint32_t> mAliasCount;  // Number of family aliases
    std::atomic<uint32_t> mLocalFaceCount;  // Number of local face names
    Pointer mFamilies;    // Pointer to array of |mFamilyCount| families
    Pointer mAliases;     // Pointer to array of |mAliasCount| aliases
    Pointer mLocalFaces;  // Pointer to array of |mLocalFaceCount| face records
  };

  /**
   * Used by the parent process to pass a handle to a shared block to a
   * specific child process.
   */
  void ShareShmBlockToProcess(uint32_t aIndex, base::ProcessId aPid,
                              base::SharedMemoryHandle* aOut) {
    MOZ_RELEASE_ASSERT(mReadOnlyShmems.Length() == mBlocks.Length());
    if (aIndex >= mReadOnlyShmems.Length()) {
      // Block index out of range
      *aOut = base::SharedMemory::NULLHandle();
    }
    if (!mReadOnlyShmems[aIndex]->ShareToProcess(aPid, aOut)) {
      MOZ_CRASH("failed to share block");
    }
  }

  /**
   * Support for memory reporter.
   */
  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    size_t result = mBlocks.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (const auto& b : mBlocks) {
      result += aMallocSizeOf(b.get()) + aMallocSizeOf(b->mShmem.get());
    }
    return result;
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  size_t AllocatedShmemSize() const {
    return mBlocks.Length() * SHM_BLOCK_SIZE;
  }

  /**
   * This must be large enough that we can allocate the largest possible
   * SharedBitSet (around 140K, see comments on SharedBitSet in gfxFontUtils.h)
   * within a single ShmBlock.
   * Using a larger block size will speed up allocation, at the cost of more
   * wasted space in the shared memory (on average).
   */
  static const size_t SHM_BLOCK_SIZE = 256 * 1024;
  static_assert(SHM_BLOCK_SIZE <= (1 << Pointer::kBlockShift),
                "SHM_BLOCK_SIZE too large");
  static_assert(SHM_BLOCK_SIZE >= SharedBitSet::kMaxSize + 4,
                "SHM_BLOCK_SIZE too small");

 private:
  struct ShmBlock {
    // Takes ownership of aShmem
    ShmBlock(base::SharedMemory* aShmem) : mShmem(aShmem) {}

    // Get pointer to the mapped memory.
    void* Memory() const { return mShmem->memory(); }

    // The first 32-bit word of each block holds the current amount allocated
    // in that block; this is updated whenever a new record is stored in the
    // block.
    std::atomic<uint32_t>& Allocated() const {
      return *static_cast<std::atomic<uint32_t>*>(Memory());
    }

    mozilla::UniquePtr<base::SharedMemory> mShmem;
  };

  Header& GetHeader() {
    // It's invalid to try and access this before the first block exists.
    MOZ_ASSERT(mBlocks.Length() > 0);
    return *static_cast<Header*>(Pointer(0, 0).ToPtr(this));
  }

  /**
   * Create a new shared memory block and append to the FontList's list
   * of blocks.
   *
   * Only used in the parent process.
   */
  bool AppendShmBlock();

  /**
   * Used by child processes to ensure all the blocks are registered.
   * Returns false on failure.
   */
  [[nodiscard]] bool UpdateShmBlocks();

  /**
   * This makes a *sync* IPC call to get a shared block from the parent.
   * As such, it may block for a while if the parent is busy; fortunately,
   * we'll generally only call this a handful of times in the course of an
   * entire session. If the requested block does not yet exist (because the
   * child is wanting to allocate an object, and there wasn't room in any
   * existing block), the parent will create a new shared block and return it.
   * This may (in rare cases) return null, if the parent has recreated the
   * font list and we actually need to reinitialize.
   */
  ShmBlock* GetBlockFromParent(uint32_t aIndex);

  void DetachShmBlocks();

  /**
   * Array of pointers to the shared-memory block records.
   * NOTE: if mBlocks.Length() < GetHeader().mBlockCount, then the parent has
   * added a block (or blocks) to the list, and we need to update!
   */
  nsTArray<mozilla::UniquePtr<ShmBlock>> mBlocks;

  /**
   * Auxiliary array, used only in the parent process; holds read-only copies
   * of the shmem blocks; these are what will be shared to child processes.
   */
  nsTArray<mozilla::UniquePtr<base::SharedMemory>> mReadOnlyShmems;
};

}  // namespace fontlist
}  // namespace mozilla

#endif /* SharedFontList_impl_h */
