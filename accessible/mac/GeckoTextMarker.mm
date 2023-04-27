/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "GeckoTextMarker.h"

#import "MacUtils.h"

#include "nsAccUtils.h"
#include "DocAccessible.h"
#include "DocAccessibleParent.h"
#include "mozilla/StaticPrefs_accessibility.h"

namespace mozilla {
namespace a11y {

struct TextMarkerData {
  TextMarkerData(uintptr_t aDoc, uintptr_t aID, int32_t aOffset, bool aLegacy)
      : mDoc(aDoc), mID(aID), mOffset(aOffset), mLegacy(aLegacy) {}
  TextMarkerData() {}
  uintptr_t mDoc;
  uintptr_t mID;
  int32_t mOffset;
  bool mLegacy;
};

// LegacyTextMarker

GeckoTextMarker::GeckoTextMarker() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    mLegacy = false;
    mCachedTextMarker = CachedTextMarker();
  } else {
    mLegacy = true;
    mLegacyTextMarker = LegacyTextMarker();
  }
}

GeckoTextMarker::GeckoTextMarker(Accessible* aContainer, int32_t aOffset) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    mLegacy = false;
    mCachedTextMarker = CachedTextMarker(aContainer, aOffset);
  } else {
    mLegacy = true;
    int32_t offset = aOffset;
    if (aOffset == nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT) {
      offset = aContainer->IsRemote() ? aContainer->AsRemote()->CharacterCount()
                                      : aContainer->AsLocal()
                                            ->Document()
                                            ->AsHyperText()
                                            ->CharacterCount();
    }
    mLegacyTextMarker = LegacyTextMarker(aContainer, offset);
  }
}

GeckoTextMarker GeckoTextMarker::MarkerFromAXTextMarker(
    Accessible* aDoc, AXTextMarkerRef aTextMarker) {
  MOZ_ASSERT(aDoc);
  if (!aTextMarker) {
    return GeckoTextMarker();
  }

  if (AXTextMarkerGetLength(aTextMarker) != sizeof(TextMarkerData)) {
    MOZ_ASSERT_UNREACHABLE("Malformed AXTextMarkerRef");
    return GeckoTextMarker();
  }

  TextMarkerData markerData;
  memcpy(&markerData, AXTextMarkerGetBytePtr(aTextMarker),
         sizeof(TextMarkerData));
  MOZ_ASSERT(StaticPrefs::accessibility_cache_enabled_AtStartup() ==
             !markerData.mLegacy);

  if (!utils::DocumentExists(aDoc, markerData.mDoc)) {
    return GeckoTextMarker();
  }

  Accessible* doc = reinterpret_cast<Accessible*>(markerData.mDoc);
  MOZ_ASSERT(doc->IsDoc());
  int32_t offset = markerData.mOffset;
  Accessible* acc = nullptr;
  if (doc->IsRemote()) {
    acc = doc->AsRemote()->AsDoc()->GetAccessible(markerData.mID);
  } else {
    acc = doc->AsLocal()->AsDoc()->GetAccessibleByUniqueID(
        reinterpret_cast<void*>(markerData.mID));
  }

  if (!acc) {
    return GeckoTextMarker();
  }

  return GeckoTextMarker(acc, offset);
}

GeckoTextMarker GeckoTextMarker::MarkerFromIndex(Accessible* aRoot,
                                                 int32_t aIndex) {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    return GeckoTextMarker(CachedTextMarker::MarkerFromIndex(aRoot, aIndex));
  }

  return GeckoTextMarker(LegacyTextMarker::MarkerFromIndex(aRoot, aIndex));
}

AXTextMarkerRef GeckoTextMarker::CreateAXTextMarker() {
  if (!IsValid()) {
    return nil;
  }

  Accessible* acc =
      mLegacy ? mLegacyTextMarker.mContainer : mCachedTextMarker.mPoint.mAcc;
  int32_t offset =
      mLegacy ? mLegacyTextMarker.mOffset : mCachedTextMarker.mPoint.mOffset;
  Accessible* doc = nsAccUtils::DocumentFor(acc);
  TextMarkerData markerData(reinterpret_cast<uintptr_t>(doc), acc->ID(), offset,
                            mLegacy);
  AXTextMarkerRef cf_text_marker = AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&markerData),
      sizeof(TextMarkerData));

  return (__bridge AXTextMarkerRef)[(__bridge id)(cf_text_marker)autorelease];
}

GeckoTextMarkerRange GeckoTextMarker::LeftWordRange() const {
  if (mLegacy) {
    return GeckoTextMarkerRange(
        mLegacyTextMarker.Range(EWhichRange::eLeftWord));
  } else {
    return GeckoTextMarkerRange(mCachedTextMarker.LeftWordRange());
  }
}

GeckoTextMarkerRange GeckoTextMarker::RightWordRange() const {
  if (mLegacy) {
    return GeckoTextMarkerRange(
        mLegacyTextMarker.Range(EWhichRange::eRightWord));
  } else {
    return GeckoTextMarkerRange(mCachedTextMarker.RightWordRange());
  }
}

GeckoTextMarkerRange GeckoTextMarker::LineRange() const {
  if (mLegacy) {
    return GeckoTextMarkerRange(
        mLegacyTextMarker.Range(EWhichRange::eLeftLine));
  } else {
    return GeckoTextMarkerRange(mCachedTextMarker.LineRange());
  }
}

GeckoTextMarkerRange GeckoTextMarker::LeftLineRange() const {
  if (mLegacy) {
    return GeckoTextMarkerRange(
        mLegacyTextMarker.Range(EWhichRange::eLeftLine));
  } else {
    return GeckoTextMarkerRange(mCachedTextMarker.LeftLineRange());
  }
}

GeckoTextMarkerRange GeckoTextMarker::RightLineRange() const {
  if (mLegacy) {
    return GeckoTextMarkerRange(
        mLegacyTextMarker.Range(EWhichRange::eRightLine));
  } else {
    return GeckoTextMarkerRange(mCachedTextMarker.RightLineRange());
  }
}

GeckoTextMarkerRange GeckoTextMarker::ParagraphRange() const {
  if (mLegacy) {
    return GeckoTextMarkerRange(
        mLegacyTextMarker.Range(EWhichRange::eParagraph));
  } else {
    return GeckoTextMarkerRange(mCachedTextMarker.ParagraphRange());
  }
}

GeckoTextMarkerRange GeckoTextMarker::StyleRange() const {
  if (mLegacy) {
    return GeckoTextMarkerRange(mLegacyTextMarker.Range(EWhichRange::eStyle));
  } else {
    return GeckoTextMarkerRange(mCachedTextMarker.StyleRange());
  }
}

GeckoTextMarkerRange::GeckoTextMarkerRange() {
  if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
    mLegacy = false;
    mCachedTextMarkerRange = CachedTextMarkerRange();
  } else {
    mLegacy = true;
    mLegacyTextMarkerRange = LegacyTextMarkerRange();
  }
}

GeckoTextMarkerRange::GeckoTextMarkerRange(Accessible* aAccessible) {
  mLegacy = !StaticPrefs::accessibility_cache_enabled_AtStartup();
  if (mLegacy) {
    mLegacyTextMarkerRange = LegacyTextMarkerRange(aAccessible);
  } else {
    mCachedTextMarkerRange = CachedTextMarkerRange(aAccessible);
  }
}

GeckoTextMarkerRange GeckoTextMarkerRange::MarkerRangeFromAXTextMarkerRange(
    Accessible* aDoc, AXTextMarkerRangeRef aTextMarkerRange) {
  if (!aTextMarkerRange ||
      CFGetTypeID(aTextMarkerRange) != AXTextMarkerRangeGetTypeID()) {
    return GeckoTextMarkerRange();
  }

  AXTextMarkerRef start_marker(
      AXTextMarkerRangeCopyStartMarker(aTextMarkerRange));
  AXTextMarkerRef end_marker(AXTextMarkerRangeCopyEndMarker(aTextMarkerRange));

  GeckoTextMarker start =
      GeckoTextMarker::MarkerFromAXTextMarker(aDoc, start_marker);
  GeckoTextMarker end =
      GeckoTextMarker::MarkerFromAXTextMarker(aDoc, end_marker);
  MOZ_ASSERT(start.mLegacy == end.mLegacy);

  CFRelease(start_marker);
  CFRelease(end_marker);

  return GeckoTextMarkerRange(start, end);
}

AXTextMarkerRangeRef GeckoTextMarkerRange::CreateAXTextMarkerRange() {
  if (!IsValid()) {
    return nil;
  }

  GeckoTextMarker start, end;
  if (mLegacy) {
    start = GeckoTextMarker(mLegacyTextMarkerRange.mStart);
    end = GeckoTextMarker(mLegacyTextMarkerRange.mEnd);
  } else {
    start = GeckoTextMarker(mCachedTextMarkerRange.mRange.Start());
    end = GeckoTextMarker(mCachedTextMarkerRange.mRange.End());
  }

  AXTextMarkerRangeRef cf_text_marker_range =
      AXTextMarkerRangeCreate(kCFAllocatorDefault, start.CreateAXTextMarker(),
                              end.CreateAXTextMarker());

  return (__bridge AXTextMarkerRangeRef)[(__bridge id)(
      cf_text_marker_range)autorelease];
}

}  // namespace a11y
}  // namespace mozilla
