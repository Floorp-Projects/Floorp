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
#ifndef nsFrameImageLoader_h___
#define nsFrameImageLoader_h___

#include "nsSize.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsIFrameImageLoader.h"
#include "nsIPresContext.h"
#include "nsIImageObserver.h"

struct nsRect;

/**
 * An image loader for frame's.
 */
class nsFrameImageLoader : public nsIFrameImageLoader,
                           public nsIImageRequestObserver
{
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIImageRequestObserver
  virtual void Notify(nsIImageRequest *aImageRequest,
                      nsIImage *aImage,
                      nsImageNotification aNotificationType,
                      PRInt32 aParam1, PRInt32 aParam2,
                      void *aParam3);
  virtual void NotifyError(nsIImageRequest *aImageRequest,
                           nsImageError aErrorType);

  // nsFrameImageLoader API
  NS_IMETHOD Init(nsIPresContext* aPresContext,
                  nsIImageGroup* aGroup,
                  const nsString& aURL,
                  const nscolor* aBackgroundColor,
                  const nsSize* aDesiredSize,
                  nsIFrame* aTargetFrame,
                  nsIFrameImageLoaderCB aCallBack,
                  void* aClosure);

  NS_IMETHOD StopImageLoad();

  NS_IMETHOD AbortImageLoad();

  NS_IMETHOD IsSameImageRequest(const nsString& aURL,
                                const nscolor* aBackgroundColor,
                                const nsSize* aDesiredSize,
                                PRBool* aResult);

  NS_IMETHOD AddFrame(nsIFrame* aFrame, nsIFrameImageLoaderCB aCallBack,
                      void* aClosure);

  NS_IMETHOD RemoveFrame(nsIFrame* aFrame);

  // See if its safe to destroy this image loader. Its safe if there
  // are no more frames using the loader and we aren't in the middle
  // of processing a callback.
  NS_IMETHOD SafeToDestroy(PRBool* aResult);

  NS_IMETHOD GetURL(nsString& aResult);

  NS_IMETHOD GetImage(nsIImage** aResult);

  // Return the size of the image, in twips
  NS_IMETHOD GetSize(nsSize& aResult);

  NS_IMETHOD GetImageLoadStatus(PRUint32* aLoadStatus);

protected:
  nsFrameImageLoader();
  virtual ~nsFrameImageLoader();

  void DamageRepairFrames(const nsRect* aDamageRect);

  void NotifyFrames(PRBool aIsSizeUpdate);

  nsIPresContext* mPresContext;
  nsIImageRequest* mImageRequest;
  nsIImage* mImage;

  PRPackedBool mHaveBackgroundColor;
  PRPackedBool mHaveDesiredSize;
  nsString mURL;
  nscolor mBackgroundColor;
  nsSize mDesiredSize;

  nsSize mImageSize;

  PRUint32 mImageLoadStatus;
  nsImageError mImageLoadError;

  PRInt32 mNotifyLockCount;

  struct PerFrameData {
    PerFrameData* mNext;
    nsIFrame* mFrame;
    nsIFrameImageLoaderCB mCallBack;
    void* mClosure;
    PRBool mNeedSizeUpdate;
  };
  PerFrameData* mFrames;

  friend NS_LAYOUT nsresult NS_NewFrameImageLoader(nsIFrameImageLoader**);
};

extern NS_LAYOUT nsresult
  NS_NewFrameImageLoader(nsIFrameImageLoader** aInstancePtrResult);

#endif /* nsFrameImageLoader_h___ */
