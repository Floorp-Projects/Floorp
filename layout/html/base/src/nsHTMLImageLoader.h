/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsHTMLImageLoader_h___
#define nsHTMLImageLoader_h___

#include "nslayout.h"
#include "nsString.h"
#include "nsIPresContext.h"

class nsIFrame;
class nsImageMap;
class nsIImage;
class nsIURL;
struct nsHTMLReflowState;
struct nsHTMLReflowMetrics;
struct nsSize;

class nsHTMLImageLoader;
typedef nsresult (*nsHTMLImageLoaderCB)(nsIPresContext* aPresContext,
                                        nsHTMLImageLoader* aLoader,
                                        nsIFrame* aFrame,
                                        void* aClosure,
                                        PRUint32 aStatus);

/**
 * HTML image loader. This is designed to encapsulate the loading
 * and sizing process of html images (basically so that the logic
 * can be reused in the image button form control code and the html
 * image layout code).
 */
class nsHTMLImageLoader {
public:
  nsHTMLImageLoader();
  ~nsHTMLImageLoader();

  void Init(nsIFrame* aFrame, nsHTMLImageLoaderCB aCallBack, void* aClosure,
            nsIURL* aBaseURL, const nsString& aURLSpec);

  nsIImage* GetImage();

  void GetURLSpec(nsString& aResult) {
    aResult = mURLSpec;
  }

  void UpdateURLSpec(nsIPresContext* aPresContext,
                     const nsString& aNewSpec);

  // Stop the current image load request from loading
  void StopLoadImage(nsIPresContext* aPresContext);

  // Stop all image load requests from loading
  void StopAllLoadImages(nsIPresContext* aPresContext);

  // Get the desired size for the image. If aReflowState is not null
  // then the image will be scaled to fit the reflow
  // constraints. Otherwise, the image will be left at its intrinsic
  // size.
  void GetDesiredSize(nsIPresContext* aPresContext,
                      const nsHTMLReflowState* aReflowState,
                      nsHTMLReflowMetrics& aDesiredSize);

  PRUint32 GetLoadStatus() const;

  PRBool GetLoadImageFailed() const;

  PRBool IsImageSizeKnown() const {
    return mHaveComputedSize;
  }

protected:
  static nsresult ImageLoadCB(nsIPresContext* aPresContext,
                              nsIFrameImageLoader* aLoader,
                              nsIFrame* aFrame,
                              void* aClosure,
                              PRUint32 aStatus);

  void Update(nsIPresContext* aPresContext,
              nsIFrame* aFrame,
              PRUint32 aStatus);

  void SetURL(const nsString& aNewSpec);

  nsresult StartLoadImage(nsIPresContext* aPresContext);

  nsIURL* mBaseURL;
  nsIFrame* mFrame;
  nsHTMLImageLoaderCB mCallBack;
  void* mClosure;

  nsString mURLSpec;
  nsString mURL;

  nsIFrameImageLoader* mImageLoader;

  PRPackedBool mLoadImageFailed;
  PRPackedBool mHaveIntrinsicImageSize;
  PRPackedBool mNeedIntrinsicImageSize;
  PRPackedBool mHaveComputedSize;

  nsSize mIntrinsicImageSize;
  nsSize mComputedImageSize;
};

#endif /* nsHTMLImageLoader_h___ */
