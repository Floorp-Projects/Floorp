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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
//#include "il_strm.h"

#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsCRT.h"
#include "nsIServiceManager.h"

static NS_DEFINE_IID(kIImageNetContextIID, IL_INETCONTEXT_IID);
static NS_DEFINE_IID(kIURLIID, NS_IURL_IID);

static NS_DEFINE_IID(kIIOServiceIID, NS_IIOSERVICE_IID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

class ImageNetContextSyncImpl : public ilINetContext {
public:
  ImageNetContextSyncImpl(ImgCachePolicy aReloadPolicy);
  virtual ~ImageNetContextSyncImpl();

  NS_DECL_ISUPPORTS

  virtual ilINetContext* Clone();

  virtual ImgCachePolicy GetReloadPolicy();

  virtual void AddReferer(ilIURL* aUrl);

  virtual void Interrupt();

  virtual ilIURL* CreateURL(const char*      aUrl, 
			                      ImgCachePolicy aReloadMethod);

  virtual PRBool IsLocalFileURL(char* aAddress);
#ifdef NU_CACHE
  virtual PRBool IsURLInCache(ilIURL* aUrl);
#else /* NU_CACHE */
  virtual PRBool IsURLInMemCache(ilIURL* aUrl);

  virtual PRBool IsURLInDiskCache(ilIURL* aUrl);
#endif /* NU_CACHE */

  virtual int GetURL(ilIURL* aUrl,ImgCachePolicy aLoadMethod,
		             ilINetReader* aReader, PRBool IsAnimationLoop);

  virtual int GetContentLength (ilIURL * aURL);

  ImgCachePolicy mReloadPolicy;
};

ImageNetContextSyncImpl::ImageNetContextSyncImpl(ImgCachePolicy aReloadPolicy)
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

ImgCachePolicy 
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
			                             ImgCachePolicy aReloadMethod)
{
  ilIURL *url;

  if (NS_NewImageURL(&url, aURL, nsnull) == NS_OK)
  {
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
ImageNetContextSyncImpl::GetContentLength (ilIURL * aURL)
{

    return 0;
}


int 
ImageNetContextSyncImpl::GetURL(ilIURL*  aURL, 
			                     ImgCachePolicy aLoadMethod,
			                     ilINetReader* aReader,
                                 PRBool IsAnimationLoop)
{
  NS_PRECONDITION(nsnull != aURL, "null URL");
  NS_PRECONDITION(nsnull != aReader, "null reader");
  if (aURL == nsnull || aReader == nsnull) {
    return -1;
  }
  
  aURL->SetReader(aReader);

  PRInt32 status = 0;

  // Get a nsIURI interface
  nsIURI* url = nsnull;
  aURL->QueryInterface(kIURLIID, (void **)&url);

  nsresult res;

  // Get a network service interface which we'll use to create a stream
  NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &res);

  if (NS_SUCCEEDED(res)) {
    nsIInputStream* stream = nsnull;


    nsIURI *uri = nsnull;
    nsresult rv;
    rv = url->QueryInterface(NS_GET_IID(nsIURI), (void**)&uri);
    if (NS_FAILED(rv)) return -1;

    nsIChannel *channel = nsnull;
    rv = service->NewChannelFromURI(uri, &channel);
    NS_RELEASE(uri);
    if (NS_FAILED(rv)) 
        return -1;

    char* aContentType = NULL;
    rv = channel->GetContentType(&aContentType); //nsCRT alloc's str
    if (NS_FAILED(rv)) {
        if(aContentType){
            nsCRT::free(aContentType);
        }
        aContentType = nsCRT::strdup("unknown");        
    }
    if(nsCRT::strlen(aContentType) > 50){
        //somethings wrong. mimetype string shouldn't be this big.
        //protect us from the user.
        nsCRT::free(aContentType);
        aContentType = nsCRT::strdup("unknown"); 
    } 

    rv = channel->OpenInputStream(&stream);
    NS_RELEASE(channel);
    if (NS_SUCCEEDED(rv)) {

      if (aReader->StreamCreated(aURL, aContentType) == PR_TRUE) {

        // Read the URL data
        char      buf[2048];
        PRUint32  count;
        nsresult  result;
        PRBool    first = PR_TRUE;

		char* uriStr = NULL;
	    uriStr = aURL->GetAddress();
        result = stream->Read(buf, sizeof(buf), &count);
        while (NS_SUCCEEDED(result) && (count > 0)) {
          if (first == PR_TRUE) {
            PRInt32 ilErr;
  
            ilErr = aReader->FirstWrite((const unsigned char *)buf, (int32)count, uriStr);
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

		nsCRT::free(uriStr);
  
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

    } else {
      aReader->StreamAbort(-1);
      status = -1;
    }

    nsCRT::free(aContentType);

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
  
  //Note default of USE_IMG_CACHE used.
  ilINetContext *cx = new ImageNetContextSyncImpl(USE_IMG_CACHE);
  if (cx == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return cx->QueryInterface(kIImageNetContextIID, (void **) aInstancePtrResult);
}
