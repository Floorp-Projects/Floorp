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
#include "nsIURLGroup.h"
#include "nsILoadAttribs.h"
#include "nsString.h"
#include "il_strm.h"


static NS_DEFINE_IID(kIImageURLIID, IL_IURL_IID);

class ImageURLImpl : public ilIURL {
public:
  ImageURLImpl(void);
  virtual ~ImageURLImpl();

  NS_DECL_ISUPPORTS

  nsresult Init(const char *aURL, nsIURLGroup* aURLGroup);

  virtual void SetReader(ilINetReader *aReader);

  virtual ilINetReader *GetReader();

  virtual int GetContentLength();

  virtual const char* GetAddress();

  virtual time_t GetExpires();

  virtual void SetBackgroundLoad(PRBool aBgload);

  virtual int GetOwnerId();

  virtual void SetOwnerId(int aOwnderId);

private:
  nsIURL *mURL;
  ilINetReader *mReader;
};

ImageURLImpl::ImageURLImpl(void)
    : mURL(nsnull), mReader(nsnull)
{
    NS_INIT_REFCNT();
}

nsresult 
ImageURLImpl::Init(const char *aURL, nsIURLGroup* aURLGroup)
{
  nsresult rv;
  if (nsnull != aURLGroup) {
    rv = aURLGroup->CreateURL(&mURL, nsnull, aURL, nsnull);
  } else {
    rv = NS_NewURL(&mURL, aURL);
  }
  return rv;
}

ImageURLImpl::~ImageURLImpl()
{
  NS_IF_RELEASE(mURL);
  NS_IF_RELEASE(mReader);
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
    NS_ADDREF(mURL);
    return NS_OK;
  }
  if (aIID.Equals(kClassIID)) {
    *aInstancePtr = (void*) this;
    NS_ADDREF_THIS();
    return NS_OK;
  } 
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(ImageURLImpl)

nsrefcnt ImageURLImpl::Release(void)                         
{                                                      
  if (--mRefCnt == 0) {                                
    NS_DELETEXPCOM(this);
    return 0;                                          
  }                                                    
  return mRefCnt;                                      
}

void 
ImageURLImpl::SetReader(ilINetReader *aReader)
{
  NS_IF_RELEASE(mReader);

  mReader = aReader;

  NS_IF_ADDREF(mReader);
}

ilINetReader *
ImageURLImpl::GetReader()
{
  NS_IF_ADDREF(mReader);
    
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
        const char* spec;
        mURL->GetSpec(&spec);
        return spec;
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
  nsILoadAttribs* loadAttributes;

  if (nsnull != mURL) {
    nsresult rv = mURL->GetLoadAttribs(&loadAttributes);
    if (rv == NS_OK && nsnull != loadAttributes) {
      if (aBgload) {
        loadAttributes->SetLoadType(nsURLLoadBackground);
      } else {
        loadAttributes->SetLoadType(nsURLLoadNormal);
      }
      NS_RELEASE(loadAttributes);
    }
  }
}

int
ImageURLImpl::GetOwnerId()
{
    return 0;
}

void
ImageURLImpl::SetOwnerId(int aOwnerId)
{
}

extern "C" NS_GFX_(nsresult)
NS_NewImageURL(ilIURL **aInstancePtrResult, const char *aURL, 
               nsIURLGroup* aURLGroup)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  
  ImageURLImpl *url = new ImageURLImpl();
  if (url == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = url->Init(aURL, aURLGroup);
  if (rv != NS_OK) {
    delete url;
    return rv;
  }

  return url->QueryInterface(kIImageURLIID, (void **) aInstancePtrResult);
}
