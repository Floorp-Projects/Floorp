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
  nsImageType_kPNG = 5
} nsImageType;

// IID for the nsIImageManager interface
#define NS_IIMAGEMANAGER_IID    \
{ 0x9f327100, 0xad5a, 0x11d1,   \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

//----------------------------------------------------------------------
// 
// Image manager. There is only a single instance, created when the
// class is initially loaded. 
class nsIImageManager : public nsISupports
{
public:
  virtual nsresult Init() = 0;

  // Set and get the cache size in bytes
  virtual void SetCacheSize(PRInt32 aCacheSize) = 0;
  virtual PRInt32 GetCacheSize(void) = 0;
  
  // Attempts to release some memory by freeing an image from the image
  // cache.  This may not always be possible either because all images
  // in the cache are in use or because the cache is empty.  Returns the
  // new approximate size of the imagelib cache. 
  virtual PRInt32 ShrinkCache(void) = 0;

  // Determine the type of the image, based on the first few bytes of data.  
  virtual nsImageType GetImageType(const char *buf, PRInt32 length) = 0;
};

extern NS_GFX nsresult
  NS_NewImageManager(nsIImageManager **aInstancePtrResult);

#endif
