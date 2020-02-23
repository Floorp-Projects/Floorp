/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedFontList-impl.h"
#include "gfxPlatformFontList.h"
#include "gfxFontUtils.h"
#include "gfxFont.h"
#include "nsReadableUtils.h"
#include "prerror.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Logging.h"

#define LOG_FONTLIST(args) \
  MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), LogLevel::Debug, args)
#define LOG_FONTLIST_ENABLED() \
  MOZ_LOG_TEST(gfxPlatform::GetLog(eGfxLog_fontlist), LogLevel::Debug)

namespace mozilla {
namespace fontlist {

static double WSSDistance(const Face* aFace, const gfxFontStyle& aStyle) {
  double stretchDist = StretchDistance(aFace->mStretch, aStyle.stretch);
  double styleDist = StyleDistance(aFace->mStyle, aStyle.style);
  double weightDist = WeightDistance(aFace->mWeight, aStyle.weight);

  // Sanity-check that the distances are within the expected range
  // (update if implementation of the distance functions is changed).
  MOZ_ASSERT(stretchDist >= 0.0 && stretchDist <= 2000.0);
  MOZ_ASSERT(styleDist >= 0.0 && styleDist <= 500.0);
  MOZ_ASSERT(weightDist >= 0.0 && weightDist <= 1600.0);

  // weight/style/stretch priority: stretch >> style >> weight
  // so we multiply the stretch and style values to make them dominate
  // the result
  return stretchDist * 1.0e8 + styleDist * 1.0e4 + weightDist;
}

void* Pointer::ToPtr(FontList* aFontList) const {
  if (IsNull()) {
    return nullptr;
  }
  uint32_t block = Block();
  // If the Pointer refers to a block we have not yet mapped in this process,
  // we first need to retrieve new block handle(s) from the parent and update
  // our mBlocks list.
  if (block >= aFontList->mBlocks.Length()) {
    if (XRE_IsParentProcess()) {
      // Shouldn't happen! A content process tried to pass a bad Pointer?
      return nullptr;
    }
    // UpdateShmBlocks can fail, if the parent has replaced the font list with
    // a new generation. In that case we just return null, and whatever font
    // the content process was trying to use will appear unusable for now. It's
    // about to receive a notification of the new font list anyhow, at which
    // point it will flush its caches and reflow everything, so the temporary
    // failure of this font will be forgotten.
    if (!aFontList->UpdateShmBlocks()) {
      return nullptr;
    }
    MOZ_ASSERT(block < aFontList->mBlocks.Length());
  }
  return static_cast<char*>(aFontList->mBlocks[block]->mAddr) + Offset();
}

void String::Assign(const nsACString& aString, FontList* aList) {
  // We only assign to previously-empty strings; they are never modified
  // after initial assignment.
  MOZ_ASSERT(mPointer.IsNull());
  mLength = aString.Length();
  mPointer = aList->Alloc(mLength + 1);
  char* p = static_cast<char*>(mPointer.ToPtr(aList));
  std::memcpy(p, aString.BeginReading(), mLength);
  p[mLength] = '\0';
}

Family::Family(FontList* aList, const InitData& aData)
    : mFaceCount(0),
      mKey(aList, aData.mKey),
      mName(aList, aData.mName),
      mCharacterMap(Pointer::Null()),
      mFaces(Pointer::Null()),
      mIndex(aData.mIndex),
      mIsHidden(aData.mHidden),
      mIsBadUnderlineFamily(aData.mBadUnderline),
      mIsForceClassic(aData.mForceClassic),
      mIsSimple(false) {
  MOZ_ASSERT(aData.mIndex <= 0x7fffffffu);
  mIndex = aData.mIndex | (aData.mBundled ? 0x80000000u : 0u);
}

class SetCharMapRunnable : public mozilla::Runnable {
 public:
  SetCharMapRunnable(uint32_t aListGeneration, Face* aFace,
                     gfxCharacterMap* aCharMap)
      : Runnable("SetCharMapRunnable"),
        mListGeneration(aListGeneration),
        mFace(aFace),
        mCharMap(aCharMap) {}

  NS_IMETHOD Run() override {
    auto* list = gfxPlatformFontList::PlatformFontList()->SharedFontList();
    if (!list || list->GetGeneration() != mListGeneration) {
      return NS_OK;
    }
    dom::ContentChild::GetSingleton()->SendSetCharacterMap(
        mListGeneration, list->ToSharedPointer(mFace), *mCharMap);
    return NS_OK;
  }

 private:
  uint32_t mListGeneration;
  Face* mFace;
  RefPtr<gfxCharacterMap> mCharMap;
};

void Face::SetCharacterMap(FontList* aList, gfxCharacterMap* aCharMap) {
  if (!XRE_IsParentProcess()) {
    if (NS_IsMainThread()) {
      Pointer ptr = aList->ToSharedPointer(this);
      dom::ContentChild::GetSingleton()->SendSetCharacterMap(
          aList->GetGeneration(), ptr, *aCharMap);
    } else {
      NS_DispatchToMainThread(
          new SetCharMapRunnable(aList->GetGeneration(), this, aCharMap));
    }
    return;
  }
  auto pfl = gfxPlatformFontList::PlatformFontList();
  mCharacterMap = pfl->GetShmemCharMap(aCharMap);
}

void Family::AddFaces(FontList* aList, const nsTArray<Face::InitData>& aFaces) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (mFaceCount > 0) {
    // Already initialized!
    return;
  }

  uint32_t count = aFaces.Length();
  bool isSimple = false;
  // A family is "simple" (i.e. simplified style selection may be used instead
  // of the full CSS font-matching algorithm) if there is at maximum one normal,
  // bold, italic, and bold-italic face; in this case, they are stored at known
  // positions in the mFaces array.
  const Face::InitData* slots[4] = {nullptr, nullptr, nullptr, nullptr};
  if (count >= 2 && count <= 4) {
    // Check if this can be treated as a "simple" family
    isSimple = true;
    for (const auto& f : aFaces) {
      if (!f.mWeight.IsSingle() || !f.mStretch.IsSingle() ||
          !f.mStyle.IsSingle()) {
        isSimple = false;
        break;
      }
      if (!f.mStretch.Min().IsNormal()) {
        isSimple = false;
        break;
      }
      // Figure out which slot (0-3) this face belongs in
      size_t slot = 0;
      static_assert((kBoldMask | kItalicMask) == 0b11, "bad bold/italic bits");
      if (f.mWeight.Min().IsBold()) {
        slot |= kBoldMask;
      }
      if (f.mStyle.Min().IsItalic() || f.mStyle.Min().IsOblique()) {
        slot |= kItalicMask;
      }
      if (slots[slot]) {
        // More than one face mapped to the same slot - not a simple family!
        isSimple = false;
        break;
      }
      slots[slot] = &f;
    }
    if (isSimple) {
      // Ensure all 4 slots will exist, even if some are empty.
      count = 4;
    }
  }

  // Allocate space for the face records, and initialize them.
  // coverity[suspicious_sizeof]
  Pointer p = aList->Alloc(count * sizeof(Pointer));
  auto facePtrs = static_cast<Pointer*>(p.ToPtr(aList));
  for (size_t i = 0; i < count; i++) {
    if (isSimple && !slots[i]) {
      facePtrs[i] = Pointer::Null();
    } else {
      Pointer fp = aList->Alloc(sizeof(Face));
      auto face = static_cast<Face*>(fp.ToPtr(aList));
      (void)new (face) Face(aList, isSimple ? *slots[i] : aFaces[i]);
      facePtrs[i] = fp;
    }
  }

  mIsSimple = isSimple;
  mFaces = p;
  mFaceCount.store(count);

  if (LOG_FONTLIST_ENABLED()) {
    const nsCString& fam = DisplayName().AsString(aList);
    for (unsigned j = 0; j < aFaces.Length(); j++) {
      nsAutoCString weight, style, stretch;
      aFaces[j].mWeight.ToString(weight);
      aFaces[j].mStyle.ToString(style);
      aFaces[j].mStretch.ToString(stretch);
      LOG_FONTLIST(
          ("(shared-fontlist) family (%s) added face (%s) index %u, weight "
           "%s, style %s, stretch %s",
           fam.get(), aFaces[j].mDescriptor.get(), aFaces[j].mIndex,
           weight.get(), style.get(), stretch.get()));
    }
  }
}

void Family::FindAllFacesForStyle(FontList* aList, const gfxFontStyle& aStyle,
                                  nsTArray<Face*>& aFaceList,
                                  bool aIgnoreSizeTolerance) const {
  MOZ_ASSERT(aFaceList.IsEmpty());
  if (!IsInitialized()) {
    return;
  }

  Pointer* facePtrs = Faces(aList);
  if (!facePtrs) {
    return;
  }

  // If the family has only one face, we simply return it; no further
  // checking needed.
  if (NumFaces() == 1) {
    MOZ_ASSERT(!facePtrs[0].IsNull());
    aFaceList.AppendElement(static_cast<Face*>(facePtrs[0].ToPtr(aList)));
    return;
  }

  // Most families are "simple", having just Regular/Bold/Italic/BoldItalic,
  // or some subset of these. In this case, we have exactly 4 entries in
  // mAvailableFonts, stored in the above order; note that some of the entries
  // may be nullptr. We can then pick the required entry based on whether the
  // request is for bold or non-bold, italic or non-italic, without running the
  // more complex matching algorithm used for larger families with many weights
  // and/or widths.

  if (mIsSimple) {
    // Family has no more than the "standard" 4 faces, at fixed indexes;
    // calculate which one we want.
    // Note that we cannot simply return it as not all 4 faces are necessarily
    // present.
    bool wantBold = aStyle.weight >= FontWeight(600);
    bool wantItalic = !aStyle.style.IsNormal();
    uint8_t faceIndex =
        (wantItalic ? kItalicMask : 0) | (wantBold ? kBoldMask : 0);

    // if the desired style is available, return it directly
    Face* face = static_cast<Face*>(facePtrs[faceIndex].ToPtr(aList));
    if (face && face->HasValidDescriptor()) {
      aFaceList.AppendElement(face);
      return;
    }

    // order to check fallback faces in a simple family, depending on requested
    // style
    static const uint8_t simpleFallbacks[4][3] = {
        {kBoldFaceIndex, kItalicFaceIndex,
         kBoldItalicFaceIndex},  // fallback sequence for Regular
        {kRegularFaceIndex, kBoldItalicFaceIndex, kItalicFaceIndex},  // Bold
        {kBoldItalicFaceIndex, kRegularFaceIndex, kBoldFaceIndex},    // Italic
        {kItalicFaceIndex, kBoldFaceIndex, kRegularFaceIndex}  // BoldItalic
    };
    const uint8_t* order = simpleFallbacks[faceIndex];

    for (uint8_t trial = 0; trial < 3; ++trial) {
      // check remaining faces in order of preference to find the first that
      // actually exists
      face = static_cast<Face*>(facePtrs[order[trial]].ToPtr(aList));
      if (face && face->HasValidDescriptor()) {
        aFaceList.AppendElement(face);
        return;
      }
    }

    // this can't happen unless we have totally broken the font-list manager!
    MOZ_ASSERT_UNREACHABLE("no face found in simple font family!");
  }

  // Pick the font(s) that are closest to the desired weight, style, and
  // stretch. Iterate over all fonts, measuring the weight/style distance.
  // Because of unicode-range values, there may be more than one font for a
  // given but the 99% use case is only a single font entry per
  // weight/style/stretch distance value. To optimize this, only add entries
  // to the matched font array when another entry already has the same
  // weight/style/stretch distance and add the last matched font entry. For
  // normal platform fonts with a single font entry for each
  // weight/style/stretch combination, only the last matched font entry will
  // be added.

  double minDistance = INFINITY;
  Face* matched = nullptr;
  for (uint32_t i = 0; i < NumFaces(); i++) {
    Face* face = static_cast<Face*>(facePtrs[i].ToPtr(aList));
    // weight/style/stretch priority: stretch >> style >> weight
    double distance = WSSDistance(face, aStyle);
    if (distance < minDistance) {
      matched = face;
      if (!aFaceList.IsEmpty()) {
        aFaceList.Clear();
      }
      minDistance = distance;
    } else if (distance == minDistance) {
      if (matched) {
        aFaceList.AppendElement(matched);
      }
      matched = face;
    }
  }

  MOZ_ASSERT(matched, "didn't match a font within a family");
  if (matched) {
    aFaceList.AppendElement(matched);
  }
}

Face* Family::FindFaceForStyle(FontList* aList, const gfxFontStyle& aStyle,
                               bool aIgnoreSizeTolerance) const {
  AutoTArray<Face*, 4> faces;
  FindAllFacesForStyle(aList, aStyle, faces, aIgnoreSizeTolerance);
  return faces.IsEmpty() ? nullptr : faces[0];
}

void Family::SearchAllFontsForChar(FontList* aList,
                                   GlobalFontMatch* aMatchData) {
  const SharedBitSet* charmap =
      static_cast<const SharedBitSet*>(mCharacterMap.ToPtr(aList));
  if (charmap && !charmap->test(aMatchData->mCh)) {
    return;
  }
  if (!IsInitialized()) {
    if (!gfxPlatformFontList::PlatformFontList()->InitializeFamily(this)) {
      return;
    }
  }
  uint32_t numFaces = NumFaces();
  uint32_t charMapsLoaded = 0;  // number of faces whose charmap is loaded
  Pointer* facePtrs = Faces(aList);
  if (!facePtrs) {
    return;
  }
  for (uint32_t i = 0; i < numFaces; i++) {
    Face* face = static_cast<Face*>(facePtrs[i].ToPtr(aList));
    if (!face) {
      continue;
    }
    MOZ_ASSERT(face->HasValidDescriptor());
    // Get the face's character map, if available (may be null!)
    charmap =
        static_cast<const SharedBitSet*>(face->mCharacterMap.ToPtr(aList));
    if (charmap) {
      ++charMapsLoaded;
    }
    // Check style distance if the char is supported, or if charmap not known
    // (so that we don't trigger cmap-loading for faces that would be a worse
    // match than what we've already found).
    if (!charmap || charmap->test(aMatchData->mCh)) {
      double distance = WSSDistance(face, aMatchData->mStyle);
      if (distance < aMatchData->mMatchDistance) {
        // It's a better style match: get a fontEntry, and if we haven't
        // already checked character coverage, do it now (note that
        // HasCharacter() will trigger loading the fontEntry's cmap, if
        // needed).
        RefPtr<gfxFontEntry> fe =
            gfxPlatformFontList::PlatformFontList()->GetOrCreateFontEntry(face,
                                                                          this);
        if (!fe) {
          continue;
        }
        if (!charmap && !fe->HasCharacter(aMatchData->mCh)) {
          continue;
        }
        aMatchData->mBestMatch = fe;
        aMatchData->mMatchDistance = distance;
        aMatchData->mMatchedSharedFamily = this;
      }
    }
  }
  if (mCharacterMap.IsNull() && charMapsLoaded == numFaces) {
    SetupFamilyCharMap(aList);
  }
}

void Family::SetFacePtrs(FontList* aList, nsTArray<Pointer>& aFaces) {
  if (aFaces.Length() >= 2 && aFaces.Length() <= 4) {
    // Check whether the faces meet the criteria for a "simple" family: no more
    // than one each of Regular, Bold, Italic, BoldItalic styles. If so, store
    // them at the appropriate slots in mFaces and set the mIsSimple flag to
    // accelerate font-matching.
    bool isSimple = true;
    Pointer slots[4] = {Pointer::Null(), Pointer::Null(), Pointer::Null(),
                        Pointer::Null()};
    for (const Pointer& fp : aFaces) {
      const Face* f = static_cast<const Face*>(fp.ToPtr(aList));
      if (!f->mWeight.IsSingle() || !f->mStyle.IsSingle() ||
          !f->mStretch.IsSingle()) {
        isSimple = false;
        break;
      }
      if (!f->mStretch.Min().IsNormal()) {
        isSimple = false;
        break;
      }
      size_t slot = 0;
      if (f->mWeight.Min().IsBold()) {
        slot |= kBoldMask;
      }
      if (f->mStyle.Min().IsItalic() || f->mStyle.Min().IsOblique()) {
        slot |= kItalicMask;
      }
      if (!slots[slot].IsNull()) {
        isSimple = false;
        break;
      }
      slots[slot] = fp;
    }
    if (isSimple) {
      size_t size = 4 * sizeof(Pointer);
      mFaces = aList->Alloc(size);
      memcpy(mFaces.ToPtr(aList), slots, size);
      mFaceCount.store(4);
      mIsSimple = true;
      return;
    }
  }
  size_t size = aFaces.Length() * sizeof(Pointer);
  mFaces = aList->Alloc(size);
  memcpy(mFaces.ToPtr(aList), aFaces.Elements(), size);
  mFaceCount.store(aFaces.Length());
}

void Family::SetupFamilyCharMap(FontList* aList) {
  // Set the character map of the family to the union of all the face cmaps,
  // to allow font fallback searches to more rapidly reject the family.
  if (!XRE_IsParentProcess()) {
    // |this| could be a Family record in either the Families() or Aliases()
    // arrays
    dom::ContentChild::GetSingleton()->SendSetupFamilyCharMap(
        aList->GetGeneration(), aList->ToSharedPointer(this));
    return;
  }
  gfxSparseBitSet familyMap;
  Pointer firstMapShmPointer;
  SharedBitSet* firstMap = nullptr;
  bool merged = false;
  Pointer* faces = Faces(aList);
  if (!faces) {
    return;
  }
  for (size_t i = 0; i < NumFaces(); i++) {
    auto f = static_cast<Face*>(faces[i].ToPtr(aList));
    if (!f) {
      continue;
    }
    auto faceMap = static_cast<SharedBitSet*>(f->mCharacterMap.ToPtr(aList));
    MOZ_ASSERT(faceMap);
    if (!firstMap) {
      firstMap = faceMap;
      firstMapShmPointer = f->mCharacterMap;
    } else if (faceMap != firstMap) {
      if (!merged) {
        familyMap.Union(*firstMap);
        merged = true;
      }
      familyMap.Union(*faceMap);
    }
  }
  if (merged) {
    mCharacterMap =
        gfxPlatformFontList::PlatformFontList()->GetShmemCharMap(&familyMap);
  } else {
    mCharacterMap = firstMapShmPointer;
  }
}

FontList::FontList(uint32_t aGeneration) {
  if (XRE_IsParentProcess()) {
    // Create the initial shared block, and initialize Header
    if (AppendShmBlock()) {
      Header& header = GetHeader();
      header.mAllocated.store(sizeof(Header));
      header.mGeneration = aGeneration;
      header.mFamilyCount = 0;
      header.mBlockCount.store(1);
      header.mAliasCount.store(0);
      header.mLocalFaceCount.store(0);
      header.mFamilies = Pointer::Null();
      header.mAliases = Pointer::Null();
      header.mLocalFaces = Pointer::Null();
    } else {
      MOZ_CRASH("parent: failed to initialize FontList");
    }
  } else {
    for (unsigned retryCount = 0; retryCount < 3; ++retryCount) {
      if (UpdateShmBlocks()) {
        return;
      }
      // The only reason for UpdateShmBlocks to fail is if the parent recreated
      // the list after we read its first block, but before we finished getting
      // them all, and so the generation check failed on a subsequent request.
      // Discarding whatever we've got and retrying should get us a new,
      // consistent set of memory blocks in this case. If this doesn't work
      // after a couple of retries, bail out.
      DetachShmBlocks();
    }
    NS_WARNING("child: failed to initialize shared FontList");
  }
}

FontList::~FontList() { DetachShmBlocks(); }

bool FontList::AppendShmBlock() {
  MOZ_ASSERT(XRE_IsParentProcess());
  ipc::SharedMemoryBasic* newShm = new ipc::SharedMemoryBasic();
  if (!newShm->Create(SHM_BLOCK_SIZE)) {
    MOZ_CRASH("failed to create shared memory");
    return false;
  }
  if (!newShm->Map(SHM_BLOCK_SIZE)) {
    MOZ_CRASH("failed to map shared memory");
  }

  char* addr = static_cast<char*>(newShm->memory());
  if (!addr) {
    MOZ_CRASH("null shared memory?");
    return false;
  }

  ShmBlock* block = new ShmBlock(newShm, addr);
  // Allocate space for the Allocated() header field present in all blocks
  block->Allocated().store(4);

  mBlocks.AppendElement(block);
  GetHeader().mBlockCount.store(mBlocks.Length());

  return true;
}

void FontList::DetachShmBlocks() {
  for (auto& i : mBlocks) {
    i->mShmem = nullptr;
  }
  mBlocks.SetLength(0);
}

FontList::ShmBlock* FontList::GetBlockFromParent(uint32_t aIndex) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  // If we have no existing blocks, we don't want a generation check yet;
  // the header in the first block will define the generation of this list
  uint32_t generation = aIndex == 0 ? 0 : GetGeneration();
  ipc::SharedMemoryBasic::Handle handle = ipc::SharedMemoryBasic::NULLHandle();
  if (!dom::ContentChild::GetSingleton()->SendGetFontListShmBlock(
          generation, aIndex, &handle)) {
    return nullptr;
  }
  RefPtr<ipc::SharedMemoryBasic> newShm = new ipc::SharedMemoryBasic();
  if (!newShm->IsHandleValid(handle)) {
    return nullptr;
  }
  if (!newShm->SetHandle(handle,
                         mozilla::ipc::SharedMemoryBasic::RightsReadOnly)) {
    MOZ_CRASH("failed to set shm handle");
  }
  if (!newShm->Map(SHM_BLOCK_SIZE)) {
    MOZ_CRASH("failed to map shared memory");
  }
  char* addr = static_cast<char*>(newShm->memory());
  if (!addr) {
    MOZ_CRASH("null shared memory?");
  }
  return new ShmBlock(newShm, addr);
}

bool FontList::UpdateShmBlocks() {
  MOZ_ASSERT(!XRE_IsParentProcess());
  while (!mBlocks.Length() || mBlocks.Length() < GetHeader().mBlockCount) {
    ShmBlock* newBlock = GetBlockFromParent(mBlocks.Length());
    if (!newBlock) {
      return false;
    }
    mBlocks.AppendElement(newBlock);
  }
  return true;
}

// The block size MUST be sufficient to allocate the largest possible
// SharedBitSet in a single contiguous block, following its own
// Allocated() field.
static_assert(FontList::SHM_BLOCK_SIZE >= 4 + SharedBitSet::kMaxSize,
              "may not be able to allocate a SharedBitSet");

Pointer FontList::Alloc(uint32_t aSize) {
  // Only the parent process does allocation.
  MOZ_ASSERT(XRE_IsParentProcess());

  // 4-byte alignment is good enough for anything we allocate in the font list,
  // as our "Pointer" (block index/offset) is a 32-bit value even on x64.
  auto align = [](uint32_t aSize) -> size_t { return (aSize + 3u) & ~3u; };

  // There's a limit to the size of object we can allocate: the block size,
  // minus the 4-byte mAllocated header field at the start of the block.
  MOZ_DIAGNOSTIC_ASSERT(aSize <= SHM_BLOCK_SIZE - 4);

  aSize = align(aSize);

  int32_t blockIndex;
  uint32_t curAlloc;
  while (true) {
    // Try to allocate in the most recently added block first, as this is
    // highly likely to succeed; if not, try earlier blocks (to fill gaps).
    const int32_t blockCount = mBlocks.Length();
    for (blockIndex = blockCount - 1; blockIndex >= 0; --blockIndex) {
      curAlloc = mBlocks[blockIndex]->Allocated();
      if (SHM_BLOCK_SIZE - curAlloc >= aSize) {
        break;
      }
    }

    if (blockIndex < 0) {
      // Couldn't find enough space: create a new block, and retry.
      if (!AppendShmBlock()) {
        return Pointer::Null();
      }
      continue;  // retry; this will check the newly-added block first,
                 // which must succeed because it's empty
    }

    // We've found a block; allocate space from it, and return
    mBlocks[blockIndex]->Allocated() = curAlloc + aSize;
    break;
  }

  return Pointer(blockIndex, curAlloc);
}

void FontList::SetFamilyNames(const nsTArray<Family::InitData>& aFamilies) {
  // Only the parent process should ever assign the list of families.
  MOZ_ASSERT(XRE_IsParentProcess());

  Header& header = GetHeader();
  MOZ_ASSERT(!header.mFamilyCount);

  size_t count = aFamilies.Length();
  header.mFamilies = Alloc(count * sizeof(Family));
  if (header.mFamilies.IsNull()) {
    return;
  }

  Family* families = static_cast<Family*>(header.mFamilies.ToPtr(this));
  for (size_t i = 0; i < count; i++) {
    (void)new (&families[i]) Family(this, aFamilies[i]);
    LOG_FONTLIST(("(shared-fontlist) family %u (%s)", (unsigned)i,
                  aFamilies[i].mName.get()));
  }

  header.mFamilyCount = count;
}

void FontList::SetAliases(
    nsClassHashtable<nsCStringHashKey, AliasData>& aAliasTable) {
  MOZ_ASSERT(XRE_IsParentProcess());

  Header& header = GetHeader();

  // Build an array of Family::InitData records based on the entries in
  // aAliasTable, then sort them and store into the fontlist.
  nsTArray<Family::InitData> aliasArray;
  aliasArray.SetCapacity(aAliasTable.Count());
  for (auto i = aAliasTable.Iter(); !i.Done(); i.Next()) {
    aliasArray.AppendElement(Family::InitData(
        i.Key(), i.Key(), i.Data()->mIndex, i.Data()->mHidden,
        i.Data()->mBundled, i.Data()->mBadUnderline, i.Data()->mForceClassic));
  }
  aliasArray.Sort();

  size_t count = aliasArray.Length();
  if (count < header.mAliasCount) {
    // This shouldn't happen, but handle it safely by just bailing out.
    NS_WARNING("cannot reduce number of aliases");
    return;
  }
  fontlist::Pointer ptr = Alloc(count * sizeof(Family));
  Family* aliases = static_cast<Family*>(ptr.ToPtr(this));
  for (size_t i = 0; i < count; i++) {
    (void)new (&aliases[i]) Family(this, aliasArray[i]);
    LOG_FONTLIST(("(shared-fontlist) alias family %u (%s)", (unsigned)i,
                  aliasArray[i].mName.get()));
    aliases[i].SetFacePtrs(this, aAliasTable.Get(aliasArray[i].mName)->mFaces);
    if (LOG_FONTLIST_ENABLED()) {
      const auto& faces = aAliasTable.Get(aliasArray[i].mName)->mFaces;
      for (unsigned j = 0; j < faces.Length(); j++) {
        auto face = static_cast<const fontlist::Face*>(faces[j].ToPtr(this));
        const nsCString& desc = face->mDescriptor.AsString(this);
        nsAutoCString weight, style, stretch;
        face->mWeight.ToString(weight);
        face->mStyle.ToString(style);
        face->mStretch.ToString(stretch);
        LOG_FONTLIST(
            ("(shared-fontlist) face (%s) index %u, weight %s, style %s, "
             "stretch %s",
             desc.get(), face->mIndex, weight.get(), style.get(),
             stretch.get()));
      }
    }
  }

  // Set the pointer before the count, so that any other process trying to read
  // will not risk out-of-bounds access to the array.
  header.mAliases = ptr;
  header.mAliasCount.store(count);
}

void FontList::SetLocalNames(
    nsDataHashtable<nsCStringHashKey, LocalFaceRec::InitData>&
        aLocalNameTable) {
  MOZ_ASSERT(XRE_IsParentProcess());
  Header& header = GetHeader();
  if (header.mLocalFaceCount > 0) {
    return;  // already been done!
  }
  nsTArray<nsCString> faceArray;
  faceArray.SetCapacity(aLocalNameTable.Count());
  for (auto i = aLocalNameTable.Iter(); !i.Done(); i.Next()) {
    faceArray.AppendElement(i.Key());
  }
  faceArray.Sort();
  size_t count = faceArray.Length();
  Family* families = Families();
  fontlist::Pointer ptr = Alloc(count * sizeof(LocalFaceRec));
  LocalFaceRec* faces = static_cast<LocalFaceRec*>(ptr.ToPtr(this));
  for (size_t i = 0; i < count; i++) {
    (void)new (&faces[i]) LocalFaceRec();
    const auto& rec = aLocalNameTable.Get(faceArray[i]);
    faces[i].mKey.Assign(faceArray[i], this);
    faces[i].mFamilyIndex = FindFamily(rec.mFamilyName) - families;
    faces[i].mFaceIndex = rec.mFaceIndex;
  }
  header.mLocalFaces = ptr;
  header.mLocalFaceCount.store(count);
}

Family* FontList::FindFamily(const nsCString& aName, bool aAllowHidden) {
  struct FamilyNameComparator {
    FamilyNameComparator(FontList* aList, const nsCString& aTarget)
        : mList(aList), mTarget(aTarget) {}

    int operator()(const Family& aVal) const {
      return mTarget.Compare(aVal.Key().BeginReading(mList));
    }

   private:
    FontList* mList;
    const nsCString& mTarget;
  };

  Header& header = GetHeader();

  Family* families = Families();
  size_t match;
  if (BinarySearchIf(families, 0, header.mFamilyCount,
                     FamilyNameComparator(this, aName), &match)) {
    return !aAllowHidden && families[match].IsHidden() ? nullptr
                                                       : &families[match];
  }

  if (header.mAliasCount) {
    families = AliasFamilies();
    size_t match;
    if (BinarySearchIf(families, 0, header.mAliasCount,
                       FamilyNameComparator(this, aName), &match)) {
      return !aAllowHidden && families[match].IsHidden() ? nullptr
                                                         : &families[match];
    }
  }

#ifdef XP_WIN
  // For Windows only, because of how DWrite munges font family names in some
  // cases (see
  // https://msdnshared.blob.core.windows.net/media/MSDNBlogsFS/prod.evol.blogs.msdn.com/CommunityServer.Components.PostAttachments/00/02/24/90/36/WPF%20Font%20Selection%20Model.pdf
  // and discussion on the OpenType list), try stripping any known "regular"
  // style name from the end of the requested family name.
  // After the deferred font loader has finished, this is no longer needed as
  // the "real" family names will have been found in AliasFamilies() above.
  if (aName.Contains(' ')) {
    auto pfl = gfxPlatformFontList::PlatformFontList();
    if (header.mAliasCount) {
      // Aliases have been fully loaded by the parent process, so just discard
      // any stray mAliasTable and mLocalNameTable entries from earlier calls
      // to this code, and return.
      pfl->mAliasTable.Clear();
      pfl->mLocalNameTable.Clear();
      return nullptr;
    }
    const nsLiteralCString kStyleSuffixes[] = {
        nsLiteralCString(" book"),   nsLiteralCString(" medium"),
        nsLiteralCString(" normal"), nsLiteralCString(" regular"),
        nsLiteralCString(" roman"),  nsLiteralCString(" upright")};
    for (const auto& styleName : kStyleSuffixes) {
      if (StringEndsWith(aName, styleName)) {
        // See if we have a known family that matches the "base" family name
        // with trailing style-name element stripped off.
        nsAutoCString strippedName(aName.BeginReading(),
                                   aName.Length() - styleName.Length());
        families = Families();
        if (BinarySearchIf(families, 0, header.mFamilyCount,
                           FamilyNameComparator(this, strippedName), &match)) {
          // If so, this may be a possible family to satisfy the search; check
          // if the extended family name was actually found as an alternate
          // (either it's already in mAliasTable, or it gets added there when
          // we call ReadFaceNamesForFamily on this candidate).
          Family* candidateFamily = &families[match];
          if (pfl->mAliasTable.Lookup(aName)) {
            return candidateFamily;
          }
          // Note that ReadFaceNamesForFamily may store entries in mAliasTable
          // (and mLocalNameTable), but if this is happening in a content
          // process (which is the common case) those entries will not be saved
          // into the shared font list; they're just used here until the "real"
          // alias list is ready, then discarded.
          pfl->ReadFaceNamesForFamily(candidateFamily, false);
          if (pfl->mAliasTable.Lookup(aName)) {
            return candidateFamily;
          }
        }
        break;
      }
    }
  }
#endif

  return nullptr;
}

LocalFaceRec* FontList::FindLocalFace(const nsCString& aName) {
  struct FaceNameComparator {
    FaceNameComparator(FontList* aList, const nsCString& aTarget)
        : mList(aList), mTarget(aTarget) {}

    int operator()(const LocalFaceRec& aVal) const {
      return mTarget.Compare(aVal.mKey.BeginReading(mList));
    }

   private:
    FontList* mList;
    const nsCString& mTarget;
  };

  Header& header = GetHeader();

  LocalFaceRec* faces = LocalFaces();
  size_t match;
  if (BinarySearchIf(faces, 0, header.mLocalFaceCount,
                     FaceNameComparator(this, aName), &match)) {
    return &faces[match];
  }

  return nullptr;
}

void FontList::SearchForLocalFace(const nsACString& aName, Family** aFamily,
                                  Face** aFace) {
  Header& header = GetHeader();
  MOZ_ASSERT(header.mLocalFaceCount == 0,
             "do not use when local face names are already set up!");
  LOG_FONTLIST(
      ("(shared-fontlist) local face search for (%s)", aName.BeginReading()));
  char initial = aName[0];
  Family* families = Families();
  for (uint32_t i = 0; i < header.mFamilyCount; i++) {
    Family* family = &families[i];
    if (family->Key().AsString(this)[0] != initial) {
      continue;
    }
    LOG_FONTLIST(("(shared-fontlist) checking family (%s)",
                  family->Key().AsString(this).BeginReading()));
    if (!family->IsInitialized()) {
      if (!gfxPlatformFontList::PlatformFontList()->InitializeFamily(family)) {
        continue;
      }
    }
    Pointer* faces = family->Faces(this);
    for (uint32_t j = 0; j < family->NumFaces(); j++) {
      Face* face = static_cast<Face*>(faces[j].ToPtr(this));
      if (!face) {
        continue;
      }
      nsAutoCString psname, fullname;
      if (gfxPlatformFontList::PlatformFontList()->ReadFaceNames(
              family, face, psname, fullname)) {
        LOG_FONTLIST(("(shared-fontlist) read psname (%s) fullname (%s)",
                      psname.get(), fullname.get()));
        ToLowerCase(psname);
        ToLowerCase(fullname);
        if (aName == psname || aName == fullname) {
          *aFamily = family;
          *aFace = face;
          return;
        }
      }
    }
  }
}

Pointer FontList::ToSharedPointer(const void* aPtr) {
  const char* p = (const char*)aPtr;
  const uint32_t blockCount = mBlocks.Length();
  for (uint32_t i = 0; i < blockCount; ++i) {
    const char* blockAddr = (const char*)mBlocks[i]->mAddr;
    if (p >= blockAddr && p < blockAddr + SHM_BLOCK_SIZE) {
      return Pointer(i, p - blockAddr);
    }
  }
  MOZ_ASSERT_UNREACHABLE("invalid shared-memory pointer");
  return Pointer::Null();
}

}  // namespace fontlist
}  // namespace mozilla
