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

#include "nsIImageManager.h"
#include "libimg.h"
#include "il_strm.h"
#include "nsCRT.h"
#include "nsImageNet.h"

static NS_DEFINE_IID(kIImageManagerIID, NS_IIMAGEMANAGER_IID);

class ImageManagerImpl : public nsIImageManager {
public:
  static ImageManagerImpl *sTheImageManager;

  ImageManagerImpl();
  ~ImageManagerImpl();

  nsresult Init();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  virtual void SetCacheSize(PRInt32 aCacheSize);
  virtual PRInt32 GetCacheSize(void);
  virtual PRInt32 ShrinkCache(void);
  virtual nsImageType GetImageType(const char *buf, PRInt32 length);

private:
  ilISystemServices *mSS;
};

ImageManagerImpl* ImageManagerImpl::sTheImageManager = nsnull;

ImageManagerImpl::ImageManagerImpl()
{
  NS_NewImageSystemServices(&mSS);
  NS_ADDREF(mSS);
  IL_Init(mSS);
}

ImageManagerImpl::~ImageManagerImpl()
{
  IL_Shutdown();
  NS_RELEASE(mSS);
}

NS_IMPL_ISUPPORTS(ImageManagerImpl, kIImageManagerIID)

nsresult 
ImageManagerImpl::Init()
{
  return NS_OK;
}

void  
ImageManagerImpl::SetCacheSize(PRInt32 aCacheSize)
{
  IL_SetCacheSize(aCacheSize);
}

PRInt32 
ImageManagerImpl::GetCacheSize()
{
  return IL_GetCacheSize();
}
 
PRInt32 
ImageManagerImpl::ShrinkCache(void)
{
  return IL_ShrinkCache();
}
 
nsImageType 
ImageManagerImpl::GetImageType(const char *buf, PRInt32 length)
{
  int ret;

  NS_PRECONDITION(nsnull != buf, "null ptr");
  ret = IL_Type(buf, length);
  
  switch(ret) {
    case(IL_GIF):
      return nsImageType_kGIF;
    case(IL_XBM):
      return nsImageType_kXBM;
    case(IL_JPEG):
      return nsImageType_kJPEG;
    case(IL_PPM):
      return nsImageType_kPPM;
    case(IL_PNG):
      return nsImageType_kPNG;
    default:
      return nsImageType_kUnknown;
  }
}

NS_GFX nsresult
NS_NewImageManager(nsIImageManager **aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  if (ImageManagerImpl::sTheImageManager == nsnull) {
    ImageManagerImpl::sTheImageManager = new ImageManagerImpl();
  }

  if (ImageManagerImpl::sTheImageManager == nsnull) {
        return NS_ERROR_OUT_OF_MEMORY;
  }

  return ImageManagerImpl::sTheImageManager->QueryInterface(kIImageManagerIID, 
                                                (void **) aInstancePtrResult);
}
