/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsBulletFrame_h___
#define nsBulletFrame_h___

#include "nsFrame.h"
#include "nsIStyleContext.h"

#include "imgIRequest.h"
#include "imgIDecoderObserver.h"
class gfxIImageFrame;

/**
 * A simple class that manages the layout and rendering of html bullets.
 * This class also supports the CSS list-style properties.
 */
class nsBulletFrame : public nsFrame {
public:
  nsBulletFrame();
  virtual ~nsBulletFrame();

  // nsIFrame
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD Destroy(nsIPresContext* aPresContext);
  NS_IMETHOD Paint(nsIPresContext*      aCX,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags = 0);
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
#endif

  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsIPresContext* aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  // nsBulletFrame
  PRInt32 SetListItemOrdinal(PRInt32 aNextOrdinal, PRBool* aChanged);


  NS_IMETHOD OnStartContainer(imgIRequest *aRequest, nsIPresContext *aCX, imgIContainer *aImage);
  NS_IMETHOD OnDataAvailable(imgIRequest *aRequest, nsIPresContext *aCX, gfxIImageFrame *aFrame, const nsRect * rect);
  NS_IMETHOD OnStopDecode(imgIRequest *aRequest, nsIPresContext *aCX, nsresult aStatus, const PRUnichar *aStatusArg);
  NS_IMETHOD FrameChanged(imgIContainer *aContainer, nsIPresContext *aCX, gfxIImageFrame *aNewframe, nsRect *aDirtyRect);

protected:
  void GetDesiredSize(nsIPresContext* aPresContext,
                      const nsHTMLReflowState& aReflowState,
                      nsHTMLReflowMetrics& aMetrics);

  void GetListItemText(nsIPresContext* aCX,
                       const nsStyleList& aStyleList,
                       nsString& aResult);

  void GetBaseURI(nsIURI **aURI);
  void GetLoadGroup(nsIPresContext *aPresContext, nsILoadGroup **aLoadGroup);

  PRInt32 mOrdinal;
  nsMargin mPadding;
  nsCOMPtr<imgIRequest> mImageRequest;
  nsCOMPtr<imgIDecoderObserver> mListener;

  nsSize mIntrinsicSize;
  nsSize mComputedSize;
};

#endif /* nsBulletFrame_h___ */
