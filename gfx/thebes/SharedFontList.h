/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SharedFontList_h
#define SharedFontList_h

#include "gfxFontEntry.h"
#include <atomic>

class gfxCharacterMap;
struct gfxFontStyle;
struct GlobalFontMatch;

namespace mozilla {
namespace fontlist {

class FontList;  // See the separate SharedFontList-impl.h header

/**
 * A Pointer in the shared font list contains a packed index/offset pair,
 * with a 12-bit index into the array of shared-memory blocks, and a 20-bit
 * offset into the block.
 * The maximum size of each block is therefore 2^20 bytes (1048576).
 */
struct Pointer {
 private:
  friend class FontList;
  static const uint32_t kIndexBits = 12u;
  static const uint32_t kBlockShift = 20u;
  static_assert(kIndexBits + kBlockShift == 32u, "bad Pointer bit count");

  static const uint32_t kNullValue = 0xffffffffu;
  static const uint32_t kOffsetMask = (1u << kBlockShift) - 1;

 public:
  static Pointer Null() { return Pointer(); }

  Pointer() : mBlockAndOffset(kNullValue) {}

  Pointer(uint32_t aBlock, uint32_t aOffset)
      : mBlockAndOffset((aBlock << kBlockShift) | aOffset) {
    MOZ_ASSERT(aBlock < (1u << kIndexBits) && aOffset < (1u << kBlockShift));
  }

  Pointer(const Pointer& aOther) {
    mBlockAndOffset.store(aOther.mBlockAndOffset);
  }

  Pointer(Pointer&& aOther) { mBlockAndOffset.store(aOther.mBlockAndOffset); }

  bool IsNull() const { return mBlockAndOffset == kNullValue; }

  uint32_t Block() const { return mBlockAndOffset >> kBlockShift; }

  uint32_t Offset() const { return mBlockAndOffset & kOffsetMask; }

  /**
   * Convert a fontlist::Pointer to a standard C++ pointer. This requires the
   * FontList, which will know where the shared memory block is mapped in
   * the current process's address space.
   */
  void* ToPtr(FontList* aFontList) const;

  Pointer& operator=(const Pointer& aOther) {
    mBlockAndOffset.store(aOther.mBlockAndOffset);
    return *this;
  }

  Pointer& operator=(Pointer&& aOther) {
    mBlockAndOffset.store(aOther.mBlockAndOffset);
    return *this;
  }

  // We store the block index and the offset within the block as a single
  // atomic 32-bit value so we can safely modify a Pointer without other
  // processes seeing a broken (partially-updated) value.
  std::atomic<uint32_t> mBlockAndOffset;
};

/**
 * Family and face names are stored as String records, which hold a length
 * (in utf-8 code units) and a Pointer to the string's UTF-8 characters.
 */
struct String {
  String() : mPointer(Pointer::Null()), mLength(0) {}

  String(FontList* aList, const nsACString& aString)
      : mPointer(Pointer::Null()) {
    Assign(aString, aList);
  }

  const nsCString AsString(FontList* aList) const {
    MOZ_ASSERT(!mPointer.IsNull());
    nsCString res;
    res.AssignLiteral(static_cast<const char*>(mPointer.ToPtr(aList)), mLength);
    return res;
  }

  void Assign(const nsACString& aString, FontList* aList);

  const char* BeginReading(FontList* aList) const {
    MOZ_ASSERT(!mPointer.IsNull());
    return static_cast<const char*>(mPointer.ToPtr(aList));
  }

  uint32_t Length() const { return mLength; }

  bool IsNull() const { return mPointer.IsNull(); }

 private:
  Pointer mPointer;
  uint32_t mLength;
};

/**
 * A Face record represents an individual font resource; it has the style
 * properties needed for font matching, as well as a pointer to a character
 * map that records the supported character set. This may be Null if we have
 * not yet loaded the data.
 * The mDescriptor and mIndex fields provide the information needed to
 * instantiate a (platform-specific) font reference that can be used with
 * platform font APIs; their content depends on the requirements of the
 * platform APIs (e.g. font PostScript name, file pathname, serialized
 * fontconfig pattern, etc).
 */
struct Face {
  // Data required to initialize a Face
  struct InitData {
    nsCString mDescriptor;  // descriptor that can be used to instantiate a
                            // platform font reference
    uint16_t mIndex;        // an index used with descriptor (on some platforms)
    bool mFixedPitch;       // is the face fixed-pitch (monospaced)?
    mozilla::WeightRange mWeight;     // CSS font-weight value
    mozilla::StretchRange mStretch;   // CSS font-stretch value
    mozilla::SlantStyleRange mStyle;  // CSS font-style value
  };

  Face(FontList* aList, const InitData& aData)
      : mDescriptor(aList, aData.mDescriptor),
        mIndex(aData.mIndex),
        mFixedPitch(aData.mFixedPitch),
        mWeight(aData.mWeight),
        mStretch(aData.mStretch),
        mStyle(aData.mStyle),
        mCharacterMap(Pointer::Null()) {}

  bool HasValidDescriptor() const {
    return !mDescriptor.IsNull() && mIndex != uint16_t(-1);
  }

  void SetCharacterMap(FontList* aList, gfxCharacterMap* aCharMap);

  String mDescriptor;
  uint16_t mIndex;
  bool mFixedPitch;
  mozilla::WeightRange mWeight;
  mozilla::StretchRange mStretch;
  mozilla::SlantStyleRange mStyle;
  Pointer mCharacterMap;
};

/**
 * A Family record represents an available (installed) font family; it has
 * a name (for display purposes) and a key (lowercased, for case-insensitive
 * lookups), as well as a pointer to an array of Faces. Depending on the
 * platform, the array of faces may be lazily initialized the first time we
 * want to use the family.
 */
struct Family {
  // Data required to initialize a Family
  struct InitData {
    InitData(const nsACString& aKey,   // lookup key (lowercased)
             const nsACString& aName,  // display name
             uint32_t aIndex = 0,  // [win] index in the system font collection
             FontVisibility aVisibility = FontVisibility::Unknown,
             bool aBundled = false,       // [win] font was bundled with the app
                                          // rather than system-installed
             bool aBadUnderline = false,  // underline-position in font is bad
             bool aForceClassic = false   // [win] use "GDI classic" rendering
             )
        : mKey(aKey),
          mName(aName),
          mIndex(aIndex),
          mVisibility(aVisibility),
          mBundled(aBundled),
          mBadUnderline(aBadUnderline),
          mForceClassic(aForceClassic) {}
    bool operator<(const InitData& aRHS) const { return mKey < aRHS.mKey; }
    bool operator==(const InitData& aRHS) const {
      return mKey == aRHS.mKey && mName == aRHS.mName &&
             mVisibility == aRHS.mVisibility && mBundled == aRHS.mBundled &&
             mBadUnderline == aRHS.mBadUnderline;
    }
    const nsCString mKey;
    const nsCString mName;
    uint32_t mIndex;
    FontVisibility mVisibility;
    bool mBundled;
    bool mBadUnderline;
    bool mForceClassic;
  };

  /**
   * Font families are considered "simple" if they contain only 4 faces with
   * style attributes corresponding to Regular, Bold, Italic, and BoldItalic
   * respectively, or a subset of these (e.g. only Regular and Bold). In this
   * case, the faces are stored at predefined positions in the mFaces array,
   * and a simplified (faster) style-matching algorithm can be used.
   */
  enum {
    // Indexes into mFaces for families where mIsSimple is true
    kRegularFaceIndex = 0,
    kBoldFaceIndex = 1,
    kItalicFaceIndex = 2,
    kBoldItalicFaceIndex = 3,
    // mask values for selecting face with bold and/or italic attributes
    kBoldMask = 0x01,
    kItalicMask = 0x02
  };

  Family(FontList* aList, const InitData& aData);

  void AddFaces(FontList* aList, const nsTArray<Face::InitData>& aFaces);

  void SetFacePtrs(FontList* aList, nsTArray<Pointer>& aFaces);

  const String& Key() const { return mKey; }

  const String& DisplayName() const { return mName; }

  uint32_t Index() const { return mIndex & 0x7fffffffu; }
  bool IsBundled() const { return mIndex & 0x80000000u; }

  uint32_t NumFaces() const {
    MOZ_ASSERT(IsInitialized());
    return mFaceCount;
  }

  Pointer* Faces(FontList* aList) const {
    MOZ_ASSERT(IsInitialized());
    return static_cast<Pointer*>(mFaces.ToPtr(aList));
  }

  FontVisibility Visibility() const { return mVisibility; }
  bool IsHidden() const { return Visibility() == FontVisibility::Hidden; }

  bool IsBadUnderlineFamily() const { return mIsBadUnderlineFamily; }
  bool IsForceClassic() const { return mIsForceClassic; }

  bool IsInitialized() const { return !mFaces.IsNull(); }

  void FindAllFacesForStyle(FontList* aList, const gfxFontStyle& aStyle,
                            nsTArray<Face*>& aFaceList,
                            bool aIgnoreSizeTolerance = false) const;

  Face* FindFaceForStyle(FontList* aList, const gfxFontStyle& aStyle,
                         bool aIgnoreSizeTolerance = false) const;

  void SearchAllFontsForChar(FontList* aList, GlobalFontMatch* aMatchData);

  void SetupFamilyCharMap(FontList* aList);

 private:
  std::atomic<uint32_t> mFaceCount;
  String mKey;
  String mName;
  Pointer mCharacterMap;  // If non-null, union of character coverage of all
                          // faces in the family
  Pointer mFaces;         // Pointer to array of |mFaceCount| face pointers
  uint32_t mIndex;        // [win] Top bit set indicates app-bundled font family
  FontVisibility mVisibility;
  bool mIsBadUnderlineFamily;
  bool mIsForceClassic;
  bool mIsSimple;  // family allows simplified style matching: mFaces contains
                   // exactly 4 entries [Regular, Bold, Italic, BoldItalic].
};

/**
 * For platforms where we build an index of local font face names (PS-name
 * and fullname of the font) to support @font-face{src:local(...)}, we map
 * each face name to an index into the family list, and an index into the
 * family's list of faces.
 */
struct LocalFaceRec {
  /**
   * The InitData struct needs to record the family name rather than index,
   * as we may be collecting these records at the same time as building the
   * family list, so we don't yet know the final family index.
   * When actually recorded in the FontList's mLocalFaces array, the family
   * will be stored as a simple index into the mFamilies array.
   */
  struct InitData {
    nsCString mFamilyName;
    uint32_t mFaceIndex;
    InitData(const nsACString& aFamily, uint32_t aFace)
        : mFamilyName(aFamily), mFaceIndex(aFace) {}
    InitData() = default;
  };
  String mKey;
  uint32_t mFamilyIndex;  // Index into the font list's Families array
  uint32_t mFaceIndex;    // Index into the family's Faces array
};

}  // namespace fontlist
}  // namespace mozilla

#include "ipc/IPCMessageUtils.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::fontlist::Pointer> {
  typedef mozilla::fontlist::Pointer paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    uint32_t v = aParam.mBlockAndOffset;
    WriteParam(aMsg, v);
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    uint32_t v;
    if (ReadParam(aMsg, aIter, &v)) {
      aResult->mBlockAndOffset.store(v);
      return true;
    }
    return false;
  }
};

}  // namespace IPC

#undef ERROR  // This is defined via Windows.h, but conflicts with some bindings
              // code when this gets included in the same compilation unit.

#endif /* SharedFontList_h */
