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

#include "libimg.h"
#include "nsImageNet.h"
#include "ilINetContext.h"
#include "ilIURL.h"
#include "ilINetReader.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsILoadGroup.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"
#include "prprf.h"


#include "nsITimer.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "prmem.h"
#include "plstr.h"

#include "merrors.h"

#include "nsNetUtil.h"

#include "nsCURILoader.h"
#include "nsIURIContentListener.h"
#include "nsIHTTPChannel.h"
#include "nsIStreamConverterService.h"
#include "nsIPref.h"

static NS_DEFINE_CID(kStreamConvServiceCID, NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_IID(kIImageNetContextIID, IL_INETCONTEXT_IID);
static NS_DEFINE_IID(kIURLIID, NS_IURL_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

PRLogModuleInfo *image_net_context_async_log_module = NULL;

#define IMAGE_BUF_SIZE 4096

class ImageConsumer;

class ImageNetContextImpl : public ilINetContext {
public:
  ImageNetContextImpl(ImgCachePolicy aReloadPolicy,
                      nsISupports * aLoadContext,
                      nsReconnectCB aReconnectCallback,
                      void* aReconnectArg);
  virtual ~ImageNetContextImpl();

  NS_DECL_ISUPPORTS

  virtual ilINetContext* Clone();

  virtual ImgCachePolicy GetReloadPolicy();
  virtual ImgCachePolicy SetReloadPolicy(ImgCachePolicy ReloadPolicy);


  virtual void AddReferer(ilIURL *aUrl);

  virtual void Interrupt();

  virtual ilIURL* CreateURL(const char *aUrl, 
			    ImgCachePolicy aReloadMethod);

  virtual PRBool IsLocalFileURL(char *aAddress);
#ifdef NU_CACHE
  virtual PRBool IsURLInCache(ilIURL *aUrl);
#else /* NU_CACHE */
  virtual PRBool IsURLInMemCache(ilIURL *aUrl);

  virtual PRBool IsURLInDiskCache(ilIURL *aUrl);
#endif

  virtual int GetURL (ilIURL * aUrl, ImgCachePolicy aLoadMethod,
		      ilINetReader *aReader, PRBool IsAnimationLoop);

  virtual int GetContentLength(ilIURL * aURL);

  nsresult RequestDone(ImageConsumer *aConsumer, nsIChannel* channel,
                       nsISupports* ctxt, nsresult status, const PRUnichar* aMsg);

  nsVoidArray *mRequests;
  ImgCachePolicy mReloadPolicy;
  nsWeakPtr mLoadContext;
  nsReconnectCB mReconnectCallback;
  void* mReconnectArg;
};

class ImageConsumer : public nsIStreamListener, public nsIURIContentListener, public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  
  ImageConsumer(ilIURL *aURL, ImageNetContextImpl *aContext);
  
  // nsIStreamObserver methods:
  NS_DECL_NSISTREAMOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIURICONTENTLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR

  void SetKeepPumpingData(nsIChannel* channel, nsISupports* context) {
    NS_ADDREF(channel);
    NS_IF_RELEASE(mChannel);
    mChannel = channel;

    NS_IF_ADDREF(context);
    NS_IF_RELEASE(mUserContext);
    mUserContext = context;
  }
  
  void Interrupt();

  static void KeepPumpingStream(nsITimer *aTimer, void *aClosure);

protected:
  virtual ~ImageConsumer();
  ilIURL *mURL;
  PRBool mInterrupted;
  ImageNetContextImpl *mContext;
  nsIInputStream *mStream;
  nsCOMPtr<nsITimer> mTimer;
  PRBool mFirstRead;
  char *mBuffer;
  PRInt32 mStatus;
  nsIChannel* mChannel;
  nsISupports* mUserContext;
  PRBool mIsMulti;
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
  mBuffer = nsnull;
  mStatus = 0;
  mChannel = nsnull;
  mUserContext = nsnull;
  mIsMulti = PR_FALSE;
}

NS_IMPL_THREADSAFE_ADDREF(ImageConsumer)
NS_IMPL_THREADSAFE_RELEASE(ImageConsumer)

NS_INTERFACE_MAP_BEGIN(ImageConsumer)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
   NS_INTERFACE_MAP_ENTRY(nsIStreamObserver)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
NS_INTERFACE_MAP_END

NS_IMETHODIMP ImageConsumer::GetInterface(const nsIID & aIID, void * *aInstancePtr)
{
   NS_ENSURE_ARG_POINTER(aInstancePtr);
   return QueryInterface(aIID, aInstancePtr);
}

// nsIURIContentListener support

NS_IMETHODIMP 
ImageConsumer::OnStartURIOpen(nsIURI* aURI, 
   const char* aWindowTarget, PRBool* aAbortOpen)
{
   return NS_OK;
}

NS_IMETHODIMP
ImageConsumer::GetProtocolHandler(nsIURI *aURI, nsIProtocolHandler **aProtocolHandler)
{
  *aProtocolHandler = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
ImageConsumer::GetParentContentListener(nsIURIContentListener** aParent)
{
  *aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
ImageConsumer::SetParentContentListener(nsIURIContentListener* aParent)
{
  return NS_OK;
}

NS_IMETHODIMP 
ImageConsumer::GetLoadCookie(nsISupports ** aLoadCookie)
{
  nsCOMPtr<nsISupports> loadContext = do_QueryReferent(mContext->mLoadContext);
  *aLoadCookie = loadContext;
  NS_IF_ADDREF(*aLoadCookie);
  return NS_OK;
}

NS_IMETHODIMP 
ImageConsumer::SetLoadCookie(nsISupports * aLoadCookie)
{
  return NS_OK;
}

NS_IMETHODIMP 
ImageConsumer::IsPreferred(const char * aContentType,
                                nsURILoadCommand aCommand,
                                const char * aWindowTarget,
                                char ** aDesiredContentType,
                                PRBool * aCanHandleContent)

{
  return CanHandleContent(aContentType, aCommand, aWindowTarget, 
                          aDesiredContentType, aCanHandleContent);
}

NS_IMETHODIMP 
ImageConsumer::CanHandleContent(const char * aContentType,
                                nsURILoadCommand aCommand,
                                const char * aWindowTarget,
                                char ** aDesiredContentType,
                                PRBool * aCanHandleContent)

{
  // if we had a webshell or doc shell around, we'd pass this call
  // through to it...but we don't =(
  if (!nsCRT::strcasecmp(aContentType, "message/rfc822"))
    *aDesiredContentType = nsCRT::strdup("text/xul");
  // since we explicilty loaded the url, we always want to handle it!
  *aCanHandleContent = PR_TRUE;
    
  return NS_OK;
} 

NS_IMETHODIMP 
ImageConsumer::DoContent(const char * aContentType,
                      nsURILoadCommand aCommand,
                      const char * aWindowTarget,
                      nsIChannel * aOpenedChannel,
                      nsIStreamListener ** aContentHandler,
                      PRBool * aAbortProcess)
{
  nsresult rv = NS_OK;
  if (aAbortProcess)
    *aAbortProcess = PR_FALSE;

  nsAutoString contentType; contentType.AssignWithConversion(aContentType);

  if (contentType.EqualsWithConversion("multipart/x-mixed-replace")
      || contentType.EqualsWithConversion("multipart/mixed")) {
    // if we're getting multipart data, we have to convert it.
    // so wedge the converter inbetween us and the consumer.
    mIsMulti= PR_TRUE;

    nsCOMPtr<nsIStreamConverterService> convServ = do_GetService(kStreamConvServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsAutoString astrix; astrix.AssignWithConversion("*/*");
    return convServ->AsyncConvertData(contentType.GetUnicode(),
                                   astrix.GetUnicode(),
                                   NS_STATIC_CAST(nsIStreamListener*, this),
                                   nsnull /*a context?*/, aContentHandler);
  }

  QueryInterface(NS_GET_IID(nsIStreamListener), (void **) aContentHandler);
  return rv;
}


NS_IMETHODIMP
ImageConsumer::OnStartRequest(nsIChannel* channel, nsISupports* aContext)
{
  PRUint32 httpStatus;
  if (mInterrupted) {
    mStatus = MK_INTERRUPTED;
    return NS_ERROR_ABORT;
  }

  mBuffer = (char *)PR_MALLOC(IMAGE_BUF_SIZE);
  if (mBuffer == nsnull) {
    mStatus = MK_IMAGE_LOSSAGE;
    return NS_ERROR_ABORT;
  }

  nsCOMPtr<nsIHTTPChannel> pHTTPCon(do_QueryInterface(channel));
  if (pHTTPCon) {
    pHTTPCon->GetResponseStatus(&httpStatus);
    if (httpStatus == 404) {
      mStatus = MK_IMAGE_LOSSAGE;
      return NS_ERROR_ABORT;
    }
  }

  ilINetReader *reader = mURL->GetReader(); //ptn test: nsCOMPtr??
  nsresult err= reader->FlushImgBuffer(); //flush current data in buffer before starting 

  nsresult rv = NS_OK;
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

  if (reader->StreamCreated(mURL, aContentType) != PR_TRUE) {
    mStatus = MK_IMAGE_LOSSAGE;
    reader->StreamAbort(mStatus);
    NS_RELEASE(reader);
    nsCRT::free(aContentType);
    return NS_ERROR_ABORT;
  }
  nsCRT::free(aContentType);
  NS_RELEASE(reader);
    
  return NS_OK;
}

#define IMAGE_BUF_SIZE 4096


NS_IMETHODIMP
ImageConsumer::OnDataAvailable(nsIChannel* channel, nsISupports* aContext, nsIInputStream *pIStream,
                               PRUint32 offset, PRUint32 length)
{
  PRUint32 max_read=0;
  PRUint32 bytes_read = 0;
  ilINetReader *reader = mURL->GetReader();

  if (mInterrupted || mStatus != 0) {
    mStatus = MK_INTERRUPTED;
    reader->StreamAbort(mStatus);
    NS_RELEASE(reader);
    return NS_ERROR_ABORT;
  }

  nsresult err = 0;
  PRUint32 nb = 0;
  char* uriStr = NULL;
  nsCOMPtr<nsIURI> uri;

  err = channel->GetURI(getter_AddRefs(uri));
  if (NS_SUCCEEDED(err)) {
      err = uri->GetSpec(&uriStr);
      if (NS_FAILED(err)){
          /* if we can't get a file spec, it is bad, very bad.*/
          mStatus = MK_INTERRUPTED;
          reader->StreamAbort(mStatus);
          NS_RELEASE(reader);
          return NS_ERROR_ABORT;
      }
  }

  do {
    err = reader->WriteReady(&max_read); //max read is most decoder can handle
    if(NS_FAILED(err))   //note length tells how much we already have.
        break;

    if(max_read < 0){
            max_read = 128;
    }

    if (max_read > (length - bytes_read)) {
      max_read = length - bytes_read;
    }

    if (max_read > IMAGE_BUF_SIZE) {
      max_read = IMAGE_BUF_SIZE;
    }
    
    // make sure there's enough data available to decode the image.
    // put test into WriteReady
    if (mFirstRead && length < 4)
      break;

    err = pIStream->Read(mBuffer,
                         max_read, &nb);

    if (err == NS_BASE_STREAM_WOULD_BLOCK) {
      NS_ASSERTION(nb == 0, "Data will be lost.");
      err = NS_OK;
      break;
    }
    if (NS_FAILED(err) || nb == 0) {
      NS_ASSERTION(nb == 0, "Data will be lost.");
      break;
    }

    bytes_read += nb;
    if (mFirstRead == PR_TRUE) {
            
      err = reader->FirstWrite((const unsigned char *)mBuffer, nb, uriStr);
      mFirstRead = PR_FALSE; //? move after err chk?
      /* 
       * If FirstWrite(...) fails then the image type
       * cannot be determined and the il_container 
       * stream functions have not been initialized!
       */
      if (NS_FAILED(err)) {
        mStatus = MK_IMAGE_LOSSAGE;
        mInterrupted = PR_TRUE;
        if(uriStr)
	        nsCRT::free(uriStr);
	    NS_RELEASE(reader);
        return NS_ERROR_ABORT;
      }
    }

    err = reader->Write((const unsigned char *)mBuffer, (int32)nb);
    if(NS_FAILED(err)){
      mStatus = MK_IMAGE_LOSSAGE;
      mInterrupted = PR_TRUE;
      if(uriStr)
	      nsCRT::free(uriStr);
	  NS_RELEASE(reader);
      return NS_ERROR_ABORT;
    }
  } while(bytes_read < length);

  if (uriStr) {
	nsCRT::free(uriStr);
  }

  if (NS_FAILED(err)) {
    mStatus = MK_IMAGE_LOSSAGE;
    mInterrupted = PR_TRUE;
  }

  if (bytes_read < length) {
    // If we haven't emptied the stream, hold onto it, because
    // we will need to read from it subsequently and we don't
    // know if we'll get a OnDataAvailable call again.
    //
    // Addref the new stream before releasing the old one,
    // in case it is the same stream!
    NS_ADDREF(pIStream);
    NS_IF_RELEASE(mStream);
    mStream = pIStream;
  } else {
    NS_IF_RELEASE(mStream);
  }
  NS_RELEASE(reader);
  return NS_OK;
}

void
ImageConsumer::KeepPumpingStream(nsITimer *aTimer, void *aClosure)
{
  ImageConsumer *consumer = (ImageConsumer *)aClosure;
  nsAutoString status;

  consumer->OnStopRequest(consumer->mChannel, consumer->mUserContext,
                          NS_BINDING_SUCCEEDED, status.GetUnicode());
}

NS_IMETHODIMP
ImageConsumer::OnStopRequest(nsIChannel* channel, nsISupports* aContext, nsresult status, const PRUnichar* aMsg)
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nsnull;
  }

  if (NS_BINDING_SUCCEEDED != status) {
    mStatus = MK_INTERRUPTED;
  }

 
  // Since we're still holding on to the stream, there's still data
  // that needs to be read. So, pump the stream ourselves.
  if((mStream != nsnull) && (status == NS_BINDING_SUCCEEDED)) {
    PRUint32 str_length;
    nsresult err = mStream->Available(&str_length);
    if (NS_SUCCEEDED(err)) {
      NS_ASSERTION((str_length > 0), "No data left in the stream!");
      err = OnDataAvailable(channel, aContext, mStream, 0, str_length);  // XXX fix offset
      if (NS_SUCCEEDED(err)) {
        // If we still have the stream, there's still data to be 
        // pumped, so we set a timer to call us back again.
        if (mStream) {
          SetKeepPumpingData(channel, aContext);

          nsresult rv;
          mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
          if (NS_FAILED(rv) ||
              (NS_OK != mTimer->Init(ImageConsumer::KeepPumpingStream, this, 0))) {
            mStatus = MK_IMAGE_LOSSAGE;
            NS_RELEASE(mStream);
          }
          else {
            return NS_OK;
          }
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
    reader->StreamComplete(mIsMulti);
  }

  if(mIsMulti)
        mFirstRead = PR_TRUE;  //reset to read new frame

  reader->NetRequestDone(mURL, mStatus);
  NS_RELEASE(reader);
  
  return mContext->RequestDone(this, channel, aContext, status, aMsg);
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
  if (mBuffer != nsnull) {
    PR_DELETE(mBuffer);
  }
  NS_IF_RELEASE(mChannel);
  NS_IF_RELEASE(mUserContext);
}

ImageNetContextImpl::ImageNetContextImpl(ImgCachePolicy aReloadPolicy,
                                         nsISupports * aLoadContext,
                                         nsReconnectCB aReconnectCallback,
                                         void* aReconnectArg)
{
  NS_INIT_REFCNT();
  mRequests = nsnull;
  mLoadContext = getter_AddRefs(NS_GetWeakReference(aLoadContext));
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
}

NS_IMPL_THREADSAFE_ISUPPORTS(ImageNetContextImpl, kIImageNetContextIID)

ilINetContext* 
ImageNetContextImpl::Clone()
{
  ilINetContext *cx;
  nsCOMPtr<nsISupports> loadContext = do_QueryReferent(mLoadContext);

  //mReconnectArg is ImageGroup. If GetURL is triggered
  //by timer for animation, ImageGroup may have been unloaded
  //before timer kicks off.
  //mReconnectCallback=nsnull; mReconnectArg=nsnull;

  if (NS_NewImageNetContext(&cx, loadContext, mReconnectCallback, mReconnectArg) == NS_OK)
  {
    return cx;
  }
  else {
    return nsnull;
  }
}

ImgCachePolicy 
ImageNetContextImpl::GetReloadPolicy()
{
  return mReloadPolicy;
}

ImgCachePolicy 
ImageNetContextImpl::SetReloadPolicy(ImgCachePolicy reloadpolicy)
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
                               ImgCachePolicy aReloadMethod)
{
  ilIURL *url;
 
  nsCOMPtr<nsISupports> loadContext (do_QueryReferent(mLoadContext)); 
  nsCOMPtr<nsILoadGroup> group (do_GetInterface(loadContext));
  if (NS_NewImageURL(&url, aURL, group) == NS_OK)
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
ImageNetContextImpl::GetContentLength (ilIURL * aURL)
{
    nsresult rv;
    int content_length=0;

    nsCOMPtr<nsIURI> nsurl = do_QueryInterface(aURL, &rv);
    if (NS_FAILED(rv)) return 0;


    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsISupports> loadContext (do_QueryReferent(mLoadContext)); 
    nsCOMPtr<nsILoadGroup> group (do_GetInterface(loadContext));
    nsCOMPtr<nsIInterfaceRequestor> sink(do_QueryInterface(loadContext));

    rv = NS_OpenURI(getter_AddRefs(channel), nsurl, nsnull, group, sink);
    if (NS_FAILED(rv)) return 0;

    rv = channel->GetContentLength(&content_length);
    return content_length;

}


int 
ImageNetContextImpl::GetURL (ilIURL * aURL, 
                             ImgCachePolicy aLoadMethod,
                             ilINetReader *aReader,
                             PRBool IsAnimationLoop)
{


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

  nsresult rv;
  nsCOMPtr<nsIURI> nsurl = do_QueryInterface(aURL, &rv);
  if (NS_FAILED(rv)) return 0;

  aURL->SetReader(aReader);
  SetReloadPolicy(aLoadMethod); 
 
  
  // Find previously created ImageConsumer if possible
  ImageConsumer *ic = new ImageConsumer(aURL, this);
  if (ic == nsnull)
    return -1;
  NS_ADDREF(ic);
        
  // See if a reconnect is being done...(XXX: hack!)
  if (mReconnectCallback == nsnull
      || !(*mReconnectCallback)(mReconnectArg, ic)) {
    // first, create a channel for the protocol....
    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsISupports> loadContext (do_QueryReferent(mLoadContext)); 
    nsCOMPtr<nsILoadGroup> group (do_GetInterface(loadContext));
    nsCOMPtr<nsIInterfaceRequestor> sink(do_QueryInterface(loadContext));

   nsLoadFlags flags=0;   
   if(IsAnimationLoop)
       flags |= nsIChannel::VALIDATE_NEVER;

    rv = NS_OpenURI(getter_AddRefs(channel), nsurl, nsnull, group, sink, flags);
    if (NS_FAILED(rv)) goto error;

    nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(channel);
    if (httpChannel)
    {
        // Get the defloadchannel from the loadgroup-
        nsCOMPtr<nsIChannel> defLoadChannel;
        if (NS_SUCCEEDED(group->GetDefaultLoadChannel(
                        getter_AddRefs(defLoadChannel))) && defLoadChannel)
        {
            // Get the referrer from the loadchannel-
            nsCOMPtr<nsIURI> referrer;
            if (NS_SUCCEEDED(defLoadChannel->GetURI(getter_AddRefs(referrer))))
            {
                // Set the referrer-
                httpChannel->SetReferrer(referrer, 
                    nsIHTTPChannel::REFERRER_INLINES);
            }
        }
    }


    rv = channel->GetLoadAttributes(&flags);
    if (NS_FAILED(rv)) goto error;

   if (aURL->GetBackgroundLoad()) 
      flags |= nsIChannel::LOAD_BACKGROUND;
       
   (void)channel->SetLoadAttributes(flags);

   nsCOMPtr<nsISupports> window (do_QueryInterface(NS_STATIC_CAST(nsIStreamListener *, ic)));

    // let's try uri dispatching...
    nsCOMPtr<nsIURILoader> pURILoader (do_GetService(NS_URI_LOADER_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv)) 
    {
      nsURILoadCommand loadCmd = nsIURILoader::viewNormal;
      PRBool bIsBackground = aURL->GetBackgroundLoad();
      if (bIsBackground) {
        loadCmd = nsIURILoader::viewNormalBackground;
      }


      rv = pURILoader->OpenURI(channel, loadCmd, nsnull /* window target */, 
                               window);
    }
    // rv = channel->AsyncRead(ic, nsnull);
    if (NS_FAILED(rv)) goto error;
  }
  return mRequests->AppendElement((void *)ic) ? 0 : -1;

error:
  NS_RELEASE(ic);
  return -1;
}

nsresult
ImageNetContextImpl::RequestDone(ImageConsumer *aConsumer, nsIChannel* channel,
                                 nsISupports* ctxt, nsresult status, const PRUnichar* aMsg)
{
  if (mRequests != nsnull) {
    if (mRequests->RemoveElement((void *)aConsumer) == PR_TRUE) {
      NS_RELEASE(aConsumer);
    }
  }
///  if (mLoadGroup)
///    return mLoadGroup->RemoveChannel(channel, ctxt, status, aMsg);
///  else
    return NS_OK;
}

extern "C" NS_GFX_(nsresult)
NS_NewImageNetContext(ilINetContext **aInstancePtrResult,
                      nsISupports * aLoadContext, 
                      nsReconnectCB aReconnectCallback,
                      void* aReconnectArg)
{
     
  PRUint32  necko_attribs;
  ImgCachePolicy imglib_attribs = USE_IMG_CACHE;
  nsLoadFlags defchan_attribs = nsIChannel::LOAD_NORMAL;

  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  if(aLoadContext){
     nsCOMPtr<nsILoadGroup> group (do_GetInterface(aLoadContext));
     nsresult rv = group->GetDefaultLoadAttributes(&necko_attribs);
/*
Need code to check freshness of necko cache.
*/
     nsCOMPtr<nsIChannel> defLoadChannel; 
     if (NS_SUCCEEDED(group->GetDefaultLoadChannel(
                        getter_AddRefs(defLoadChannel))) && defLoadChannel)
     defLoadChannel->GetLoadAttributes(&defchan_attribs);

#if defined( DEBUG )
     if (image_net_context_async_log_module == NULL) {
    	image_net_context_async_log_module = PR_NewLogModule("IMAGENETCTXASYNC");
     }          
#endif
     if((nsIChannel::FORCE_VALIDATION & defchan_attribs)||   
        (nsIChannel::VALIDATE_ALWAYS & defchan_attribs) ||
        (nsIChannel::INHIBIT_PERSISTENT_CACHING & defchan_attribs)||
        (nsIChannel::FORCE_RELOAD & defchan_attribs)) {
     		imglib_attribs = DONT_USE_IMG_CACHE;
#if defined( DEBUG )
		PR_LOG(image_net_context_async_log_module, 1, ("ImageNetContextAsync: NS_NewImageNetContext: DONT_USE_IMAGE_CACHE\n"));
#endif
     }
  }

  ilINetContext *cx = new ImageNetContextImpl( imglib_attribs,
                                              aLoadContext,
                                              aReconnectCallback,
                                              aReconnectArg);
  if (cx == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return cx->QueryInterface(kIImageNetContextIID, (void **) aInstancePtrResult);

}
