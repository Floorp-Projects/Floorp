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

#include "libimg.h"
#include "nsImageNet.h"
#include "ilINetContext.h"
#include "ilINetReader.h"
#include "ilIURL.h"
#include "nsIURL.h"
#include "nsString.h"
#include "il_strm.h"


static NS_DEFINE_IID(kIImageURLIID, IL_IURL_IID);

class ImageURLImpl : public ilIURL {
public:
  ImageURLImpl(const char *aURL);
  ~ImageURLImpl();

  NS_DECL_ISUPPORTS

  virtual void SetReader(ilINetReader *aReader);

  virtual ilINetReader *GetReader();

  virtual int GetContentLength();

  virtual const char* GetAddress();

  virtual time_t GetExpires();

  virtual void SetBackgroundLoad(PRBool aBgload);

private:
  nsIURL *mURL;
  ilINetReader *mReader;
};

ImageURLImpl::ImageURLImpl(const char *aURL)
{
    NS_INIT_REFCNT();
    NS_NewURL(&mURL, aURL);
    mReader = nsnull;
}

ImageURLImpl::~ImageURLImpl()
{
    if (mURL != nsnull) {
        NS_RELEASE(mURL);
    }

    if (mReader != nsnull) {
        NS_RELEASE(mReader);
    }
}

nsresult 
ImageURLImpl::QueryInterface(const nsIID& aIID, 
			     void** aInstancePtr) 
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kIImageURLIID); 
  static NS_DEFINE_IID(kURLIID, NS_IURL_IID);

  if (aIID.Equals(kURLIID)) {
    *aInstancePtr = (void*) mURL;
    mURL->AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kClassIID)) {
    *aInstancePtr = (void*) this;
    AddRef();
    return NS_OK;
  } 
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(ImageURLImpl)

nsrefcnt ImageURLImpl::Release(void)                         
{                                                      
  if (--mRefCnt == 0) {                                
    delete this;                                       
    return 0;                                          
  }                                                    
  return mRefCnt;                                      
}

void 
ImageURLImpl::SetReader(ilINetReader *aReader)
{
    if (mReader != nsnull) {
        NS_RELEASE(mReader);
    }

    mReader = aReader;

    if (mReader != nsnull) {
        NS_ADDREF(mReader);
    }
}

ilINetReader *
ImageURLImpl::GetReader()
{
    if (mReader != nsnull) {
        NS_ADDREF(mReader);
    }
    
    return mReader;
}

int 
ImageURLImpl::GetContentLength()
{
    return 0;
}

const char* 
ImageURLImpl::GetAddress()
{
    if (mURL != nsnull) {
        return mURL->GetSpec();
    }
    else {
        return nsnull;
    }
}

time_t 
ImageURLImpl::GetExpires()
{
    return 0x7FFFFFFF;
}

void 
ImageURLImpl::SetBackgroundLoad(PRBool aBgload)
{
}

nsresult 
NS_NewImageURL(ilIURL **aInstancePtrResult, const char *aURL)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  
  ilIURL *url = new ImageURLImpl(aURL);
  if (url == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return url->QueryInterface(kIImageURLIID, (void **) aInstancePtrResult);
}
