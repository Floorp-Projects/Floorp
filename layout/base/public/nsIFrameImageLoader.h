/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

enum nsImageAnimation {
    eImageAnimation_Normal      = 0,            // looping controlled by image
    eImageAnimation_None        = 1,            // don't loop; just show first frame
    eImageAnimation_LoopOnce    = 2             // loop just once
};

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
  static const nsIID& GetIID() {static nsIID iid = NS_IFRAME_IMAGE_LOADER_IID; return iid;}

  NS_IMETHOD Init(nsIPresContext* aPresContext,
                  nsIImageGroup* aGroup,
                  const nsString& aURL,
                  const nscolor* aBackgroundColor,
                  const nsSize* aDesiredSize,
                  nsIFrame* aFrame,
                  nsImageAnimation aAnimationMode,
                  nsIFrameImageLoaderCB aCallBack,
                  void* aClosure, void* aKey) = 0;

  NS_IMETHOD StopImageLoad(PRBool aStopChrome = PR_TRUE) = 0;

  NS_IMETHOD AbortImageLoad() = 0;

  NS_IMETHOD IsSameImageRequest(const nsString& aURL,
                                const nscolor* aBackgroundColor,
                                const nsSize* aDesiredSize,
                                PRBool* aResult) = 0;

  NS_IMETHOD AddFrame(nsIFrame* aFrame, nsIFrameImageLoaderCB aCallBack,
                      void* aClosure, void* aKey) = 0;

  NS_IMETHOD RemoveFrame(void* aKey) = 0;

  NS_IMETHOD SafeToDestroy(PRBool* aResult) = 0;

  NS_IMETHOD GetURL(nsString& aResult) = 0;
  NS_IMETHOD GetPresContext(nsIPresContext** aPresContext) = 0;

  NS_IMETHOD GetImage(nsIImage** aResult) = 0;

  // Return the size of the image, in twips
  NS_IMETHOD GetSize(nsSize& aResult) = 0;

  NS_IMETHOD GetImageLoadStatus(PRUint32* aLoadStatus) = 0;

  // Return the intrinsic (or natural) size of the image, in twips.
  // Returns 0,0 if the dimensions are unknown
  NS_IMETHOD GetIntrinsicSize(nsSize& aResult) = 0;
  NS_IMETHOD GetNaturalImageSize(PRUint32* naturalWidth, PRUint32 *naturalHeight) = 0;
};

// Image load status bit values
#define NS_IMAGE_LOAD_STATUS_NONE             0x0
#define NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE   0x1
#define NS_IMAGE_LOAD_STATUS_IMAGE_READY      0x2
#define NS_IMAGE_LOAD_STATUS_ERROR            0x4

#endif /* nsIFrameImageLoader_h___ */
