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

#include "nsAccessibleWrap.h"
#include "nsIAccessibleText.h"
#include "nsIAccessibleHyperText.h"
#include "nsIAccessibleEditableText.h"
#include "nsAccessibleEventData.h"
#include "nsFrameSelection.h"
#include "nsISelectionController.h"

enum EGetTextType { eGetBefore=-1, eGetAt=0, eGetAfter=1 };

// This character marks where in the text returned via nsIAccessibleText(),
// that embedded object characters exist
const PRUnichar kEmbeddedObjectChar = 0xfffc;
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
  nsHyperTextAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell);
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLETEXT
  NS_DECL_NSIACCESSIBLEHYPERTEXT
  NS_DECL_NSIACCESSIBLEEDITABLETEXT
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_HYPERTEXTACCESSIBLE_IMPL_CID)

  NS_IMETHOD GetRole(PRUint32 *aRole);
  NS_IMETHOD GetState(PRUint32 *aState, PRUint32 *aExtraState);
  virtual nsresult GetAttributesInternal(nsIPersistentProperties *aAttributes);
  void CacheChildren();

  // Convert content offset to rendered text offset  
  static nsresult ContentToRenderedOffset(nsIFrame *aFrame, PRInt32 aContentOffset,
                                          PRUint32 *aRenderedOffset);
  
  // Convert rendered text offset to content offset
  static nsresult RenderedToContentOffset(nsIFrame *aFrame, PRUint32 aRenderedOffset,
                                          PRInt32 *aContentOffset);

  /**
    * Turn a DOM Node and offset into a character offset into this hypertext.
    * Will look for closest match when the DOM node does not have an accessible
    * object associated with it. Will return an offset for the end of
    * the string if the node is not found.
    *
    * @param aNode - the node to look for
    * @param aNodeOffset - the offset to look for
    * @param aResultOffset - the character offset into the current
    *                        nsHyperTextAccessible
    * @param aFinalAccessible [optional] - returns the accessible child which
    *                                      contained the offset, if it is within
    *                                      the current nsHyperTextAccessible,
    *                                      otherwise it is set to nsnull.
    */
  nsresult DOMPointToHypertextOffset(nsIDOMNode* aNode, PRInt32 aNodeOffset,
                                     PRInt32 *aHypertextOffset,
                                     nsIAccessible **aFinalAccessible = nsnull);

protected:
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
    * Used by GetPosAndText to move backward/forward from a given point by word/line/etc.
    * @param aPresShell, the current presshell we're moving in
    * @param aFromFrame, the starting frame we're moving from
    * @param aFromOffset, the starting offset we're moving from
    * @param aAmount, how much are we moving (word/line/etc.) ?
    * @param aDirection, forward or backward?
    * @param aNeedsStart, for word and line cases, are we basing this on the start or end?
    * @return, the resulting offset into this hypertext
    */
  PRInt32 GetRelativeOffset(nsIPresShell *aPresShell, nsIFrame *aFromFrame, PRInt32 aFromOffset,
                            nsSelectionAmount aAmount, nsDirection aDirection, PRBool aNeedsStart);

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
                          nsIAccessible **aStartAcc = nsnull,
                          nsIAccessible **aEndAcc = nsnull);

  nsIntRect GetBoundsForString(nsIFrame *aFrame, PRUint32 aStartRenderedOffset, PRUint32 aEndRenderedOffset);

  // Selection helpers
  nsresult GetSelections(nsISelectionController **aSelCon, nsISelection **aDomSel);
  nsresult SetSelectionRange(PRInt32 aStartPos, PRInt32 aEndPos);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsHyperTextAccessible,
                              NS_HYPERTEXTACCESSIBLE_IMPL_CID)

#endif  // _nsHyperTextAccessible_H_

