/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TextLeafRange_h__
#define mozilla_a11y_TextLeafRange_h__

#include <stdint.h>

#include "nsDirection.h"
#include "nsIAccessibleText.h"

namespace mozilla::a11y {
class Accessible;

/**
 * Represents a point within accessible text.
 * This is stored as a leaf Accessible and an offset into that Accessible.
 * For an empty Accessible, the offset will always be 0.
 * This will eventually replace TextPoint. Unlike TextPoint, this does not
 * use HyperTextAccessible offsets.
 */
class TextLeafPoint final {
 public:
  TextLeafPoint(Accessible* aAcc, int32_t aOffset);

  /**
   * Constructs an invalid TextPoint (mAcc is null).
   * A TextLeafPoint in this state will evaluate to false.
   * mAcc can be set later. Alternatively, this can be used to indicate an error
   * (e.g. if a requested point couldn't be found).
   */
  TextLeafPoint() : mAcc(nullptr), mOffset(0) {}

  Accessible* mAcc;
  int32_t mOffset;

  bool operator==(const TextLeafPoint& aPoint) const {
    return mAcc == aPoint.mAcc && mOffset == aPoint.mOffset;
  }

  bool operator!=(const TextLeafPoint& aPoint) const {
    return !(*this == aPoint);
  }

  bool operator<(const TextLeafPoint& aPoint) const;

  /**
   * A valid TextLeafPoint evaluates to true. An invalid TextLeafPoint
   * evaluates to false.
   */
  explicit operator bool() const { return !!mAcc; }

  /**
   * Find a boundary (word start, line start, etc.) in a specific direction.
   * If no boundary is found, the start/end of the document is returned
   * (depending on the direction).
   * If aIncludeorigin is true and this is at a boundary, this will be
   * returned unchanged.
   */
  TextLeafPoint FindBoundary(AccessibleTextBoundary aBoundaryType,
                             nsDirection aDirection,
                             bool aIncludeOrigin = false) const;

  /**
   * These two functions find a line start boundary within the same
   * LocalAccessible as this. That is, they do not cross Accessibles. If no
   * boundary is found, an invalid TextLeafPoint is returned.
   * These are used by FindBoundary. Most callers will want FindBoundary
   * instead.
   */
  TextLeafPoint FindPrevLineStartSameLocalAcc(bool aIncludeOrigin) const;
  TextLeafPoint FindNextLineStartSameLocalAcc(bool aIncludeOrigin) const;

  /**
   * These two functions find a word start boundary within the same
   * Accessible as this. That is, they do not cross Accessibles. If no
   * boundary is found, an invalid TextLeafPoint is returned.
   * These are used by FindBoundary. Most callers will want FindBoundary
   * instead.
   */
  TextLeafPoint FindPrevWordStartSameAcc(bool aIncludeOrigin) const;
  TextLeafPoint FindNextWordStartSameAcc(bool aIncludeOrigin) const;

 private:
  bool IsEmptyLastLine() const;

  TextLeafPoint FindLineStartSameRemoteAcc(nsDirection aDirection,
                                           bool aIncludeOrigin) const;

  /**
   * Helper which just calls the appropriate function based on whether mAcc
   *is local or remote.
   */
  TextLeafPoint FindLineStartSameAcc(nsDirection aDirection,
                                     bool aIncludeOrigin) const;
};

/**
 * Represents a range of accessible text.
 * This will eventually replace TextRange.
 */
class TextLeafRange final {
 public:
  TextLeafRange(const TextLeafPoint& aStart, const TextLeafPoint& aEnd)
      : mStart(aStart), mEnd(aEnd) {}
  explicit TextLeafRange(const TextLeafPoint& aStart)
      : mStart(aStart), mEnd(aStart) {}

  TextLeafPoint Start() { return mStart; }
  void SetStart(const TextLeafPoint& aStart) { mStart = aStart; }
  TextLeafPoint End() { return mEnd; }
  void SetEnd(const TextLeafPoint& aEnd) { mEnd = aEnd; }

 private:
  TextLeafPoint mStart;
  TextLeafPoint mEnd;
};

}  // namespace mozilla::a11y

#endif
