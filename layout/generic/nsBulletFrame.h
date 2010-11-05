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

/* rendering object for list-item bullets */

#ifndef nsBulletFrame_h___
#define nsBulletFrame_h___

#include "nsFrame.h"
#include "nsStyleContext.h"

#include "imgIRequest.h"
#include "imgIDecoderObserver.h"

/**
 * A simple class that manages the layout and rendering of html bullets.
 * This class also supports the CSS list-style properties.
 */
class nsBulletFrame : public nsFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  nsBulletFrame(nsStyleContext* aContext) : nsFrame(aContext) {}
  virtual ~nsBulletFrame();

  // nsIFrame
  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);
  virtual nsIAtom* GetType() const;
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);
#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);

  // nsBulletFrame
  PRInt32 SetListItemOrdinal(PRInt32 aNextOrdinal, PRBool* aChanged);


  NS_IMETHOD OnStartContainer(imgIRequest *aRequest, imgIContainer *aImage);
  NS_IMETHOD OnDataAvailable(imgIRequest *aRequest,
                             PRBool aCurrentFrame,
                             const nsIntRect *aRect);
  NS_IMETHOD OnStopDecode(imgIRequest *aRequest,
                          nsresult aStatus,
                          const PRUnichar *aStatusArg);
  NS_IMETHOD FrameChanged(imgIContainer *aContainer,
                          const nsIntRect *aDirtyRect);

  /* get list item text, without '.' */
  static PRBool AppendCounterText(PRInt32 aListStyleType,
                                  PRInt32 aOrdinal,
                                  nsString& aResult);

  /* get list item text, with '.' */
  PRBool GetListItemText(const nsStyleList& aStyleList,
                         nsString& aResult);
                         
  void PaintBullet(nsIRenderingContext& aRenderingContext, nsPoint aPt,
                   const nsRect& aDirtyRect);
  
  virtual PRBool IsEmpty();
  virtual PRBool IsSelfEmpty();
  virtual nscoord GetBaseline() const;

protected:
  void GetDesiredSize(nsPresContext* aPresContext,
                      nsIRenderingContext *aRenderingContext,
                      nsHTMLReflowMetrics& aMetrics);

  void GetLoadGroup(nsPresContext *aPresContext, nsILoadGroup **aLoadGroup);

  nsMargin mPadding;
  nsCOMPtr<imgIRequest> mImageRequest;
  nsCOMPtr<imgIDecoderObserver> mListener;

  nsSize mIntrinsicSize;
  nsSize mComputedSize;
  PRInt32 mOrdinal;
  PRBool mTextIsRTL;
};

#endif /* nsBulletFrame_h___ */
