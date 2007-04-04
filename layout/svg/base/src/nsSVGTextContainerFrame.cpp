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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsSVGContainerFrame.h"
#include "nsSVGTextFrame.h"
#include "nsSVGUtils.h"
#include "nsSVGMatrix.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsIDOMSVGTextElement.h"
#include "nsIDOMSVGAnimatedLengthList.h"
#include "nsISVGGlyphFragmentLeaf.h"
#include "nsDOMError.h"

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGTextContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGTextContentMetrics)
NS_INTERFACE_MAP_END_INHERITING(nsSVGDisplayContainerFrame)

void
nsSVGTextContainerFrame::UpdateGraphic()
{
  nsSVGTextFrame *textFrame = GetTextFrame();
  if (textFrame)
    textFrame->NotifyGlyphMetricsChange();
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTextContainerFrame::GetX()
{
  nsCOMPtr<nsIDOMSVGTextPositioningElement> tpElement =
    do_QueryInterface(mContent);

  if (!tpElement)
    return nsnull;

  if (!mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::x))
    return nsnull;

  nsCOMPtr<nsIDOMSVGAnimatedLengthList> animLengthList;
  tpElement->GetX(getter_AddRefs(animLengthList));
  nsIDOMSVGLengthList *retval;
  animLengthList->GetAnimVal(&retval);
  return retval;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTextContainerFrame::GetY()
{
  nsCOMPtr<nsIDOMSVGTextPositioningElement> tpElement =
    do_QueryInterface(mContent);

  if (!tpElement)
    return nsnull;

  if (!mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::y))
    return nsnull;

  nsCOMPtr<nsIDOMSVGAnimatedLengthList> animLengthList;
  tpElement->GetY(getter_AddRefs(animLengthList));
  nsIDOMSVGLengthList *retval;
  animLengthList->GetAnimVal(&retval);
  return retval;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTextContainerFrame::GetDx()
{
  nsCOMPtr<nsIDOMSVGTextPositioningElement> tpElement =
    do_QueryInterface(mContent);

  if (!tpElement)
    return nsnull;

  nsCOMPtr<nsIDOMSVGAnimatedLengthList> animLengthList;
  tpElement->GetDx(getter_AddRefs(animLengthList));
  nsIDOMSVGLengthList *retval;
  animLengthList->GetAnimVal(&retval);
  return retval;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTextContainerFrame::GetDy()
{
  nsCOMPtr<nsIDOMSVGTextPositioningElement> tpElement =
    do_QueryInterface(mContent);

  if (!tpElement)
    return nsnull;

  nsCOMPtr<nsIDOMSVGAnimatedLengthList> animLengthList;
  tpElement->GetDy(getter_AddRefs(animLengthList));
  nsIDOMSVGLengthList *retval;
  animLengthList->GetAnimVal(&retval);
  return retval;
}

//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGTextContainerFrame::RemoveFrame(nsIAtom *aListName, nsIFrame *aOldFrame)
{
  nsSVGTextFrame *textFrame = GetTextFrame();

  nsresult rv = nsSVGDisplayContainerFrame::RemoveFrame(aListName, aOldFrame);

  if (textFrame)
    textFrame->NotifyGlyphMetricsChange();

  return rv;
}

//----------------------------------------------------------------------
// nsISVGTextContentMetrics methods

NS_IMETHODIMP
nsSVGTextContainerFrame::GetNumberOfChars(PRInt32 *_retval)
{
  *_retval = GetNumberOfChars();

  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextContainerFrame::GetComputedTextLength(float *_retval)
{
  *_retval = GetComputedTextLength();

  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextContainerFrame::GetSubStringLength(PRUint32 charnum,
                                            PRUint32 nchars,
                                            float *_retval)
{
  if (nchars == 0) {
    *_retval = 0.0f;
    return NS_OK;
  }

  if (charnum + nchars > GetNumberOfChars()) {
    *_retval = 0.0f;
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  *_retval = GetSubStringLengthNoValidation(charnum, nchars);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGTextContainerFrame::GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;

  if (charnum >= GetNumberOfChars()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsISVGGlyphFragmentNode *node = GetFirstGlyphFragmentChildNode();
  if (!node) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 offset;
  nsISVGGlyphFragmentLeaf *fragment = GetGlyphFragmentAtCharNum(node, charnum, &offset);
  if (!fragment) {
    return NS_ERROR_FAILURE;
  }

  return fragment->GetStartPositionOfChar(charnum - offset, _retval);
}

NS_IMETHODIMP
nsSVGTextContainerFrame::GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  *_retval = nsnull;

  if (charnum >= GetNumberOfChars()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsISVGGlyphFragmentNode *node = GetFirstGlyphFragmentChildNode();
  if (!node) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 offset;
  nsISVGGlyphFragmentLeaf *fragment = GetGlyphFragmentAtCharNum(node, charnum, &offset);
  if (!fragment) {
    return NS_ERROR_FAILURE;
  }

  return fragment->GetEndPositionOfChar(charnum - offset, _retval);
}

NS_IMETHODIMP
nsSVGTextContainerFrame::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  if (charnum >= GetNumberOfChars()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsISVGGlyphFragmentNode *node = GetFirstGlyphFragmentChildNode();
  if (!node) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 offset;
  nsISVGGlyphFragmentLeaf *fragment = GetGlyphFragmentAtCharNum(node, charnum, &offset);
  if (!fragment) {
    return NS_ERROR_FAILURE;
  }

  return fragment->GetExtentOfChar(charnum - offset, _retval);
}

NS_IMETHODIMP
nsSVGTextContainerFrame::GetRotationOfChar(PRUint32 charnum, float *_retval)
{
  *_retval = 0.0f;

  if (charnum >= GetNumberOfChars()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsISVGGlyphFragmentNode *node = GetFirstGlyphFragmentChildNode();
  if (!node) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 offset;
  nsISVGGlyphFragmentLeaf *fragment = GetGlyphFragmentAtCharNum(node, charnum, &offset);
  if (!fragment) {
    return NS_ERROR_FAILURE;
  }

  return fragment->GetRotationOfChar(charnum - offset, _retval);
}

NS_IMETHODIMP
nsSVGTextContainerFrame::GetCharNumAtPosition(nsIDOMSVGPoint *point, PRInt32 *_retval)
{
  *_retval = GetCharNumAtPosition(point);

  return NS_OK;
}

// -------------------------------------------------------------------------
// Protected functions
// -------------------------------------------------------------------------

nsISVGGlyphFragmentNode *
nsSVGTextContainerFrame::GetFirstGlyphFragmentChildNode()
{
  nsISVGGlyphFragmentNode *retval = nsnull;
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    CallQueryInterface(kid, &retval);
    if (retval) break;
    kid = kid->GetNextSibling();
  }
  return retval;
}

nsISVGGlyphFragmentNode *
nsSVGTextContainerFrame::GetNextGlyphFragmentChildNode(nsISVGGlyphFragmentNode *node)
{
  nsISVGGlyphFragmentNode *retval = nsnull;
  nsIFrame *frame = nsnull;
  CallQueryInterface(node, &frame);
  NS_ASSERTION(frame, "interface not implemented");
  frame = frame->GetNextSibling();
  while (frame) {
    CallQueryInterface(frame, &retval);
    if (retval) break;
    frame = frame->GetNextSibling();
  }
  return retval;
}

void
nsSVGTextContainerFrame::SetWhitespaceHandling()
{
  // init children:
  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();
  nsISVGGlyphFragmentNode* next;

  PRUint8 whitespaceHandling = COMPRESS_WHITESPACE | TRIM_LEADING_WHITESPACE;

  for (nsIFrame *frame = this; frame != nsnull; frame = frame->GetParent()) {
    nsIContent *content = frame->GetContent();
    static nsIContent::AttrValuesArray strings[] =
      {&nsGkAtoms::preserve, &nsGkAtoms::_default, nsnull};

    PRInt32 index = content->FindAttrValueIn(kNameSpaceID_XML,
                                             nsGkAtoms::space,
                                             strings, eCaseMatters);
    if (index == 0) {
      whitespaceHandling = PRESERVE_WHITESPACE;
      break;
    }
    if (index != nsIContent::ATTR_MISSING ||
        (frame->GetStateBits() & NS_STATE_IS_OUTER_SVG))
      break;
  }

  while (node) {
    next = GetNextGlyphFragmentChildNode(node);
    if (!next && (whitespaceHandling & COMPRESS_WHITESPACE)) {
      whitespaceHandling |= TRIM_TRAILING_WHITESPACE;
    }
    node->SetWhitespaceHandling(whitespaceHandling);
    node = next;
    whitespaceHandling &= ~TRIM_LEADING_WHITESPACE;
  }
}

PRUint32
nsSVGTextContainerFrame::GetNumberOfChars()
{
  PRUint32 nchars = 0;
  nsISVGGlyphFragmentNode* node;
  node = GetFirstGlyphFragmentChildNode();

  while (node) {
    nchars += node->GetNumberOfChars();
    node = GetNextGlyphFragmentChildNode(node);
  }

  return nchars;
}

float
nsSVGTextContainerFrame::GetComputedTextLength()
{
  float length = 0.0f;
  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();

  while (node) {
    length += node->GetComputedTextLength();
    node = GetNextGlyphFragmentChildNode(node);
  }

  return length;
}

float
nsSVGTextContainerFrame::GetSubStringLengthNoValidation(PRUint32 charnum,
                                                        PRUint32 nchars)
{
  float length = 0.0f;
  nsISVGGlyphFragmentNode *node = GetFirstGlyphFragmentChildNode();

  while (node) {
    PRUint32 count = node->GetNumberOfChars();
    if (count > charnum) {
      PRUint32 fragmentChars = PR_MIN(nchars, count);
      float fragmentLength = node->GetSubStringLength(charnum, fragmentChars);
      length += fragmentLength;
      nchars -= fragmentChars;
      if (nchars == 0) break;
    }
    charnum -= PR_MIN(charnum, count);
    node = GetNextGlyphFragmentChildNode(node);
  }

  return length;
}

PRInt32
nsSVGTextContainerFrame::GetCharNumAtPosition(nsIDOMSVGPoint *point)
{
  PRInt32 index = -1;
  PRInt32 offset = 0;
  nsISVGGlyphFragmentNode *node = GetFirstGlyphFragmentChildNode();

  while (node) {
    PRUint32 count = node->GetNumberOfChars();
    if (count > 0) {
      PRInt32 charnum = node->GetCharNumAtPosition(point);
      if (charnum >= 0) {
        index = charnum + offset;
      }
      offset += count;
      // Keep going, multiple characters may match 
      // and we must return the last one
    }
    node = GetNextGlyphFragmentChildNode(node);
  }

  return index;
}

// -------------------------------------------------------------------------
// Private functions
// -------------------------------------------------------------------------

nsISVGGlyphFragmentLeaf *
nsSVGTextContainerFrame::GetGlyphFragmentAtCharNum(nsISVGGlyphFragmentNode* node,
                                                   PRUint32 charnum,
                                                   PRUint32 *offset)
{
  nsISVGGlyphFragmentLeaf *fragment = node->GetFirstGlyphFragment();
  *offset = 0;
  
  while (fragment) {
    PRUint32 count = fragment->GetNumberOfChars();
    if (count > charnum)
      return fragment;
    charnum -= count;
    *offset += count;
    fragment = fragment->GetNextGlyphFragment();
  }

  // not found
  return nsnull;
}

nsSVGTextFrame *
nsSVGTextContainerFrame::GetTextFrame()
{
  for (nsIFrame *frame = this; frame != nsnull; frame = frame->GetParent()) {
    if (frame->GetType() == nsGkAtoms::svgTextFrame) {
      return NS_STATIC_CAST(nsSVGTextFrame*, frame);
    }
  }
  return nsnull;
}
