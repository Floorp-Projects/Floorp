/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "LegacyTextMarker.h"
#import "MacUtils.h"

#include "DocAccessible.h"
#include "DocAccessibleParent.h"
#include "AccAttributes.h"
#include "nsCocoaUtils.h"
#include "MOXAccessibleBase.h"
#include "mozAccessible.h"

#include "mozilla/a11y/DocAccessiblePlatformExtParent.h"

namespace mozilla {
namespace a11y {

LegacyTextMarker LegacyTextMarker::MarkerFromIndex(Accessible* aRoot,
                                                   int32_t aIndex) {
  if (aRoot->IsRemote()) {
    int32_t offset = 0;
    uint64_t containerID = 0;
    DocAccessibleParent* ipcDoc = aRoot->AsRemote()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendOffsetAtIndex(
        aRoot->AsRemote()->ID(), aIndex, &containerID, &offset);
    RemoteAccessible* container = ipcDoc->GetAccessible(containerID);
    return LegacyTextMarker(container, offset);
  } else if (auto htWrap = static_cast<HyperTextAccessibleWrap*>(
                 aRoot->AsLocal()->AsHyperText())) {
    int32_t offset = 0;
    HyperTextAccessible* container = nullptr;
    htWrap->OffsetAtIndex(aIndex, &container, &offset);
    return LegacyTextMarker(container, offset);
  }

  return LegacyTextMarker();
}

bool LegacyTextMarker::operator<(const LegacyTextMarker& aPoint) const {
  if (mContainer == aPoint.mContainer) return mOffset < aPoint.mOffset;

  // Build the chain of parents
  AutoTArray<Accessible*, 30> parents1, parents2;
  Accessible* p1 = mContainer;
  while (p1) {
    parents1.AppendElement(p1);
    p1 = p1->Parent();
  }

  Accessible* p2 = aPoint.mContainer;
  while (p2) {
    parents2.AppendElement(p2);
    p2 = p2->Parent();
  }

  // An empty chain of parents means one of the containers was null.
  MOZ_ASSERT(parents1.Length() != 0 && parents2.Length() != 0,
             "have empty chain of parents!");

  // Find where the parent chain differs
  uint32_t pos1 = parents1.Length(), pos2 = parents2.Length();
  for (uint32_t len = std::min(pos1, pos2); len > 0; --len) {
    Accessible* child1 = parents1.ElementAt(--pos1);
    Accessible* child2 = parents2.ElementAt(--pos2);
    if (child1 != child2) {
      return child1->IndexInParent() < child2->IndexInParent();
    }
  }

  if (pos1 != 0) {
    // If parents1 is a superset of parents2 then mContainer is a
    // descendant of aPoint.mContainer. The next element down in parents1
    // is mContainer's ancestor that is the child of aPoint.mContainer.
    // We compare its end offset in aPoint.mContainer with aPoint.mOffset.
    Accessible* child = parents1.ElementAt(pos1 - 1);
    MOZ_ASSERT(child->Parent() == aPoint.mContainer);
    uint32_t endOffset = child->EndOffset();
    return endOffset < static_cast<uint32_t>(aPoint.mOffset);
  }

  if (pos2 != 0) {
    // If parents2 is a superset of parents1 then aPoint.mContainer is a
    // descendant of mContainer. The next element down in parents2
    // is aPoint.mContainer's ancestor that is the child of mContainer.
    // We compare its start offset in mContainer with mOffset.
    Accessible* child = parents2.ElementAt(pos2 - 1);
    MOZ_ASSERT(child->Parent() == mContainer);
    uint32_t startOffset = child->StartOffset();
    return static_cast<uint32_t>(mOffset) <= startOffset;
  }

  MOZ_ASSERT_UNREACHABLE("Broken tree?!");
  return false;
}

bool LegacyTextMarker::IsEditableRoot() {
  uint64_t state = mContainer->IsRemote() ? mContainer->AsRemote()->State()
                                          : mContainer->AsLocal()->State();
  if ((state & states::EDITABLE) == 0) {
    return false;
  }

  Accessible* parent = mContainer->Parent();
  if (!parent) {
    // Not sure when this can happen, but it would technically be an editable
    // root.
    return true;
  }

  state = parent->IsRemote() ? parent->AsRemote()->State()
                             : parent->AsLocal()->State();

  return (state & states::EDITABLE) == 0;
}

bool LegacyTextMarker::Next() {
  if (mContainer->IsRemote()) {
    int32_t nextOffset = 0;
    uint64_t nextContainerID = 0;
    DocAccessibleParent* ipcDoc = mContainer->AsRemote()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendNextClusterAt(
        mContainer->AsRemote()->ID(), mOffset, &nextContainerID, &nextOffset);
    RemoteAccessible* nextContainer = ipcDoc->GetAccessible(nextContainerID);
    bool moved =
        nextContainer != mContainer->AsRemote() || nextOffset != mOffset;
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

bool LegacyTextMarker::Previous() {
  if (mContainer->IsRemote()) {
    int32_t prevOffset = 0;
    uint64_t prevContainerID = 0;
    DocAccessibleParent* ipcDoc = mContainer->AsRemote()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendPreviousClusterAt(
        mContainer->AsRemote()->ID(), mOffset, &prevContainerID, &prevOffset);
    RemoteAccessible* prevContainer = ipcDoc->GetAccessible(prevContainerID);
    bool moved =
        prevContainer != mContainer->AsRemote() || prevOffset != mOffset;
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

static uint32_t CharacterCount(Accessible* aContainer) {
  if (aContainer->IsRemote()) {
    return aContainer->AsRemote()->CharacterCount();
  }

  if (aContainer->AsLocal()->IsHyperText()) {
    return aContainer->AsLocal()->AsHyperText()->CharacterCount();
  }

  return 0;
}

LegacyTextMarkerRange LegacyTextMarker::Range(EWhichRange aRangeType) const {
  MOZ_ASSERT(mContainer);
  if (mContainer->IsRemote()) {
    int32_t startOffset = 0, endOffset = 0;
    uint64_t startContainerID = 0, endContainerID = 0;
    DocAccessibleParent* ipcDoc = mContainer->AsRemote()->Document();
    bool success = ipcDoc->GetPlatformExtension()->SendRangeAt(
        mContainer->AsRemote()->ID(), mOffset, aRangeType, &startContainerID,
        &startOffset, &endContainerID, &endOffset);
    if (success) {
      return LegacyTextMarkerRange(
          LegacyTextMarker(ipcDoc->GetAccessible(startContainerID),
                           startOffset),
          LegacyTextMarker(ipcDoc->GetAccessible(endContainerID), endOffset));
    }
  } else if (auto htWrap = ContainerAsHyperTextWrap()) {
    int32_t startOffset = 0, endOffset = 0;
    HyperTextAccessible* startContainer = nullptr;
    HyperTextAccessible* endContainer = nullptr;
    htWrap->RangeAt(mOffset, aRangeType, &startContainer, &startOffset,
                    &endContainer, &endOffset);
    return LegacyTextMarkerRange(LegacyTextMarker(startContainer, startOffset),
                                 LegacyTextMarker(endContainer, endOffset));
  }

  return LegacyTextMarkerRange(LegacyTextMarker(), LegacyTextMarker());
}

Accessible* LegacyTextMarker::Leaf() {
  MOZ_ASSERT(mContainer);
  if (mContainer->IsRemote()) {
    uint64_t leafID = 0;
    DocAccessibleParent* ipcDoc = mContainer->AsRemote()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendLeafAtOffset(
        mContainer->AsRemote()->ID(), mOffset, &leafID);
    return ipcDoc->GetAccessible(leafID);
  } else if (auto htWrap = ContainerAsHyperTextWrap()) {
    return htWrap->LeafAtOffset(mOffset);
  }

  return mContainer;
}

// LegacyTextMarkerRange

LegacyTextMarkerRange::LegacyTextMarkerRange(Accessible* aAccessible) {
  if (aAccessible->IsHyperText()) {
    // The accessible is a hypertext. Initialize range to its inner text range.
    mStart = LegacyTextMarker(aAccessible, 0);
    mEnd = LegacyTextMarker(aAccessible, (CharacterCount(aAccessible)));
  } else {
    // The accessible is not a hypertext (maybe a text leaf?). Initialize range
    // to its offsets in its container.
    mStart = LegacyTextMarker(aAccessible->Parent(), 0);
    mEnd = LegacyTextMarker(aAccessible->Parent(), 0);
    if (mStart.mContainer->IsRemote()) {
      DocAccessibleParent* ipcDoc = mStart.mContainer->AsRemote()->Document();
      Unused << ipcDoc->GetPlatformExtension()->SendRangeOfChild(
          mStart.mContainer->AsRemote()->ID(), aAccessible->AsRemote()->ID(),
          &mStart.mOffset, &mEnd.mOffset);
    } else if (auto htWrap = mStart.ContainerAsHyperTextWrap()) {
      htWrap->RangeOfChild(aAccessible->AsLocal(), &mStart.mOffset,
                           &mEnd.mOffset);
    }
  }
}

NSString* LegacyTextMarkerRange::Text() const {
  nsAutoString text;
  if (mStart.mContainer->IsRemote() && mEnd.mContainer->IsRemote()) {
    DocAccessibleParent* ipcDoc = mStart.mContainer->AsRemote()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendTextForRange(
        mStart.mContainer->AsRemote()->ID(), mStart.mOffset,
        mEnd.mContainer->AsRemote()->ID(), mEnd.mOffset, &text);
  } else if (auto htWrap = mStart.ContainerAsHyperTextWrap()) {
    htWrap->TextForRange(text, mStart.mOffset, mEnd.ContainerAsHyperTextWrap(),
                         mEnd.mOffset);
  }
  return nsCocoaUtils::ToNSString(text);
}

NSAttributedString* LegacyTextMarkerRange::AttributedText() const {
  NSMutableAttributedString* str =
      [[[NSMutableAttributedString alloc] init] autorelease];

  if (mStart.mContainer->IsRemote() && mEnd.mContainer->IsRemote()) {
    nsTArray<TextAttributesRun> textAttributesRuns;
    DocAccessibleParent* ipcDoc = mStart.mContainer->AsRemote()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendAttributedTextForRange(
        mStart.mContainer->AsRemote()->ID(), mStart.mOffset,
        mEnd.mContainer->AsRemote()->ID(), mEnd.mOffset, &textAttributesRuns);

    for (size_t i = 0; i < textAttributesRuns.Length(); i++) {
      AccAttributes* attributes =
          textAttributesRuns.ElementAt(i).TextAttributes();
      RemoteAccessible* container =
          ipcDoc->GetAccessible(textAttributesRuns.ElementAt(i).ContainerID());

      NSAttributedString* substr = [[[NSAttributedString alloc]
          initWithString:nsCocoaUtils::ToNSString(
                             textAttributesRuns.ElementAt(i).Text())
              attributes:utils::StringAttributesFromAccAttributes(
                             attributes, container)] autorelease];

      [str appendAttributedString:substr];
    }
  } else if (auto htWrap = mStart.ContainerAsHyperTextWrap()) {
    nsTArray<nsString> texts;
    nsTArray<LocalAccessible*> containers;
    nsTArray<RefPtr<AccAttributes>> props;

    htWrap->AttributedTextForRange(texts, props, containers, mStart.mOffset,
                                   mEnd.ContainerAsHyperTextWrap(),
                                   mEnd.mOffset);

    MOZ_ASSERT(texts.Length() == props.Length() &&
               texts.Length() == containers.Length());

    for (size_t i = 0; i < texts.Length(); i++) {
      NSAttributedString* substr = [[[NSAttributedString alloc]
          initWithString:nsCocoaUtils::ToNSString(texts.ElementAt(i))
              attributes:utils::StringAttributesFromAccAttributes(
                             props.ElementAt(i), containers.ElementAt(i))]
          autorelease];
      [str appendAttributedString:substr];
    }
  }

  return str;
}

int32_t LegacyTextMarkerRange::Length() const {
  int32_t length = 0;
  if (mStart.mContainer->IsRemote() && mEnd.mContainer->IsRemote()) {
    DocAccessibleParent* ipcDoc = mStart.mContainer->AsRemote()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendLengthForRange(
        mStart.mContainer->AsRemote()->ID(), mStart.mOffset,
        mEnd.mContainer->AsRemote()->ID(), mEnd.mOffset, &length);
  } else if (auto htWrap = mStart.ContainerAsHyperTextWrap()) {
    length = htWrap->LengthForRange(
        mStart.mOffset, mEnd.ContainerAsHyperTextWrap(), mEnd.mOffset);
  }

  return length;
}

NSValue* LegacyTextMarkerRange::Bounds() const {
  LayoutDeviceIntRect rect;
  if (mStart.mContainer->IsRemote() && mEnd.mContainer->IsRemote()) {
    DocAccessibleParent* ipcDoc = mStart.mContainer->AsRemote()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendBoundsForRange(
        mStart.mContainer->AsRemote()->ID(), mStart.mOffset,
        mEnd.mContainer->AsRemote()->ID(), mEnd.mOffset, &rect);
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

void LegacyTextMarkerRange::Select() const {
  if (mStart.mContainer->IsRemote() && mEnd.mContainer->IsRemote()) {
    DocAccessibleParent* ipcDoc = mStart.mContainer->AsRemote()->Document();
    Unused << ipcDoc->GetPlatformExtension()->SendSelectRange(
        mStart.mContainer->AsRemote()->ID(), mStart.mOffset,
        mEnd.mContainer->AsRemote()->ID(), mEnd.mOffset);
  } else if (RefPtr<HyperTextAccessibleWrap> htWrap =
                 mStart.ContainerAsHyperTextWrap()) {
    RefPtr<HyperTextAccessibleWrap> end = mEnd.ContainerAsHyperTextWrap();
    htWrap->SelectRange(mStart.mOffset, end, mEnd.mOffset);
  }
}

bool LegacyTextMarkerRange::Crop(Accessible* aContainer) {
  LegacyTextMarkerRange containerRange(aContainer);

  if (mEnd < containerRange.mStart || containerRange.mEnd < mStart) {
    // The range ends before the container, or starts after it.
    return false;
  }

  if (mStart < containerRange.mStart) {
    // If range start is before container start, adjust range start to
    // start of container.
    mStart = containerRange.mStart;
  }

  if (containerRange.mEnd < mEnd) {
    // If range end is after container end, adjust range end to end of
    // container.
    mEnd = containerRange.mEnd;
  }

  return true;
}
}  // namespace a11y
}  // namespace mozilla
