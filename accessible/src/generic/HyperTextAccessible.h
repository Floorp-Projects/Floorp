/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_HyperTextAccessible_h__
#define mozilla_a11y_HyperTextAccessible_h__

#include "AccessibleWrap.h"
#include "nsIAccessibleTypes.h"
#include "xpcAccessibleHyperText.h"

#include "nsFrameSelection.h"
#include "nsISelectionController.h"

namespace mozilla {
namespace a11y {

struct DOMPoint {
  nsINode* node;
  int32_t idx;
};

enum EGetTextType { eGetBefore=-1, eGetAt=0, eGetAfter=1 };

// This character marks where in the text returned via nsIAccessibleText(),
// that embedded object characters exist
const PRUnichar kEmbeddedObjectChar = 0xfffc;
const PRUnichar kImaginaryEmbeddedObjectChar = ' ';
const PRUnichar kForcedNewLineChar = '\n';

/**
  * Special Accessible that knows how contain both text and embedded objects
  */
class HyperTextAccessible : public AccessibleWrap,
                            public xpcAccessibleHyperText
{
public:
  HyperTextAccessible(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~HyperTextAccessible() { }

  NS_DECL_ISUPPORTS_INHERITED

  // Accessible
  virtual int32_t GetLevelInternal();
  virtual already_AddRefed<nsIPersistentProperties> NativeAttributes() MOZ_OVERRIDE;
  virtual mozilla::a11y::role NativeRole();
  virtual uint64_t NativeState();

  virtual void InvalidateChildren();
  virtual bool RemoveChild(Accessible* aAccessible);

  // HyperTextAccessible (static helper method)

  // Convert content offset to rendered text offset
  nsresult ContentToRenderedOffset(nsIFrame *aFrame, int32_t aContentOffset,
                                   uint32_t *aRenderedOffset) const;

  // Convert rendered text offset to content offset
  nsresult RenderedToContentOffset(nsIFrame *aFrame, uint32_t aRenderedOffset,
                                   int32_t *aContentOffset) const;

  //////////////////////////////////////////////////////////////////////////////
  // HyperLinkAccessible

  /**
   * Return link count within this hypertext accessible.
   */
  uint32_t LinkCount()
    { return EmbeddedChildCount(); }

  /**
   * Return link accessible at the given index.
   */
  Accessible* LinkAt(uint32_t aIndex)
  {
    return GetEmbeddedChildAt(aIndex);
  }

  /**
   * Return index for the given link accessible.
   */
  int32_t LinkIndexOf(Accessible* aLink)
  {
    return GetIndexOfEmbeddedChild(aLink);
  }

  /**
   * Return link accessible at the given text offset.
   */
  int32_t LinkIndexAtOffset(uint32_t aOffset)
  {
    Accessible* child = GetChildAtOffset(aOffset);
    return child ? LinkIndexOf(child) : -1;
  }

  //////////////////////////////////////////////////////////////////////////////
  // HyperTextAccessible: DOM point to text offset conversions.

  /**
    * Turn a DOM Node and offset into a character offset into this hypertext.
    * Will look for closest match when the DOM node does not have an accessible
    * object associated with it. Will return an offset for the end of
    * the string if the node is not found.
    *
    * @param aNode - the node to look for
    * @param aNodeOffset - the offset to look for
    *                      if -1 just look directly for the node
    *                      if >=0 and aNode is text, this represents a char offset
    *                      if >=0 and aNode is not text, this represents a child node offset
    * @param aResultOffset - the character offset into the current
    *                        HyperTextAccessible
    * @param aIsEndOffset - if true, then then this offset is not inclusive. The character
    *                       indicated by the offset returned is at [offset - 1]. This means
    *                       if the passed-in offset is really in a descendant, then the offset returned
    *                       will come just after the relevant embedded object characer.
    *                       If false, then the offset is inclusive. The character indicated
    *                       by the offset returned is at [offset]. If the passed-in offset in inside a
    *                       descendant, then the returned offset will be on the relevant embedded object char.
    *
    * @return               the accessible child which contained the offset, if
    *                       it is within the current HyperTextAccessible,
    *                       otherwise nullptr
    */
  Accessible* DOMPointToHypertextOffset(nsINode *aNode,
                                        int32_t aNodeOffset,
                                        int32_t* aHypertextOffset,
                                        bool aIsEndOffset = false) const;

  /**
   * Turn a start and end hypertext offsets into DOM range.
   *
   * @param  aStartHTOffset  [in] the given start hypertext offset
   * @param  aEndHTOffset    [in] the given end hypertext offset
   * @param  aRange      [out] the range whose bounds to set
   */
  nsresult HypertextOffsetsToDOMRange(int32_t aStartHTOffset,
                                      int32_t aEndHTOffset,
                                      nsRange* aRange);

  /**
   * Return true if the used ARIA role (if any) allows the hypertext accessible
   * to expose text interfaces.
   */
  bool IsTextRole();

  //////////////////////////////////////////////////////////////////////////////
  // TextAccessible

  /**
   * Return character count within the hypertext accessible.
   */
  uint32_t CharacterCount()
  {
    return GetChildOffset(ChildCount());
  }

  /**
   * Get a character at the given offset (don't support magic offsets).
   */
  bool CharAt(int32_t aOffset, nsAString& aChar)
  {
    int32_t childIdx = GetChildIndexAtOffset(aOffset);
    if (childIdx == -1)
      return false;

    Accessible* child = GetChildAt(childIdx);
    child->AppendTextTo(aChar, aOffset - GetChildOffset(childIdx), 1);
    return true;
  }

  /**
   * Return true if char at the given offset equals to given char.
   */
  bool IsCharAt(int32_t aOffset, char aChar)
  {
    nsAutoString charAtOffset;
    CharAt(aOffset, charAtOffset);
    return charAtOffset.CharAt(0) == aChar;
  }

  /**
   * Return true if terminal char is at the given offset.
   */
  bool IsLineEndCharAt(int32_t aOffset)
    { return IsCharAt(aOffset, '\n'); }

  /**
   * Get a character before/at/after the given offset.
   *
   * @param aOffset       [in] the given offset
   * @param aShift        [in] specifies whether to get a char before/at/after
   *                        offset
   * @param aChar         [out] the character
   * @param aStartOffset  [out, optional] the start offset of the character
   * @param aEndOffset    [out, optional] the end offset of the character
   * @return               false if offset at the given shift is out of range
   */
  bool GetCharAt(int32_t aOffset, EGetTextType aShift, nsAString& aChar,
                 int32_t* aStartOffset = nullptr, int32_t* aEndOffset = nullptr);

  /**
   * Return text between given offsets.
   */
  void TextSubstring(int32_t aStartOffset, int32_t aEndOffset, nsAString& aText);

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
   * Return text attributes for the given text range.
   */
  already_AddRefed<nsIPersistentProperties>
    TextAttributes(bool aIncludeDefAttrs, int32_t aOffset,
                   int32_t* aStartOffset, int32_t* aEndOffset);

  /**
   * Return text attributes applied to the accessible.
   */
  already_AddRefed<nsIPersistentProperties> DefaultTextAttributes();

  /**
   * Return text offset of the given child accessible within hypertext
   * accessible.
   *
   * @param  aChild           [in] accessible child to get text offset for
   * @param  aInvalidateAfter [in, optional] indicates whether invalidate
   *                           cached offsets for next siblings of the child
   */
  int32_t GetChildOffset(Accessible* aChild,
                         bool aInvalidateAfter = false)
  {
    int32_t index = GetIndexOf(aChild);
    return index == -1 ? -1 : GetChildOffset(index, aInvalidateAfter);
  }

  /**
   * Return text offset for the child accessible index.
   */
  int32_t GetChildOffset(uint32_t aChildIndex,
                         bool aInvalidateAfter = false);

  /**
   * Return child accessible at the given text offset.
   *
   * @param  aOffset  [in] the given text offset
   */
  int32_t GetChildIndexAtOffset(uint32_t aOffset);

  /**
   * Return child accessible at the given text offset.
   *
   * @param  aOffset  [in] the given text offset
   */
  Accessible* GetChildAtOffset(uint32_t aOffset)
  {
    return GetChildAt(GetChildIndexAtOffset(aOffset));
  }

  /**
   * Return true if the given offset/range is valid.
   */
  bool IsValidOffset(int32_t aOffset);
  bool IsValidRange(int32_t aStartOffset, int32_t aEndOffset);

  /**
   * Return an offset at the given point.
   */
  int32_t OffsetAtPoint(int32_t aX, int32_t aY, uint32_t aCoordType);

  /**
   * Return a rect of the given text range relative given coordinate system.
   */
  nsIntRect TextBounds(int32_t aStartOffset, int32_t aEndOffset,
                       uint32_t aCoordType = nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE);

  /**
   * Return a rect for character at given offset relative given coordinate
   * system.
   */
  nsIntRect CharBounds(int32_t aOffset, uint32_t aCoordType)
    { return TextBounds(aOffset, aOffset + 1, aCoordType); }

  /**
   * Get/set caret offset, if no caret then -1.
   */
  int32_t CaretOffset() const;
  void SetCaretOffset(int32_t aOffset) { SetSelectionRange(aOffset, aOffset); }

  /**
   * Provide the line number for the caret.
   * @return 1-based index for the line number with the caret
   */
  int32_t CaretLineNumber();

  /**
   * Return the caret rect and the widget containing the caret within this
   * text accessible.
   *
   * @param [out] the widget containing the caret
   * @return      the caret rect
   */
  nsIntRect GetCaretRect(nsIWidget** aWidget);

  /**
   * Return selected regions count within the accessible.
   */
  int32_t SelectionCount();

  /**
   * Return the start and end offset of the specified selection.
   */
  bool SelectionBoundsAt(int32_t aSelectionNum,
                         int32_t* aStartOffset, int32_t* aEndOffset);

  /*
   * Changes the start and end offset of the specified selection.
   * @return true if succeeded
   */
  bool SetSelectionBoundsAt(int32_t aSelectionNum,
                            int32_t aStartOffset, int32_t aEndOffset);

  /**
   * Adds a selection bounded by the specified offsets.
   * @return true if succeeded
   */
  bool AddToSelection(int32_t aStartOffset, int32_t aEndOffset);

  /*
   * Removes the specified selection.
   * @return true if succeeded
   */
  bool RemoveFromSelection(int32_t aSelectionNum);

  /**
   * Scroll the given text range into view.
   */
  void ScrollSubstringTo(int32_t aStartOffset, int32_t aEndOffset,
                         uint32_t aScrollType);

  /**
   * Scroll the given text range to the given point.
   */
  void ScrollSubstringToPoint(int32_t aStartOffset,
                              int32_t aEndOffset,
                              uint32_t aCoordinateType,
                              int32_t aX, int32_t aY);

  //////////////////////////////////////////////////////////////////////////////
  // EditableTextAccessible

  void ReplaceText(const nsAString& aText);
  void InsertText(const nsAString& aText, int32_t aPosition);
  void CopyText(int32_t aStartPos, int32_t aEndPos);
  void CutText(int32_t aStartPos, int32_t aEndPos);
  void DeleteText(int32_t aStartPos, int32_t aEndPos);
  void PasteText(int32_t aPosition);

  /**
   * Return the editor associated with the accessible.
   */
  virtual already_AddRefed<nsIEditor> GetEditor() const;

protected:
  // Accessible
  virtual ENameValueFlag NativeName(nsString& aName) MOZ_OVERRIDE;
  virtual void CacheChildren() MOZ_OVERRIDE;

  // HyperTextAccessible

  /**
   * Transform magic offset into text offset.
   */
  int32_t ConvertMagicOffset(int32_t aOffset);

  /**
   * Adjust an offset the caret stays at to get a text by line boundary.
   */
  int32_t AdjustCaretOffset(int32_t aOffset) const;

  /**
   * Return true if the given offset points to terminal empty line if any.
   */
  bool IsEmptyLastLineOffset(int32_t aOffset)
  {
    return aOffset == static_cast<int32_t>(CharacterCount()) &&
      IsLineEndCharAt(aOffset - 1);
  }

  /**
   * Return an offset of the found word boundary.
   */
  int32_t FindWordBoundary(int32_t aOffset, nsDirection aDirection,
                           EWordMovementType aWordMovementType)
  {
    return FindOffset(aOffset, aDirection, eSelectWord, aWordMovementType);
  }

  /**
   * Used to get begin/end of previous/this/next line. Note: end of line
   * is an offset right before '\n' character if any, the offset is right after
   * '\n' character is begin of line. In case of wrap word breaks these offsets
   * are equal.
   */
  enum EWhichLineBoundary {
    ePrevLineBegin,
    ePrevLineEnd,
    eThisLineBegin,
    eThisLineEnd,
    eNextLineBegin,
    eNextLineEnd
  };

  /**
   * Return an offset for requested line boundary. See constants above.
   */
  int32_t FindLineBoundary(int32_t aOffset,
                           EWhichLineBoundary aWhichLineBoundary);

  /**
   * Return an offset corresponding to the given direction and selection amount
   * relative the given offset. A helper used to find word or line boundaries.
   */
  int32_t FindOffset(int32_t aOffset, nsDirection aDirection,
                     nsSelectionAmount aAmount,
                     EWordMovementType aWordMovementType = eDefaultBehavior);

  /**
    * Used by FindOffset() to move backward/forward from a given point
    * by word/line/etc.
    *
    * @param  aPresShell       the current presshell we're moving in
    * @param  aFromFrame       the starting frame we're moving from
    * @param  aFromOffset      the starting offset we're moving from
    * @param  aFromAccessible  the starting accessible we're moving from
    * @param  aAmount          how much are we moving (word/line/etc.) ?
    * @param  aDirection       forward or backward?
    * @param  aNeedsStart      for word and line cases, are we basing this on
    *                          the start or end?
    * @return                  the resulting offset into this hypertext
    */
  int32_t GetRelativeOffset(nsIPresShell *aPresShell, nsIFrame *aFromFrame,
                            int32_t aFromOffset, Accessible* aFromAccessible,
                            nsSelectionAmount aAmount, nsDirection aDirection,
                            bool aNeedsStart,
                            EWordMovementType aWordMovementType);

  /**
    * Provides information for substring that is defined by the given start
    * and end offsets for this hyper text.
    *
    * @param  aStartOffset  [inout] the start offset into the hyper text. This
    *                       is also an out parameter used to return the offset
    *                       into the start frame's rendered text content
    *                       (start frame is the @return)
    *
    * @param  aEndOffset    [inout] the end offset into the hyper text. This is
    *                       also an out parameter used to return
    *                       the offset into the end frame's rendered
    *                       text content.
    *
    * @param  aText         [out, optional] return the substring's text
    * @param  aEndFrame     [out, optional] return the end frame for this
    *                       substring
    * @param  aBoundsRect   [out, optional] return the bounds rectangle for this
    *                       substring
    * @param  aStartAcc     [out, optional] return the start accessible for this
    *                       substring
    * @param  aEndAcc       [out, optional] return the end accessible for this
    *                       substring
    * @return               the start frame for this substring
    */
  nsIFrame* GetPosAndText(int32_t& aStartOffset, int32_t& aEndOffset,
                          nsAString *aText = nullptr,
                          nsIFrame **aEndFrame = nullptr,
                          nsIntRect *aBoundsRect = nullptr,
                          Accessible** aStartAcc = nullptr,
                          Accessible** aEndAcc = nullptr);

  nsIntRect GetBoundsForString(nsIFrame *aFrame, uint32_t aStartRenderedOffset, uint32_t aEndRenderedOffset);

  // Selection helpers

  /**
   * Return frame/DOM selection object for the accessible.
   */
  virtual already_AddRefed<nsFrameSelection> FrameSelection() const;
  Selection* DOMSelection() const;

  /**
   * Return selection ranges within the accessible subtree.
   */
  void GetSelectionDOMRanges(int16_t aType, nsTArray<nsRange*>* aRanges);

  nsresult SetSelectionRange(int32_t aStartPos, int32_t aEndPos);

  // Helpers
  nsresult GetDOMPointByFrameOffset(nsIFrame* aFrame, int32_t aOffset,
                                    Accessible* aAccessible,
                                    mozilla::a11y::DOMPoint* aPoint);


  /**
   * Return hyper text offset for the specified bound of the given DOM range.
   * If the bound is outside of the hyper text then offset value is either
   * 0 or number of characters of hyper text, it depends on type of requested
   * offset. The method is a wrapper for DOMPointToHypertextOffset.
   *
   * @param aRange          [in] the given range
   * @param aIsStartBound   [in] specifies whether the required range bound is
   *                        start bound
   * @param aIsStartOffset  [in] the offset type, used when the range bound is
   *                        outside of hyper text
   * @param aHTOffset       [out] the result offset
   */
  nsresult RangeBoundToHypertextOffset(nsRange *aRange,
                                       bool aIsStartBound,
                                       bool aIsStartOffset,
                                       int32_t *aHTOffset);

  /**
   * Set 'misspelled' text attribute and return range offsets where the
   * attibute is stretched. If the text is not misspelled at the given offset
   * then we expose only range offsets where text is not misspelled. The method
   * is used by TextAttributes() method.
   *
   * @param aIncludeDefAttrs  [in] points whether text attributes having default
   *                          values of attributes should be included
   * @param aSourceNode       [in] the node we start to traverse from
   * @param aStartOffset      [in, out] the start offset
   * @param aEndOffset        [in, out] the end offset
   * @param aAttributes       [out, optional] result attributes
   */
  nsresult GetSpellTextAttribute(nsINode* aNode, int32_t aNodeOffset,
                                 int32_t *aStartOffset,
                                 int32_t *aEndOffset,
                                 nsIPersistentProperties *aAttributes);

private:
  /**
   * End text offsets array.
   */
  nsTArray<uint32_t> mOffsets;
};


////////////////////////////////////////////////////////////////////////////////
// Accessible downcasting method

inline HyperTextAccessible*
Accessible::AsHyperText()
{
  return IsHyperText() ? static_cast<HyperTextAccessible*>(this) : nullptr;
}

} // namespace a11y
} // namespace mozilla

#endif

