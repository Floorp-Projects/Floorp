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

#ifndef nsIImageGroup_h___
#define nsIImageGroup_h___

#include <stdio.h>
#include "nsISupports.h"
#include "nscore.h"
#include "nsColor.h"

class nsIImageGroupObserver;
class nsIImageRequestObserver;
class nsIImageRequest;
class nsIDeviceContext;
class nsIStreamListener;
class nsILoadGroup;

/** For important images, like backdrops. */
#define nsImageLoadFlags_kHighPriority  0x01   
/** Don't throw this image out of cache.  */
#define nsImageLoadFlags_kSticky        0x02   
/** Don't get image out of image cache.   */
#define nsImageLoadFlags_kBypassCache   0x04   
/** Don't load if image cache misses.     */
#define nsImageLoadFlags_kOnlyFromCache 0x08   


// IID for the nsIImageGroup interface
#define NS_IIMAGEGROUP_IID    \
{ 0xbe927e40, 0xaeaa, 0x11d1,  \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

/**
 *
 * Image group. A convenient way to group a set of image load requests
 * and control them as a group.
 */
class nsIImageGroup : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IIMAGEGROUP_IID)

  /**
   * Initialize an image group with a device context. All images
   * in this group will be decoded for the specified device context.
   */
  virtual nsresult Init(nsIDeviceContext *aDeviceContext, nsISupports *aLoadContext) = 0;

  /** 
   * Add an observers to be informed of image group notifications.
   *
   * @param aObserver - An observer to add to the observer list.
   * @param boolean indicating whether addition was successful.
   */
  virtual PRBool AddObserver(nsIImageGroupObserver *aObserver) = 0;

  /**
   * Remove a previously added observer from the observer list.
   *
   * @param aObserver - An observer to remove from the observer list.
   * @param boolean indicating whether the removal was successful.
   */ 
  virtual PRBool RemoveObserver(nsIImageGroupObserver *aObserver) = 0;

  /**
   *  Fetch the image corresponding to the specified URL. 
   * 
   *  @param aUrl - the URL of the image to be loaded.
   *  @param aObserver - the observer is notified at significant 
   *       points in image loading.
   *  @param aWidth, aHeight - These parameters specify the target 
   *       dimensions of the image.  The image will be stretched 
   *       horizontally, vertically or both to meet these parameters.  
   *       If both width and height are zero, the image is decoded 
   *       using its "natural" size.  If only one of width and height 
   *       is zero, the image is scaled to the provided dimension, with
   *       the unspecified dimension scaled to maintain the image's 
   *       original aspect ratio.
   *  @param aBackgroundColor - If the background color is NULL, a mask 
   *       will be generated for any transparent images.  If background 
   *       color is non-NULL, it indicates the RGB value to be painted 
   *       into the image for "transparent" areas of the image, in which 
   *       case no mask is created.  This is intended for backdrops and 
   *       printing.
   *  @param aFlags - image loading flags are specified above
   *
   *  @return the resulting image request object.
   */
  virtual nsIImageRequest* GetImage(const char* aUrl, 
                                    nsIImageRequestObserver *aObserver,
                                    const nscolor* aBackgroundColor,
                                    PRUint32 aWidth, PRUint32 aHeight,
                                    PRUint32 aFlags) = 0;

  /**
   * Like GetImage except load the image from a live stream.
   * The call returns an nsIImageRequest and an nsIStreamListener
   * that should be connected to the live stream to accept
   * the image data.
   */
  NS_IMETHOD GetImageFromStream(const char* aURL,
                                nsIImageRequestObserver *aObserver,
                                const nscolor* aBackgroundColor,
                                PRUint32 aWidth, PRUint32 aHeight,
                                PRUint32 aFlags,
                                nsIImageRequest*& aResult,
                                nsIStreamListener*& aListenerResult) = 0;

  /**
   *  Halt decoding of images or animation without destroying associated
   *  pixmap data. All ImageRequests created with this ImageGroup
   *  are interrupted.
   */
  virtual void Interrupt(void) = 0;

  NS_IMETHOD SetImgLoadAttributes(PRUint32 a_grouploading_attribs)=0;
  NS_IMETHOD GetImgLoadAttributes(PRUint32 *a_grouploading_attribs)=0;

};

/// Factory method for creating an image group
extern "C" NS_GFX_(nsresult)
  NS_NewImageGroup(nsIImageGroup **aInstancePtrResult);                   

#endif
