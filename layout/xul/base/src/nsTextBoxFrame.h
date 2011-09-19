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
class nsAsyncAccesskeyUpdate;

typedef nsLeafBoxFrame nsTextBoxFrameSuper;
class nsTextBoxFrame : public nsTextBoxFrameSuper
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIBox
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState);
  NS_IMETHOD DoLayout(nsBoxLayoutState& aBoxLayoutState);
  virtual void MarkIntrinsicWidthsDirty();

  enum CroppingStyle { CropNone, CropLeft, CropRight, CropCenter };

  friend nsIFrame* NS_NewTextBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD  Init(nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIFrame*        asPrevInFlow);

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  NS_IMETHOD AttributeChanged(PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  void UpdateAttributes(nsIAtom*         aAttribute,
                        PRBool&          aResize,
                        PRBool&          aRedraw);

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  virtual ~nsTextBoxFrame();

  void PaintTitle(nsRenderingContext& aRenderingContext,
                  const nsRect&        aDirtyRect,
                  nsPoint              aPt,
                  const nscolor*       aOverrideColor);

  nsRect GetComponentAlphaBounds();

  virtual PRBool ComputesOwnOverflowArea();

protected:
  friend class nsAsyncAccesskeyUpdate;
  friend class nsDisplayXULTextBox;
  // Should be called only by nsAsyncAccesskeyUpdate.
  // Returns PR_TRUE if accesskey was updated.
  PRBool UpdateAccesskey(nsWeakFrame& aWeakThis);
  void UpdateAccessTitle();
  void UpdateAccessIndex();

  // REVIEW: SORRY! Couldn't resist devirtualizing these
  void LayoutTitle(nsPresContext*      aPresContext,
                   nsRenderingContext& aRenderingContext,
                   const nsRect&        aRect);

  void CalculateUnderline(nsRenderingContext& aRenderingContext);

  void CalcTextSize(nsBoxLayoutState& aBoxLayoutState);

  void CalcDrawRect(nsRenderingContext &aRenderingContext);

  nsTextBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext);

  nscoord CalculateTitleForWidth(nsPresContext*      aPresContext,
                                 nsRenderingContext& aRenderingContext,
                                 nscoord              aWidth);

  void GetTextSize(nsPresContext*      aPresContext,
                   nsRenderingContext& aRenderingContext,
                   const nsString&      aString,
                   nsSize&              aSize,
                   nscoord&             aAscent);

  nsresult RegUnregAccessKey(PRBool aDoReg);

private:

  PRBool AlwaysAppendAccessKey();
  PRBool InsertSeparatorBeforeAccessKey();

  void DrawText(nsRenderingContext& aRenderingContext,
                const nsRect&       aDirtyRect,
                const nsRect&       aTextRect,
                const nscolor*      aOverrideColor);

  nsString mTitle;
  nsString mCroppedTitle;
  nsString mAccessKey;
  nsSize mTextSize;
  nsRect mTextDrawRect;
  nsAccessKeyInfo* mAccessKeyInfo;

  CroppingStyle mCropType;
  nscoord mAscent;
  PRPackedBool mNeedsRecalc;
  PRPackedBool mNeedsReflowCallback;

  static PRBool gAlwaysAppendAccessKey;
  static PRBool gAccessKeyPrefInitialized;
  static PRBool gInsertSeparatorBeforeAccessKey;
  static PRBool gInsertSeparatorPrefInitialized;

}; // class nsTextBoxFrame

#endif /* nsTextBoxFrame_h___ */
