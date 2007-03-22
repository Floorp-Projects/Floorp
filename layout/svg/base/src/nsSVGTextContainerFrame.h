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

#include "nsSVGContainerFrame.h"
#include "nsIDOMSVGLengthList.h"
#include "nsISVGTextContentMetrics.h"

class nsISVGGlyphFragmentNode;
class nsISVGGlyphFragmentLeaf;

class nsSVGTextFrame;

class nsSVGTextContainerFrame : public nsSVGDisplayContainerFrame,
                                public nsISVGTextContentMetrics
{
public:
  nsSVGTextContainerFrame(nsStyleContext* aContext) :
    nsSVGDisplayContainerFrame(aContext) {}

  void UpdateGraphic();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetX();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetY();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDx();
  NS_IMETHOD_(already_AddRefed<nsIDOMSVGLengthList>) GetDy();
  
   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  

public:
  // nsIFrame
  NS_IMETHOD RemoveFrame(nsIAtom *aListName, nsIFrame *aOldFrame);

  // nsISVGTextContentMetrics
  NS_IMETHOD GetNumberOfChars(PRInt32 *_retval);
  NS_IMETHOD GetComputedTextLength(float *_retval);
  NS_IMETHOD GetSubStringLength(PRUint32 charnum, PRUint32 nchars, float *_retval);
  NS_IMETHOD GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval);
  NS_IMETHOD GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval);
  NS_IMETHOD GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval);
  NS_IMETHOD GetRotationOfChar(PRUint32 charnum, float *_retval);
  NS_IMETHOD GetCharNumAtPosition(nsIDOMSVGPoint *point, PRInt32 *_retval);

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

  /*
   * Set Whitespace handling
   */
  void SetWhitespaceHandling();

  /*
   * Returns the number of characters in a string
   */
  PRUint32 GetNumberOfChars();

  /*
   * Determines the length of a string
   */
  float GetComputedTextLength();

  /*
   * Determines the length of a substring
   */
  float GetSubStringLengthNoValidation(PRUint32 charnum, PRUint32 nchars);

  /*
   * Get the character at the specified position
   */
  PRInt32 GetCharNumAtPosition(nsIDOMSVGPoint *point);

private:
  /*
   * Returns the glyph fragment containing a particular character
   */
  static nsISVGGlyphFragmentLeaf *
  GetGlyphFragmentAtCharNum(nsISVGGlyphFragmentNode* node,
                            PRUint32 charnum,
                            PRUint32 *offset);

  /*
   * Returns the text frame ancestor of this frame (or the frame itself
   * if this is a text frame)
   */
  nsSVGTextFrame * GetTextFrame();
};

#endif
