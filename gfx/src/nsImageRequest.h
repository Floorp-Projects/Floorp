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

#ifndef nsImageRequest_h___
#define nsImageRequest_h___

#include "nsIImageRequest.h"
#include "libimg.h"
#include "nsCRT.h"
#include "nsColor.h"

class nsVoidArray;
class nsIImageRequestObserver;
class ilINetContext;

class ImageRequestImpl : public nsIImageRequest {
public:
  ImageRequestImpl();
  virtual ~ImageRequestImpl();
  
  nsresult Init(IL_GroupContext *aGroupContext, const char* aUrl, 
                nsIImageRequestObserver *aObserver,
                const nscolor* aBackgroundColor,
                PRUint32 aWidth, PRUint32 aHeight,
                PRUint32 aFlags,
                ilINetContext* aNetContext);

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  // Get the image associated with the request.
  virtual nsIImage* GetImage();
  
  // Return the natural dimensions of the image.  Returns 0,0 
  //if the dimensions are unknown.
  virtual void GetNaturalDimensions(PRUint32 *aWidth, PRUint32 *aHeight);

  // Add and remove observers to listen in on image loading notifications
  virtual PRBool AddObserver(nsIImageRequestObserver *aObserver);
  virtual PRBool RemoveObserver(nsIImageRequestObserver *aObserver);

  // Interrupt loading of just this image.
  virtual void Interrupt();

  // XXX These should go: fix ns_observer_proc to be a static method
  IL_ImageReq *GetImageRequest() { return mImageReq; }
  void SetImageRequest(IL_ImageReq *aImageReq) { mImageReq = aImageReq; }
  nsVoidArray *GetObservers() { return mObservers; }

  void ImageDestroyed();

private:
  IL_ImageReq *mImageReq;
  IL_GroupContext *mGroupContext;
  nsVoidArray *mObservers;
  XP_ObserverList mXPObserver;
};

#endif
