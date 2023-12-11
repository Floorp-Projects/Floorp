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
 * The maximum size of each block is therefore 2^20 bytes (1 MB) if sub-parts
 * of the block are to be allocated; however, a larger block (up to 2^32 bytes)
 * can be created and used as a single allocation if necessary.
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

  /**
   * Check if a Pointer has the null value.
   *
   * NOTE!
   * In a child process, it is possible that conversion to a "real" pointer
   * using ToPtr() will fail even when IsNull() is false, so calling code
   * that may run in child processes must be prepared to handle this.
   */
  bool IsNull() const { return mBlockAndOffset == kNullValue; }

  uint32_t Block() const { return mBlockAndOffset >> kBlockShift; }

  uint32_t Offset() const { return mBlockAndOffset & kOffsetMask; }

  /**
   * Convert a fontlist::Pointer to a standard C++ pointer. This requires the
   * FontList, which will know where the shared memory block is mapped in
   * the current process's address space.
   *
   * aSize is the expected size of the pointed-to object, for bounds checking.
   *
   * NOTE!
   * In child processes this may fail and return nullptr, even if IsNull() is
   * false, in cases where the font list is in the process of being rebuilt.
   */
  void* ToPtr(FontList* aFontList, size_t aSize) const;

  template <typename T>
  T* ToPtr(FontList* aFontList) const {
    return static_cast<T*>(ToPtr(aFontList, sizeof(T)));
  }

  template <typename T>
  T* ToArray(FontList* aFontList, size_t aCount) const {
    return static_cast<T*>(ToPtr(aFontList, sizeof(T) * aCount));
  }

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
    // It's tempting to use AssignLiteral here so that we get an nsCString that
    // simply wraps the character data in the shmem block without needing to
    // allocate or copy. But that's unsafe because in the event of font-list
    // reinitalization, that shared memory will be unmapped; then any copy of
    // the nsCString that may still be around will crash if accessed.
    return nsCString(mPointer.ToArray<const char>(aList, mLength), mLength);
  }

  void Assign(const nsACString& aString, FontList* aList);

  const char* BeginReading(FontList* aList) const {
    MOZ_ASSERT(!mPointer.IsNull());
    auto* str = mPointer.ToArray<const char>(aList, mLength);
    return str ? str : "";
  }

  uint32_t Length() const { return mLength; }

  /**
   * Return whether the String has been set to a value.
   *
   * NOTE!
   * In a child process, accessing the value could fail even if IsNull()
   * returned false. In this case, the nsCString constructor used by AsString()
   * will be passed a null pointer, and return an empty string despite the
   * non-zero Length() recorded here.
   */
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
#ifdef MOZ_WIDGET_GTK
    uint16_t mSize;  // pixel size if bitmap; zero indicates scalable
#endif
    bool mFixedPitch;                  // is the face fixed-pitch (monospaced)?
    mozilla::WeightRange mWeight;      // CSS font-weight value
    mozilla::StretchRange mStretch;    // CSS font-stretch value
    mozilla::SlantStyleRange mStyle;   // CSS font-style value
    RefPtr<gfxCharacterMap> mCharMap;  // character map, or null if not loaded
  };

  // Note that mCharacterMap is not set from the InitData by this constructor;
  // the caller must use SetCharacterMap to handle that separately if required.
  Face(FontList* aList, const InitData& aData)
      : mDescriptor(aList, aData.mDescriptor),
        mIndex(aData.mIndex),
#ifdef MOZ_WIDGET_GTK
        mSize(aData.mSize),
#endif
        mFixedPitch(aData.mFixedPitch),
        mWeight(aData.mWeight),
        mStretch(aData.mStretch),
        mStyle(aData.mStyle),
        mCharacterMap(Pointer::Null()) {
  }

  bool HasValidDescriptor() const {
    return !mDescriptor.IsNull() && mIndex != uint16_t(-1);
  }

  void SetCharacterMap(FontList* aList, gfxCharacterMap* aCharMap);

  String mDescriptor;
  uint16_t mIndex;
#ifdef MOZ_WIDGET_GTK
  uint16_t mSize;
#endif
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
  static constexpr uint32_t kNoIndex = uint32_t(-1);

  // Data required to initialize a Family
  struct InitData {
    InitData(const nsACString& aKey,      // lookup key (lowercased)
             const nsACString& aName,     // display name
             uint32_t aIndex = kNoIndex,  // [win] system collection index
             FontVisibility aVisibility = FontVisibility::Unknown,
             bool aBundled = false,       // [win] font was bundled with the app
                                          // rather than system-installed
             bool aBadUnderline = false,  // underline-position in font is bad
             bool aForceClassic = false,  // [win] use "GDI classic" rendering
             bool aAltLocale = false      // font is alternate localized family
             )
        : mKey(aKey),
          mName(aName),
          mIndex(aIndex),
          mVisibility(aVisibility),
          mBundled(aBundled),
          mBadUnderline(aBadUnderline),
          mForceClassic(aForceClassic),
          mAltLocale(aAltLocale) {}
    bool operator<(const InitData& aRHS) const { return mKey < aRHS.mKey; }
    bool operator==(const InitData& aRHS) const {
      return mKey == aRHS.mKey && mName == aRHS.mName &&
             mVisibility == aRHS.mVisibility && mBundled == aRHS.mBundled &&
             mBadUnderline == aRHS.mBadUnderline;
    }
    nsCString mKey;
    nsCString mName;
    uint32_t mIndex;
    FontVisibility mVisibility;
    bool mBundled;
    bool mBadUnderline;
    bool mForceClassic;
    bool mAltLocale;
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

  uint32_t Index() const { return mIndex; }
  bool IsBundled() const { return mIsBundled; }

  uint32_t NumFaces() const {
    MOZ_ASSERT(IsInitialized());
    return mFaceCount;
  }

  Pointer* Faces(FontList* aList) const {
    MOZ_ASSERT(IsInitialized());
    return mFaces.ToArray<Pointer>(aList, mFaceCount);
  }

  FontVisibility Visibility() const { return mVisibility; }
  bool IsHidden() const { return Visibility() == FontVisibility::Hidden; }

  bool IsBadUnderlineFamily() const { return mIsBadUnderlineFamily; }
  bool IsForceClassic() const { return mIsForceClassic; }
  bool IsSimple() const { return mIsSimple; }
  bool IsAltLocaleFamily() const { return mIsAltLocale; }

  // IsInitialized indicates whether the family has been populated with faces,
  // and is therefore ready to use.
  // It is possible that character maps have not yet been loaded.
  bool IsInitialized() const { return !mFaces.IsNull(); }

  // IsFullyInitialized indicates that not only faces but also character maps
  // have been set up, so the family can be searched without the possibility
  // that IPC messaging will be triggered.
  bool IsFullyInitialized() const {
    return IsInitialized() && !mCharacterMap.IsNull();
  }

  void FindAllFacesForStyle(FontList* aList, const gfxFontStyle& aStyle,
                            nsTArray<Face*>& aFaceList,
                            bool aIgnoreSizeTolerance = false) const;

  Face* FindFaceForStyle(FontList* aList, const gfxFontStyle& aStyle,
                         bool aIgnoreSizeTolerance = false) const;

  void SearchAllFontsForChar(FontList* aList, GlobalFontMatch* aMatchData);

  void SetupFamilyCharMap(FontList* aList);

 private:
  // Returns true if there are specifically-sized bitmap faces in the list,
  // so size selection still needs to be done. (Currently only on Linux.)
  bool FindAllFacesForStyleInternal(FontList* aList, const gfxFontStyle& aStyle,
                                    nsTArray<Face*>& aFaceList) const;

  std::atomic<uint32_t> mFaceCount;
  String mKey;
  String mName;
  Pointer mCharacterMap;  // If non-null, union of character coverage of all
                          // faces in the family
  Pointer mFaces;         // Pointer to array of |mFaceCount| face pointers
  uint32_t mIndex;        // [win] Top bit set indicates app-bundled font family
  FontVisibility mVisibility;
  bool mIsSimple;  // family allows simplified style matching: mFaces contains
                   // exactly 4 entries [Regular, Bold, Italic, BoldItalic].
  bool mIsBundled : 1;
  bool mIsBadUnderlineFamily : 1;
  bool mIsForceClassic : 1;
  bool mIsAltLocale : 1;
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
   * Likewise, in some cases we don't know the final face index because the
   * faces may be re-sorted to fit into predefined positions in a "simple"
   * family (if we're reading names before the family has been fully set up).
   * In that case, we'll store uint32_t(-1) as mFaceIndex, and record the
   * string descriptor instead.
   * When actually recorded in the FontList's mLocalFaces array, the family
   * will be stored as a simple index into the mFamilies array, and the face
   * as an index into the family's mFaces.
   */
  struct InitData {
    nsCString mFamilyName;
    nsCString mFaceDescriptor;
    uint32_t mFaceIndex = uint32_t(-1);
    InitData(const nsACString& aFamily, const nsACString& aFace)
        : mFamilyName(aFamily), mFaceDescriptor(aFace) {}
    InitData(const nsACString& aFamily, uint32_t aFaceIndex)
        : mFamilyName(aFamily), mFaceIndex(aFaceIndex) {}
    InitData() = default;
  };
  String mKey;
  uint32_t mFamilyIndex;  // Index into the font list's Families array
  uint32_t mFaceIndex;    // Index into the family's Faces array
};

}  // namespace fontlist
}  // namespace mozilla

#undef ERROR  // This is defined via Windows.h, but conflicts with some bindings
              // code when this gets included in the same compilation unit.

#endif /* SharedFontList_h */
