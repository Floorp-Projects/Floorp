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
#include "ilIURL.h"
#include "ilINetReader.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "prmem.h"
#include "plstr.h"
#include "il_strm.h"
#include "nsINetService.h"
#include "nsIServiceManager.h"

static NS_DEFINE_IID(kIImageNetContextIID, IL_INETCONTEXT_IID);
static NS_DEFINE_IID(kIURLIID, NS_IURL_IID);
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);

class ImageNetContextSyncImpl : public ilINetContext {
public:
  ImageNetContextSyncImpl(NET_ReloadMethod aReloadPolicy);
  virtual ~ImageNetContextSyncImpl();

  NS_DECL_ISUPPORTS

  virtual ilINetContext* Clone();

  virtual NET_ReloadMethod GetReloadPolicy();

  virtual void AddReferer(ilIURL* aUrl);

  virtual void Interrupt();

  virtual ilIURL* CreateURL(const char*      aUrl, 
			                      NET_ReloadMethod aReloadMethod);

  virtual PRBool IsLocalFileURL(char* aAddress);
#ifdef NU_CACHE
  virtual PRBool IsURLInCache(ilIURL* aUrl);
#else /* NU_CACHE */
  virtual PRBool IsURLInMemCache(ilIURL* aUrl);

  virtual PRBool IsURLInDiskCache(ilIURL* aUrl);
#endif /* NU_CACHE */

  virtual int GetURL(ilIURL*          aUrl,
                     NET_ReloadMethod aLoadMethod,
		                 ilINetReader*    aReader);

  NET_ReloadMethod mReloadPolicy;
};

ImageNetContextSyncImpl::ImageNetContextSyncImpl(NET_ReloadMethod aReloadPolicy)
{
  NS_INIT_REFCNT();
  mReloadPolicy = aReloadPolicy;
}

ImageNetContextSyncImpl::~ImageNetContextSyncImpl()
{
}

NS_IMPL_ISUPPORTS(ImageNetContextSyncImpl, kIImageNetContextIID)

ilINetContext* 
ImageNetContextSyncImpl::Clone()
{
  ilINetContext *cx;

  if (NS_NewImageNetContextSync(&cx) == NS_OK) {
    return cx;
  }

  return nsnull;
}

NET_ReloadMethod 
ImageNetContextSyncImpl::GetReloadPolicy()
{
  return mReloadPolicy;
}

void 
ImageNetContextSyncImpl::AddReferer(ilIURL *aUrl)
{
}

void 
ImageNetContextSyncImpl::Interrupt()
{
}

ilIURL* 
ImageNetContextSyncImpl::CreateURL(const char*      aURL, 
			                             NET_ReloadMethod aReloadMethod)
{
  ilIURL *url;

  if (NS_NewImageURL(&url, aURL, nsnull) == NS_OK) {
    return url;
  }

  return nsnull;
}

PRBool 
ImageNetContextSyncImpl::IsLocalFileURL(char *aAddress)
{
  if (PL_strncasecmp(aAddress, "file:", 5) == 0) {
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }
}

#ifdef NU_CACHE
PRBool 
ImageNetContextSyncImpl::IsURLInCache(ilIURL *aUrl)
{
  return PR_TRUE;
}
#else /* NU_CACHE */
PRBool 
ImageNetContextSyncImpl::IsURLInMemCache(ilIURL *aUrl)
{
  return PR_FALSE;
}

PRBool 
ImageNetContextSyncImpl::IsURLInDiskCache(ilIURL *aUrl)
{
  return PR_FALSE;
}
#endif /* NU_CACHE */

int 
ImageNetContextSyncImpl::GetURL(ilIURL*          aURL, 
			                          NET_ReloadMethod aLoadMethod,
			                          ilINetReader*    aReader)
{
  NS_PRECONDITION(nsnull != aURL, "null URL");
  NS_PRECONDITION(nsnull != aReader, "null reader");
  if (aURL == nsnull || aReader == nsnull) {
    return -1;
  }
  
  aURL->SetReader(aReader);

  PRInt32 status = 0;

  // Get a nsIURL interface
  nsIURL* url = nsnull;
  aURL->QueryInterface(kIURLIID, (void **)&url);

  // Get a network service interface which we'll use to create a stream
  nsINetService *service;
  nsresult res = nsServiceManager::GetService(kNetServiceCID,
                                          kINetServiceIID,
                                          (nsISupports **)&service);

  if (NS_SUCCEEDED(res)) {
    nsIInputStream* stream = nsnull;

    // Initiate a synchronous URL load
    if (NS_SUCCEEDED(service->OpenBlockingStream(url, nsnull, &stream)) &&
        (aReader->StreamCreated(aURL, IL_UNKNOWN) == PR_TRUE)) {

      // Read the URL data
      char      buf[2048];
      PRUint32  count;
      nsresult  result;
      PRBool    first = PR_TRUE;

      result = stream->Read(buf, sizeof(buf), &count);
      while (NS_SUCCEEDED(result) && (count > 0)) {
        if (first == PR_TRUE) {
          PRInt32 ilErr;
  
          ilErr = aReader->FirstWrite((const unsigned char *)buf, (int32)count);
          first = PR_FALSE;
          // If FirstWrite fails then the image type cannot be determined
          if (ilErr != 0) {
            result = NS_ERROR_ABORT;
            break;
          }
        }
                 
        aReader->Write((const unsigned char *)buf, (int32)count);
  
        // Get the next block
        result = stream->Read(buf, sizeof(buf), &count);
      }
  
      if (NS_FAILED(result)) {
        aReader->StreamAbort(-1);
        status = -1;
  
      } else {
        NS_ASSERTION(0 == count, "expected EOF");
        aReader->StreamComplete(PR_FALSE);
      }

    } else {
      aReader->StreamAbort(-1);
      status = -1;
    }

    NS_IF_RELEASE(stream);
    NS_RELEASE(service);

  } else {
    aReader->StreamAbort(-1);
    status = -1;
  }
  
  aReader->NetRequestDone(aURL, status);
  NS_IF_RELEASE(url);

  return 0;
}

nsresult NS_NewImageNetContextSync(ilINetContext **aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  
  ilINetContext *cx = new ImageNetContextSyncImpl(NET_NORMAL_RELOAD);
  if (cx == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return cx->QueryInterface(kIImageNetContextIID, (void **) aInstancePtrResult);
}
