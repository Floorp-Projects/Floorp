/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SVGTEXTCONTAINERFRAME_H
#define NS_SVGTEXTCONTAINERFRAME_H

#include "nsFrame.h"
#include "nsIFrame.h"
#include "nsISVGChildFrame.h"
#include "nsQueryFrame.h"
#include "nsSVGContainerFrame.h"
#include "nsTArray.h"

class nsFrameList;
class nsIDOMSVGPoint;
class nsIDOMSVGRect;
class nsISVGGlyphFragmentNode;
class nsStyleContext;
class nsSVGGlyphFrame;
class nsSVGTextFrame;

class nsSVGTextContainerFrame : public nsSVGDisplayContainerFrame
{
protected:
  nsSVGTextContainerFrame(nsStyleContext* aContext) :
    nsSVGDisplayContainerFrame(aContext) {}

public:
  void NotifyGlyphMetricsChange();
  virtual void GetXY(SVGUserUnitList *aX, SVGUserUnitList *aY);
  virtual void GetDxDy(SVGUserUnitList *aDx, SVGUserUnitList *aDy);
  virtual const SVGNumberList *GetRotate();
  
  NS_DECL_QUERYFRAME_TARGET(nsSVGTextContainerFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  // nsIFrame
  NS_IMETHOD InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList);
  NS_IMETHOD RemoveFrame(ChildListID aListID, nsIFrame *aOldFrame);

  NS_IMETHOD GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval);
  NS_IMETHOD GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval);
  NS_IMETHOD GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval);
  NS_IMETHOD GetRotationOfChar(PRUint32 charnum, float *_retval);

  /*
   * Returns the number of characters in a string
   */
  virtual PRUint32 GetNumberOfChars();

  /*
   * Determines the length of a string
   */
  virtual float GetComputedTextLength();

  /*
   * Determines the length of a substring
   */
  virtual float GetSubStringLength(PRUint32 charnum, PRUint32 nchars);

  /*
   * Get the character at the specified position
   */
  virtual PRInt32 GetCharNumAtPosition(nsIDOMSVGPoint *point);
  void GetEffectiveXY(nsTArray<float> &aX, nsTArray<float> &aY);
  void GetEffectiveDxDy(nsTArray<float> &aDx, nsTArray<float> &aDy);
  void GetEffectiveRotate(nsTArray<float> &aRotate);

protected:
  /*
   * Returns the first child node for a frame
   */
  nsISVGGlyphFragmentNode *
  GetFirstGlyphFragmentChildNode();

  /*
   * Returns the next child node for a frame
   */
  nsISVGGlyphFragmentNode *
  GetNextGlyphFragmentChildNode(nsISVGGlyphFragmentNode *node);

  void CopyPositionList(nsTArray<float> *parentList,
                        SVGUserUnitList *selfList,
                        nsTArray<float> &dstList,
                        PRUint32 aOffset);
  void CopyRotateList(nsTArray<float> *parentList,
                      const SVGNumberList *selfList,
                      nsTArray<float> &dstList,
                      PRUint32 aOffset);
  PRUint32 BuildPositionList(PRUint32 aOffset, PRUint32 aDepth);

  void SetWhitespaceCompression();
private:
  /*
   * Returns the glyph frame containing a particular character
   */
  static nsSVGGlyphFrame *
  GetGlyphFrameAtCharNum(nsISVGGlyphFragmentNode* node,
                         PRUint32 charnum,
                         PRUint32 *offset);

  /*
   * Returns the text frame ancestor of this frame (or the frame itself
   * if this is a text frame)
   */
  nsSVGTextFrame * GetTextFrame();
  nsTArray<float> mX;
  nsTArray<float> mY;
  nsTArray<float> mDx;
  nsTArray<float> mDy;
  nsTArray<float> mRotate;
};

#endif
