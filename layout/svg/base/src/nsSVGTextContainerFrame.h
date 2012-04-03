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
