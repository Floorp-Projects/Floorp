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
#ifndef nsImageBoxFrame_h___
#define nsImageBoxFrame_h___

#include "nsLeafBoxFrame.h"

#include "imgILoader.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "nsStubImageDecoderObserver.h"

class nsImageBoxFrame;

class nsImageBoxListener : public nsStubImageDecoderObserver
{
public:
  nsImageBoxListener();
  virtual ~nsImageBoxListener();

  NS_DECL_ISUPPORTS
  // imgIDecoderObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD OnStartContainer(imgIRequest *request, imgIContainer *image);
  NS_IMETHOD OnStopContainer(imgIRequest *request, imgIContainer *image);
  NS_IMETHOD OnStopDecode(imgIRequest *request, nsresult status,
                          const PRUnichar *statusArg);
  // imgIContainerObserver (override nsStubImageDecoderObserver)
  NS_IMETHOD FrameChanged(imgIContainer *aContainer,
                          const nsIntRect *aDirtyRect);

  void SetFrame(nsImageBoxFrame *frame) { mFrame = frame; }

private:
  nsImageBoxFrame *mFrame;
};

class nsImageBoxFrame : public nsLeafBoxFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS

  // nsIBox
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState);
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState);
  virtual void MarkIntrinsicWidthsDirty();

  friend nsIFrame* NS_NewImageBoxFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  // nsIBox frame interface

  NS_IMETHOD  Init(nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIFrame*        asPrevInFlow);

  NS_IMETHOD AttributeChanged(PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType);

  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  virtual nsIAtom* GetType() const;
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  /** 
   * Update mUseSrcAttr from appropriate content attributes or from
   * style, throw away the current image, and load the appropriate
   * image.
   * */
  void UpdateImage();

  /**
   * Update mLoadFlags from content attributes. Does not attempt to reload the
   * image using the new load flags.
   */
  void UpdateLoadFlags();

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  NS_IMETHOD OnStartContainer(imgIRequest *request, imgIContainer *image);
  NS_IMETHOD OnStopContainer(imgIRequest *request, imgIContainer *image);
  NS_IMETHOD OnStopDecode(imgIRequest *request,
                          nsresult status,
                          const PRUnichar *statusArg);
  NS_IMETHOD FrameChanged(imgIContainer *aContainer,
                          const nsIntRect *aDirtyRect);

  virtual ~nsImageBoxFrame();

  void  PaintImage(nsRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsPoint aPt, PRUint32 aFlags);

protected:
  nsImageBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext);

  virtual void GetImageSize();

private:

  nsRect mSubRect; ///< If set, indicates that only the portion of the image specified by the rect should be used.
  nsSize mIntrinsicSize;
  nsSize mImageSize;

  nsCOMPtr<imgIRequest> mImageRequest;
  nsCOMPtr<imgIDecoderObserver> mListener;

  PRInt32 mLoadFlags;

  bool mUseSrcAttr; ///< Whether or not the image src comes from an attribute.
  bool mSuppressStyleCheck;
}; // class nsImageBoxFrame

#endif /* nsImageBoxFrame_h___ */
