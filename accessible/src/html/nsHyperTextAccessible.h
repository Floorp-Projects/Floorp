/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developers of the Original Code are
 * Sun Microsystems and IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ginn Chen (ginn.chen@sun.com)
 *   Aaron Leventhal (aleventh@us.ibm.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsHyperTextAccessible_H_
#define _nsHyperTextAccessible_H_

#include "nsIAccessibleText.h"
#include "nsIAccessibleHyperText.h"
#include "nsIAccessibleEditableText.h"

#include "AccCollector.h"
#include "nsAccessibleWrap.h"

#include "nsFrameSelection.h"
#include "nsISelectionController.h"

enum EGetTextType { eGetBefore=-1, eGetAt=0, eGetAfter=1 };

// This character marks where in the text returned via nsIAccessibleText(),
// that embedded object characters exist
const PRUnichar kEmbeddedObjectChar = 0xfffc;
const PRUnichar kImaginaryEmbeddedObjectChar = ' ';
const PRUnichar kForcedNewLineChar = '\n';

#define NS_HYPERTEXTACCESSIBLE_IMPL_CID                 \
{  /* 245f3bc9-224f-4839-a92e-95239705f30b */           \
  0x245f3bc9,                                           \
  0x224f,                                               \
  0x4839,                                               \
  { 0xa9, 0x2e, 0x95, 0x23, 0x97, 0x05, 0xf3, 0x0b }    \
}

/**
  * Special Accessible that knows how contain both text and embedded objects
  */
class nsHyperTextAccessible : public nsAccessibleWrap,
                              public nsIAccessibleText,
                              public nsIAccessibleHyperText,
                              public nsIAccessibleEditableText
{
public:
  nsHyperTextAccessible(nsIContent* aContent, nsDocAccessible* aDoc);
  virtual ~nsHyperTextAccessible() { }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLETEXT
  NS_DECL_NSIACCESSIBLEHYPERTEXT
  NS_DECL_NSIACCESSIBLEEDITABLETEXT
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_HYPERTEXTACCESSIBLE_IMPL_CID)

  // nsAccessible
  virtual PRInt32 GetLevelInternal();
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
  virtual nsresult GetNameInternal(nsAString& aName);
  virtual mozilla::a11y::role NativeRole();
  virtual PRUint64 NativeState();

  virtual void InvalidateChildren();
  virtual bool RemoveChild(nsAccessible* aAccessible);

  // nsHyperTextAccessible (static helper method)

  // Convert content offset to rendered text offset  
  static nsresult ContentToRenderedOffset(nsIFrame *aFrame, PRInt32 aContentOffset,
                                          PRUint32 *aRenderedOffset);
  
  // Convert rendered text offset to content offset
  static nsresult RenderedToContentOffset(nsIFrame *aFrame, PRUint32 aRenderedOffset,
                                          PRInt32 *aContentOffset);

  //////////////////////////////////////////////////////////////////////////////
  // HyperLinkAccessible

  /**
   * Return link count within this hypertext accessible.
   */
  inline PRUint32 GetLinkCount()
  {
    return GetEmbeddedChildCount();
  }

  /**
   * Return link accessible at the given index.
   */
  inline nsAccessible* GetLinkAt(PRUint32 aIndex)
  {
    return GetEmbeddedChildAt(aIndex);
  }

  /**
   * Return index for the given link accessible.
   */
  inline PRInt32 GetLinkIndex(nsAccessible* aLink)
  {
    return GetIndexOfEmbeddedChild(aLink);
  }

  /**
   * Return link accessible at the given text offset.
   */
  inline PRInt32 GetLinkIndexAtOffset(PRUint32 aOffset)
  {
    nsAccessible* child = GetChildAtOffset(aOffset);
    return child ? GetLinkIndex(child) : -1;
  }

  //////////////////////////////////////////////////////////////////////////////
  // nsHyperTextAccessible: DOM point to text offset conversions.

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
    *                        nsHyperTextAccessible
    * @param aIsEndOffset - if true, then then this offset is not inclusive. The character
    *                       indicated by the offset returned is at [offset - 1]. This means
    *                       if the passed-in offset is really in a descendant, then the offset returned
    *                       will come just after the relevant embedded object characer.
    *                       If false, then the offset is inclusive. The character indicated
    *                       by the offset returned is at [offset]. If the passed-in offset in inside a
    *                       descendant, then the returned offset will be on the relevant embedded object char.
    *
    * @return               the accessible child which contained the offset, if
    *                       it is within the current nsHyperTextAccessible,
    *                       otherwise nsnull
    */
  nsAccessible *DOMPointToHypertextOffset(nsINode *aNode,
                                          PRInt32 aNodeOffset,
                                          PRInt32 *aHypertextOffset,
                                          bool aIsEndOffset = false);

  /**
   * Turn a hypertext offsets into DOM point.
   *
   * @param  aHTOffset  [in] the given start hypertext offset
   * @param  aNode      [out] start node
   * @param  aOffset    [out] offset inside the start node
   */
  nsresult HypertextOffsetToDOMPoint(PRInt32 aHTOffset,
                                     nsIDOMNode **aNode,
                                     PRInt32 *aOffset);

  /**
   * Turn a start and end hypertext offsets into DOM range.
   *
   * @param  aStartHTOffset  [in] the given start hypertext offset
   * @param  aEndHTOffset    [in] the given end hypertext offset
   * @param  aStartNode      [out] start node of the range
   * @param  aStartOffset    [out] start offset of the range
   * @param  aEndNode        [out] end node of the range
   * @param  aEndOffset      [out] end offset of the range
   */
  nsresult HypertextOffsetsToDOMRange(PRInt32 aStartHTOffset,
                                      PRInt32 aEndHTOffset,
                                      nsIDOMNode **aStartNode,
                                      PRInt32 *aStartOffset,
                                      nsIDOMNode **aEndNode,
                                      PRInt32 *aEndOffset);

  //////////////////////////////////////////////////////////////////////////////
  // TextAccessible

  /**
   * Return character count within the hypertext accessible.
   */
  inline PRUint32 CharacterCount()
  {
    return GetChildOffset(GetChildCount());
  }

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
  bool GetCharAt(PRInt32 aOffset, EGetTextType aShift, nsAString& aChar,
                 PRInt32* aStartOffset = nsnull, PRInt32* aEndOffset = nsnull);

  /**
   * Return text offset of the given child accessible within hypertext
   * accessible.
   *
   * @param  aChild           [in] accessible child to get text offset for
   * @param  aInvalidateAfter [in, optional] indicates whether invalidate
   *                           cached offsets for next siblings of the child
   */
  PRInt32 GetChildOffset(nsAccessible* aChild,
                         bool aInvalidateAfter = false)
  {
    PRInt32 index = GetIndexOf(aChild);
    return index == -1 ? -1 : GetChildOffset(index, aInvalidateAfter);
  }

  /**
   * Return text offset for the child accessible index.
   */
  PRInt32 GetChildOffset(PRUint32 aChildIndex,
                         bool aInvalidateAfter = false);

  /**
   * Return child accessible at the given text offset.
   *
   * @param  aOffset  [in] the given text offset
   */
  PRInt32 GetChildIndexAtOffset(PRUint32 aOffset);

  /**
   * Return child accessible at the given text offset.
   *
   * @param  aOffset  [in] the given text offset
   */
  nsAccessible* GetChildAtOffset(PRUint32 aOffset)
  {
    return GetChildAt(GetChildIndexAtOffset(aOffset));
  }

protected:
  // nsHyperTextAccessible

  /**
   * Transform magic offset into text offset.
   */
  inline PRInt32 ConvertMagicOffset(PRInt32 aOffset)
  {
    if (aOffset == nsIAccessibleText::TEXT_OFFSET_END_OF_TEXT)
      return CharacterCount();

    if (aOffset == nsIAccessibleText::TEXT_OFFSET_CARET) {
      PRInt32 caretOffset = -1;
      GetCaretOffset(&caretOffset);
      return caretOffset;
    }

    return aOffset;
  }

  /*
   * This does the work for nsIAccessibleText::GetText[At|Before|After]Offset
   * @param aType, eGetBefore, eGetAt, eGetAfter
   * @param aBoundaryType, char/word-start/word-end/line-start/line-end/paragraph/attribute
   * @param aOffset, offset into the hypertext to start from
   * @param *aStartOffset, the resulting start offset for the returned substring
   * @param *aEndOffset, the resulting end offset for the returned substring
   * @param aText, the resulting substring
   * @return success/failure code
   */
  nsresult GetTextHelper(EGetTextType aType, nsAccessibleTextBoundary aBoundaryType,
                         PRInt32 aOffset, PRInt32 *aStartOffset, PRInt32 *aEndOffset,
                         nsAString & aText);

  /**
    * Used by GetTextHelper() to move backward/forward from a given point
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
  PRInt32 GetRelativeOffset(nsIPresShell *aPresShell, nsIFrame *aFromFrame,
                            PRInt32 aFromOffset, nsAccessible *aFromAccessible,
                            nsSelectionAmount aAmount, nsDirection aDirection,
                            bool aNeedsStart);

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
  nsIFrame* GetPosAndText(PRInt32& aStartOffset, PRInt32& aEndOffset,
                          nsAString *aText = nsnull,
                          nsIFrame **aEndFrame = nsnull,
                          nsIntRect *aBoundsRect = nsnull,
                          nsAccessible **aStartAcc = nsnull,
                          nsAccessible **aEndAcc = nsnull);

  nsIntRect GetBoundsForString(nsIFrame *aFrame, PRUint32 aStartRenderedOffset, PRUint32 aEndRenderedOffset);

  // Selection helpers

  /**
   * Return frame selection object for the accessible.
   */
  virtual already_AddRefed<nsFrameSelection> FrameSelection();

  /**
   * Return selection ranges within the accessible subtree.
   */
  void GetSelectionDOMRanges(PRInt16 aType, nsTArray<nsRange*>* aRanges);

  nsresult SetSelectionRange(PRInt32 aStartPos, PRInt32 aEndPos);

  /**
   * Provide the line number for the caret, relative to the
   * current DOM node.
   * @return 1-based index for the line number with the caret
   */
  PRInt32 GetCaretLineNumber();

  // Helpers
  nsresult GetDOMPointByFrameOffset(nsIFrame *aFrame, PRInt32 aOffset,
                                    nsIAccessible *aAccessible,
                                    nsIDOMNode **aNode, PRInt32 *aNodeOffset);

  
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
                                       PRInt32 *aHTOffset);

  /**
   * Set 'misspelled' text attribute and return range offsets where the
   * attibute is stretched. If the text is not misspelled at the given offset
   * then we expose only range offsets where text is not misspelled. The method
   * is used by GetTextAttributes() method.
   *
   * @param aIncludeDefAttrs  [in] points whether text attributes having default
   *                          values of attributes should be included
   * @param aSourceNode       [in] the node we start to traverse from
   * @param aStartOffset      [in, out] the start offset
   * @param aEndOffset        [in, out] the end offset
   * @param aAttributes       [out, optional] result attributes
   */
  nsresult GetSpellTextAttribute(nsINode* aNode, PRInt32 aNodeOffset,
                                 PRInt32 *aStartOffset,
                                 PRInt32 *aEndOffset,
                                 nsIPersistentProperties *aAttributes);

private:
  /**
   * End text offsets array.
   */
  nsTArray<PRUint32> mOffsets;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsHyperTextAccessible,
                              NS_HYPERTEXTACCESSIBLE_IMPL_CID)


////////////////////////////////////////////////////////////////////////////////
// nsAccessible downcasting method

inline nsHyperTextAccessible*
nsAccessible::AsHyperText()
{
  return mFlags & eHyperTextAccessible ?
    static_cast<nsHyperTextAccessible*>(this) : nsnull;
}

#endif  // _nsHyperTextAccessible_H_

