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

#ifndef nsIImageObserver_h___
#define nsIImageObserver_h___

#include <stdio.h>
#include "nsISupports.h"
#include "nscore.h"

class nsIImage;
class nsIImageRequest;
class nsIImageGroup;

/// Image request notifications
typedef enum {
  nsImageNotification_kStartURL,      // Start of decode/display for URL.
  nsImageNotification_kDescription,   // Availability of image description.
  nsImageNotification_kDimensions,    // Availability of image dimensions. 
  nsImageNotification_kIsTransparent, // This image is transparent. 
  nsImageNotification_kPixmapUpdate,  // Change in a rectangular area of 
                                      // pixels.
  nsImageNotification_kFrameComplete, // Completion of a frame of an animated
                                      // image.
  nsImageNotification_kProgress,      // Notification of percentage decoded.
  nsImageNotification_kImageComplete, // Completion of image decoding.  There
                                      // may be multiple instances of this
                                      // event per URL due to server push,
                                      // client pull or looping GIF animation.
  nsImageNotification_kStopURL,       // Completion of decode/display for URL.
  nsImageNotification_kImageDestroyed,// Finalization of an image request. This
                                      // is an indication to perform any
                                      // observer related cleanup.
  nsImageNotification_kAborted,       // Image decode was aborted by either
                                      // the network library or Interrupt().
  nsImageNotification_kInternalImage // Internal image icon.
} nsImageNotification;

/// Image group notifications
typedef enum {

  // Start of image loading. Sent when  a loading image is 
  // added to an image group which currently has no loading images.
  nsImageGroupNotification_kStartedLoading,

  // Some images were aborted.  A finished loading message will not be sent 
  // until the aborted images have been destroyed.
  nsImageGroupNotification_kAbortedLoading, 

  // End of image loading. Sent when the last of the images currently 
  // in the image group has finished loading.
  nsImageGroupNotification_kFinishedLoading, 

  // Start of image looping. Sent when an animated image starts looping in 
  // an image group which currently has no looping animations.
  nsImageGroupNotification_kStartedLooping,  

  // End of image looping. Sent when the last of the images currently in 
  // the image group has finished looping.
  nsImageGroupNotification_kFinishedLooping  

} nsImageGroupNotification;

/// Image loading errors
typedef enum {
  nsImageError_kNotInCache,           // Image URL not available in cache when 
                                      // kOnlyFromCache flag was set.
  nsImageError_kNoData,               // Network library unable to fetch 
                                      // provided URL.
  nsImageError_kImageDataCorrupt,     // Checksum error of some kind in 
                                      // image data.
  nsImageError_kImageDataTruncated,   // Missing data at end of stream.
  nsImageError_kImageDataIllegal,     // Generic image data error.
  nsImageError_kInternalError         // Internal Image Library error.
  
} nsImageError;

// IID for the nsIImageRequestObserver interface
#define NS_IIMAGEREQUESTOBSERVER_IID    \
{ 0x965467a0, 0xb8f4, 0x11d1,           \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

// IID for the nsIImageGroupObserver interface
#define NS_IIMAGEGROUPOBSERVER_IID    \
{ 0xb3cad300, 0xb8f4, 0x11d1,         \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

/**
 * Image request observer interface. The implementor will be notified
 * of significant loading events or loading errors.
 */
class nsIImageRequestObserver : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IIMAGEREQUESTOBSERVER_IID)

  /** 
   * Notify the observer of some significant image event. The parameter
   * values depend on the notification type as specified below.
   *
   * kDescription - aParam3 is a string containing the human readable
   *                description of an image, e.g. "GIF89a 320 x 240 pixels".  
   *                The string storage is static, so it must be copied if
   *                it is to be preserved after the call to the observer. 
   * kDimensions - aParam1 and aParam2 are the width and height respectively
   *               of the image in pixels.
   * kPixmapUpdate - aParame3 is a pointer to a nsRect struct containing the
   *                 rectangular area of pixels which has been modified by 
   *                 the image library.  This notification enables the 
   *                 client to drive the progressive display of the image.
   * kProgress - aParam1 represents the estimated percentage decoded.  This
   *             notification occurs at unspecified intervals.  Provided 
   *             that decoding proceeds without error, it is guaranteed that
   *             notification will take place on completion with a 
   *             percent_progress value of 100.
   *
   * @param aImageRequest - the image request in question
   * @param aImage - the corresponding image object
   * @param aNotificationType - the type of notification
   * @param aParam1, aParam2, aParam3 - additional information as described
   *       above.
   */
  virtual void Notify(nsIImageRequest *aImageRequest,
                      nsIImage *aImage,
                      nsImageNotification aNotificationType,
                      PRInt32 aParam1, PRInt32 aParam2,
                      void *aParam3)=0;

  /**
   *  Notify the observer of an error during image loading.
   *
   *  @param aImageRequest - the image request in question
   *  @param aErrorType - the error code
   */
  virtual void NotifyError(nsIImageRequest *aImageRequest,
                           nsImageError aErrorType)=0;
};

/**
 * Image group observer interface. The implementor will be notified
 * of significant image group events.
 */
class nsIImageGroupObserver : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IIMAGEGROUPOBSERVER_IID)

  /**
   *  Notify the observer of some significant image group event.
   *
   *  @param aImageGroup - the image group in question
   *  @param aNotificationType - the notification code
   */
  virtual void Notify(nsIImageGroup *aImageGroup,
                      nsImageGroupNotification aNotificationType)=0;
};

#endif
