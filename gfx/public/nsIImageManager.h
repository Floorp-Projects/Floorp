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

#ifndef nsIImageManager_h___
#define nsIImageManager_h___

#include <stdio.h>
#include "nsISupports.h"
#include "nscore.h"

typedef enum 
{
  nsImageType_kUnknown = 0,
  nsImageType_kGIF = 1,
  nsImageType_kXBM = 2,
  nsImageType_kJPEG = 3,
  nsImageType_kPPM = 4,
  nsImageType_kPNG = 5,
  nsImageType_kART = 6
} nsImageType;

// IID for the nsIImageManager interface
#define NS_IIMAGEMANAGER_IID    \
{ 0x9f327100, 0xad5a, 0x11d1,   \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

/**
 *
 * Image manager. There is only a single instance, returned when invoking
 * the factory instantiation method. A user of the image library should
 * hold on to the singleton image manager.
 */
class nsIImageManager : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IIMAGEMANAGER_IID)

  /// Initialization method to be called before use
  virtual nsresult Init() = 0;

  /// Set the size (in bytes) image cache maintained by the image manager
  virtual void SetCacheSize(PRInt32 aCacheSize) = 0;

  /// @return the current size of the image cache (in bytes)
  virtual PRInt32 GetCacheSize(void) = 0;
  
  /**
   *  Attempts to release some memory by freeing an image from the image
   *  cache.  This may not always be possible either because all images
   *  in the cache are in use or because the cache is empty.  
   *
   *  @return the new approximate size of the imagelib cache. 
   */
  virtual PRInt32 ShrinkCache(void) = 0;

  /**
   * Flush the image cache as much as possible. The only images not
   * flushed out will be those that are in use.
   */
  NS_IMETHOD FlushCache(PRUint8 img_catagory) = 0;

  /**
   * Determine the type of the image, based on the first few bytes of data.  
   *
   * @param buf - a buffer of image data
   * @param length - the length of the buffer
   *
   * @return the type of the image, if known
   */
 // virtual nsImageType GetImageType(const char *buf, PRInt32 length) = 0;
};

/// Factory method to get a reference to the singleton image manager
extern "C" NS_GFX_(nsresult)
  NS_NewImageManager(nsIImageManager **aInstancePtrResult);

extern "C" NS_GFX_(void)
  NS_FreeImageManager();

#endif
