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
#include "nsIImageObserver.h"
class nsIFrame;
class nsIImage;
class nsIImageGroup;
class nsIPresContext;
class nsString;
struct nsSize;

/* a9970300-e918-11d1-89cc-006008911b81 */
#define NS_IFRAME_IMAGE_LOADER_IID \
 { 0xa9970300,0xe918,0x11d1,{0x89, 0xcc, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

/**
 * Abstract interface for frame image loaders. Frame image loaders
 * know how to respond to nsIImageRequestObserver notifications and
 * generate the appropriate rendering/reflow operation for a target
 * frame.
 */
class nsIFrameImageLoader : public nsIImageRequestObserver {
public:
  NS_IMETHOD Init(nsIPresContext* aPresContext,
                  nsIImageGroup* aGroup,
                  const nsString& aURL,
                  nsIFrame* aTargetFrame,
                  PRBool aNeedSizeUpdate) = 0;

  NS_IMETHOD StopImageLoad() = 0;

  NS_IMETHOD AbortImageLoad() = 0;

  NS_IMETHOD GetTargetFrame(nsIFrame*& aFrameResult) const = 0;

  NS_IMETHOD GetURL(nsString& aResult) const = 0;

  NS_IMETHOD GetImage(nsIImage*& aResult) const = 0;

  NS_IMETHOD GetSize(nsSize& aResult) const = 0;

  NS_IMETHOD GetImageLoadStatus(PRIntn& aLoadStatus) const = 0;
};

// Image load status bit values
#define NS_IMAGE_LOAD_STATUS_NONE           0x0
#define NS_IMAGE_LOAD_STATUS_SIZE_REQUESTED 0x1
#define NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE 0x2
#define NS_IMAGE_LOAD_STATUS_IMAGE_READY    0x4
#define NS_IMAGE_LOAD_STATUS_ERROR          0x8

#endif /* nsIFrameImageLoader_h___ */
