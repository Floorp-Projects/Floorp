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
#include "nsITimer.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "prmem.h"
#include "plstr.h"
#include "il_strm.h"
#include "merrors.h"

static NS_DEFINE_IID(kIImageNetContextIID, IL_INETCONTEXT_IID);
static NS_DEFINE_IID(kIURLIID, NS_IURL_IID);

#define IMAGE_BUF_SIZE 4096

class ImageConsumer;

class ImageNetContextImpl : public ilINetContext {
public:
  ImageNetContextImpl(NET_ReloadMethod aReloadPolicy);
  ~ImageNetContextImpl();

  NS_DECL_ISUPPORTS

  virtual ilINetContext* Clone();

  virtual NET_ReloadMethod GetReloadPolicy();

  virtual void AddReferer(ilIURL *aUrl);

  virtual void Interrupt();

  virtual ilIURL* CreateURL(const char *aUrl, 
			    NET_ReloadMethod aReloadMethod);

  virtual PRBool IsLocalFileURL(char *aAddress);

  virtual PRBool IsURLInMemCache(ilIURL *aUrl);

  virtual PRBool IsURLInDiskCache(ilIURL *aUrl);

  virtual int GetURL (ilIURL * aUrl, NET_ReloadMethod aLoadMethod,
		      ilINetReader *aReader);

  void RequestDone(ImageConsumer *aConsumer);

  nsVoidArray *mRequests;
  NET_ReloadMethod mReloadPolicy;
};

class ImageConsumer : public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  
  ImageConsumer(ilIURL *aURL, ImageNetContextImpl *aContext);
  
  NS_IMETHOD GetBindInfo(void);
  NS_IMETHOD OnProgress(PRInt32 Progress, PRInt32 ProgressMax, const nsString& aMsg);
  NS_IMETHOD OnStartBinding(const char *aContentType);
  NS_IMETHOD OnDataAvailable(nsIInputStream *pIStream, PRInt32 length);
  NS_IMETHOD OnStopBinding(PRInt32 status, const nsString& aMsg);
  
  void Interrupt();

protected:
  ~ImageConsumer();
  ilIURL *mURL;
  PRBool mInterrupted;
  ImageNetContextImpl *mContext;
  nsIInputStream *mStream;
  nsITimer *mTimer;
  PRBool mFirstRead;
  char *mBuffer;
  PRInt32 mStatus;
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
}

NS_DEFINE_IID(kIStreamNotificationIID, NS_ISTREAMLISTENER_IID);
NS_IMPL_ISUPPORTS(ImageConsumer,kIStreamNotificationIID);

NS_IMETHODIMP
ImageConsumer::GetBindInfo(void)
{
  return 0;
}

NS_IMETHODIMP
ImageConsumer::OnProgress(PRInt32 Progress, PRInt32 ProgressMax, const nsString& aMsg)
{
  return 0;
}

NS_IMETHODIMP
ImageConsumer::OnStartBinding(const char *aContentType)
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
    
  return NS_OK;
}

#define IMAGE_BUF_SIZE 4096


NS_IMETHODIMP
ImageConsumer::OnDataAvailable(nsIInputStream *pIStream, PRInt32 length)
{
  PRInt32 max_read;
  PRInt32 bytes_read = 0;

  // If we previously held onto a stream, drop our reference
  if (mStream != nsnull) {
    NS_RELEASE(mStream);
  }

  ilINetReader *reader = mURL->GetReader();

  if (mInterrupted || mStatus != 0) {
    mStatus = MK_INTERRUPTED;
    reader->StreamAbort(mStatus);
    NS_RELEASE(reader);
    return NS_ERROR_ABORT;
  }

  PRInt32 err = 0;
  PRInt32 nb;
  do {
    max_read = (PRInt32)reader->WriteReady();
    if (0 == max_read) {
      break;
    }
    if (max_read > IMAGE_BUF_SIZE) {
      max_read = IMAGE_BUF_SIZE;
    }

    nb = pIStream->Read(&err, mBuffer, 0, 
                        max_read);
    bytes_read += nb;
    if (err != 0) {
      break;
    }

    if (mFirstRead == PR_TRUE) {
      PRInt32 ilErr;
            
      ilErr = reader->FirstWrite((const unsigned char *)mBuffer, nb);
      mFirstRead = PR_FALSE;
      /* 
       * If FirstWrite(...) fails then the image type
       * cannot be determined and the il_container 
       * stream functions have not been initialized!
       */
      if (ilErr != 0) {
        mStatus = MK_IMAGE_LOSSAGE;
        mInterrupted = PR_TRUE;
        break;
      }
    }
        
    reader->Write((const unsigned char *)mBuffer, nb);
  } while(nb != 0);

  if ((0 != err) && (NS_INPUTSTREAM_EOF != err)) {
    mStatus = MK_IMAGE_LOSSAGE;
    mInterrupted = PR_TRUE;
  }

  if (bytes_read < pIStream->GetLength()) {
    // If we haven't emptied the stream, hold onto it, because
    // we will need to read from it subsequently and we don't
    // know if we'll get a OnDataAvailable call again.
    mStream = pIStream;
    NS_ADDREF(mStream);
  }

  NS_RELEASE(reader);
  return NS_OK;
}

static void
KeepPumpingStream(nsITimer *aTimer, void *aClosure)
{
  ImageConsumer *consumer = (ImageConsumer *)aClosure;
  nsAutoString status;

  consumer->OnStopBinding(NS_BINDING_SUCCEEDED, status);
}

NS_IMETHODIMP
ImageConsumer::OnStopBinding(PRInt32 status, const nsString& aMsg)
{
  if (mTimer != nsnull) {
    NS_RELEASE(mTimer);
  }

  // Since we're still holding on to the stream, there's still data
  // that needs to be read. So, pump the stream ourselves.
  if((mStream != nsnull) && (status == NS_BINDING_SUCCEEDED)) {
    nsresult err = OnDataAvailable(mStream, mStream->GetLength());
    // If we still have the stream, there's still data to be 
    // pumped, so we set a timer to call us back again.
    if (mStream != nsnull) {
      if ((NS_OK != NS_NewTimer(&mTimer)) ||
          (NS_OK != mTimer->Init(KeepPumpingStream, this, 0))) {
        mStatus = MK_IMAGE_LOSSAGE;
        NS_RELEASE(mStream);
      }
      else {
        return NS_OK;
      }
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
  
  mContext->RequestDone(this);
  return NS_OK;
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
}

ImageNetContextImpl::ImageNetContextImpl(NET_ReloadMethod aReloadPolicy)
{
  NS_INIT_REFCNT();
  mRequests = nsnull;
  mReloadPolicy = aReloadPolicy;
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

NS_IMPL_ISUPPORTS(ImageNetContextImpl, kIImageNetContextIID)

ilINetContext* 
ImageNetContextImpl::Clone()
{
  ilINetContext *cx;

  if (NS_NewImageNetContext(&cx) == NS_OK) {
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

  if (NS_NewImageURL(&url, aURL) == NS_OK) {
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

int 
ImageNetContextImpl::GetURL (ilIURL * aURL, 
                             NET_ReloadMethod aLoadMethod,
                             ilINetReader *aReader)
{
  nsIURL *nsurl;
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
        
    ImageConsumer *ic = new ImageConsumer(aURL, this);
    NS_ADDREF(ic);
        
    if (nsurl->Open(ic) == NS_OK) {
      mRequests->AppendElement((void *)ic);
    }
    else {
      NS_RELEASE(ic);
    }

    NS_RELEASE(nsurl);
  }
    
  return 0;
}

void 
ImageNetContextImpl::RequestDone(ImageConsumer *aConsumer)
{
  if (mRequests != nsnull) {
    if (mRequests->RemoveElement((void *)aConsumer) == PR_TRUE) {
      NS_RELEASE(aConsumer);
    }
  }
}

nsresult 
NS_NewImageNetContext(ilINetContext **aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  
  ilINetContext *cx = new ImageNetContextImpl(NET_NORMAL_RELOAD);
  if (cx == nsnull) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return cx->QueryInterface(kIImageNetContextIID, (void **) aInstancePtrResult);
}
