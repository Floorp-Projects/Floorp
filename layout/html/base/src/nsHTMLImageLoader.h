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
 * The Original Code is mozilla.org code.
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
#ifndef nsHTMLImageLoader_h___
#define nsHTMLImageLoader_h___

#include "nslayout.h"
#include "nsString.h"
#include "nsIPresContext.h"
#include "nsSize.h"

class nsIFrame;
class nsImageMap;
class nsIImage;
class nsIURI;
class nsISizeOfHandler;
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
            nsIURI* aBaseURL, const nsString& aURLSpec);

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
  PRBool GetDesiredSize(nsIPresContext* aPresContext,
                        const nsHTMLReflowState* aReflowState,
                        nsHTMLReflowMetrics& aDesiredSize);

  PRUint32 GetLoadStatus() const;

  PRBool GetLoadImageFailed() const;

  PRBool IsImageSizeKnown() const {
    return mFlags.mHaveComputedSize;
  }

  // Get the intrinsic (natural) size for the image. Returns 0,0 if
  // the dimensions are not known
  void GetIntrinsicSize(nsSize& aSize);
  void GetNaturalImageSize(PRUint32* naturalWidth, PRUint32* naturalHeight);

#ifdef DEBUG
  void SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

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

  nsIURI* mBaseURL;
  nsIFrame* mFrame;
  nsHTMLImageLoaderCB mCallBack;
  void* mClosure;

  nsString mURLSpec;
  nsString mURL;

  nsIFrameImageLoader* mImageLoader;

public:
  struct _indFlags {
    PRUint32 mLoadImageFailed : 1;
    PRUint32 mHaveIntrinsicImageSize : 1;
    PRUint32 mNeedIntrinsicImageSize : 1;
    PRUint32 mAutoImageSize : 1;
    PRUint32 mHaveComputedSize : 1;
    PRUint32 mSquelchCallback : 1;
    PRUint32 mNeedSizeNotification : 1;
    PRUint32 mInRecalcMode : 1; // a special flag used only in very rare circumstances, see nsHTMLImageLoader::GetDesiredSize
  } ;

protected:
  union {
    PRUint32 mAllFlags;
    _indFlags mFlags;
  };

  nsSize mIntrinsicImageSize;
  nsSize mComputedImageSize;
};

#endif /* nsHTMLImageLoader_h___ */
