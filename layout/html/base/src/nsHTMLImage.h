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
#ifndef nsHTMLImage_h___
#define nsHTMLImage_h___

#include "nslayout.h"
#include "nsIFrameImageLoader.h"
class nsIFrame;
class nsIFrameImageLoader;
class nsIPresContext;
struct nsReflowState;
struct nsReflowMetrics;
struct nsSize;

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

  void DestroyLoader() {
    NS_IF_RELEASE(mImageLoader);
  }

  nsIImage* GetImage();

  nsresult SetURL(const nsString& aURLSpec);

  nsresult LoadImage(nsIPresContext* aPresContext,
                     nsIFrame* aForFrame,
                     PRBool aNeedSizeUpdate,
                     PRIntn& aLoadStatus);

  void GetDesiredSize(nsIPresContext* aPresContext,
                      const nsReflowState& aReflowState,
                      const nsSize& aMaxSize,
                      nsReflowMetrics& aDesiredSize);

protected:
  nsIFrameImageLoader* mImageLoader;
  PRPackedBool mLoadImageFailed;
  PRPackedBool mLoadBrokenImageFailed;
  nsString* mURLSpec;
};

#endif /* nsHTMLImage_h___ */
