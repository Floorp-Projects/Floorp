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

#include "nsIFrameImageLoader.h"
#include "nsSize.h"
#include "nsString.h"

/**
 * An image loader for frame's.
 */
class nsFrameImageLoader : public nsIFrameImageLoader {
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

  // nsIFrameImageLoader
  NS_IMETHOD Init(nsIPresContext* aPresContext,
                  nsIImageGroup* aGroup,
                  const nsString& aURL,
                  nsIFrame* aTargetFrame,
                  PRBool aNeedSizeUpdate);
  NS_IMETHOD StopImageLoad();
  NS_IMETHOD AbortImageLoad();
  NS_IMETHOD GetTargetFrame(nsIFrame*& aFrameResult) const;
  NS_IMETHOD GetURL(nsString& aResult) const;
  NS_IMETHOD GetImage(nsIImage*& aResult) const;
  NS_IMETHOD GetSize(nsSize& aResult) const;
  NS_IMETHOD GetImageLoadStatus(PRIntn& aLoadStatus) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;

protected:
  nsFrameImageLoader();
  ~nsFrameImageLoader();

  void ReflowFrame();

  void DamageRepairFrame();

  nsString mURL;
  nsIImage* mImage;
  nsSize mSize;
  nsImageError mError;
  nsIFrame* mTargetFrame;
  nsIPresContext* mPresContext;
  nsIImageRequest* mImageRequest;
  PRUint8 mImageLoadStatus;

  friend NS_LAYOUT nsresult NS_NewFrameImageLoader(nsIFrameImageLoader**);
};

extern NS_LAYOUT nsresult
  NS_NewFrameImageLoader(nsIFrameImageLoader** aInstancePtrResult);

#endif /* nsFrameImageLoader_h___ */
