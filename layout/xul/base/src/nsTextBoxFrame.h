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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Annema <disttsc@bart.nl>
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
#ifndef nsTextBoxFrame_h___
#define nsTextBoxFrame_h___

#include "nsLeafBoxFrame.h"

class nsAccessKeyInfo;

typedef nsLeafBoxFrame nsTextBoxFrameSuper;
class nsTextBoxFrame : public nsTextBoxFrameSuper
{
public:

  // nsIBox
  NS_IMETHOD GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize);
  NS_IMETHOD GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent);
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD NeedsRecalc();

  enum CroppingStyle { CropNone, CropLeft, CropRight, CropCenter };

  friend nsresult NS_NewTextBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD  Init(nsPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsStyleContext*  aContext,
                   nsIFrame*        asPrevInFlow);

  NS_IMETHOD Destroy(nsPresContext* aPresContext);

  NS_IMETHOD AttributeChanged(nsPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  virtual void UpdateAttributes(nsPresContext*  aPresContext,
                                nsIAtom*         aAttribute,
                                PRBool&          aResize,
                                PRBool&          aRedraw);


  NS_IMETHOD Paint(nsPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);


  virtual ~nsTextBoxFrame();
protected:

  void UpdateAccessTitle();
  void UpdateAccessIndex();

  NS_IMETHOD PaintTitle(nsPresContext*      aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect&        aDirtyRect,
                        const nsRect&        aRect);

  virtual void LayoutTitle(nsPresContext*      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect&        aRect);

  virtual void CalculateUnderline(nsIRenderingContext& aRenderingContext);

  virtual void CalcTextSize(nsBoxLayoutState& aBoxLayoutState);

  nsTextBoxFrame(nsIPresShell* aShell);

  virtual void CalculateTitleForWidth(nsPresContext*      aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      nscoord              aWidth);

  virtual void GetTextSize(nsPresContext*      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsString&      aString,
                           nsSize&              aSize,
                           nscoord&             aAscent);

  nsresult RegUnregAccessKey(nsPresContext* aPresContext,
                             PRBool          aDoReg);

private:

  PRBool  AlwaysAppendAccessKey();

  CroppingStyle mCropType;
  nsString mTitle;
  nsString mCroppedTitle;
  nsString mAccessKey;
  nscoord mTitleWidth;
  nsAccessKeyInfo* mAccessKeyInfo;
  PRBool mNeedsRecalc;
  nsSize mTextSize;
  nscoord mAscent;

  static PRBool gAlwaysAppendAccessKey;
  static PRBool gAccessKeyPrefInitialized;

}; // class nsTextBoxFrame

#endif /* nsTextBoxFrame_h___ */
