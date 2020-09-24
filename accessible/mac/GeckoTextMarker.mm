/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleParent.h"
#include "AccessibleOrProxy.h"
#include "nsCocoaUtils.h"

#include "mozilla/a11y/DocAccessiblePlatformExtParent.h"

#import "GeckoTextMarker.h"

extern "C" {

CFTypeID AXTextMarkerGetTypeID();

AXTextMarkerRef AXTextMarkerCreate(CFAllocatorRef allocator, const UInt8* bytes,
                                   CFIndex length);

const UInt8* AXTextMarkerGetBytePtr(AXTextMarkerRef text_marker);

size_t AXTextMarkerGetLength(AXTextMarkerRef text_marker);

CFTypeID AXTextMarkerRangeGetTypeID();

AXTextMarkerRangeRef AXTextMarkerRangeCreate(CFAllocatorRef allocator,
                                             AXTextMarkerRef start_marker,
                                             AXTextMarkerRef end_marker);

AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(
    AXTextMarkerRangeRef text_marker_range);

AXTextMarkerRef AXTextMarkerRangeCopyEndMarker(
    AXTextMarkerRangeRef text_marker_range);
}

namespace mozilla {
namespace a11y {

struct OpaqueGeckoTextMarker {
  OpaqueGeckoTextMarker(uintptr_t aID, int32_t aOffset)
      : mID(aID), mOffset(aOffset) {}
  OpaqueGeckoTextMarker() {}
  uintptr_t mID;
  int32_t mOffset;
};

// GeckoTextMarker

GeckoTextMarker::GeckoTextMarker(AccessibleOrProxy aDoc,
                                 AXTextMarkerRef aTextMarker) {
  MOZ_ASSERT(!aDoc.IsNull());
  OpaqueGeckoTextMarker opaqueMarker;
  if (AXTextMarkerGetLength(aTextMarker) == sizeof(OpaqueGeckoTextMarker)) {
    memcpy(&opaqueMarker, AXTextMarkerGetBytePtr(aTextMarker),
           sizeof(OpaqueGeckoTextMarker));
    if (aDoc.IsProxy()) {
      mContainer = aDoc.AsProxy()->AsDoc()->GetAccessible(opaqueMarker.mID);
    } else {
      mContainer = aDoc.AsAccessible()->AsDoc()->GetAccessibleByUniqueID(
          reinterpret_cast<void*>(opaqueMarker.mID));
    }

    mOffset = opaqueMarker.mOffset;
  }
}

GeckoTextMarker GeckoTextMarker::MarkerFromIndex(const AccessibleOrProxy& aRoot,
                                                 int32_t aIndex) {
  if (aRoot.IsProxy()) {
    int32_t offset = 0;
    uint64_t containerID = 0;
    DocAccessibleParent* ipcDoc = aRoot.AsProxy()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendOffsetAtIndex(
        aRoot.AsProxy()->ID(), aIndex, &containerID, &offset);
    ProxyAccessible* container = ipcDoc->GetAccessible(containerID);
    return GeckoTextMarker(container, offset);
  } else if (auto htWrap = static_cast<HyperTextAccessibleWrap*>(
                 aRoot.AsAccessible()->AsHyperText())) {
    int32_t offset = 0;
    HyperTextAccessible* container = nullptr;
    htWrap->OffsetAtIndex(aIndex, &container, &offset);
    return GeckoTextMarker(container, offset);
  }

  return GeckoTextMarker();
}

id GeckoTextMarker::CreateAXTextMarker() {
  uintptr_t identifier =
      mContainer.IsProxy()
          ? mContainer.AsProxy()->ID()
          : reinterpret_cast<uintptr_t>(mContainer.AsAccessible()->UniqueID());
  OpaqueGeckoTextMarker opaqueMarker(identifier, mOffset);
  AXTextMarkerRef cf_text_marker = AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&opaqueMarker),
      sizeof(OpaqueGeckoTextMarker));

  return [static_cast<id>(cf_text_marker) autorelease];
}

bool GeckoTextMarker::operator<(const GeckoTextMarker& aPoint) const {
  if (mContainer == aPoint.mContainer) return mOffset < aPoint.mOffset;

  // Build the chain of parents
  AutoTArray<AccessibleOrProxy, 30> parents1, parents2;
  AccessibleOrProxy p1 = mContainer;
  while (!p1.IsNull()) {
    parents1.AppendElement(p1);
    p1 = p1.Parent();
  }

  AccessibleOrProxy p2 = aPoint.mContainer;
  while (!p2.IsNull()) {
    parents2.AppendElement(p2);
    p2 = p2.Parent();
  }

  // An empty chain of parents means one of the containers was null.
  MOZ_ASSERT(parents1.Length() != 0 && parents2.Length() != 0,
             "have empty chain of parents!");

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

bool GeckoTextMarker::IsEditableRoot() {
  uint64_t state = mContainer.IsProxy() ? mContainer.AsProxy()->State()
                                        : mContainer.AsAccessible()->State();
  if ((state & states::EDITABLE) == 0) {
    return false;
  }

  AccessibleOrProxy parent = mContainer.Parent();
  if (parent.IsNull()) {
    // Not sure when this can happen, but it would technically be an editable
    // root.
    return true;
  }

  state = parent.IsProxy() ? parent.AsProxy()->State()
                           : parent.AsAccessible()->State();

  return (state & states::EDITABLE) == 0;
}

bool GeckoTextMarker::Next() {
  if (mContainer.IsProxy()) {
    int32_t nextOffset = 0;
    uint64_t nextContainerID = 0;
    DocAccessibleParent* ipcDoc = mContainer.AsProxy()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendNextClusterAt(
        mContainer.AsProxy()->ID(), mOffset, &nextContainerID, &nextOffset);
    ProxyAccessible* nextContainer = ipcDoc->GetAccessible(nextContainerID);
    bool moved = nextContainer != mContainer.AsProxy() || nextOffset != mOffset;
    mContainer = nextContainer;
    mOffset = nextOffset;
    return moved;
  } else if (auto htWrap = ContainerAsHyperTextWrap()) {
    HyperTextAccessible* nextContainer = nullptr;
    int32_t nextOffset = 0;
    htWrap->NextClusterAt(mOffset, &nextContainer, &nextOffset);
    bool moved = nextContainer != htWrap || nextOffset != mOffset;
    mContainer = nextContainer;
    mOffset = nextOffset;
    return moved;
  }

  return false;
}

bool GeckoTextMarker::Previous() {
  if (mContainer.IsProxy()) {
    int32_t prevOffset = 0;
    uint64_t prevContainerID = 0;
    DocAccessibleParent* ipcDoc = mContainer.AsProxy()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendPreviousClusterAt(
        mContainer.AsProxy()->ID(), mOffset, &prevContainerID, &prevOffset);
    ProxyAccessible* prevContainer = ipcDoc->GetAccessible(prevContainerID);
    bool moved = prevContainer != mContainer.AsProxy() || prevOffset != mOffset;
    mContainer = prevContainer;
    mOffset = prevOffset;
    return moved;
  } else if (auto htWrap = ContainerAsHyperTextWrap()) {
    HyperTextAccessible* prevContainer = nullptr;
    int32_t prevOffset = 0;
    htWrap->PreviousClusterAt(mOffset, &prevContainer, &prevOffset);
    bool moved = prevContainer != htWrap || prevOffset != mOffset;
    mContainer = prevContainer;
    mOffset = prevOffset;
    return moved;
  }

  return false;
}

uint32_t GeckoTextMarker::CharacterCount(const AccessibleOrProxy& aContainer) {
  if (aContainer.IsProxy()) {
    return aContainer.AsProxy()->CharacterCount();
  }

  if (aContainer.AsAccessible()->IsHyperText()) {
    return aContainer.AsAccessible()->AsHyperText()->CharacterCount();
  }

  return 0;
}

GeckoTextMarkerRange GeckoTextMarker::Range(EWhichRange aRangeType) {
  MOZ_ASSERT(!mContainer.IsNull());
  if (mContainer.IsProxy()) {
    int32_t startOffset = 0, endOffset = 0;
    uint64_t startContainerID = 0, endContainerID = 0;
    DocAccessibleParent* ipcDoc = mContainer.AsProxy()->Document();
    bool success = ipcDoc->GetPlatformExtension()->SendRangeAt(
        mContainer.AsProxy()->ID(), mOffset, aRangeType, &startContainerID,
        &startOffset, &endContainerID, &endOffset);
    if (success) {
      return GeckoTextMarkerRange(
          GeckoTextMarker(ipcDoc->GetAccessible(startContainerID), startOffset),
          GeckoTextMarker(ipcDoc->GetAccessible(endContainerID), endOffset));
    }
  } else if (auto htWrap = ContainerAsHyperTextWrap()) {
    int32_t startOffset = 0, endOffset = 0;
    HyperTextAccessible* startContainer = nullptr;
    HyperTextAccessible* endContainer = nullptr;
    htWrap->RangeAt(mOffset, aRangeType, &startContainer, &startOffset,
                    &endContainer, &endOffset);
    return GeckoTextMarkerRange(GeckoTextMarker(startContainer, startOffset),
                                GeckoTextMarker(endContainer, endOffset));
  }

  return GeckoTextMarkerRange(GeckoTextMarker(), GeckoTextMarker());
}

AccessibleOrProxy GeckoTextMarker::Leaf() {
  MOZ_ASSERT(!mContainer.IsNull());
  if (mContainer.IsProxy()) {
    uint64_t leafID = 0;
    DocAccessibleParent* ipcDoc = mContainer.AsProxy()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendLeafAtOffset(
        mContainer.AsProxy()->ID(), mOffset, &leafID);
    return ipcDoc->GetAccessible(leafID);
  } else if (auto htWrap = ContainerAsHyperTextWrap()) {
    return htWrap->LeafAtOffset(mOffset);
  }

  return mContainer;
}

// GeckoTextMarkerRange

GeckoTextMarkerRange::GeckoTextMarkerRange(
    AccessibleOrProxy aDoc, AXTextMarkerRangeRef aTextMarkerRange) {
  if (CFGetTypeID(aTextMarkerRange) != AXTextMarkerRangeGetTypeID()) {
    return;
  }

  AXTextMarkerRef start_marker(
      AXTextMarkerRangeCopyStartMarker(aTextMarkerRange));
  AXTextMarkerRef end_marker(AXTextMarkerRangeCopyEndMarker(aTextMarkerRange));

  mStart = GeckoTextMarker(aDoc, start_marker);
  mEnd = GeckoTextMarker(aDoc, end_marker);

  CFRelease(start_marker);
  CFRelease(end_marker);
}

GeckoTextMarkerRange::GeckoTextMarkerRange(
    const AccessibleOrProxy& aAccessible) {
  mStart = GeckoTextMarker(aAccessible.Parent(), 0);
  mEnd = GeckoTextMarker(aAccessible.Parent(), 0);
  if (mStart.mContainer.IsProxy()) {
    DocAccessibleParent* ipcDoc = mStart.mContainer.AsProxy()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendRangeOfChild(
        mStart.mContainer.AsProxy()->ID(), aAccessible.AsProxy()->ID(),
        &mStart.mOffset, &mEnd.mOffset);
  } else if (auto htWrap = mStart.ContainerAsHyperTextWrap()) {
    htWrap->RangeOfChild(aAccessible.AsAccessible(), &mStart.mOffset,
                         &mEnd.mOffset);
  }
}

id GeckoTextMarkerRange::CreateAXTextMarkerRange() {
  AXTextMarkerRangeRef cf_text_marker_range =
      AXTextMarkerRangeCreate(kCFAllocatorDefault, mStart.CreateAXTextMarker(),
                              mEnd.CreateAXTextMarker());
  return [static_cast<id>(cf_text_marker_range) autorelease];
}

NSString* GeckoTextMarkerRange::Text() const {
  nsAutoString text;
  if (mStart.mContainer.IsProxy() && mEnd.mContainer.IsProxy()) {
    DocAccessibleParent* ipcDoc = mStart.mContainer.AsProxy()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendTextForRange(
        mStart.mContainer.AsProxy()->ID(), mStart.mOffset,
        mEnd.mContainer.AsProxy()->ID(), mEnd.mOffset, &text);
  } else if (auto htWrap = mStart.ContainerAsHyperTextWrap()) {
    htWrap->TextForRange(text, mStart.mOffset, mEnd.ContainerAsHyperTextWrap(),
                         mEnd.mOffset);
  }
  return nsCocoaUtils::ToNSString(text);
}

int32_t GeckoTextMarkerRange::Length() const {
  int32_t length = 0;
  if (mStart.mContainer.IsProxy() && mEnd.mContainer.IsProxy()) {
    DocAccessibleParent* ipcDoc = mStart.mContainer.AsProxy()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendLengthForRange(
        mStart.mContainer.AsProxy()->ID(), mStart.mOffset,
        mEnd.mContainer.AsProxy()->ID(), mEnd.mOffset, &length);
  } else if (auto htWrap = mStart.ContainerAsHyperTextWrap()) {
    length = htWrap->LengthForRange(
        mStart.mOffset, mEnd.ContainerAsHyperTextWrap(), mEnd.mOffset);
  }

  return length;
}

NSValue* GeckoTextMarkerRange::Bounds() const {
  nsIntRect rect;
  if (mStart.mContainer.IsProxy() && mEnd.mContainer.IsProxy()) {
    DocAccessibleParent* ipcDoc = mStart.mContainer.AsProxy()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendBoundsForRange(
        mStart.mContainer.AsProxy()->ID(), mStart.mOffset,
        mEnd.mContainer.AsProxy()->ID(), mEnd.mOffset, &rect);
  } else if (auto htWrap = mStart.ContainerAsHyperTextWrap()) {
    rect = htWrap->BoundsForRange(
        mStart.mOffset, mEnd.ContainerAsHyperTextWrap(), mEnd.mOffset);
  }

  NSScreen* mainView = [[NSScreen screens] objectAtIndex:0];
  CGFloat scaleFactor = nsCocoaUtils::GetBackingScaleFactor(mainView);
  NSRect r =
      NSMakeRect(static_cast<CGFloat>(rect.x) / scaleFactor,
                 [mainView frame].size.height -
                     static_cast<CGFloat>(rect.y + rect.height) / scaleFactor,
                 static_cast<CGFloat>(rect.width) / scaleFactor,
                 static_cast<CGFloat>(rect.height) / scaleFactor);

  return [NSValue valueWithRect:r];
}

void GeckoTextMarkerRange::Select() const {
  if (mStart.mContainer.IsProxy() && mEnd.mContainer.IsProxy()) {
    DocAccessibleParent* ipcDoc = mStart.mContainer.AsProxy()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendSelectRange(
        mStart.mContainer.AsProxy()->ID(), mStart.mOffset,
        mEnd.mContainer.AsProxy()->ID(), mEnd.mOffset);
  } else if (RefPtr<HyperTextAccessibleWrap> htWrap =
                 mStart.ContainerAsHyperTextWrap()) {
    RefPtr<HyperTextAccessibleWrap> end = mEnd.ContainerAsHyperTextWrap();
    htWrap->SelectRange(mStart.mOffset, end, mEnd.mOffset);
  }
}
}
}
