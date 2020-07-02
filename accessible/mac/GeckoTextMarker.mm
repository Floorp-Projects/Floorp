/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleParent.h"
#include "AccessibleOrProxy.h"
#include "nsCocoaUtils.h"

#import "GeckoTextMarker.h"

extern "C" {

CFTypeID AXTextMarkerGetTypeID();

AXTextMarkerRef AXTextMarkerCreate(CFAllocatorRef allocator, const UInt8* bytes, CFIndex length);

const UInt8* AXTextMarkerGetBytePtr(AXTextMarkerRef text_marker);

size_t AXTextMarkerGetLength(AXTextMarkerRef text_marker);

CFTypeID AXTextMarkerRangeGetTypeID();

AXTextMarkerRangeRef AXTextMarkerRangeCreate(CFAllocatorRef allocator, AXTextMarkerRef start_marker,
                                             AXTextMarkerRef end_marker);

AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(AXTextMarkerRangeRef text_marker_range);

AXTextMarkerRef AXTextMarkerRangeCopyEndMarker(AXTextMarkerRangeRef text_marker_range);
}

namespace mozilla {
namespace a11y {

struct OpaqueGeckoTextMarker {
  OpaqueGeckoTextMarker(uintptr_t aID, int32_t aOffset) : mID(aID), mOffset(aOffset) {}
  OpaqueGeckoTextMarker() {}
  uintptr_t mID;
  int32_t mOffset;
};

// GeckoTextMarker

GeckoTextMarker::GeckoTextMarker(AccessibleOrProxy aDoc, AXTextMarkerRef aTextMarker) {
  MOZ_ASSERT(!aDoc.IsNull());
  OpaqueGeckoTextMarker opaqueMarker;
  if (AXTextMarkerGetLength(aTextMarker) == sizeof(OpaqueGeckoTextMarker)) {
    memcpy(&opaqueMarker, AXTextMarkerGetBytePtr(aTextMarker), sizeof(OpaqueGeckoTextMarker));
    if (aDoc.IsProxy()) {
      mContainer = aDoc.AsProxy()->AsDoc()->GetAccessible(opaqueMarker.mID);
    } else {
      mContainer = aDoc.AsAccessible()->AsDoc()->GetAccessibleByUniqueID(
          reinterpret_cast<void*>(opaqueMarker.mID));
    }

    mOffset = opaqueMarker.mOffset;
  }
}

id GeckoTextMarker::CreateAXTextMarker() {
  uintptr_t identifier = mContainer.IsProxy()
                             ? mContainer.AsProxy()->ID()
                             : reinterpret_cast<uintptr_t>(mContainer.AsAccessible()->UniqueID());
  OpaqueGeckoTextMarker opaqueMarker(identifier, mOffset);
  AXTextMarkerRef cf_text_marker =
      AXTextMarkerCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&opaqueMarker),
                         sizeof(OpaqueGeckoTextMarker));

  return [static_cast<id>(cf_text_marker) autorelease];
}

bool GeckoTextMarker::operator<(const GeckoTextMarker& aPoint) const {
  if (mContainer == aPoint.mContainer) return mOffset < aPoint.mOffset;

  // Build the chain of parents
  AccessibleOrProxy p1 = mContainer;
  AccessibleOrProxy p2 = aPoint.mContainer;
  AutoTArray<AccessibleOrProxy, 30> parents1, parents2;
  do {
    parents1.AppendElement(p1);
    p1 = p1.Parent();
  } while (!p1.IsNull());
  do {
    parents2.AppendElement(p2);
    p2 = p2.Parent();
  } while (!p2.IsNull());

  // Find where the parent chain differs
  uint32_t pos1 = parents1.Length(), pos2 = parents2.Length();
  for (uint32_t len = std::min(pos1, pos2); len > 0; --len) {
    AccessibleOrProxy child1 = parents1.ElementAt(--pos1);
    AccessibleOrProxy child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      return child1.IndexInParent() < child2.IndexInParent();
    }
  }

  MOZ_ASSERT_UNREACHABLE("Broken tree?!");
  return false;
}

// GeckoTextMarkerRange

GeckoTextMarkerRange::GeckoTextMarkerRange(AccessibleOrProxy aDoc,
                                           AXTextMarkerRangeRef aTextMarkerRange) {
  if (CFGetTypeID(aTextMarkerRange) != AXTextMarkerRangeGetTypeID()) {
    return;
  }

  AXTextMarkerRef start_marker(AXTextMarkerRangeCopyStartMarker(aTextMarkerRange));
  AXTextMarkerRef end_marker(AXTextMarkerRangeCopyEndMarker(aTextMarkerRange));

  mStart = GeckoTextMarker(aDoc, start_marker);
  mEnd = GeckoTextMarker(aDoc, end_marker);

  CFRelease(start_marker);
  CFRelease(end_marker);
}

id GeckoTextMarkerRange::CreateAXTextMarkerRange() {
  AXTextMarkerRangeRef cf_text_marker_range = AXTextMarkerRangeCreate(
      kCFAllocatorDefault, mStart.CreateAXTextMarker(), mEnd.CreateAXTextMarker());
  return [static_cast<id>(cf_text_marker_range) autorelease];
}

NSString* GeckoTextMarkerRange::Text() const {
  nsAutoString text;
  TextInternal(text, mStart.mContainer, mStart.mOffset);
  return nsCocoaUtils::ToNSString(text);
}

void GeckoTextMarkerRange::AppendTextTo(const AccessibleOrProxy& aContainer, nsAString& aText,
                                        uint32_t aStartOffset, uint32_t aEndOffset) const {
  nsAutoString text;
  if (aContainer.IsProxy()) {
    aContainer.AsProxy()->TextSubstring(aStartOffset, aEndOffset, text);
  } else if (aContainer.AsAccessible()->IsHyperText()) {
    aContainer.AsAccessible()->AsHyperText()->TextSubstring(aStartOffset, aEndOffset, text);
  }

  aText.Append(text);
}

int32_t GeckoTextMarkerRange::StartOffset(const AccessibleOrProxy& aChild) const {
  if (aChild.IsProxy()) {
    bool unused;
    return aChild.AsProxy()->StartOffset(&unused);
  }

  return aChild.AsAccessible()->StartOffset();
}

int32_t GeckoTextMarkerRange::EndOffset(const AccessibleOrProxy& aChild) const {
  if (aChild.IsProxy()) {
    bool unused;
    return aChild.AsProxy()->EndOffset(&unused);
  }

  return aChild.AsAccessible()->EndOffset();
}

int32_t GeckoTextMarkerRange::LinkCount(const AccessibleOrProxy& aContainer) const {
  if (aContainer.IsProxy()) {
    return aContainer.AsProxy()->LinkCount();
  }

  if (aContainer.AsAccessible()->IsHyperText()) {
    return aContainer.AsAccessible()->AsHyperText()->LinkCount();
  }

  return 0;
}

AccessibleOrProxy GeckoTextMarkerRange::LinkAt(const AccessibleOrProxy& aContainer,
                                               uint32_t aIndex) const {
  if (aContainer.IsProxy()) {
    return aContainer.AsProxy()->LinkAt(aIndex);
  }

  if (aContainer.AsAccessible()->IsHyperText()) {
    return aContainer.AsAccessible()->AsHyperText()->LinkAt(aIndex);
  }

  return AccessibleOrProxy();
}

bool GeckoTextMarkerRange::TextInternal(nsAString& aText, AccessibleOrProxy aCurrent,
                                        int32_t aStartIntlOffset) const {
  int32_t endIntlOffset = aCurrent == mEnd.mContainer ? mEnd.mOffset : -1;
  if (endIntlOffset == 0) {
    return false;
  }

  AccessibleOrProxy next;
  int32_t linkCount = LinkCount(aCurrent);
  int32_t linkStartOffset = 0;
  int32_t end = endIntlOffset;
  // Find the first link that is at or after the start offset.
  for (int32_t i = 0; i < linkCount; i++) {
    AccessibleOrProxy link = LinkAt(aCurrent, i);
    linkStartOffset = StartOffset(link);
    if (aStartIntlOffset <= linkStartOffset) {
      next = link;
      break;
    }
  }

  bool endIsBeyondLink = !next.IsNull() && (endIntlOffset < 0 || endIntlOffset > linkStartOffset);

  if (endIsBeyondLink) {
    end = linkStartOffset;
  }

  AppendTextTo(aCurrent, aText, aStartIntlOffset, end);

  if (endIsBeyondLink) {
    if (!TextInternal(aText, next, 0)) {
      return false;
    }
  }

  if (endIntlOffset >= 0) {
    // If our original end marker is positive, we know we found all the text we
    // needed within this current node, and our search is complete.
    return false;
  }

  next = aCurrent.Parent();
  if (!next.IsNull()) {
    // The end offset is passed this link, go back to the parent
    // and continue from this link's end offset.
    return TextInternal(aText, next, EndOffset(aCurrent));
  }

  return true;
}
}
}
