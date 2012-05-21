/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGContainerFrame.h"

// Keep others in (case-insensitive) order:
#include "nsDOMError.h"
#include "nsSVGGlyphFrame.h"
#include "nsSVGTextFrame.h"
#include "nsSVGUtils.h"
#include "SVGAnimatedNumberList.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"

using namespace mozilla;

//----------------------------------------------------------------------
// nsQueryFrame methods

NS_QUERYFRAME_HEAD(nsSVGTextContainerFrame)
  NS_QUERYFRAME_ENTRY(nsSVGTextContainerFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGDisplayContainerFrame)

NS_IMPL_FRAMEARENA_HELPERS(nsSVGTextContainerFrame)

void
nsSVGTextContainerFrame::NotifyGlyphMetricsChange()
{
  nsSVGTextFrame *textFrame = GetTextFrame();
  if (textFrame)
    textFrame->NotifyGlyphMetricsChange();
}

void
nsSVGTextContainerFrame::GetXY(SVGUserUnitList *aX, SVGUserUnitList *aY)
{
  static_cast<nsSVGElement*>(mContent)->
    GetAnimatedLengthListValues(aX, aY, nsnull);
}

void
nsSVGTextContainerFrame::GetDxDy(SVGUserUnitList *aDx, SVGUserUnitList *aDy)
{
  // SVGUserUnitList is lazy, so there's little overhead it getting the x
  // and y lists even though we ignore them.
  SVGUserUnitList xLengthList, yLengthList;
  static_cast<nsSVGElement*>(mContent)->
    GetAnimatedLengthListValues(&xLengthList, &yLengthList, aDx, aDy, nsnull);
}

const SVGNumberList*
nsSVGTextContainerFrame::GetRotate()
{
  SVGAnimatedNumberList *animList =
    static_cast<nsSVGElement*>(mContent)->
      GetAnimatedNumberList(nsGkAtoms::rotate);
  return animList ? &animList->GetAnimValue() : nsnull;
}

//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGTextContainerFrame::InsertFrames(ChildListID aListID,
                                      nsIFrame* aPrevFrame,
                                      nsFrameList& aFrameList)
{
  nsresult rv = nsSVGDisplayContainerFrame::InsertFrames(aListID,
                                                         aPrevFrame,
                                                         aFrameList);

  NotifyGlyphMetricsChange();
  return rv;
}

NS_IMETHODIMP
nsSVGTextContainerFrame::RemoveFrame(ChildListID aListID, nsIFrame *aOldFrame)
{
  nsSVGTextFrame *textFrame = GetTextFrame();

  nsresult rv = nsSVGDisplayContainerFrame::RemoveFrame(aListID, aOldFrame);

  if (textFrame)
    textFrame->NotifyGlyphMetricsChange();

  return rv;
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
  nsSVGGlyphFrame *frame = GetGlyphFrameAtCharNum(node, charnum, &offset);
  if (!frame) {
    return NS_ERROR_FAILURE;
  }

  return frame->GetStartPositionOfChar(charnum - offset, _retval);
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
  nsSVGGlyphFrame *frame = GetGlyphFrameAtCharNum(node, charnum, &offset);
  if (!frame) {
    return NS_ERROR_FAILURE;
  }

  return frame->GetEndPositionOfChar(charnum - offset, _retval);
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
  nsSVGGlyphFrame *frame = GetGlyphFrameAtCharNum(node, charnum, &offset);
  if (!frame) {
    return NS_ERROR_FAILURE;
  }

  return frame->GetExtentOfChar(charnum - offset, _retval);
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
  nsSVGGlyphFrame *frame = GetGlyphFrameAtCharNum(node, charnum, &offset);
  if (!frame) {
    return NS_ERROR_FAILURE;
  }

  return frame->GetRotationOfChar(charnum - offset, _retval);
}

PRUint32
nsSVGTextContainerFrame::GetNumberOfChars()
{
  PRUint32 nchars = 0;
  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();

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
nsSVGTextContainerFrame::GetSubStringLength(PRUint32 charnum, PRUint32 nchars)
{
  float length = 0.0f;
  nsISVGGlyphFragmentNode *node = GetFirstGlyphFragmentChildNode();

  while (node) {
    PRUint32 count = node->GetNumberOfChars();
    if (count > charnum) {
      PRUint32 fragmentChars = NS_MIN(nchars, count);
      float fragmentLength = node->GetSubStringLength(charnum, fragmentChars);
      length += fragmentLength;
      nchars -= fragmentChars;
      if (nchars == 0) break;
    }
    charnum -= NS_MIN(charnum, count);
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
// Protected functions
// -------------------------------------------------------------------------

nsISVGGlyphFragmentNode *
nsSVGTextContainerFrame::GetFirstGlyphFragmentChildNode()
{
  nsISVGGlyphFragmentNode *retval = nsnull;
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    retval = do_QueryFrame(kid);
    if (retval) break;
    kid = kid->GetNextSibling();
  }
  return retval;
}

nsISVGGlyphFragmentNode *
nsSVGTextContainerFrame::GetNextGlyphFragmentChildNode(nsISVGGlyphFragmentNode *node)
{
  nsISVGGlyphFragmentNode *retval = nsnull;
  nsIFrame *frame = do_QueryFrame(node);
  NS_ASSERTION(frame, "interface not implemented");
  frame = frame->GetNextSibling();
  while (frame) {
    retval = do_QueryFrame(frame);
    if (retval) break;
    frame = frame->GetNextSibling();
  }
  return retval;
}

// -------------------------------------------------------------------------
// Private functions
// -------------------------------------------------------------------------

nsSVGGlyphFrame *
nsSVGTextContainerFrame::GetGlyphFrameAtCharNum(nsISVGGlyphFragmentNode* node,
                                                PRUint32 charnum,
                                                PRUint32 *offset)
{
  nsSVGGlyphFrame *frame = node->GetFirstGlyphFrame();
  *offset = 0;
  
  while (frame) {
    PRUint32 count = frame->GetNumberOfChars();
    if (count > charnum)
      return frame;
    charnum -= count;
    *offset += count;
    frame = frame->GetNextGlyphFrame();
  }

  // not found
  return nsnull;
}

nsSVGTextFrame *
nsSVGTextContainerFrame::GetTextFrame()
{
  for (nsIFrame *frame = this; frame != nsnull; frame = frame->GetParent()) {
    if (frame->GetType() == nsGkAtoms::svgTextFrame) {
      return static_cast<nsSVGTextFrame*>(frame);
    }
  }
  return nsnull;
}

void
nsSVGTextContainerFrame::CopyPositionList(nsTArray<float> *parentList,
                                        SVGUserUnitList *selfList,
                                        nsTArray<float> &dstList,
                                        PRUint32 aOffset)
{
  dstList.Clear();

  PRUint32 strLength = GetNumberOfChars();
  PRUint32 parentCount = 0;
  if (parentList && parentList->Length() > aOffset) {
    parentCount = NS_MIN(parentList->Length() - aOffset, strLength);
  }

  PRUint32 selfCount = NS_MIN(selfList->Length(), strLength);

  PRUint32 count = NS_MAX(parentCount, selfCount);

  if (!dstList.SetLength(count))
    return;

  for (PRUint32 i = 0; i < selfCount; i++) {
    dstList[i] = (*selfList)[i];
  }
  for (PRUint32 i = selfCount; i < parentCount; i++) {
    dstList[i] = (*parentList)[aOffset + i];
  }

}

void
nsSVGTextContainerFrame::CopyRotateList(nsTArray<float> *parentList,
                                        const SVGNumberList *selfList,
                                        nsTArray<float> &dstList,
                                        PRUint32 aOffset)
{
  dstList.Clear();

  PRUint32 strLength = GetNumberOfChars();
  PRUint32 parentCount = 0;
  if (parentList && parentList->Length() > aOffset) {
    parentCount = NS_MIN(parentList->Length() - aOffset, strLength);
  }

  PRUint32 selfCount = NS_MIN(selfList ? selfList->Length() : 0, strLength);
  PRUint32 count = NS_MAX(parentCount, selfCount);

  if (count > 0) {
    if (!dstList.SetLength(count))
      return;
    for (PRUint32 i = 0; i < selfCount; i++) {
      dstList[i] = (*selfList)[i];
    }
    for (PRUint32 i = selfCount; i < parentCount; i++) {
      dstList[i] = (*parentList)[aOffset + i];
    }
  } else if (parentList && !parentList->IsEmpty()) {
    // rotate is applied to extra characters too
    dstList.AppendElement((*parentList)[parentList->Length() - 1]);
  }
}

PRUint32
nsSVGTextContainerFrame::BuildPositionList(PRUint32 aOffset,
                                           PRUint32 aDepth)
{
  nsSVGTextContainerFrame *parent = do_QueryFrame(mParent);
  nsTArray<float> *parentX = nsnull, *parentY = nsnull;
  nsTArray<float> *parentDx = nsnull, *parentDy = nsnull;
  nsTArray<float> *parentRotate = nsnull;
  if (parent) {
    parentX = &(parent->mX);
    parentY = &(parent->mY);
    parentDx = &(parent->mDx);
    parentDy = &(parent->mDy);
    parentRotate = &(parent->mRotate);
  }

  SVGUserUnitList x, y;
  GetXY(&x, &y);
  CopyPositionList(parentX, &x, mX, aOffset);
  CopyPositionList(parentY, &y, mY, aOffset);

  SVGUserUnitList dx, dy;
  GetDxDy(&dx, &dy);
  CopyPositionList(parentDx, &dx, mDx, aOffset);
  CopyPositionList(parentDy, &dy, mDy, aOffset);

  const SVGNumberList *rotate = GetRotate();
  CopyRotateList(parentRotate, rotate, mRotate, aOffset);

  PRUint32 startIndex = 0;
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsSVGTextContainerFrame *text = do_QueryFrame(kid);
    if (text) {
      startIndex += text->BuildPositionList(startIndex, aDepth + 1);
    } else if (kid->GetType() == nsGkAtoms::svgGlyphFrame) {
      nsSVGGlyphFrame *leaf = static_cast<nsSVGGlyphFrame*>(kid);
      leaf->SetStartIndex(startIndex);
      startIndex += leaf->GetNumberOfChars();
    }
    kid = kid->GetNextSibling();
  }
  return startIndex;
}

void
nsSVGTextContainerFrame::GetEffectiveXY(nsTArray<float> &aX,
                                        nsTArray<float> &aY)
{
  aX.AppendElements(mX);
  aY.AppendElements(mY);
}

void
nsSVGTextContainerFrame::GetEffectiveDxDy(nsTArray<float> &aDx,
                                          nsTArray<float> &aDy)
{
  aDx.AppendElements(mDx);
  aDy.AppendElements(mDy);
}

void
nsSVGTextContainerFrame::GetEffectiveRotate(nsTArray<float> &aRotate)
{
  aRotate.AppendElements(mRotate);
}

void
nsSVGTextContainerFrame::SetWhitespaceCompression()
{
  bool compressWhitespace = true;

  for (const nsIFrame *frame = this; frame != nsnull; frame = frame->GetParent()) {
    static const nsIContent::AttrValuesArray strings[] =
      {&nsGkAtoms::preserve, &nsGkAtoms::_default, nsnull};

    PRInt32 index = frame->GetContent()->FindAttrValueIn(
                                           kNameSpaceID_XML,
                                           nsGkAtoms::space,
                                           strings, eCaseMatters);
    if (index == 0) {
      compressWhitespace = false;
      break;
    }
    if (index != nsIContent::ATTR_MISSING ||
        (frame->GetStateBits() & NS_STATE_IS_OUTER_SVG))
      break;
  }

  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();

  while (node) {
    node->SetWhitespaceCompression(compressWhitespace);
    node = GetNextGlyphFragmentChildNode(node);
  }
}
