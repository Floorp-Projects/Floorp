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

#ifndef nsIImageGroup_h___
#define nsIImageGroup_h___

#include <stdio.h>
#include "nsISupports.h"
#include "nscore.h"
#include "nsColor.h"

class nsIImageGroupObserver;
class nsIImageRequestObserver;
class nsIImageRequest;
class nsIRenderingContext;

/* For important images, like backdrops. */
#define nsImageLoadFlags_kHighPriority  0x01   
/* Don't throw this image out of cache.  */
#define nsImageLoadFlags_kSticky        0x02   
/* Don't get image out of image cache.   */
#define nsImageLoadFlags_kBypassCache   0x04   
/* Don't load if image cache misses.     */
#define nsImageLoadFlags_kOnlyFromCache 0x08   

// IID for the nsIImageGroup interface
#define NS_IIMAGEGROUP_IID    \
{ 0xbe927e40, 0xaeaa, 0x11d1,  \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

//----------------------------------------------------------------------
// 
// Image group. A convenient way to group a set of image load requests
// and control them as a group.
class nsIImageGroup : public nsISupports
{
public:
  virtual nsresult Init(nsIRenderingContext *aRenderingContext) = 0;

  // Add and remove observers to listen in on image group notifications
  virtual PRBool AddObserver(nsIImageGroupObserver *aObserver) = 0;
  virtual PRBool RemoveObserver(nsIImageGroupObserver *aObserver) = 0;

  // Fetch the image corresponding to the specified URL. 
  // 
  // The observer is notified at significant points in image loading.
  // 
  // The width and height parameters specify the target dimensions of
  // the image.  The image will be stretched horizontally, vertically or
  // both to meet these parameters.  If both width and height are zero,
  // the image is decoded using its "natural" size.  If only one of
  // width and height is zero, the image is scaled to the provided
  // dimension, with the unspecified dimension scaled to maintain the
  // image's original aspect ratio.
  //  
  // If the background color is NULL, a mask will be generated for any
  // transparent images.  If background color is non-NULL, it indicates 
  // the RGB value to be painted into the image for "transparent" areas 
  // of the image, in which case no mask is created.  This is intended 
  // for backdrops and printing.
  //
  // Image load flags are specified above
  virtual nsIImageRequest* GetImage(const char* aUrl, 
                                    nsIImageRequestObserver *aObserver,
                                    nscolor aBackgroundColor,
                                    PRUint32 aWidth, PRUint32 aHeight,
                                    PRUint32 aFlags) = 0;

  // Halt decoding of images or animation without destroying associated
  // pixmap data. All ImageRequests created with this ImageGroup
  // are interrupted.
  virtual void Interrupt(void) = 0;
};

extern NS_GFX nsresult
  NS_NewImageGroup(nsIImageGroup **aInstancePtrResult);                   

#endif
