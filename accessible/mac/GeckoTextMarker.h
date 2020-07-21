/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GeckoTextMarker_H_
#define _GeckoTextMarker_H_

typedef CFTypeRef AXTextMarkerRef;
typedef CFTypeRef AXTextMarkerRangeRef;

namespace mozilla {
namespace a11y {

class AccessibleOrProxy;
class GeckoTextMarkerRange;

class GeckoTextMarker final {
 public:
  GeckoTextMarker(const AccessibleOrProxy& aContainer, int32_t aOffset)
      : mContainer(aContainer), mOffset(aOffset) {}

  GeckoTextMarker(const GeckoTextMarker& aPoint)
      : mContainer(aPoint.mContainer), mOffset(aPoint.mOffset) {}

  GeckoTextMarker(AccessibleOrProxy aDoc, AXTextMarkerRef aTextMarker);

  GeckoTextMarker() : mContainer(nullptr), mOffset(0) {}

  id CreateAXTextMarker();

  // Mutate marker so that its offset references an actual character
  // and not an embedded link. Or, if the offset is at the end of the
  // container, mutate the marker to the end offset of an ancestor
  // container that has following non-link text.
  void NormalizeNext();

  // Mutate the marker so that its offset is preceded by a non-link
  // offset, If the marker's offset is at the begining of a container,
  // mutate the marker to point to the top-most link offset in an ancestor.
  void NormalizePrevious();

  // Return true if offset is at the end of the container.
  bool AtEnd() { return static_cast<uint32_t>(mOffset) >= CharacterCount(mContainer); }

  // Return a word range for the given offset.
  GeckoTextMarkerRange WordRange();

  bool operator<(const GeckoTextMarker& aPoint) const;

  AccessibleOrProxy mContainer;
  int32_t mOffset;

 private:
  uint32_t CharacterCount(const AccessibleOrProxy& aContainer);

  bool IsEditableRoot();
};

class GeckoTextMarkerRange final {
 public:
  GeckoTextMarkerRange(const GeckoTextMarker& aStart, const GeckoTextMarker& aEnd)
      : mStart(aStart), mEnd(aEnd) {}

  GeckoTextMarkerRange(AccessibleOrProxy aDoc, AXTextMarkerRangeRef aTextMarkerRange);

  id CreateAXTextMarkerRange();

  bool IsValid() const { return !mStart.mContainer.IsNull() && !mEnd.mContainer.IsNull(); };

  /**
   * Return text enclosed by the range.
   */
  NSString* Text() const;

  GeckoTextMarker mStart;
  GeckoTextMarker mEnd;

 private:
  int32_t StartOffset(const AccessibleOrProxy& aChild) const;

  int32_t EndOffset(const AccessibleOrProxy& aChild) const;

  int32_t LinkCount(const AccessibleOrProxy& aContainer) const;

  AccessibleOrProxy LinkAt(const AccessibleOrProxy& aContainer, uint32_t aIndex) const;

  void AppendTextTo(const AccessibleOrProxy& aContainer, nsAString& aText, uint32_t aStartOffset,
                    uint32_t aEndOffset) const;

  /**
   * Text() method helper.
   * @param  aText            [in,out] calculated text
   * @param  aCurrent         [in] currently traversed node
   * @param  aStartIntlOffset [in] start offset if current node is a text node
   * @return                   true if calculation is not finished yet
   */
  bool TextInternal(nsAString& aText, AccessibleOrProxy aCurrent, int32_t aStartIntlOffset) const;
};

}  // namespace a11y
}  // namespace mozilla

#endif
