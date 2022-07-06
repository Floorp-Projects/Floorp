/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TextLeafRange_h__
#define mozilla_a11y_TextLeafRange_h__

#include <stdint.h>

#include "AccAttributes.h"
#include "nsDirection.h"
#include "nsIAccessibleText.h"

class nsRange;

namespace mozilla {
namespace dom {
class Document;
}

namespace a11y {
class Accessible;
class LocalAccessible;

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

  /**
   * Construct a TextLeafPoint representing the caret.
   * The actual offset used for the caret differs depending on whether the
   * caret is at the end of a line and the query being made. Thus, mOffset on
   * the returned TextLeafPoint is not a valid offset.
   */
  static TextLeafPoint GetCaret(Accessible* aAcc) {
    return TextLeafPoint(aAcc, nsIAccessibleText::TEXT_OFFSET_CARET);
  }

  Accessible* mAcc;
  int32_t mOffset;

  bool operator==(const TextLeafPoint& aPoint) const {
    return mAcc == aPoint.mAcc && mOffset == aPoint.mOffset;
  }

  bool operator!=(const TextLeafPoint& aPoint) const {
    return !(*this == aPoint);
  }

  bool operator<(const TextLeafPoint& aPoint) const;

  bool operator<=(const TextLeafPoint& aPoint) const;

  /**
   * A valid TextLeafPoint evaluates to true. An invalid TextLeafPoint
   * evaluates to false.
   */
  explicit operator bool() const { return !!mAcc; }

  bool IsCaret() const {
    return mOffset == nsIAccessibleText::TEXT_OFFSET_CARET;
  }

  bool IsCaretAtEndOfLine() const;

  /**
   * Get a TextLeafPoint at the actual caret offset.
   * This should only be called on a TextLeafPoint created with GetCaret.
   * If aAdjustAtEndOfLine is true, the point will be adjusted if the caret is
   * at the end of a line so that word and line boundaries can be calculated
   * correctly.
   */
  TextLeafPoint ActualizeCaret(bool aAdjustAtEndOfLine = true) const;

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

  /**
   * Get the text attributes at this point.
   * If aIncludeDefaults is true, default attributes on the HyperTextAccessible
   * will be included.
   */
  already_AddRefed<AccAttributes> GetTextAttributes(
      bool aIncludeDefaults = true) const;

  /**
   * Get Get the text attributes at this point in a LocalAccessible.
   * This is used by GetTextAttributes. Most callers will want GetTextAttributes
   * instead.
   */
  already_AddRefed<AccAttributes> GetTextAttributesLocalAcc(
      bool aIncludeDefaults = true) const;

  /**
   * Get the offsets of all spelling errors in a given LocalAccessible. This
   * should only be used when pushing the cache. Most callers will want
   * FindTextAttrsStart instead.
   */
  static nsTArray<int32_t> GetSpellingErrorOffsets(LocalAccessible* aAcc);

  /**
   * Queue a cache update for a spelling error in a given DOM range.
   */
  static void UpdateCachedSpellingError(dom::Document* aDocument,
                                        const nsRange& aRange);

  /**
   * Find the start of a run of text attributes in a specific direction.
   * A text attributes run is a span of text where the attributes are the same.
   * If no boundary is found, the start/end of the container is returned
   * (depending on the direction).
   * If aIncludeorigin is true and this is at a boundary, this will be
   * returned unchanged.
   */
  TextLeafPoint FindTextAttrsStart(nsDirection aDirection,
                                   bool aIncludeOrigin = false) const;

  /**
   * Returns a rect (in dev pixels) describing position and size of
   * the character at mOffset in mAcc. This rect is screen-relative.
   * This function only works on remote accessibles, and assumes caching
   * is enabled.
   */
  LayoutDeviceIntRect CharBounds();

  /**
   * Returns true if the given point (in screen coords) is contained
   * in the char bounds of the current TextLeafPoint. Returns false otherwise.
   * If the current point is an empty container, we use the acc's bounds instead
   * of char bounds. Because this depends on CharBounds, this function only
   * works on remote accessibles, and assumes caching is enabled.
   */
  bool ContainsPoint(int32_t aX, int32_t aY);

  bool IsLineFeedChar() const { return GetChar() == '\n'; }

  bool IsSpace() const;

 private:
  bool IsEmptyLastLine() const;

  char16_t GetChar() const;

  TextLeafPoint FindLineStartSameRemoteAcc(nsDirection aDirection,
                                           bool aIncludeOrigin) const;

  /**
   * Helper which just calls the appropriate function based on whether mAcc
   *is local or remote.
   */
  TextLeafPoint FindLineStartSameAcc(nsDirection aDirection,
                                     bool aIncludeOrigin) const;

  TextLeafPoint FindLineEnd(nsDirection aDirection, bool aIncludeOrigin) const;
  TextLeafPoint FindWordEnd(nsDirection aDirection, bool aIncludeOrigin) const;

  TextLeafPoint FindParagraphSameAcc(nsDirection aDirection,
                                     bool aIncludeOrigin) const;

  bool IsInSpellingError() const;

  /**
   * Find a spelling error boundary in the same Accessible. This function
   * searches for either start or end points, since either means a change in
   * text attributes.
   */
  TextLeafPoint FindSpellingErrorSameAcc(nsDirection aDirection,
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

}  // namespace a11y
}  // namespace mozilla

#endif
