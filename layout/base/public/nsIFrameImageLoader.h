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
#ifndef nsIFrameImageLoader_h___
#define nsIFrameImageLoader_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsColor.h"

class nsIFrame;
class nsIImage;
class nsIImageGroup;
class nsIPresContext;
class nsString;
struct nsSize;

/* a6cf90ec-15b3-11d2-932e-00805f8add32 */
#define NS_IFRAME_IMAGE_LOADER_IID \
 { 0xa6cf90ec, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// Type of callback function used during image loading. The frame
// image loader will invoke this callback as notifications occur from
// the image library.
class nsIFrameImageLoader;
typedef nsresult (*nsIFrameImageLoaderCB)(nsIPresContext* aPresContext,
                                          nsIFrameImageLoader* aLoader,
                                          nsIFrame* aFrame,
                                          void* aClosure,
                                          PRUint32 aStatus);

/**
 * Abstract interface for frame image loaders. Frame image loaders
 * know how to respond to nsIImageRequestObserver notifications and
 * generate the appropriate rendering/reflow operation for a target
 * frame.
 */
class nsIFrameImageLoader : public nsISupports {
public:
  NS_IMETHOD Init(nsIPresContext* aPresContext,
                  nsIImageGroup* aGroup,
                  const nsString& aURL,
                  const nscolor* aBackgroundColor,
                  const nsSize* aDesiredSize,
                  nsIFrame* aFrame,
                  nsIFrameImageLoaderCB aCallBack,
                  void* aClosure) = 0;

  NS_IMETHOD StopImageLoad() = 0;

  NS_IMETHOD AbortImageLoad() = 0;

  NS_IMETHOD IsSameImageRequest(const nsString& aURL,
                                const nscolor* aBackgroundColor,
                                const nsSize* aDesiredSize,
                                PRBool* aResult) = 0;

  NS_IMETHOD AddFrame(nsIFrame* aFrame, nsIFrameImageLoaderCB aCallBack,
                      void* aClosure) = 0;

  NS_IMETHOD RemoveFrame(nsIFrame* aFrame) = 0;

  NS_IMETHOD SafeToDestroy(PRBool* aResult) = 0;

  NS_IMETHOD GetURL(nsString& aResult) = 0;

  NS_IMETHOD GetImage(nsIImage** aResult) = 0;

  // Return the size of the image, in twips
  NS_IMETHOD GetSize(nsSize& aResult) = 0;

  NS_IMETHOD GetImageLoadStatus(PRUint32* aLoadStatus) = 0;
};

// Image load status bit values
#define NS_IMAGE_LOAD_STATUS_NONE             0x0
#define NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE   0x1
#define NS_IMAGE_LOAD_STATUS_IMAGE_READY      0x2
#define NS_IMAGE_LOAD_STATUS_ERROR            0x4

#endif /* nsIFrameImageLoader_h___ */
