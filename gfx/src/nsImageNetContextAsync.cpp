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
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#ifdef NECKO
#include "nsILoadGroup.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#else
#include "nsIURLGroup.h"
#endif
#include "nsITimer.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "prmem.h"
#include "plstr.h"
#include "il_strm.h"
#include "merrors.h"

#ifndef NECKO
#include "nsINetService.h"
#else
#include "nsIIOService.h"
#include "nsIBufferInputStream.h"
#include "nsNeckoUtil.h"
#endif // NECKO

#ifndef NECKO
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
#endif // NECKO

static NS_DEFINE_IID(kIImageNetContextIID, IL_INETCONTEXT_IID);
static NS_DEFINE_IID(kIURLIID, NS_IURL_IID);

#define IMAGE_BUF_SIZE 4096

class ImageConsumer;

class ImageNetContextImpl : public ilINetContext {
public:
  ImageNetContextImpl(NET_ReloadMethod aReloadPolicy,
                      nsILoadGroup* aLoadGroup,
                      nsReconnectCB aReconnectCallback,
                      void* aReconnectArg);
  virtual ~ImageNetContextImpl();

  NS_DECL_ISUPPORTS

  virtual ilINetContext* Clone();

  virtual NET_ReloadMethod GetReloadPolicy();
  virtual NET_ReloadMethod SetReloadPolicy(NET_ReloadMethod ReloadPolicy);


  virtual void AddReferer(ilIURL *aUrl);

  virtual void Interrupt();

  virtual ilIURL* CreateURL(const char *aUrl, 
			    NET_ReloadMethod aReloadMethod);

  virtual PRBool IsLocalFileURL(char *aAddress);
#ifdef NU_CACHE
  virtual PRBool IsURLInCache(ilIURL *aUrl);
#else /* NU_CACHE */
  virtual PRBool IsURLInMemCache(ilIURL *aUrl);

  virtual PRBool IsURLInDiskCache(ilIURL *aUrl);
#endif

  virtual int GetURL (ilIURL * aUrl, NET_ReloadMethod aLoadMethod,
		      ilINetReader *aReader);

#ifdef NECKO
  nsresult RequestDone(ImageConsumer *aConsumer, nsIChannel* channel,
                       nsISupports* ctxt, nsresult status, const PRUnichar* aMsg);
#else
  void RequestDone(ImageConsumer *aConsumer);
#endif

  nsVoidArray *mRequests;
  NET_ReloadMethod mReloadPolicy;
  nsILoadGroup* mLoadGroup;
  nsReconnectCB mReconnectCallback;
  void* mReconnectArg;
};

class ImageConsumer : public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  
  ImageConsumer(ilIURL *aURL, ImageNetContextImpl *aContext);
  
#ifdef NECKO
  // nsIStreamObserver methods:
  NS_DECL_NSISTREAMOBSERVER

  // nsIStreamListener methods:
  NS_DECL_NSISTREAMLISTENER

  void SetKeepPumpingData(nsIChannel* channel, nsISupports* context) {
    NS_IF_RELEASE(mChannel);
    mChannel = channel;
    NS_ADDREF(mChannel);
    NS_IF_RELEASE(mUserContext);
    mUserContext = context;
    if (mUserContext)
        NS_ADDREF(mUserContext);
  }
#else
  NS_IMETHOD GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo);
  NS_IMETHOD OnProgress(nsIURI* aURL, PRUint32 Progress, PRUint32 ProgressMax);
  NS_IMETHOD OnStatus(nsIURI* aURL, const PRUnichar* aMsg);
  NS_IMETHOD OnStartRequest(nsIURI* aURL, const char *aContentType);
  NS_IMETHOD OnDataAvailable(nsIURI* aURL, nsIInputStream *pIStream, PRUint32 length);
  NS_IMETHOD OnStopRequest(nsIURI* aURL, nsresult status, const PRUnichar* aMsg);
#endif
  
  void Interrupt();

  static void KeepPumpingStream(nsITimer *aTimer, void *aClosure);

protected:
  virtual ~ImageConsumer();
  ilIURL *mURL;
  PRBool mInterrupted;
  ImageNetContextImpl *mContext;
  nsIInputStream *mStream;
  nsITimer *mTimer;
  PRBool mFirstRead;
  char *mBuffer;
  PRInt32 mStatus;
#ifdef NECKO
  nsIChannel* mChannel;
  nsISupports* mUserContext;
#endif
};

ImageConsumer::ImageConsumer(ilIURL *aURL, ImageNetContextImpl *aContext)
{
  NS_INIT_REFCNT();
  mURL = aURL;
  NS_ADDREF(mURL);
  mContext = aContext;
  NS_ADDREF(mContext);
  mInterrupted = PR_FALSE;
  mFirstRead = PR_TRUE;
  mStream = nsnull;
  mTimer = nsnull;
  mBuffer = nsnull;
  mStatus = 0;
#ifdef NECKO
  mChannel = nsnull;
  mUserContext = nsnull;
#endif
}

NS_DEFINE_IID(kIStreamNotificationIID, NS_ISTREAMLISTENER_IID);
NS_IMPL_ISUPPORTS(ImageConsumer,kIStreamNotificationIID);

#ifndef NECKO
NS_IMETHODIMP
ImageConsumer::GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo)
{
  return 0;
}

NS_IMETHODIMP
ImageConsumer::OnProgress(nsIURI* aURL, PRUint32 Progress, PRUint32 ProgressMax)
{
  return 0;
}

NS_IMETHODIMP
ImageConsumer::OnStatus(nsIURI* aURL, const PRUnichar* aMsg)
{
  return 0;
}
#endif

NS_IMETHODIMP
#ifdef NECKO
ImageConsumer::OnStartRequest(nsIChannel* channel, nsISupports* aContext)
#else
ImageConsumer::OnStartRequest(nsIURI* aURL, const char *aContentType)
#endif
{
  if (mInterrupted) {
    mStatus = MK_INTERRUPTED;
    return NS_ERROR_ABORT;
  }

  mBuffer = (char *)PR_MALLOC(IMAGE_BUF_SIZE);
  if (mBuffer == nsnull) {
    mStatus = MK_IMAGE_LOSSAGE;
    return NS_ERROR_ABORT;
  }

  ilINetReader *reader = mURL->GetReader();
  if (reader->StreamCreated(mURL, IL_UNKNOWN) != PR_TRUE) {
    mStatus = MK_IMAGE_LOSSAGE;
    reader->StreamAbort(mStatus);
    NS_RELEASE(reader);
    return NS_ERROR_ABORT;
  }
  NS_RELEASE(reader);
    
  return NS_OK;
}

#define IMAGE_BUF_SIZE 4096


NS_IMETHODIMP
#ifdef NECKO
ImageConsumer::OnDataAvailable(nsIChannel* channel, nsISupports* aContext, nsIInputStream *pIStream,
                               PRUint32 offset, PRUint32 length)
#else
ImageConsumer::OnDataAvailable(nsIURI* aURL, nsIInputStream *pIStream, PRUint32 length)
#endif
{
  PRUint32 max_read=0;
  PRUint32 bytes_read = 0, str_length;
  ilINetReader *reader = mURL->GetReader();

  if (mInterrupted || mStatus != 0) {
    mStatus = MK_INTERRUPTED;
    reader->StreamAbort(mStatus);
    NS_RELEASE(reader);
    return NS_ERROR_ABORT;
  }
  
  nsresult err = 0;
  PRUint32 nb;

  do {
    err = reader->WriteReady(&max_read); //max read is most decoder can handle
    if(NS_FAILED(err))   //note length tells how much we already have.
        break;

    if(max_read <= 0){
            max_read =128;
    }

    if (max_read > IMAGE_BUF_SIZE) {
      max_read = IMAGE_BUF_SIZE;
    }
    
    // make sure there's enough data available to decode the image.
    // put test into WriteReady
    if (mFirstRead && length < 4)
      break;

    err = pIStream->Read(mBuffer,
                         IMAGE_BUF_SIZE, &nb);
    if (err == NS_BASE_STREAM_WOULD_BLOCK) {
      err = NS_OK;
      break;
    }
    if (err != NS_OK) {
      break;
    }
    bytes_read += nb;

    if (mFirstRead == PR_TRUE) {
            
      err = reader->FirstWrite((const unsigned char *)mBuffer, nb);
      mFirstRead = PR_FALSE; //? move after err chk?
      /* 
       * If FirstWrite(...) fails then the image type
       * cannot be determined and the il_container 
       * stream functions have not been initialized!
       */
      if (NS_FAILED(err)) {
        mStatus = MK_IMAGE_LOSSAGE;
        mInterrupted = PR_TRUE;
	    NS_RELEASE(reader);
        return NS_ERROR_ABORT;
      }
    }

    err = reader->Write((const unsigned char *)mBuffer, (int32)nb);
	if(NS_FAILED(err)){
        mStatus = MK_IMAGE_LOSSAGE;
        mInterrupted = PR_TRUE;
	    NS_RELEASE(reader);
		return NS_ERROR_ABORT;
	}
  } while(nb != 0);

  if ((NS_OK != err) && (NS_BASE_STREAM_EOF != err)) {
    mStatus = MK_IMAGE_LOSSAGE;
    mInterrupted = PR_TRUE;
  }

  err = pIStream->GetLength(&str_length);

  if ((NS_OK == err) && (bytes_read < str_length)) {
    // If we haven't emptied the stream, hold onto it, because
    // we will need to read from it subsequently and we don't
    // know if we'll get a OnDataAvailable call again.
    //
    // Addref the new stream before releasing the old one,
    // in case it is the same stream!
    NS_ADDREF(pIStream);
    NS_IF_RELEASE(mStream);
    mStream = pIStream;
  }

  NS_RELEASE(reader);
  return NS_OK;
}

void
ImageConsumer::KeepPumpingStream(nsITimer *aTimer, void *aClosure)
{
  nsIURI* url = nsnull;
  ImageConsumer *consumer = (ImageConsumer *)aClosure;
  nsAutoString status;

  if (consumer->mURL) {
    consumer->mURL->QueryInterface(kIURLIID, (void**)&url);
  }
#ifdef NECKO
  consumer->OnStopRequest(consumer->mChannel, consumer->mUserContext,
                          NS_BINDING_SUCCEEDED, status.GetUnicode());
#else
  consumer->OnStopRequest(url, NS_BINDING_SUCCEEDED, status.GetUnicode());
#endif

  NS_IF_RELEASE(url);
}

NS_IMETHODIMP
#ifdef NECKO
ImageConsumer::OnStopRequest(nsIChannel* channel, nsISupports* aContext, nsresult status, const PRUnichar* aMsg)
#else
ImageConsumer::OnStopRequest(nsIURI* aURL, nsresult status, const PRUnichar* aMsg)
#endif
{
  if (mTimer != nsnull) {
    NS_RELEASE(mTimer);
  }

  if (NS_BINDING_SUCCEEDED != status) {
    mStatus = MK_INTERRUPTED;
  }

  // Since we're still holding on to the stream, there's still data
  // that needs to be read. So, pump the stream ourselves.
  if((mStream != nsnull) && (status == NS_BINDING_SUCCEEDED)) {
    PRUint32 str_length;
    nsresult err = mStream->GetLength(&str_length);
    if (err == NS_OK) {
#ifdef NECKO
      err = OnDataAvailable(channel, aContext, mStream, 0, str_length);  // XXX fix offset
      SetKeepPumpingData(channel, aContext);
#else
      err = OnDataAvailable(aURL, mStream, str_length);
#endif
      // If we still have the stream, there's still data to be 
      // pumped, so we set a timer to call us back again.
      if ((err == NS_OK) && (mStream != nsnull)) {
        if ((NS_OK != NS_NewTimer(&mTimer)) ||
            (NS_OK != mTimer->Init(ImageConsumer::KeepPumpingStream, this, 0))) {
          mStatus = MK_IMAGE_LOSSAGE;
          NS_RELEASE(mStream);
        }
        else {
          return NS_OK;
        }
      }
      else {
        mStatus = MK_IMAGE_LOSSAGE;
        NS_IF_RELEASE(mStream);
      }
    }
    else {
      mStatus = MK_IMAGE_LOSSAGE;
      NS_IF_RELEASE(mStream);
    }
  }

  ilINetReader *reader = mURL->GetReader();
  if (0 != mStatus) {
    reader->StreamAbort(mStatus);
  }
  else {
    reader->StreamComplete(PR_FALSE);
  }
  reader->NetRequestDone(mURL, mStatus);
  NS_RELEASE(reader);
  
#ifdef NECKO
  return mContext->RequestDone(this, channel, aContext, status, aMsg);
#else
  mContext->RequestDone(this);
  return NS_OK;
#endif
}

void
ImageConsumer::Interrupt()
{
  mInterrupted = PR_TRUE;
}

ImageConsumer::~ImageConsumer()
{
  NS_IF_RELEASE(mURL);
  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mStream);
  NS_IF_RELEASE(mTimer);
  if (mBuffer != nsnull) {
    PR_DELETE(mBuffer);
  }
#ifdef NECKO
  NS_IF_RELEASE(mChannel);
  NS_IF_RELEASE(mUserContext);
#endif
}

ImageNetContextImpl::ImageNetContextImpl(NET_ReloadMethod aReloadPolicy,
                                         nsILoadGroup* aLoadGroup,
                                         nsReconnectCB aReconnectCallback,
                                         void* aReconnectArg)
{
  NS_INIT_REFCNT();
  mRequests = nsnull;
  mLoadGroup = aLoadGroup;
  NS_IF_ADDREF(mLoadGroup);
  mReloadPolicy = aReloadPolicy;
  mReconnectCallback = aReconnectCallback;
  mReconnectArg = aReconnectArg;
}

ImageNetContextImpl::~ImageNetContextImpl()
{
  if (mRequests != nsnull) {
    int i, count = mRequests->Count();
    for (i=0; i < count; i++) {
      ImageConsumer *ic = (ImageConsumer *)mRequests->ElementAt(i);
          
      NS_RELEASE(ic);
    }
    delete mRequests;
  }
  NS_IF_RELEASE(mLoadGroup);
}

NS_IMPL_ISUPPORTS(ImageNetContextImpl, kIImageNetContextIID)

ilINetContext* 
ImageNetContextImpl::Clone()
{
  ilINetContext *cx;

  if (NS_NewImageNetContext(&cx, mLoadGroup, mReconnectCallback, mReconnectArg) == NS_OK)
  {
    return cx;
  }
  else {
    return nsnull;
  }
}

NET_ReloadMethod 
ImageNetContextImpl::GetReloadPolicy()
{
  return mReloadPolicy;
}

NET_ReloadMethod 
ImageNetContextImpl::SetReloadPolicy(NET_ReloadMethod reloadpolicy)
{
  mReloadPolicy=reloadpolicy;
  return mReloadPolicy;
}

void 
ImageNetContextImpl::AddReferer(ilIURL *aUrl)
{
}

void 
ImageNetContextImpl::Interrupt()
{
  if (mRequests != nsnull) {
    int i, count = mRequests->Count();
    for (i=0; i < count; i++) {
      ImageConsumer *ic = (ImageConsumer *)mRequests->ElementAt(i);
      ic->Interrupt();
    }
  }
}

ilIURL* 
ImageNetContextImpl::CreateURL(const char *aURL, 
                               NET_ReloadMethod aReloadMethod)
{
  ilIURL *url;

  if (NS_NewImageURL(&url, aURL, mLoadGroup) == NS_OK)
  {
    return url;
  }
  else {
    return nsnull;
  }
}

PRBool 
ImageNetContextImpl::IsLocalFileURL(char *aAddress)
{
  if (PL_strncasecmp(aAddress, "file:", 5) == 0) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
}

#ifdef NU_CACHE
PRBool 
ImageNetContextImpl::IsURLInCache(ilIURL *aUrl)
{
  return PR_TRUE;
}
#else /* NU_CACHE */
PRBool 
ImageNetContextImpl::IsURLInMemCache(ilIURL *aUrl)
{
  return PR_FALSE;
}

PRBool 
ImageNetContextImpl::IsURLInDiskCache(ilIURL *aUrl)
{
  return PR_TRUE;
}
#endif /* NU_CACHE */

int 
ImageNetContextImpl::GetURL (ilIURL * aURL, 
                             NET_ReloadMethod aLoadMethod,
                             ilINetReader *aReader)
{
  nsIURI *nsurl;
  NS_PRECONDITION(nsnull != aURL, "null URL");
  NS_PRECONDITION(nsnull != aReader, "null reader");
  if (aURL == nsnull || aReader == nsnull) {
    return -1;
  }

  if (mRequests == nsnull) {
    mRequests = new nsVoidArray();
    if (mRequests == nsnull) {
      // XXX Should still call exit function
      return -1;
    }
  }

  if (aURL->QueryInterface(kIURLIID, (void **)&nsurl) == NS_OK) {
    aURL->SetReader(aReader);

  SetReloadPolicy(aLoadMethod);    
  
    // Find previously created ImageConsumer if possible
    ImageConsumer *ic = new ImageConsumer(aURL, this);
    NS_ADDREF(ic);
        
    // See if a reconnect is being done...(XXX: hack!)
    if (mReconnectCallback && (*mReconnectCallback)(mReconnectArg, ic)) {
      mRequests->AppendElement((void *)ic);
    }
    else {
#ifdef NECKO
      nsCOMPtr<nsIChannel> channel;
      nsresult rv = NS_OpenURI(getter_AddRefs(channel), nsurl, mLoadGroup);

      if (NS_SUCCEEDED(rv)) {
        PRBool bIsBackground = aURL->GetBackgroundLoad();
        if (bIsBackground) {
          channel->SetLoadAttributes(nsIChannel::LOAD_BACKGROUND);
        }
        rv = channel->AsyncRead(0, -1, nsnull, ic);
      }
#else
      nsresult rv = NS_OpenURL(nsurl, ic);
#endif
      if (rv == NS_OK) {
        mRequests->AppendElement((void *)ic);
      }
      else {
        NS_RELEASE(ic);
      }
    }

    NS_RELEASE(nsurl);
  }
    
  return 0;
}

#ifdef NECKO
nsresult
ImageNetContextImpl::RequestDone(ImageConsumer *aConsumer, nsIChannel* channel,
                                 nsISupports* ctxt, nsresult status, const PRUnichar* aMsg)
#else
void 
ImageNetContextImpl::RequestDone(ImageConsumer *aConsumer)
#endif
{
  if (mRequests != nsnull) {
    if (mRequests->RemoveElement((void *)aConsumer) == PR_TRUE) {
      NS_RELEASE(aConsumer);
    }
  }
#ifdef NECKO
///  if (mLoadGroup)
///    return mLoadGroup->RemoveChannel(channel, ctxt, status, aMsg);
///  else
    return NS_OK;
#endif
}

extern "C" NS_GFX_(nsresult)
NS_NewImageNetContext(ilINetContext **aInstancePtrResult,
                      nsILoadGroup* aLoadGroup,
                      nsReconnectCB aReconnectCallback,
                      void* aReconnectArg)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  
  ilINetContext *cx = new ImageNetContextImpl(NET_NORMAL_RELOAD,
                                              aLoadGroup,
                                              aReconnectCallback,
                                              aReconnectArg);
  if (cx == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return cx->QueryInterface(kIImageNetContextIID, (void **) aInstancePtrResult);
}
