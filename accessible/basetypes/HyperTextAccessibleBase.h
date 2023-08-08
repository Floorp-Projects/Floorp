/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _HyperTextAccessibleBase_H_
#define _HyperTextAccessibleBase_H_

#include "AccAttributes.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleTypes.h"

namespace mozilla::a11y {
class Accessible;
class TextLeafPoint;
class TextRange;

// This character marks where in the text returned via Text interface,
// that embedded object characters exist
const char16_t kEmbeddedObjectChar = 0xfffc;
const char16_t kImaginaryEmbeddedObjectChar = ' ';
const char16_t kForcedNewLineChar = '\n';

/**
 * An index type. Assert if out of range value was attempted to be used.
 */
class index_t {
 public:
  MOZ_IMPLICIT index_t(int32_t aVal) : mVal(aVal) {}

  operator uint32_t() const {
    MOZ_ASSERT(mVal >= 0, "Attempt to use wrong index!");
    return mVal;
  }

  bool IsValid() const { return mVal >= 0; }

 private:
  int32_t mVal;
};

class HyperTextAccessibleBase {
 public:
  /**
   * Return child accessible at the given text offset.
   *
   * @param  aOffset  [in] the given text offset
   */
  virtual int32_t GetChildIndexAtOffset(uint32_t aOffset) const;

  /**
   * Return child accessible at the given text offset.
   *
   * @param  aOffset  [in] the given text offset
   */
  virtual Accessible* GetChildAtOffset(uint32_t aOffset) const;

  /**
   * Return text offset of the given child accessible within hypertext
   * accessible.
   *
   * @param  aChild           [in] accessible child to get text offset for
   * @param  aInvalidateAfter [in, optional] indicates whether to invalidate
   *                           cached offsets for subsequent siblings of the
   *                           child.
   */
  int32_t GetChildOffset(const Accessible* aChild,
                         bool aInvalidateAfter = false) const;

  /**
   * Return text offset for the child accessible index.
   */
  virtual int32_t GetChildOffset(uint32_t aChildIndex,
                                 bool aInvalidateAfter = false) const;

  /**
   * Return character count within the hypertext accessible.
   */
  uint32_t CharacterCount() const;

  /**
   * Get/set caret offset, if no caret then -1.
   */
  virtual int32_t CaretOffset() const;
  virtual void SetCaretOffset(int32_t aOffset) = 0;

  /**
   * Provide the line number for the caret.
   * @return 1-based index for the line number with the caret
   */
  virtual int32_t CaretLineNumber();

  /**
   * Transform magic offset into text offset.
   */
  index_t ConvertMagicOffset(int32_t aOffset) const;

  /**
   * Return text between given offsets.
   */
  void TextSubstring(int32_t aStartOffset, int32_t aEndOffset,
                     nsAString& aText) const;

  /**
   * Get a character at the given offset (don't support magic offsets).
   */
  bool CharAt(int32_t aOffset, nsAString& aChar,
              int32_t* aStartOffset = nullptr, int32_t* aEndOffset = nullptr);

  char16_t CharAt(int32_t aOffset) {
    nsAutoString charAtOffset;
    CharAt(aOffset, charAtOffset);
    return charAtOffset.CharAt(0);
  }

  /**
   * Return a rect (in dev pixels) for character at given offset relative
   * given coordinate system.
   */
  LayoutDeviceIntRect CharBounds(int32_t aOffset, uint32_t aCoordType);

  /**
   * Return a rect (in dev pixels) of the given text range relative given
   * coordinate system.
   */
  LayoutDeviceIntRect TextBounds(
      int32_t aStartOffset, int32_t aEndOffset,
      uint32_t aCoordType =
          nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE);

  /**
   * Return the offset of the char that contains the given coordinates.
   */
  virtual int32_t OffsetAtPoint(int32_t aX, int32_t aY, uint32_t aCoordType);

  /**
   * Get a TextLeafPoint for a given offset in this HyperTextAccessible.
   * If the offset points to an embedded object and aDescendToEnd is true,
   * the point right at the end of this subtree will be returned instead of the
   * start.
   */
  TextLeafPoint ToTextLeafPoint(int32_t aOffset, bool aDescendToEnd = false);

  /**
   * Return text before/at/after the given offset corresponding to
   * the boundary type.
   */
  void TextBeforeOffset(int32_t aOffset, AccessibleTextBoundary aBoundaryType,
                        int32_t* aStartOffset, int32_t* aEndOffset,
                        nsAString& aText);
  void TextAtOffset(int32_t aOffset, AccessibleTextBoundary aBoundaryType,
                    int32_t* aStartOffset, int32_t* aEndOffset,
                    nsAString& aText);
  void TextAfterOffset(int32_t aOffset, AccessibleTextBoundary aBoundaryType,
                       int32_t* aStartOffset, int32_t* aEndOffset,
                       nsAString& aText);

  /**
   * Return true if the given offset/range is valid.
   */
  bool IsValidOffset(int32_t aOffset);
  bool IsValidRange(int32_t aStartOffset, int32_t aEndOffset);

  /**
   * Return link count within this hypertext accessible.
   */
  uint32_t LinkCount();

  /**
   * Return link accessible at the given index.
   */
  Accessible* LinkAt(uint32_t aIndex);

  /**
   * Return index for the given link accessible.
   */
  int32_t LinkIndexOf(Accessible* aLink);

  /**
   * Return link accessible at the given text offset.
   */
  int32_t LinkIndexAtOffset(uint32_t aOffset) {
    Accessible* child = GetChildAtOffset(aOffset);
    return child ? LinkIndexOf(child) : -1;
  }

  /**
   * Transform the given a11y point into an offset relative to this hypertext.
   * Returns {success, offset}, where success is true if successful.
   * If unsuccessful, the returned offset will be CharacterCount() if
   * aIsEndOffset is true, 0 otherwise. This means most callers can ignore the
   * success return value.
   */
  std::pair<bool, int32_t> TransformOffset(Accessible* aDescendant,
                                           int32_t aOffset,
                                           bool aIsEndOffset) const;

  /**
   * Return text attributes for the given text range.
   */
  already_AddRefed<AccAttributes> TextAttributes(bool aIncludeDefAttrs,
                                                 int32_t aOffset,
                                                 int32_t* aStartOffset,
                                                 int32_t* aEndOffset);

  /**
   * Return text attributes applied to the accessible.
   */
  virtual already_AddRefed<AccAttributes> DefaultTextAttributes() = 0;

  /**
   * Return an array of disjoint ranges for selected text within the text
   * control or the document this accessible belongs to.
   */
  virtual void SelectionRanges(nsTArray<TextRange>* aRanges) const = 0;

  /**
   * Return text selection ranges cropped to this Accessible (rather than for
   * the entire text control or document). This also excludes collapsed ranges.
   */
  void CroppedSelectionRanges(nsTArray<TextRange>& aRanges) const;

  /**
   * Return selected regions count within the accessible.
   */
  virtual int32_t SelectionCount();

  /**
   * Return the start and end offset of the specified selection.
   */
  virtual bool SelectionBoundsAt(int32_t aSelectionNum, int32_t* aStartOffset,
                                 int32_t* aEndOffset);

  /**
   * Changes the start and end offset of the specified selection.
   * @return true if succeeded
   */
  // TODO: annotate this with `MOZ_CAN_RUN_SCRIPT` instead.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual bool SetSelectionBoundsAt(
      int32_t aSelectionNum, int32_t aStartOffset, int32_t aEndOffset);

  /**
   * Adds a selection bounded by the specified offsets.
   * @return true if succeeded
   */
  bool AddToSelection(int32_t aStartOffset, int32_t aEndOffset) {
    return SetSelectionBoundsAt(-1, aStartOffset, aEndOffset);
  }

  /**
   * Removes the specified selection.
   * @return true if succeeded
   */
  // TODO: annotate this with `MOZ_CAN_RUN_SCRIPT` instead.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual bool RemoveFromSelection(
      int32_t aSelectionNum) = 0;

  /**
   * Scroll the given text range into view.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void ScrollSubstringTo(int32_t aStartOffset,
                                                     int32_t aEndOffset,
                                                     uint32_t aScrollType);

  /**
   * Scroll the given text range to the given point.
   */
  virtual void ScrollSubstringToPoint(int32_t aStartOffset, int32_t aEndOffset,
                                      uint32_t aCoordinateType, int32_t aX,
                                      int32_t aY) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // EditableTextAccessible

  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual void ReplaceText(
      const nsAString& aText) = 0;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual void InsertText(const nsAString& aText,
                                                      int32_t aPosition) = 0;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual void CopyText(int32_t aStartPos,
                                                    int32_t aEndPos) = 0;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual void CutText(int32_t aStartPos,
                                                   int32_t aEndPos) = 0;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual void DeleteText(int32_t aStartPos,
                                                      int32_t aEndPos) = 0;
  MOZ_CAN_RUN_SCRIPT virtual void PasteText(int32_t aPosition) = 0;

 protected:
  virtual const Accessible* Acc() const = 0;
  Accessible* Acc() {
    const Accessible* acc =
        const_cast<const HyperTextAccessibleBase*>(this)->Acc();
    return const_cast<Accessible*>(acc);
  }

  /**
   * Get the cached map of child indexes to HyperText offsets.
   * This is an array which contains the exclusive end offset for each child.
   * That is, the start offset for child c is array index c - 1.
   */
  virtual nsTArray<int32_t>& GetCachedHyperTextOffsets() = 0;

 private:
  /**
   * Helper method for TextBefore/At/AfterOffset.
   * If BOUNDARY_LINE_END was requested and the origin is itself a line end
   * boundary, we must use the line which ends at the origin. We must do
   * similarly for BOUNDARY_WORD_END. This method adjusts the origin
   * accordingly.
   */
  void AdjustOriginIfEndBoundary(TextLeafPoint& aOrigin,
                                 AccessibleTextBoundary aBoundaryType,
                                 bool aAtOffset = false) const;
};

}  // namespace mozilla::a11y

#endif
