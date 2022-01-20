/* -*- Mode: ObjC; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: BSD
 *
 * Copyright (C) 2006-2009 Mozilla Corporation.  All rights reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   John Daggett <jdaggett@mozilla.com>
 *   Jonathan Kew <jfkthame@gmail.com>
 *
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/Logging.h"

#include <algorithm>

#import <AppKit/AppKit.h>

#include "gfxFontConstants.h"
#include "gfxPlatformMac.h"
#include "gfxMacPlatformFontList.h"
#include "gfxMacFont.h"
#include "gfxUserFontSet.h"
#include "SharedFontList-impl.h"

#include "harfbuzz/hb.h"

#include "AppleUtils.h"
#include "MainThreadUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIDirectoryEnumerator.h"
#include "nsCharTraits.h"
#include "nsCocoaFeatures.h"
#include "nsCocoaUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/Telemetry.h"
#include "mozilla/gfx/2D.h"

#include <unistd.h>
#include <time.h>
#include <dlfcn.h>

#include "StandardFonts-macos.inc"

using namespace mozilla;
using namespace mozilla::gfx;

// indexes into the NSArray objects that the Cocoa font manager returns
// as the available members of a family
#define INDEX_FONT_POSTSCRIPT_NAME 0
#define INDEX_FONT_FACE_NAME 1
#define INDEX_FONT_WEIGHT 2
#define INDEX_FONT_TRAITS 3

static const int kAppleMaxWeight = 14;
static const int kAppleExtraLightWeight = 3;
static const int kAppleUltraLightWeight = 2;

static const int gAppleWeightToCSSWeight[] = {
    0,
    1,  // 1.
    1,  // 2.  W1, ultralight
    2,  // 3.  W2, extralight
    3,  // 4.  W3, light
    4,  // 5.  W4, semilight
    5,  // 6.  W5, medium
    6,  // 7.
    6,  // 8.  W6, semibold
    7,  // 9.  W7, bold
    8,  // 10. W8, extrabold
    8,  // 11.
    9,  // 12. W9, ultrabold
    9,  // 13
    9   // 14
};

// cache Cocoa's "shared font manager" for performance
static NSFontManager* sFontManager;

static void GetStringForNSString(const NSString* aSrc, nsAString& aDest) {
  aDest.SetLength([aSrc length]);
  [aSrc getCharacters:reinterpret_cast<unichar*>(aDest.BeginWriting())
                range:NSMakeRange(0, [aSrc length])];
}

static NSString* GetNSStringForString(const nsAString& aSrc) {
  return [NSString stringWithCharacters:reinterpret_cast<const unichar*>(aSrc.BeginReading())
                                 length:aSrc.Length()];
}

#define LOG_FONTLIST(args) \
  MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), mozilla::LogLevel::Debug, args)
#define LOG_FONTLIST_ENABLED() \
  MOZ_LOG_TEST(gfxPlatform::GetLog(eGfxLog_fontlist), mozilla::LogLevel::Debug)
#define LOG_CMAPDATA_ENABLED() \
  MOZ_LOG_TEST(gfxPlatform::GetLog(eGfxLog_cmapdata), mozilla::LogLevel::Debug)

#pragma mark -

// Complex scripts will not render correctly unless appropriate AAT or OT
// layout tables are present.
// For OpenType, we also check that the GSUB table supports the relevant
// script tag, to avoid using things like Arial Unicode MS for Lao (it has
// the characters, but lacks OpenType support).

// TODO: consider whether we should move this to gfxFontEntry and do similar
// cmap-masking on other platforms to avoid using fonts that won't shape
// properly.

nsresult MacOSFontEntry::ReadCMAP(FontInfoData* aFontInfoData) {
  // attempt this once, if errors occur leave a blank cmap
  if (mCharacterMap || mShmemCharacterMap) {
    return NS_OK;
  }

  RefPtr<gfxCharacterMap> charmap;
  nsresult rv;

  if (aFontInfoData && (charmap = GetCMAPFromFontInfo(aFontInfoData, mUVSOffset))) {
    rv = NS_OK;
  } else {
    uint32_t kCMAP = TRUETYPE_TAG('c', 'm', 'a', 'p');
    charmap = new gfxCharacterMap();
    AutoTable cmapTable(this, kCMAP);

    if (cmapTable) {
      uint32_t cmapLen;
      const uint8_t* cmapData =
          reinterpret_cast<const uint8_t*>(hb_blob_get_data(cmapTable, &cmapLen));
      rv = gfxFontUtils::ReadCMAP(cmapData, cmapLen, *charmap, mUVSOffset);
    } else {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
  }

  if (NS_SUCCEEDED(rv) && !mIsDataUserFont && !HasGraphiteTables()) {
    // For downloadable fonts, trust the author and don't
    // try to munge the cmap based on script shaping support.

    // We also assume a Graphite font knows what it's doing,
    // and provides whatever shaping is needed for the
    // characters it supports, so only check/clear the
    // complex-script ranges for non-Graphite fonts

    // for layout support, check for the presence of mort/morx/kerx and/or
    // opentype layout tables
    bool hasAATLayout = HasFontTable(TRUETYPE_TAG('m', 'o', 'r', 'x')) ||
                        HasFontTable(TRUETYPE_TAG('m', 'o', 'r', 't'));
    bool hasAppleKerning = HasFontTable(TRUETYPE_TAG('k', 'e', 'r', 'x'));
    bool hasGSUB = HasFontTable(TRUETYPE_TAG('G', 'S', 'U', 'B'));
    bool hasGPOS = HasFontTable(TRUETYPE_TAG('G', 'P', 'O', 'S'));
    if ((hasAATLayout && !(hasGSUB || hasGPOS)) || hasAppleKerning) {
      mRequiresAAT = true;  // prefer CoreText if font has no OTL tables,
                            // or if it uses the Apple-specific 'kerx'
                            // variant of kerning table
    }

    for (const ScriptRange* sr = gfxPlatformFontList::sComplexScriptRanges; sr->rangeStart; sr++) {
      // check to see if the cmap includes complex script codepoints
      if (charmap->TestRange(sr->rangeStart, sr->rangeEnd)) {
        if (hasAATLayout) {
          // prefer CoreText for Apple's complex-script fonts,
          // even if they also have some OpenType tables
          // (e.g. Geeza Pro Bold on 10.6; see bug 614903)
          mRequiresAAT = true;
          // and don't mask off complex-script ranges, we assume
          // the AAT tables will provide the necessary shaping
          continue;
        }

        // We check for GSUB here, as GPOS alone would not be ok.
        if (hasGSUB && SupportsScriptInGSUB(sr->tags, sr->numTags)) {
          continue;
        }

        charmap->ClearRange(sr->rangeStart, sr->rangeEnd);
      }
    }

    // Bug 1360309, 1393624: several of Apple's Chinese fonts have spurious
    // blank glyphs for obscure Tibetan and Arabic-script codepoints.
    // Blocklist these so that font fallback will not use them.
    if (mRequiresAAT &&
        (FamilyName().EqualsLiteral("Songti SC") || FamilyName().EqualsLiteral("Songti TC") ||
         FamilyName().EqualsLiteral("STSong") ||
         // Bug 1390980: on 10.11, the Kaiti fonts are also affected.
         FamilyName().EqualsLiteral("Kaiti SC") || FamilyName().EqualsLiteral("Kaiti TC") ||
         FamilyName().EqualsLiteral("STKaiti"))) {
      charmap->ClearRange(0x0f6b, 0x0f70);
      charmap->ClearRange(0x0f8c, 0x0f8f);
      charmap->clear(0x0f98);
      charmap->clear(0x0fbd);
      charmap->ClearRange(0x0fcd, 0x0fff);
      charmap->clear(0x0620);
      charmap->clear(0x065f);
      charmap->ClearRange(0x06ee, 0x06ef);
      charmap->clear(0x06ff);
    }
  }

  mHasCmapTable = NS_SUCCEEDED(rv);
  if (mHasCmapTable) {
    gfxPlatformFontList* pfl = gfxPlatformFontList::PlatformFontList();
    fontlist::FontList* sharedFontList = pfl->SharedFontList();
    if (!IsUserFont() && mShmemFace) {
      mShmemFace->SetCharacterMap(sharedFontList, charmap);  // async
      if (!TrySetShmemCharacterMap()) {
        // Temporarily retain charmap, until the shared version is
        // ready for use.
        mCharacterMap = charmap;
      }
    } else {
      mCharacterMap = pfl->FindCharMap(charmap);
    }
  } else {
    // if error occurred, initialize to null cmap
    mCharacterMap = new gfxCharacterMap();
  }

  LOG_FONTLIST(("(fontlist-cmap) name: %s, size: %zu hash: %8.8x%s\n", mName.get(),
                charmap->SizeOfIncludingThis(moz_malloc_size_of), charmap->mHash,
                mCharacterMap == charmap ? " new" : ""));
  if (LOG_CMAPDATA_ENABLED()) {
    char prefix[256];
    SprintfLiteral(prefix, "(cmapdata) name: %.220s", mName.get());
    charmap->Dump(prefix, eGfxLog_cmapdata);
  }

  return rv;
}

gfxFont* MacOSFontEntry::CreateFontInstance(const gfxFontStyle* aFontStyle) {
  RefPtr<UnscaledFontMac> unscaledFont(mUnscaledFont);
  if (!unscaledFont) {
    CGFontRef baseFont = GetFontRef();
    if (!baseFont) {
      return nullptr;
    }
    unscaledFont = new UnscaledFontMac(baseFont, mIsDataUserFont);
    mUnscaledFont = unscaledFont;
  }

  return new gfxMacFont(unscaledFont, this, aFontStyle);
}

bool MacOSFontEntry::HasVariations() {
  if (!mHasVariationsInitialized) {
    mHasVariationsInitialized = true;
    mHasVariations = gfxPlatform::GetPlatform()->HasVariationFontSupport() &&
                     HasFontTable(TRUETYPE_TAG('f', 'v', 'a', 'r'));
  }

  return mHasVariations;
}

void MacOSFontEntry::GetVariationAxes(nsTArray<gfxFontVariationAxis>& aVariationAxes) {
  // We could do this by creating a CTFont and calling CTFontCopyVariationAxes,
  // but it is expensive to instantiate a CTFont for every face just to set up
  // the axis information.
  // Instead we use gfxFontUtils to read the font tables directly.
  gfxFontUtils::GetVariationData(this, &aVariationAxes, nullptr);
}

void MacOSFontEntry::GetVariationInstances(nsTArray<gfxFontVariationInstance>& aInstances) {
  // Core Text doesn't offer API for this, so we use gfxFontUtils to read the
  // font tables directly.
  gfxFontUtils::GetVariationData(this, nullptr, &aInstances);
}

bool MacOSFontEntry::IsCFF() {
  if (!mIsCFFInitialized) {
    mIsCFFInitialized = true;
    mIsCFF = HasFontTable(TRUETYPE_TAG('C', 'F', 'F', ' '));
  }

  return mIsCFF;
}

MacOSFontEntry::MacOSFontEntry(const nsACString& aPostscriptName, WeightRange aWeight,
                               bool aIsStandardFace, double aSizeHint)
    : gfxFontEntry(aPostscriptName, aIsStandardFace),
      mFontRef(NULL),
      mSizeHint(aSizeHint),
      mFontRefInitialized(false),
      mRequiresAAT(false),
      mIsCFF(false),
      mIsCFFInitialized(false),
      mHasVariations(false),
      mHasVariationsInitialized(false),
      mHasAATSmallCaps(false),
      mHasAATSmallCapsInitialized(false) {
  mWeightRange = aWeight;
  mOpszAxis.mTag = 0;
}

MacOSFontEntry::MacOSFontEntry(const nsACString& aPostscriptName, CGFontRef aFontRef,
                               WeightRange aWeight, StretchRange aStretch, SlantStyleRange aStyle,
                               bool aIsDataUserFont, bool aIsLocalUserFont)
    : gfxFontEntry(aPostscriptName, false),
      mFontRef(NULL),
      mSizeHint(0.0),
      mFontRefInitialized(false),
      mRequiresAAT(false),
      mIsCFF(false),
      mIsCFFInitialized(false),
      mHasVariations(false),
      mHasVariationsInitialized(false),
      mHasAATSmallCaps(false),
      mHasAATSmallCapsInitialized(false) {
  mFontRef = aFontRef;
  mFontRefInitialized = true;
  ::CFRetain(mFontRef);

  mWeightRange = aWeight;
  mStretchRange = aStretch;
  mFixedPitch = false;  // xxx - do we need this for downloaded fonts?
  mStyleRange = aStyle;
  mOpszAxis.mTag = 0;

  NS_ASSERTION(!(aIsDataUserFont && aIsLocalUserFont),
               "userfont is either a data font or a local font");
  mIsDataUserFont = aIsDataUserFont;
  mIsLocalUserFont = aIsLocalUserFont;
}

gfxFontEntry* MacOSFontEntry::Clone() const {
  MOZ_ASSERT(!IsUserFont(), "we can only clone installed fonts!");
  MacOSFontEntry* fe = new MacOSFontEntry(Name(), Weight(), mStandardFace, mSizeHint);
  fe->mStyleRange = mStyleRange;
  fe->mStretchRange = mStretchRange;
  fe->mFixedPitch = mFixedPitch;
  return fe;
}

CGFontRef MacOSFontEntry::GetFontRef() {
  if (!mFontRefInitialized) {
    // Cache the CGFontRef, to be released by our destructor.
    mFontRef = CreateOrCopyFontRef();
    mFontRefInitialized = true;
  }
  // Return a non-retained reference; caller does not need to release.
  return mFontRef;
}

CGFontRef MacOSFontEntry::CreateOrCopyFontRef() {
  if (mFontRef) {
    // We have a cached CGFont, just add a reference. Caller must
    // release, but we'll still own our reference.
    ::CGFontRetain(mFontRef);
    return mFontRef;
  }
  // Create a new CGFont; caller will own the only reference to it.
  NSString* psname = GetNSStringForString(NS_ConvertUTF8toUTF16(mName));
  CGFontRef ref = CGFontCreateWithFontName(CFStringRef(psname));
  if (!ref) {
    // This happens on macOS 10.12 for font entry names that start with
    // .AppleSystemUIFont. For those fonts, we need to go through NSFont
    // to get the correct CGFontRef.
    // Both the Text and the Display variant of the display font use
    // .AppleSystemUIFontSomethingSomething as their member names.
    // That's why we're carrying along mSizeHint to this place so that
    // we get the variant that we want for this family.
    NSFont* font = [NSFont fontWithName:psname size:mSizeHint];
    if (font) {
      ref = CTFontCopyGraphicsFont((CTFontRef)font, nullptr);
    }
  }
  return ref;  // Not saved in mFontRef; caller will own the reference
}

// For a logging build, we wrap the CFDataRef in a FontTableRec so that we can
// use the MOZ_COUNT_[CD]TOR macros in it. A release build without logging
// does not get this overhead.
class FontTableRec {
 public:
  explicit FontTableRec(CFDataRef aDataRef) : mDataRef(aDataRef) { MOZ_COUNT_CTOR(FontTableRec); }

  ~FontTableRec() {
    MOZ_COUNT_DTOR(FontTableRec);
    ::CFRelease(mDataRef);
  }

 private:
  CFDataRef mDataRef;
};

/*static*/ void MacOSFontEntry::DestroyBlobFunc(void* aUserData) {
#ifdef NS_BUILD_REFCNT_LOGGING
  FontTableRec* ftr = static_cast<FontTableRec*>(aUserData);
  delete ftr;
#else
  ::CFRelease((CFDataRef)aUserData);
#endif
}

hb_blob_t* MacOSFontEntry::GetFontTable(uint32_t aTag) {
  AutoCFRelease<CGFontRef> fontRef = CreateOrCopyFontRef();
  if (!fontRef) {
    return nullptr;
  }

  CFDataRef dataRef = ::CGFontCopyTableForTag(fontRef, aTag);
  if (dataRef) {
    return hb_blob_create((const char*)::CFDataGetBytePtr(dataRef), ::CFDataGetLength(dataRef),
                          HB_MEMORY_MODE_READONLY,
#ifdef NS_BUILD_REFCNT_LOGGING
                          new FontTableRec(dataRef),
#else
                          (void*)dataRef,
#endif
                          DestroyBlobFunc);
  }

  return nullptr;
}

bool MacOSFontEntry::HasFontTable(uint32_t aTableTag) {
  if (mAvailableTables.Count() == 0) {
    nsAutoreleasePool localPool;

    AutoCFRelease<CGFontRef> fontRef = CreateOrCopyFontRef();
    if (!fontRef) {
      return false;
    }
    AutoCFRelease<CFArrayRef> tags = ::CGFontCopyTableTags(fontRef);
    if (!tags) {
      return false;
    }
    int numTags = (int)::CFArrayGetCount(tags);
    for (int t = 0; t < numTags; t++) {
      uint32_t tag = (uint32_t)(uintptr_t)::CFArrayGetValueAtIndex(tags, t);
      mAvailableTables.PutEntry(tag);
    }
  }

  return mAvailableTables.GetEntry(aTableTag);
}

static bool CheckForAATSmallCaps(CFArrayRef aFeatures) {
  // Walk the array of feature descriptors from the font, and see whether
  // a small-caps feature setting is available.
  // Just bail out (returning false) if at any point we fail to find the
  // expected dictionary keys, etc; if the font has bad data, we don't even
  // try to search the rest of it.
  auto numFeatures = CFArrayGetCount(aFeatures);
  for (auto f = 0; f < numFeatures; ++f) {
    auto featureDict = (CFDictionaryRef)CFArrayGetValueAtIndex(aFeatures, f);
    if (!featureDict) {
      return false;
    }
    auto featureNum =
        (CFNumberRef)CFDictionaryGetValue(featureDict, CFSTR("CTFeatureTypeIdentifier"));
    if (!featureNum) {
      return false;
    }
    int16_t featureType;
    if (!CFNumberGetValue(featureNum, kCFNumberSInt16Type, &featureType)) {
      return false;
    }
    if (featureType == kLetterCaseType || featureType == kLowerCaseType) {
      // Which selector to look for, depending whether we've found the
      // legacy LetterCase feature or the new LowerCase one.
      const uint16_t smallCaps =
          (featureType == kLetterCaseType) ? kSmallCapsSelector : kLowerCaseSmallCapsSelector;
      auto selectors =
          (CFArrayRef)CFDictionaryGetValue(featureDict, CFSTR("CTFeatureTypeSelectors"));
      if (!selectors) {
        return false;
      }
      auto numSelectors = CFArrayGetCount(selectors);
      for (auto s = 0; s < numSelectors; s++) {
        auto selectorDict = (CFDictionaryRef)CFArrayGetValueAtIndex(selectors, s);
        if (!selectorDict) {
          return false;
        }
        auto selectorNum =
            (CFNumberRef)CFDictionaryGetValue(selectorDict, CFSTR("CTFeatureSelectorIdentifier"));
        if (!selectorNum) {
          return false;
        }
        int16_t selectorValue;
        if (!CFNumberGetValue(selectorNum, kCFNumberSInt16Type, &selectorValue)) {
          return false;
        }
        if (selectorValue == smallCaps) {
          return true;
        }
      }
    }
  }
  return false;
}

bool MacOSFontEntry::SupportsOpenTypeFeature(Script aScript, uint32_t aFeatureTag) {
  // If we're going to shape with Core Text, we don't support added
  // OpenType features (aside from any CT applies by default), except
  // for 'smcp' which we map to an AAT feature selector.
  if (RequiresAATLayout()) {
    if (aFeatureTag != HB_TAG('s', 'm', 'c', 'p')) {
      return false;
    }
    if (mHasAATSmallCapsInitialized) {
      return mHasAATSmallCaps;
    }
    mHasAATSmallCapsInitialized = true;
    CGFontRef cgFont = GetFontRef();
    if (!cgFont) {
      return mHasAATSmallCaps;
    }
    AutoCFRelease<CTFontRef> ctFont = CTFontCreateWithGraphicsFont(cgFont, 0.0, nullptr, nullptr);
    if (ctFont) {
      AutoCFRelease<CFArrayRef> features = CTFontCopyFeatures(ctFont);
      if (features) {
        mHasAATSmallCaps = CheckForAATSmallCaps(features);
      }
    }
    return mHasAATSmallCaps;
  }
  return gfxFontEntry::SupportsOpenTypeFeature(aScript, aFeatureTag);
}

void MacOSFontEntry::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                            FontListSizes* aSizes) const {
  aSizes->mFontListSize += aMallocSizeOf(this);
  AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

/* gfxMacFontFamily */
#pragma mark -

class gfxMacFontFamily final : public gfxFontFamily {
 public:
  gfxMacFontFamily(const nsACString& aName, FontVisibility aVisibility, double aSizeHint)
      : gfxFontFamily(aName, aVisibility), mSizeHint(aSizeHint) {}

  virtual ~gfxMacFontFamily() = default;

  virtual void LocalizedName(nsACString& aLocalizedName);

  virtual void FindStyleVariations(FontInfoData* aFontInfoData = nullptr);

 protected:
  double mSizeHint;
};

void gfxMacFontFamily::LocalizedName(nsACString& aLocalizedName) {
  nsAutoreleasePool localPool;

  // It's unsafe to call HasOtherFamilyNames off the main thread because
  // it entrains FindStyleVariations, which calls GetWeightOverride, which
  // retrieves prefs.  And the pref names can change (via user overrides),
  // so we can't use StaticPrefs to access them.
  if (NS_IsMainThread() && !HasOtherFamilyNames()) {
    aLocalizedName = mName;
    return;
  }

  NSString* family = GetNSStringForString(NS_ConvertUTF8toUTF16(mName));
  NSString* localized = [sFontManager localizedNameForFamily:family face:nil];

  if (localized) {
    nsAutoString locName;
    GetStringForNSString(localized, locName);
    CopyUTF16toUTF8(locName, aLocalizedName);
    return;
  }

  // failed to get localized name, just use the canonical one
  aLocalizedName = mName;
}

// Return the CSS weight value to use for the given face, overriding what
// AppKit gives us (used to adjust families with bad weight values, see
// bug 931426).
// A return value of 0 indicates no override - use the existing weight.
static inline int GetWeightOverride(const nsAString& aPSName) {
  nsAutoCString prefName("font.weight-override.");
  // The PostScript name is required to be ASCII; if it's not, the font is
  // broken anyway, so we really don't care that this is lossy.
  LossyAppendUTF16toASCII(aPSName, prefName);
  return Preferences::GetInt(prefName.get(), 0);
}

void gfxMacFontFamily::FindStyleVariations(FontInfoData* aFontInfoData) {
  if (mHasStyles) {
    return;
  }

  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING("gfxMacFontFamily::FindStyleVariations", LAYOUT, mName);

  nsAutoreleasePool localPool;

  NSString* family = GetNSStringForString(NS_ConvertUTF8toUTF16(mName));

  // create a font entry for each face
  NSArray* fontfaces = [sFontManager
      availableMembersOfFontFamily:family];  // returns an array of [psname, style name, weight,
                                             // traits] elements, goofy api
  int faceCount = [fontfaces count];
  int faceIndex;

  for (faceIndex = 0; faceIndex < faceCount; faceIndex++) {
    NSArray* face = [fontfaces objectAtIndex:faceIndex];
    NSString* psname = [face objectAtIndex:INDEX_FONT_POSTSCRIPT_NAME];
    int32_t appKitWeight = [[face objectAtIndex:INDEX_FONT_WEIGHT] unsignedIntValue];
    uint32_t macTraits = [[face objectAtIndex:INDEX_FONT_TRAITS] unsignedIntValue];
    NSString* facename = [face objectAtIndex:INDEX_FONT_FACE_NAME];
    bool isStandardFace = false;

    if (appKitWeight == kAppleExtraLightWeight) {
      // if the facename contains UltraLight, set the weight to the ultralight weight value
      NSRange range = [facename rangeOfString:@"ultralight" options:NSCaseInsensitiveSearch];
      if (range.location != NSNotFound) {
        appKitWeight = kAppleUltraLightWeight;
      }
    }

    // make a nsString
    nsAutoString postscriptFontName;
    GetStringForNSString(psname, postscriptFontName);

    int32_t cssWeight = gfxMacPlatformFontList::AppleWeightToCSSWeight(appKitWeight);
    // If we are on the startup-time InitFontList thread, we skip this as it
    // wants to retrieve faces for the default font family, but cannot safely
    // call the Preferences APIs. Fortunately, the default font doesn't actually
    // depend on a weight override pref.
    if (!gfxPlatformFontList::IsInitFontListThread()) {
      int32_t weightOverride = GetWeightOverride(postscriptFontName);
      if (weightOverride) {
        // scale down and clamp, to get a value from 1..9
        cssWeight = ((weightOverride + 50) / 100);
        cssWeight = std::max(1, std::min(cssWeight, 9));
      }
    }
    cssWeight *= 100;  // scale up to CSS values

    if ([facename isEqualToString:@"Regular"] || [facename isEqualToString:@"Bold"] ||
        [facename isEqualToString:@"Italic"] || [facename isEqualToString:@"Oblique"] ||
        [facename isEqualToString:@"Bold Italic"] || [facename isEqualToString:@"Bold Oblique"]) {
      isStandardFace = true;
    }

    // create a font entry
    MacOSFontEntry* fontEntry =
        new MacOSFontEntry(NS_ConvertUTF16toUTF8(postscriptFontName),
                           WeightRange(FontWeight(cssWeight)), isStandardFace, mSizeHint);
    if (!fontEntry) {
      break;
    }

    // set additional properties based on the traits reported by Cocoa
    if (macTraits & (NSCondensedFontMask | NSNarrowFontMask | NSCompressedFontMask)) {
      fontEntry->mStretchRange = StretchRange(FontStretch::Condensed());
    } else if (macTraits & NSExpandedFontMask) {
      fontEntry->mStretchRange = StretchRange(FontStretch::Expanded());
    }
    // Cocoa fails to set the Italic traits bit for HelveticaLightItalic,
    // at least (see bug 611855), so check for style name endings as well
    if ((macTraits & NSItalicFontMask) || [facename hasSuffix:@"Italic"] ||
        [facename hasSuffix:@"Oblique"]) {
      fontEntry->mStyleRange = SlantStyleRange(FontSlantStyle::Italic());
    }
    if (macTraits & NSFixedPitchFontMask) {
      fontEntry->mFixedPitch = true;
    }

    if (gfxPlatform::GetPlatform()->HasVariationFontSupport()) {
      fontEntry->SetupVariationRanges();
    }

    if (LOG_FONTLIST_ENABLED()) {
      nsAutoCString weightString;
      fontEntry->Weight().ToString(weightString);
      nsAutoCString stretchString;
      fontEntry->Stretch().ToString(stretchString);
      LOG_FONTLIST(("(fontlist) added (%s) to family (%s)"
                    " with style: %s weight: %s stretch: %s"
                    " (apple-weight: %d macTraits: %8.8x)",
                    fontEntry->Name().get(), Name().get(),
                    fontEntry->IsItalic() ? "italic" : "normal", weightString.get(),
                    stretchString.get(), appKitWeight, macTraits));
    }

    // insert into font entry array of family
    AddFontEntry(fontEntry);
  }

  SortAvailableFonts();
  SetHasStyles(true);

  if (mIsBadUnderlineFamily) {
    SetBadUnderlineFonts();
  }

  CheckForSimpleFamily();
}

/* gfxSingleFaceMacFontFamily */
#pragma mark -

class gfxSingleFaceMacFontFamily final : public gfxFontFamily {
 public:
  gfxSingleFaceMacFontFamily(const nsACString& aName, FontVisibility aVisibility)
      : gfxFontFamily(aName, aVisibility) {
    mFaceNamesInitialized = true;  // omit from face name lists
  }

  virtual ~gfxSingleFaceMacFontFamily() = default;

  virtual void LocalizedName(nsACString& aLocalizedName);

  virtual void ReadOtherFamilyNames(gfxPlatformFontList* aPlatformFontList);

  virtual bool IsSingleFaceFamily() const { return true; }
};

void gfxSingleFaceMacFontFamily::LocalizedName(nsACString& aLocalizedName) {
  nsAutoreleasePool localPool;

  if (!HasOtherFamilyNames()) {
    aLocalizedName = mName;
    return;
  }

  gfxFontEntry* fe = mAvailableFonts[0];
  NSFont* font = [NSFont fontWithName:GetNSStringForString(NS_ConvertUTF8toUTF16(fe->Name()))
                                 size:0.0];
  if (font) {
    NSString* localized = [font displayName];
    if (localized) {
      nsAutoString locName;
      GetStringForNSString(localized, locName);
      CopyUTF16toUTF8(locName, aLocalizedName);
      return;
    }
  }

  // failed to get localized name, just use the canonical one
  aLocalizedName = mName;
}

void gfxSingleFaceMacFontFamily::ReadOtherFamilyNames(gfxPlatformFontList* aPlatformFontList) {
  if (mOtherFamilyNamesInitialized) {
    return;
  }

  gfxFontEntry* fe = mAvailableFonts[0];
  if (!fe) {
    return;
  }

  const uint32_t kNAME = TRUETYPE_TAG('n', 'a', 'm', 'e');

  gfxFontEntry::AutoTable nameTable(fe, kNAME);
  if (!nameTable) {
    return;
  }

  mHasOtherFamilyNames = ReadOtherFamilyNamesForFace(aPlatformFontList, nameTable, true);

  mOtherFamilyNamesInitialized = true;
}

/* gfxMacPlatformFontList */
#pragma mark -

gfxMacPlatformFontList::gfxMacPlatformFontList()
    : gfxPlatformFontList(false), mDefaultFont(nullptr), mUseSizeSensitiveSystemFont(false) {
  CheckFamilyList(kBaseFonts);

  // cache this in a static variable so that MacOSFontFamily objects
  // don't have to repeatedly look it up
  sFontManager = [NSFontManager sharedFontManager];

  // Load the font-list preferences now, so that we don't have to do it from
  // Init[Shared]FontListForPlatform, which may be called off-main-thread.
  gfxFontUtils::GetPrefsFontList("font.single-face-list", mSingleFaceFonts);
  gfxFontUtils::GetPrefsFontList("font.preload-names-list", mPreloadFonts);
}

gfxMacPlatformFontList::~gfxMacPlatformFontList() {
  if (XRE_IsParentProcess()) {
    ::CFNotificationCenterRemoveObserver(::CFNotificationCenterGetLocalCenter(), this,
                                         kCTFontManagerRegisteredFontsChangedNotification, 0);
  }

  if (mDefaultFont) {
    ::CFRelease(mDefaultFont);
  }
}

void gfxMacPlatformFontList::AddFamily(const nsACString& aFamilyName, FontVisibility aVisibility) {
  double sizeHint = 0.0;
  if (aVisibility == FontVisibility::Hidden && mUseSizeSensitiveSystemFont &&
      mSystemDisplayFontFamilyName.Equals(aFamilyName)) {
    sizeHint = 128.0;
  }

  nsAutoCString key;
  ToLowerCase(aFamilyName, key);

  RefPtr<gfxFontFamily> familyEntry = new gfxMacFontFamily(aFamilyName, aVisibility, sizeHint);
  mFontFamilies.InsertOrUpdate(key, RefPtr{familyEntry});

  // check the bad underline blocklist
  if (mBadUnderlineFamilyNames.ContainsSorted(key)) {
    familyEntry->SetBadUnderlineFamily();
  }
}

FontVisibility gfxMacPlatformFontList::GetVisibilityForFamily(const nsACString& aName) const {
  if (aName[0] == '.' || aName.LowerCaseEqualsLiteral("lastresort")) {
    return FontVisibility::Hidden;
  }
  if (FamilyInList(aName, kBaseFonts)) {
    return FontVisibility::Base;
  }
  return FontVisibility::User;
}

void gfxMacPlatformFontList::AddFamily(CFStringRef aFamily) {
  NSString* family = (NSString*)aFamily;

  // CTFontManager includes weird internal family names and
  // LastResort, skip over those
  if (!family || [family caseInsensitiveCompare:@"LastResort"] == NSOrderedSame ||
      [family caseInsensitiveCompare:@".LastResort"] == NSOrderedSame) {
    return;
  }

  nsAutoString familyName;
  nsCocoaUtils::GetStringForNSString(family, familyName);

  NS_ConvertUTF16toUTF8 nameUtf8(familyName);
  AddFamily(nameUtf8, GetVisibilityForFamily(nameUtf8));
}

void gfxMacPlatformFontList::ReadSystemFontList(dom::SystemFontList* aList) {
  // Note: We rely on the records for mSystemTextFontFamilyName and
  // mSystemDisplayFontFamilyName (if present) being *before* the main
  // font list, so that those names are known in the content process
  // by the time we add the actual family records to the font list.
  aList->entries().AppendElement(FontFamilyListEntry(
      mSystemTextFontFamilyName, FontVisibility::Unknown, kTextSizeSystemFontFamily));
  if (mUseSizeSensitiveSystemFont) {
    aList->entries().AppendElement(FontFamilyListEntry(
        mSystemDisplayFontFamilyName, FontVisibility::Unknown, kDisplaySizeSystemFontFamily));
  }
  // Now collect the list of available families, with visibility attributes.
  for (auto f = mFontFamilies.Iter(); !f.Done(); f.Next()) {
    auto macFamily = f.Data().get();
    if (macFamily->IsSingleFaceFamily()) {
      continue;  // skip, this will be recreated separately in the child
    }
    aList->entries().AppendElement(
        FontFamilyListEntry(macFamily->Name(), macFamily->Visibility(), kStandardFontFamily));
  }
}

void gfxMacPlatformFontList::PreloadNamesList() {
  uint32_t numFonts = mPreloadFonts.Length();
  for (uint32_t i = 0; i < numFonts; i++) {
    nsAutoCString key;
    GenerateFontListKey(mPreloadFonts[i], key);

    // only search canonical names!
    gfxFontFamily* familyEntry = mFontFamilies.GetWeak(key);
    if (familyEntry) {
      familyEntry->ReadOtherFamilyNames(this);
    }
  }
}

nsresult gfxMacPlatformFontList::InitFontListForPlatform() {
  nsAutoreleasePool localPool;

  // The font registration thread was created early in startup, to give the
  // system a head start on activating all the supplemental-language fonts.
  // Here, we need to wait until it has finished its work.
  gfxPlatformMac::WaitForFontRegistration();

  Telemetry::AutoTimer<Telemetry::MAC_INITFONTLIST_TOTAL> timer;

  InitSystemFontNames();

  if (XRE_IsParentProcess()) {
    static bool firstTime = true;
    if (firstTime) {
      ::CFNotificationCenterAddObserver(::CFNotificationCenterGetLocalCenter(), this,
                                        RegisteredFontsChangedNotificationCallback,
                                        kCTFontManagerRegisteredFontsChangedNotification, 0,
                                        CFNotificationSuspensionBehaviorDeliverImmediately);
      firstTime = false;
    }

    // We're not a content process, so get the available fonts directly
    // from Core Text.
    AutoCFRelease<CFArrayRef> familyNames = CTFontManagerCopyAvailableFontFamilyNames();
    for (NSString* familyName in (NSArray*)(CFArrayRef)familyNames) {
      AddFamily((CFStringRef)familyName);
    }
  } else {
    // Content process: use font list passed from the chrome process via
    // the GetXPCOMProcessAttributes message, because it's much faster than
    // querying Core Text again in the child.
    auto& fontList = dom::ContentChild::GetSingleton()->SystemFontList();
    for (FontFamilyListEntry& ffe : fontList.entries()) {
      switch (ffe.entryType()) {
        case kStandardFontFamily:
          // On Catalina or later, we pre-initialize system font-family entries
          // in InitSystemFontNames(), so we can just skip them here.
          if (nsCocoaFeatures::OnCatalinaOrLater() &&
              (ffe.familyName() == mSystemTextFontFamilyName ||
               ffe.familyName() == mSystemDisplayFontFamilyName)) {
            continue;
          }
          AddFamily(ffe.familyName(), ffe.visibility());
          break;
        case kTextSizeSystemFontFamily:
          mSystemTextFontFamilyName = ffe.familyName();
          break;
        case kDisplaySizeSystemFontFamily:
          mSystemDisplayFontFamilyName = ffe.familyName();
          mUseSizeSensitiveSystemFont = true;
          break;
      }
    }
    fontList.entries().Clear();
  }

  InitSingleFaceList();

  // to avoid full search of font name tables, seed the other names table with localized names from
  // some of the prefs fonts which are accessed via their localized names.  changes in the pref
  // fonts will only cause a font lookup miss earlier. this is a simple optimization, it's not
  // required for correctness
  PreloadNamesList();

  // start the delayed cmap loader
  GetPrefsAndStartLoader();

  return NS_OK;
}

void gfxMacPlatformFontList::InitSharedFontListForPlatform() {
  nsAutoreleasePool localPool;

  gfxPlatformMac::WaitForFontRegistration();

  InitSystemFontNames();

  if (XRE_IsParentProcess()) {
    // Only the parent process listens for OS font-changed notifications;
    // after rebuilding its list, it will update the content processes.
    static bool firstTime = true;
    if (firstTime) {
      ::CFNotificationCenterAddObserver(::CFNotificationCenterGetLocalCenter(), this,
                                        RegisteredFontsChangedNotificationCallback,
                                        kCTFontManagerRegisteredFontsChangedNotification, 0,
                                        CFNotificationSuspensionBehaviorDeliverImmediately);
      firstTime = false;
    }

    AutoCFRelease<CFArrayRef> familyNames = CTFontManagerCopyAvailableFontFamilyNames();
    nsTArray<fontlist::Family::InitData> families;
    for (NSString* familyName in (NSArray*)(CFArrayRef)familyNames) {
      nsAutoString name16;
      GetStringForNSString(familyName, name16);
      NS_ConvertUTF16toUTF8 name(name16);
      nsAutoCString key;
      GenerateFontListKey(name, key);
      families.AppendElement(fontlist::Family::InitData(key, name, fontlist::Family::kNoIndex,
                                                        GetVisibilityForFamily(name)));
    }
    SharedFontList()->SetFamilyNames(families);
    InitAliasesForSingleFaceList();
    GetPrefsAndStartLoader();
  }
}

void gfxMacPlatformFontList::InitAliasesForSingleFaceList() {
  for (const auto& familyName : mSingleFaceFonts) {
    LOG_FONTLIST(("(fontlist-singleface) face name: %s\n", familyName.get()));
    // Each entry in the "single face families" list is expected to be a
    // colon-separated pair of FaceName:Family,
    // where FaceName is the individual face name (psname) of a font
    // that should be exposed as a separate family name,
    // and Family is the standard family to which that face belongs.
    // The only such face listed by default is
    //    Osaka-Mono:Osaka
    auto colon = familyName.FindChar(':');
    if (colon == kNotFound) {
      continue;
    }

    // Look for the parent family in the main font family list,
    // and ensure we have loaded its list of available faces.
    nsAutoCString key;
    GenerateFontListKey(Substring(familyName, colon + 1), key);
    fontlist::Family* family = SharedFontList()->FindFamily(key);
    if (!family) {
      // The parent family is not present, so just ignore this entry.
      continue;
    }
    if (!family->IsInitialized()) {
      if (!gfxPlatformFontList::InitializeFamily(family)) {
        // This shouldn't ever fail, but if it does, we can safely ignore it.
        MOZ_ASSERT(false, "failed to initialize font family");
        continue;
      }
    }

    // Truncate the entry from prefs at the colon, so now it is just the
    // desired single-face-family name.
    nsAutoCString aliasName(Substring(familyName, 0, colon));

    // Look through the family's faces to see if this one is present.
    fontlist::FontList* list = SharedFontList();
    const fontlist::Pointer* facePtrs = family->Faces(list);
    for (size_t i = 0; i < family->NumFaces(); i++) {
      if (facePtrs[i].IsNull()) {
        continue;
      }
      auto face = static_cast<const fontlist::Face*>(facePtrs[i].ToPtr(list));
      if (face->mDescriptor.AsString(list).Equals(aliasName)) {
        // Found it! Create an entry in the Alias table.
        GenerateFontListKey(aliasName, key);
        if (SharedFontList()->FindFamily(key) || mAliasTable.Get(key)) {
          // If the family name is already known, something's misconfigured;
          // just ignore it.
          MOZ_ASSERT(false, "single-face family already known");
          break;
        }
        auto aliasData = mAliasTable.GetOrInsertNew(key);
        // The "alias" here isn't based on an existing family, so we don't call
        // aliasData->InitFromFamily(); the various flags are left as defaults.
        aliasData->mFaces.AppendElement(facePtrs[i]);
        aliasData->mBaseFamily = aliasName;
        aliasData->mVisibility = family->Visibility();
        break;
      }
    }
  }
  if (!mAliasTable.IsEmpty()) {
    // This will be updated when the font loader completes, but we require
    // at least the Osaka-Mono alias to be available immediately.
    SharedFontList()->SetAliases(mAliasTable);
  }
}

void gfxMacPlatformFontList::InitSingleFaceList() {
  for (const auto& familyName : mSingleFaceFonts) {
    LOG_FONTLIST(("(fontlist-singleface) face name: %s\n", familyName.get()));
    // Each entry in the "single face families" list is expected to be a
    // colon-separated pair of FaceName:Family,
    // where FaceName is the individual face name (psname) of a font
    // that should be exposed as a separate family name,
    // and Family is the standard family to which that face belongs.
    // The only such face listed by default is
    //    Osaka-Mono:Osaka
    auto colon = familyName.FindChar(':');
    if (colon == kNotFound) {
      continue;
    }

    // Look for the parent family in the main font family list,
    // and ensure we have loaded its list of available faces.
    nsAutoCString key(Substring(familyName, colon + 1));
    ToLowerCase(key);
    gfxFontFamily* family = mFontFamilies.GetWeak(key);
    if (!family || family->IsHidden()) {
      continue;
    }
    family->FindStyleVariations();

    // Truncate the entry from prefs at the colon, so now it is just the
    // desired single-face-family name.
    nsAutoCString aliasName(Substring(familyName, 0, colon));

    // Look through the family's faces to see if this one is present.
    const gfxFontEntry* fe = nullptr;
    for (const auto& face : family->GetFontList()) {
      if (face->Name().Equals(aliasName)) {
        fe = face;
        break;
      }
    }
    if (!fe) {
      continue;
    }

    // We found the correct face, so create the single-face family record.
    GenerateFontListKey(aliasName, key);
    LOG_FONTLIST(("(fontlist-singleface) family name: %s, key: %s\n", aliasName.get(), key.get()));

    // add only if doesn't exist already
    if (!mFontFamilies.GetWeak(key)) {
      RefPtr<gfxFontFamily> familyEntry =
          new gfxSingleFaceMacFontFamily(aliasName, family->Visibility());
      // We need a separate font entry, because its family name will
      // differ from the one we found in the main list.
      MacOSFontEntry* fontEntry = new MacOSFontEntry(
          fe->Name(), fe->Weight(), true, static_cast<const MacOSFontEntry*>(fe)->mSizeHint);
      familyEntry->AddFontEntry(fontEntry);
      familyEntry->SetHasStyles(true);
      mFontFamilies.InsertOrUpdate(key, std::move(familyEntry));
      LOG_FONTLIST(
          ("(fontlist-singleface) added new family: %s, key: %s\n", aliasName.get(), key.get()));
    }
  }
}

// System fonts under OSX may contain weird "meta" names but if we create
// a new font using just the Postscript name, the NSFont api returns an object
// with the actual real family name. For example, under OSX 10.11:
//
// [[NSFont menuFontOfSize:8.0] familyName] ==> .AppleSystemUIFont
// [[NSFont fontWithName:[[[NSFont menuFontOfSize:8.0] fontDescriptor] postscriptName]
//          size:8.0] familyName] ==> .SF NS Text

static NSString* GetRealFamilyName(NSFont* aFont) {
  NSString* psName = [[aFont fontDescriptor] postscriptName];
  // With newer macOS versions and SDKs (e.g. when compiled against SDK 10.15),
  // [NSFont fontWithName:] fails for hidden system fonts, because the underlying
  // Core Text functions it uses reject such names and tell us to use the special
  // CTFontCreateUIFontForLanguage API instead.
  // To work around this, as we don't yet work directly with the CTFontUIFontType
  // identifiers, we create a Core Graphics font (as it doesn't reject system font
  // names), and use this to create a Core Text font that we can query for the
  // family name.
  // Eventually we should move to using CTFontUIFontType constants to identify
  // system fonts, and eliminate the need to instantiate them (indirectly) from
  // their postscript names.
  AutoCFRelease<CGFontRef> cgFont = CGFontCreateWithFontName(CFStringRef(psName));
  if (!cgFont) {
    return [aFont familyName];
  }

  AutoCFRelease<CTFontRef> ctFont = CTFontCreateWithGraphicsFont(cgFont, 0.0, nullptr, nullptr);
  if (!ctFont) {
    return [aFont familyName];
  }
  NSString* familyName = (NSString*)CTFontCopyFamilyName(ctFont);

  return [familyName autorelease];
}

// Create a gfxFontFamily that corresponds to the "system" font name,
// and populate it with the given font face. We only use this on Catalina or later,
// so we expect the system font to be a variable-weight face rather than requiring
// a number of discrete faces of different weights.
static gfxFontFamily* CreateFamilyForSystemFont(NSFont* aFont, const nsACString& aFamilyName) {
  gfxFontFamily* familyEntry = new gfxFontFamily(aFamilyName, FontVisibility::Unknown);

  NSString* psNameNS = [[aFont fontDescriptor] postscriptName];
  nsAutoString nameUTF16;
  nsAutoCString psName;
  nsCocoaUtils::GetStringForNSString(psNameNS, nameUTF16);
  CopyUTF16toUTF8(nameUTF16, psName);

  MacOSFontEntry* fe = new MacOSFontEntry(psName, WeightRange(FontWeight::Normal()), true, 0.0);
  MOZ_ASSERT(gfxPlatform::GetPlatform()->HasVariationFontSupport());
  fe->SetupVariationRanges();

  familyEntry->AddFontEntry(fe);
  familyEntry->SetHasStyles(true);

  return familyEntry;
}

// System fonts under OSX 10.11 use a combination of two families, one
// for text sizes and another for larger, display sizes. Each has a
// different number of weights. There aren't efficient API's for looking
// this information up, so hard code the logic here but confirm via
// debug assertions that the logic is correct.

const CGFloat kTextDisplayCrossover = 20.0;  // use text family below this size

void gfxMacPlatformFontList::InitSystemFontNames() {
  // On Catalina+, the system font uses optical sizing rather than individual
  // faces, so we don't need to look for a separate display-sized face.
  mUseSizeSensitiveSystemFont = !nsCocoaFeatures::OnCatalinaOrLater();

  // text font family
  NSFont* sys = [NSFont systemFontOfSize:0.0];
  NSString* textFamilyName = GetRealFamilyName(sys);
  nsAutoString familyName;
  nsCocoaUtils::GetStringForNSString(textFamilyName, familyName);
  CopyUTF16toUTF8(familyName, mSystemTextFontFamilyName);

  // On Catalina or later, we store an in-process gfxFontFamily for the system font
  // even if using the shared fontlist to manage "normal" fonts, because the hidden
  // system fonts may be excluded from the font list altogether.
  if (nsCocoaFeatures::OnCatalinaOrLater()) {
    RefPtr<gfxFontFamily> fam = CreateFamilyForSystemFont(sys, mSystemTextFontFamilyName);
    if (fam) {
      nsAutoCString key;
      GenerateFontListKey(mSystemTextFontFamilyName, key);
      mFontFamilies.InsertOrUpdate(key, std::move(fam));
    }
  }

  // display font family, if on OSX 10.11 - 10.14
  if (mUseSizeSensitiveSystemFont) {
    NSFont* displaySys = [NSFont systemFontOfSize:128.0];
    NSString* displayFamilyName = GetRealFamilyName(displaySys);
    if ([displayFamilyName isEqualToString:textFamilyName]) {
      mUseSizeSensitiveSystemFont = false;
    } else {
      nsCocoaUtils::GetStringForNSString(displayFamilyName, familyName);
      CopyUTF16toUTF8(familyName, mSystemDisplayFontFamilyName);
    }
  }

#ifdef DEBUG
  // different system font API's always map to the same family under OSX, so
  // just assume that and emit a warning if that ever changes
  NSString* sysFamily = GetRealFamilyName([NSFont systemFontOfSize:0.0]);
  if ([sysFamily compare:GetRealFamilyName([NSFont boldSystemFontOfSize:0.0])] != NSOrderedSame ||
      [sysFamily compare:GetRealFamilyName([NSFont controlContentFontOfSize:0.0])] !=
          NSOrderedSame ||
      [sysFamily compare:GetRealFamilyName([NSFont menuBarFontOfSize:0.0])] != NSOrderedSame ||
      [sysFamily compare:GetRealFamilyName([NSFont toolTipsFontOfSize:0.0])] != NSOrderedSame) {
    NS_WARNING("system font types map to different font families"
               " -- please log a bug!!");
  }
#endif
}

gfxFontFamily* gfxMacPlatformFontList::FindSystemFontFamily(const nsACString& aFamily) {
  nsAutoCString key;
  GenerateFontListKey(aFamily, key);

  gfxFontFamily* familyEntry;
  if ((familyEntry = mFontFamilies.GetWeak(key))) {
    return CheckFamily(familyEntry);
  }

  return nullptr;
}

void gfxMacPlatformFontList::RegisteredFontsChangedNotificationCallback(
    CFNotificationCenterRef center, void* observer, CFStringRef name, const void* object,
    CFDictionaryRef userInfo) {
  if (!::CFEqual(name, kCTFontManagerRegisteredFontsChangedNotification)) {
    return;
  }

  gfxMacPlatformFontList* fl = static_cast<gfxMacPlatformFontList*>(observer);
  if (!fl->IsInitialized()) {
    return;
  }

  // xxx - should be carefully pruning the list of fonts, not rebuilding it from scratch
  fl->UpdateFontList();

  gfxPlatform::ForceGlobalReflow(gfxPlatform::NeedsReframe::Yes);
  dom::ContentParent::NotifyUpdatedFonts(true);
}

gfxFontEntry* gfxMacPlatformFontList::PlatformGlobalFontFallback(nsPresContext* aPresContext,
                                                                 const uint32_t aCh,
                                                                 Script aRunScript,
                                                                 const gfxFontStyle* aMatchStyle,
                                                                 FontFamily& aMatchedFamily) {
  CFStringRef str;
  UniChar ch[2];
  CFIndex length = 1;

  if (IS_IN_BMP(aCh)) {
    ch[0] = aCh;
    str = ::CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, ch, 1, kCFAllocatorNull);
  } else {
    ch[0] = H_SURROGATE(aCh);
    ch[1] = L_SURROGATE(aCh);
    str = ::CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, ch, 2, kCFAllocatorNull);
    if (!str) {
      return nullptr;
    }
    length = 2;
  }

  // use CoreText to find the fallback family

  gfxFontEntry* fontEntry = nullptr;
  bool cantUseFallbackFont = false;

  if (!mDefaultFont) {
    mDefaultFont = ::CTFontCreateWithName(CFSTR("LucidaGrande"), 12.f, NULL);
  }

  AutoCFRelease<CTFontRef> fallback =
      ::CTFontCreateForString(mDefaultFont, str, ::CFRangeMake(0, length));

  if (fallback) {
    AutoCFRelease<CFStringRef> familyNameRef = ::CTFontCopyFamilyName(fallback);

    if (familyNameRef &&
        ::CFStringCompare(familyNameRef, CFSTR("LastResort"), kCFCompareCaseInsensitive) !=
            kCFCompareEqualTo &&
        ::CFStringCompare(familyNameRef, CFSTR(".LastResort"), kCFCompareCaseInsensitive) !=
            kCFCompareEqualTo) {
      AutoTArray<UniChar, 1024> buffer;
      CFIndex familyNameLen = ::CFStringGetLength(familyNameRef);
      buffer.SetLength(familyNameLen + 1);
      ::CFStringGetCharacters(familyNameRef, ::CFRangeMake(0, familyNameLen), buffer.Elements());
      buffer[familyNameLen] = 0;
      NS_ConvertUTF16toUTF8 familyNameString(reinterpret_cast<char16_t*>(buffer.Elements()),
                                             familyNameLen);

      if (SharedFontList()) {
        fontlist::Family* family = FindSharedFamily(aPresContext, familyNameString);
        if (family) {
          fontlist::Face* face = family->FindFaceForStyle(SharedFontList(), *aMatchStyle);
          if (face) {
            fontEntry = GetOrCreateFontEntry(face, family);
          }
          if (fontEntry) {
            if (fontEntry->HasCharacter(aCh)) {
              aMatchedFamily = FontFamily(family);
            } else {
              fontEntry = nullptr;
              cantUseFallbackFont = true;
            }
          }
        }
      }

      // The macOS system font does not appear in the shared font list, so if
      // we didn't find the fallback font above, we should also check for an
      // unshared fontFamily in the system list.
      if (!fontEntry) {
        gfxFontFamily* family = FindSystemFontFamily(familyNameString);
        if (family) {
          fontEntry = family->FindFontForStyle(*aMatchStyle);
          if (fontEntry) {
            if (fontEntry->HasCharacter(aCh)) {
              aMatchedFamily = FontFamily(family);
            } else {
              fontEntry = nullptr;
              cantUseFallbackFont = true;
            }
          }
        }
      }
    }
  }

  if (cantUseFallbackFont) {
    Telemetry::Accumulate(Telemetry::BAD_FALLBACK_FONT, cantUseFallbackFont);
  }

  ::CFRelease(str);

  return fontEntry;
}

FontFamily gfxMacPlatformFontList::GetDefaultFontForPlatform(nsPresContext* aPresContext,
                                                             const gfxFontStyle* aStyle,
                                                             nsAtom* aLanguage) {
  nsAutoreleasePool localPool;

  NSString* defaultFamily = [[NSFont userFontOfSize:aStyle->size] familyName];
  nsAutoString familyName;

  GetStringForNSString(defaultFamily, familyName);
  return FindFamily(aPresContext, NS_ConvertUTF16toUTF8(familyName));
}

int32_t gfxMacPlatformFontList::AppleWeightToCSSWeight(int32_t aAppleWeight) {
  if (aAppleWeight < 1)
    aAppleWeight = 1;
  else if (aAppleWeight > kAppleMaxWeight)
    aAppleWeight = kAppleMaxWeight;
  return gAppleWeightToCSSWeight[aAppleWeight];
}

gfxFontEntry* gfxMacPlatformFontList::LookupLocalFont(nsPresContext* aPresContext,
                                                      const nsACString& aFontName,
                                                      WeightRange aWeightForEntry,
                                                      StretchRange aStretchForEntry,
                                                      SlantStyleRange aStyleForEntry) {
  if (aFontName.IsEmpty() || aFontName[0] == '.') {
    return nullptr;
  }

  nsAutoreleasePool localPool;

  NSString* faceName = GetNSStringForString(NS_ConvertUTF8toUTF16(aFontName));

  // lookup face based on postscript or full name
  AutoCFRelease<CGFontRef> fontRef = CGFontCreateWithFontName(CFStringRef(faceName));
  if (!fontRef) {
    return nullptr;
  }

  // It's possible for CGFontCreateWithFontName to return a font that has been
  // deactivated/uninstalled, or a font that is excluded from the font list due
  // to CSS font-visibility restriction. So we need to check whether this font is
  // allowed to be used.

  // CGFontRef doesn't offer a family-name API, so we go via a CTFontRef.
  AutoCFRelease<CTFontRef> ctFont = CTFontCreateWithGraphicsFont(fontRef, 0.0, nullptr, nullptr);
  if (!ctFont) {
    return nullptr;
  }
  AutoCFRelease<CFStringRef> name = CTFontCopyFamilyName(ctFont);

  // Convert the family name to a key suitable for font-list lookup (8-bit, lowercased).
  nsAutoCString key;
  // CFStringGetLength is in UTF-16 code units. The maximum this count can expand
  // when converted to UTF-8 is 3x. We add 1 to ensure there will also be space for
  // null-termination of the resulting C string.
  key.SetLength((CFStringGetLength(name) + 1) * 3);
  if (!CFStringGetCString(name, key.BeginWriting(), key.Length(), kCFStringEncodingUTF8)) {
    // This shouldn't ever happen, but if it does we just bail.
    NS_WARNING("Failed to get family name?");
    key.Truncate(0);
  }
  if (key.IsEmpty()) {
    return nullptr;
  }
  // Reset our string length to match the actual C string we got, which will usually
  // be much shorter than the maximal buffer we allocated.
  key.Truncate(strlen(key.get()));
  ToLowerCase(key);
  // If the family can't be looked up, this font is not available for use.
  FontFamily family = FindFamily(aPresContext, key);
  if (family.IsNull()) {
    return nullptr;
  }

  return new MacOSFontEntry(aFontName, fontRef, aWeightForEntry, aStretchForEntry, aStyleForEntry,
                            false, true);
}

static void ReleaseData(void* info, const void* data, size_t size) { free((void*)data); }

gfxFontEntry* gfxMacPlatformFontList::MakePlatformFont(const nsACString& aFontName,
                                                       WeightRange aWeightForEntry,
                                                       StretchRange aStretchForEntry,
                                                       SlantStyleRange aStyleForEntry,
                                                       const uint8_t* aFontData, uint32_t aLength) {
  NS_ASSERTION(aFontData, "MakePlatformFont called with null data");

  // create the font entry
  nsAutoString uniqueName;

  nsresult rv = gfxFontUtils::MakeUniqueUserFontName(uniqueName);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  AutoCFRelease<CGDataProviderRef> provider =
      ::CGDataProviderCreateWithData(nullptr, aFontData, aLength, &ReleaseData);
  AutoCFRelease<CGFontRef> fontRef = ::CGFontCreateWithDataProvider(provider);
  if (!fontRef) {
    return nullptr;
  }

  auto newFontEntry =
      MakeUnique<MacOSFontEntry>(NS_ConvertUTF16toUTF8(uniqueName), fontRef, aWeightForEntry,
                                 aStretchForEntry, aStyleForEntry, true, false);
  return newFontEntry.release();
}

// Webkit code uses a system font meta name, so mimic that here
// WebCore/platform/graphics/mac/FontCacheMac.mm
static const char kSystemFont_system[] = "-apple-system";

bool gfxMacPlatformFontList::FindAndAddFamilies(nsPresContext* aPresContext,
                                                StyleGenericFontFamily aGeneric,
                                                const nsACString& aFamily,
                                                nsTArray<FamilyAndGeneric>* aOutput,
                                                FindFamiliesFlags aFlags, gfxFontStyle* aStyle,
                                                nsAtom* aLanguage, gfxFloat aDevToCssSize) {
  if (aFamily.EqualsLiteral(kSystemFont_system)) {
    // Search for special system font name, -apple-system. This is not done via
    // the shared fontlist on Catalina or later, because the hidden system font
    // may not be included there; we create a separate gfxFontFamily to manage
    // this family.
    const nsCString& systemFontFamilyName =
        mUseSizeSensitiveSystemFont && aStyle &&
                (aStyle->size * aDevToCssSize) >= kTextDisplayCrossover
            ? mSystemDisplayFontFamilyName
            : mSystemTextFontFamilyName;
    if (SharedFontList() && !nsCocoaFeatures::OnCatalinaOrLater()) {
      FindFamiliesFlags flags = aFlags | FindFamiliesFlags::eSearchHiddenFamilies;
      return gfxPlatformFontList::FindAndAddFamilies(aPresContext, aGeneric, systemFontFamilyName,
                                                     aOutput, flags, aStyle, aLanguage,
                                                     aDevToCssSize);
    } else {
      if (auto* fam = FindSystemFontFamily(systemFontFamilyName)) {
        aOutput->AppendElement(fam);
        return true;
      }
    }
    return false;
  }

  return gfxPlatformFontList::FindAndAddFamilies(aPresContext, aGeneric, aFamily, aOutput, aFlags,
                                                 aStyle, aLanguage, aDevToCssSize);
}

void gfxMacPlatformFontList::LookupSystemFont(LookAndFeel::FontID aSystemFontID,
                                              nsACString& aSystemFontName,
                                              gfxFontStyle& aFontStyle) {
  // Provide a local pool because we may be called from stylo threads.
  nsAutoreleasePool localPool;

  // code moved here from widget/cocoa/nsLookAndFeel.mm
  NSFont* font = nullptr;
  char* systemFontName = nullptr;
  switch (aSystemFontID) {
    case LookAndFeel::FontID::MessageBox:
    case LookAndFeel::FontID::StatusBar:
    case LookAndFeel::FontID::MozList:
    case LookAndFeel::FontID::MozField:
    case LookAndFeel::FontID::MozButton:
      font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
      systemFontName = (char*)kSystemFont_system;
      break;

    case LookAndFeel::FontID::SmallCaption:
      font = [NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]];
      systemFontName = (char*)kSystemFont_system;
      break;

    case LookAndFeel::FontID::Icon:  // used in urlbar; tried labelFont, but too small
    case LookAndFeel::FontID::MozWorkspace:
    case LookAndFeel::FontID::MozDesktop:
    case LookAndFeel::FontID::MozInfo:
      font = [NSFont controlContentFontOfSize:0.0];
      systemFontName = (char*)kSystemFont_system;
      break;

    case LookAndFeel::FontID::MozPullDownMenu:
      font = [NSFont menuBarFontOfSize:0.0];
      systemFontName = (char*)kSystemFont_system;
      break;

    case LookAndFeel::FontID::Caption:
    case LookAndFeel::FontID::Menu:
    case LookAndFeel::FontID::MozDialog:
    default:
      font = [NSFont systemFontOfSize:0.0];
      systemFontName = (char*)kSystemFont_system;
      break;
  }
  NS_ASSERTION(font, "system font not set");
  NS_ASSERTION(systemFontName, "system font name not set");

  if (systemFontName) {
    aSystemFontName.AssignASCII(systemFontName);
  }

  NSFontSymbolicTraits traits = [[font fontDescriptor] symbolicTraits];
  aFontStyle.style =
      (traits & NSFontItalicTrait) ? FontSlantStyle::Italic() : FontSlantStyle::Normal();
  aFontStyle.weight = (traits & NSFontBoldTrait) ? FontWeight::Bold() : FontWeight::Normal();
  aFontStyle.stretch = (traits & NSFontExpandedTrait)    ? FontStretch::Expanded()
                       : (traits & NSFontCondensedTrait) ? FontStretch::Condensed()
                                                         : FontStretch::Normal();
  aFontStyle.size = [font pointSize];
  aFontStyle.systemFont = true;
}

// used to load system-wide font info on off-main thread
class MacFontInfo final : public FontInfoData {
 public:
  MacFontInfo(bool aLoadOtherNames, bool aLoadFaceNames, bool aLoadCmaps)
      : FontInfoData(aLoadOtherNames, aLoadFaceNames, aLoadCmaps) {}

  virtual ~MacFontInfo() = default;

  virtual void Load() {
    nsAutoreleasePool localPool;
    FontInfoData::Load();
  }

  // loads font data for all members of a given family
  virtual void LoadFontFamilyData(const nsACString& aFamilyName);
};

void MacFontInfo::LoadFontFamilyData(const nsACString& aFamilyName) {
  // family name ==> CTFontDescriptor
  NSString* famName = GetNSStringForString(NS_ConvertUTF8toUTF16(aFamilyName));
  CFStringRef family = CFStringRef(famName);

  AutoCFRelease<CFMutableDictionaryRef> attr = CFDictionaryCreateMutable(
      NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionaryAddValue(attr, kCTFontFamilyNameAttribute, family);
  AutoCFRelease<CTFontDescriptorRef> fd = CTFontDescriptorCreateWithAttributes(attr);
  AutoCFRelease<CFArrayRef> matchingFonts = CTFontDescriptorCreateMatchingFontDescriptors(fd, NULL);
  if (!matchingFonts) {
    return;
  }

  nsTArray<nsCString> otherFamilyNames;
  bool hasOtherFamilyNames = true;

  // iterate over faces in the family
  int f, numFaces = (int)CFArrayGetCount(matchingFonts);
  for (f = 0; f < numFaces; f++) {
    mLoadStats.fonts++;

    CTFontDescriptorRef faceDesc = (CTFontDescriptorRef)CFArrayGetValueAtIndex(matchingFonts, f);
    if (!faceDesc) {
      continue;
    }
    AutoCFRelease<CTFontRef> fontRef = CTFontCreateWithFontDescriptor(faceDesc, 0.0, nullptr);
    if (!fontRef) {
      NS_WARNING("failed to create a CTFontRef");
      continue;
    }

    if (mLoadCmaps) {
      // face name
      AutoCFRelease<CFStringRef> faceName =
          (CFStringRef)CTFontDescriptorCopyAttribute(faceDesc, kCTFontNameAttribute);

      AutoTArray<UniChar, 1024> buffer;
      CFIndex len = CFStringGetLength(faceName);
      buffer.SetLength(len + 1);
      CFStringGetCharacters(faceName, ::CFRangeMake(0, len), buffer.Elements());
      buffer[len] = 0;
      NS_ConvertUTF16toUTF8 fontName(reinterpret_cast<char16_t*>(buffer.Elements()), len);

      // load the cmap data
      FontFaceData fontData;
      AutoCFRelease<CFDataRef> cmapTable =
          CTFontCopyTable(fontRef, kCTFontTableCmap, kCTFontTableOptionNoOptions);

      if (cmapTable) {
        const uint8_t* cmapData = (const uint8_t*)CFDataGetBytePtr(cmapTable);
        uint32_t cmapLen = CFDataGetLength(cmapTable);
        RefPtr<gfxCharacterMap> charmap = new gfxCharacterMap();
        uint32_t offset;
        nsresult rv;

        rv = gfxFontUtils::ReadCMAP(cmapData, cmapLen, *charmap, offset);
        if (NS_SUCCEEDED(rv)) {
          fontData.mCharacterMap = charmap;
          fontData.mUVSOffset = offset;
          mLoadStats.cmaps++;
        }
      }

      mFontFaceData.InsertOrUpdate(fontName, fontData);
    }

    if (mLoadOtherNames && hasOtherFamilyNames) {
      AutoCFRelease<CFDataRef> nameTable =
          CTFontCopyTable(fontRef, kCTFontTableName, kCTFontTableOptionNoOptions);

      if (nameTable) {
        const char* nameData = (const char*)CFDataGetBytePtr(nameTable);
        uint32_t nameLen = CFDataGetLength(nameTable);
        gfxFontUtils::ReadOtherFamilyNamesForFace(aFamilyName, nameData, nameLen, otherFamilyNames,
                                                  false);
        hasOtherFamilyNames = otherFamilyNames.Length() != 0;
      }
    }
  }

  // if found other names, insert them in the hash table
  if (otherFamilyNames.Length() != 0) {
    mOtherFamilyNames.InsertOrUpdate(aFamilyName, otherFamilyNames);
    mLoadStats.othernames += otherFamilyNames.Length();
  }
}

already_AddRefed<FontInfoData> gfxMacPlatformFontList::CreateFontInfoData() {
  bool loadCmaps =
      !UsesSystemFallback() || gfxPlatform::GetPlatform()->UseCmapsDuringSystemFallback();

  RefPtr<MacFontInfo> fi = new MacFontInfo(true, NeedFullnamePostscriptNames(), loadCmaps);
  return fi.forget();
}

gfxFontFamily* gfxMacPlatformFontList::CreateFontFamily(const nsACString& aName,
                                                        FontVisibility aVisibility) const {
  return new gfxMacFontFamily(aName, aVisibility, 0.0);
}

gfxFontEntry* gfxMacPlatformFontList::CreateFontEntry(fontlist::Face* aFace,
                                                      const fontlist::Family* aFamily) {
  MacOSFontEntry* fe =
      new MacOSFontEntry(aFace->mDescriptor.AsString(SharedFontList()), aFace->mWeight, false,
                         0.0);  // XXX standardFace, sizeHint
  fe->InitializeFrom(aFace, aFamily);
  return fe;
}

void gfxMacPlatformFontList::GetFacesInitDataForFamily(const fontlist::Family* aFamily,
                                                       nsTArray<fontlist::Face::InitData>& aFaces,
                                                       bool aLoadCmaps) const {
  nsAutoreleasePool localPool;

  NS_ConvertUTF8toUTF16 name(aFamily->Key().AsString(SharedFontList()));
  NSString* family = GetNSStringForString(name);

  // returns an array of [psname, style name, weight, traits] elements, goofy api
  NSArray* fontfaces = [sFontManager availableMembersOfFontFamily:family];
  int faceCount = [fontfaces count];
  for (int faceIndex = 0; faceIndex < faceCount; faceIndex++) {
    NSArray* face = [fontfaces objectAtIndex:faceIndex];
    NSString* psname = [face objectAtIndex:INDEX_FONT_POSTSCRIPT_NAME];
    int32_t appKitWeight = [[face objectAtIndex:INDEX_FONT_WEIGHT] unsignedIntValue];
    uint32_t macTraits = [[face objectAtIndex:INDEX_FONT_TRAITS] unsignedIntValue];
    NSString* facename = [face objectAtIndex:INDEX_FONT_FACE_NAME];

    if (appKitWeight == kAppleExtraLightWeight) {
      // if the facename contains UltraLight, set the weight to the ultralight weight value
      NSRange range = [facename rangeOfString:@"ultralight" options:NSCaseInsensitiveSearch];
      if (range.location != NSNotFound) {
        appKitWeight = kAppleUltraLightWeight;
      }
    }

    // make a nsString
    nsAutoString postscriptFontName;
    GetStringForNSString(psname, postscriptFontName);

    int32_t cssWeight = gfxMacPlatformFontList::AppleWeightToCSSWeight(appKitWeight);
    if (PR_GetCurrentThread() != sInitFontListThread) {
      int32_t weightOverride = GetWeightOverride(postscriptFontName);
      if (weightOverride) {
        // scale down and clamp, to get a value from 1..9
        cssWeight = ((weightOverride + 50) / 100);
        cssWeight = std::max(1, std::min(cssWeight, 9));
      }
    }
    cssWeight *= 100;  // scale up to CSS values

    StretchRange stretch(FontStretch::Normal());
    if (macTraits & (NSCondensedFontMask | NSNarrowFontMask | NSCompressedFontMask)) {
      stretch = StretchRange(FontStretch::Condensed());
    } else if (macTraits & NSExpandedFontMask) {
      stretch = StretchRange(FontStretch::Expanded());
    }
    // Cocoa fails to set the Italic traits bit for HelveticaLightItalic,
    // at least (see bug 611855), so check for style name endings as well
    SlantStyleRange slantStyle(FontSlantStyle::Normal());
    if ((macTraits & NSItalicFontMask) || [facename hasSuffix:@"Italic"] ||
        [facename hasSuffix:@"Oblique"]) {
      slantStyle = SlantStyleRange(FontSlantStyle::Italic());
    }

    bool fixedPitch = (macTraits & NSFixedPitchFontMask) ? true : false;

    RefPtr<gfxCharacterMap> charmap;
    if (aLoadCmaps) {
      AutoCFRelease<CGFontRef> font = CGFontCreateWithFontName(CFStringRef(psname));
      if (font) {
        uint32_t kCMAP = TRUETYPE_TAG('c', 'm', 'a', 'p');
        AutoCFRelease<CFDataRef> data = CGFontCopyTableForTag(font, kCMAP);
        if (data) {
          uint32_t offset;
          charmap = new gfxCharacterMap();
          gfxFontUtils::ReadCMAP(CFDataGetBytePtr(data), CFDataGetLength(data), *charmap, offset);
        }
      }
    }

    // Ensure that a face named "Regular" goes to the front of the list, so it
    // will take precedence over other faces with the same style attributes but
    // a different name (such as "Outline").
    auto data = fontlist::Face::InitData{
        NS_ConvertUTF16toUTF8(postscriptFontName),
        0,
        fixedPitch,
        WeightRange(FontWeight(cssWeight)),
        stretch,
        slantStyle,
        charmap,
    };
    if ([facename caseInsensitiveCompare:@"Regular"] == NSOrderedSame) {
      aFaces.InsertElementAt(0, std::move(data));
    } else {
      aFaces.AppendElement(std::move(data));
    }
  }
}

void gfxMacPlatformFontList::ReadFaceNamesForFamily(fontlist::Family* aFamily,
                                                    bool aNeedFullnamePostscriptNames) {
  if (!aFamily->IsInitialized()) {
    if (!InitializeFamily(aFamily)) {
      return;
    }
  }
  const uint32_t kNAME = TRUETYPE_TAG('n', 'a', 'm', 'e');
  fontlist::FontList* list = SharedFontList();
  nsAutoCString canonicalName(aFamily->DisplayName().AsString(list));
  const fontlist::Pointer* facePtrs = aFamily->Faces(list);
  for (uint32_t i = 0, n = aFamily->NumFaces(); i < n; i++) {
    auto face = static_cast<fontlist::Face*>(facePtrs[i].ToPtr(list));
    if (!face) {
      continue;
    }
    nsAutoCString name(face->mDescriptor.AsString(list));
    // We create a temporary MacOSFontEntry just to read family names from the
    // 'name' table in the font resource. The style attributes here are ignored
    // as this entry is not used for font style matching.
    // The size hint might be used to select which face is accessed in the case
    // of the macOS UI font; see MacOSFontEntry::GetFontRef(). We pass 16.0 in
    // order to get a standard text-size face in this case, although it's
    // unlikely to matter for the purpose of just reading family names.
    auto fe = MakeUnique<MacOSFontEntry>(name, WeightRange(FontWeight::Normal()), false, 16.0);
    if (!fe) {
      continue;
    }
    gfxFontEntry::AutoTable nameTable(fe.get(), kNAME);
    if (!nameTable) {
      continue;
    }
    uint32_t dataLength;
    const char* nameData = hb_blob_get_data(nameTable, &dataLength);
    AutoTArray<nsCString, 4> otherFamilyNames;
    gfxFontUtils::ReadOtherFamilyNamesForFace(canonicalName, nameData, dataLength, otherFamilyNames,
                                              false);
    for (const auto& alias : otherFamilyNames) {
      nsAutoCString key;
      GenerateFontListKey(alias, key);
      auto aliasData = mAliasTable.GetOrInsertNew(key);
      aliasData->InitFromFamily(aFamily, canonicalName);
      aliasData->mFaces.AppendElement(facePtrs[i]);
    }
  }
}
