/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIURL.h"
#include "nsIEventQueueService.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIStreamConverter.h"
#include "nsIMimeStreamConverter.h"
#include "nsFileStream.h"
#include "nsFileSpec.h"
#include "nsMimeTypes.h"
#include "nsIPref.h"
#include "nsICharsetConverterManager.h"
#include "prprf.h"
#include "nsMsgQuote.h" 
#include "nsMsgCompUtils.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#include "nsMsgDeliveryListener.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsMsgMimeCID.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

//
// Implementation...
//
nsMsgQuote::nsMsgQuote()
{
	NS_INIT_REFCNT();

  mTmpFileSpec = nsnull;
  mTmpIFileSpec = nsnull;
  mURI = nsnull;
  mMessageService = nsnull;
  mQuoteHeaders = PR_FALSE;
}

nsMsgQuote::~nsMsgQuote()
{
  if (mMessageService)
  {
    ReleaseMessageServiceFromURI(mURI, mMessageService);
    mMessageService = nsnull;
  }

  PR_FREEIF(mURI);
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgQuote, nsCOMTypeInfo<nsIMsgQuote>::GetIID());

/* this function will be used by the factory to generate an Message Compose Fields Object....*/
nsresult 
NS_NewMsgQuote(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMsgQuote *pQuote = new nsMsgQuote();
		if (pQuote)
			return pQuote->QueryInterface(aIID, aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}


// stream converter
static NS_DEFINE_CID(kStreamConverterCID,    NS_MAILNEWS_MIME_STREAM_CONVERTER_CID);

////////////////////////////////////////////////////////////////////////////////////
// THIS IS A TEMPORARY CLASS THAT MAKES A DISK FILE LOOK LIKE A nsIInputStream 
// INTERFACE...this may already exist, but I didn't find it. Eventually, you would
// just plugin a Necko stream when they are all rewritten to be new style streams
////////////////////////////////////////////////////////////////////////////////////
class FileInputStreamImpl : public nsIInputStream
{
public:
  FileInputStreamImpl(void) 
  { 
    NS_INIT_REFCNT(); 
    mBufLen = 0;
    mInFile = nsnull;
  }
  virtual ~FileInputStreamImpl(void) 
  {
    if (mInFile) delete mInFile;
  }
  
  // nsISupports interface
  NS_DECL_ISUPPORTS
    
  // nsIBaseStream interface
  NS_IMETHOD Close(void) 
  {
    if (mInFile)
      mInFile->close();  
    return NS_OK;
  }
  
  // nsIInputStream interface
  NS_IMETHOD GetLength(PRUint32 *_retval)
  {
    *_retval = mBufLen;
    return NS_OK;
  }
  
  /* unsigned long Read (in charStar buf, in unsigned long count); */
  NS_IMETHOD Read(char * buf, PRUint32 count, PRUint32 *_retval)
  {
    nsCRT::memcpy(buf, mBuf, mBufLen);
    *_retval = mBufLen;
    return NS_OK;
  }
  
  NS_IMETHOD OpenDiskFile(nsFileSpec fs);
  NS_IMETHOD PumpFileStream();

private:
  PRUint32        mBufLen;
  char            mBuf[8192];
  nsIOFileStream  *mInFile;
};

nsresult
FileInputStreamImpl::OpenDiskFile(nsFileSpec fs)
{
  mInFile = new nsIOFileStream(fs);
  if (!mInFile)
    return NS_ERROR_NULL_POINTER;
  mInFile->seek(0);
  return NS_OK;
}

nsresult
FileInputStreamImpl::PumpFileStream()
{
  if (mInFile->eof())
    return NS_BASE_STREAM_EOF;
  
  mBufLen = mInFile->read(mBuf, sizeof(mBuf));
  if (mBufLen > 0)
    return NS_OK;
  else
    return NS_BASE_STREAM_EOF;
}

NS_IMPL_ISUPPORTS(FileInputStreamImpl, nsCOMTypeInfo<nsIInputStream>::GetIID());

////////////////////////////////////////////////////////////////////////////////////
// End of FileInputStreamImpl()
////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////

nsresult
SaveQuoteMessageCompleteCallback(nsIURI *aURL, nsresult aExitCode, void *tagData)
{
  nsresult        rv = NS_OK;

  if (!tagData)
  {
    return NS_ERROR_INVALID_ARG;
  }

  nsMsgQuote *ptr = (nsMsgQuote *) tagData;
  if (ptr->mMessageService)
  {
    ReleaseMessageServiceFromURI(ptr->mURI, ptr->mMessageService);
    ptr->mMessageService = nsnull;
  }

  /* mscott - the NS_BINDING_ABORTED is a hack to get around a problem I have
     with the necko code...it returns this and treats it as an error when
	 it really isn't an error! I'm trying to get them to change this.
   */
  if (NS_FAILED(aExitCode) && aExitCode != NS_BINDING_ABORTED)
  {
    NS_RELEASE(ptr);
    return aExitCode;
  }

  // Create a mime parser (nsIStreamConverter)!
  nsCOMPtr<nsIStreamConverter> mimeParser;
  rv = nsComponentManager::CreateInstance(kStreamConverterCID, 
                                          NULL, nsCOMTypeInfo<nsIStreamConverter>::GetIID(), 
                                          (void **) getter_AddRefs(mimeParser)); 
  if (NS_FAILED(rv) || !mimeParser)
  {
    NS_RELEASE(ptr);
    printf("Failed to create MIME stream converter...\n");
    return rv;
  }
  
  // This is the producer stream that will deliver data from the disk file...
  // ...someday, we'll just get streams from Necko.
  // mscott --> the type for a nsCOMPtr needs to be an interface.
  // but this class (which is only temporary anyway) is mixing and matching
  // interface calls and implementation calls....so you really can't use a
  // com ptr. to get around it, I'm using fileStream to make calls on the
  // methods that aren't supported by the nsIInputStream and "in" for
  // methods that are supported as part of the interface...
  FileInputStreamImpl * fileStream = new FileInputStreamImpl();
  nsCOMPtr<nsIInputStream> in = do_QueryInterface(fileStream);
  if (!in || !fileStream)
  {
    NS_RELEASE(ptr);
    printf("Failed to create nsIInputStream\n");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (NS_FAILED(fileStream->OpenDiskFile(*(ptr->mTmpFileSpec))))
  {
    NS_RELEASE(ptr);
    printf("Unable to open input file\n");
    return NS_ERROR_FAILURE;
  }
  
  // Set us as the output stream for HTML data from libmime...
  nsCOMPtr<nsIMimeStreamConverter> mimeConverter = do_QueryInterface(mimeParser);
  if (mimeConverter)
  {
	  if (ptr->mQuoteHeaders)
		mimeConverter->SetMimeOutputType(nsMimeOutput::nsMimeMessageQuoting);
	  else
		mimeConverter->SetMimeOutputType(nsMimeOutput::nsMimeMessageBodyQuoting);
  }

  nsCOMPtr<nsIChannel> dummyChannel;
  NS_WITH_SERVICE(nsIIOService, netService, kIOServiceCID, &rv);
  rv = netService->NewInputStreamChannel(aURL, nsnull, nsnull, getter_AddRefs(dummyChannel));
  if (NS_FAILED(mimeParser->AsyncConvertData(nsnull, nsnull, ptr->mStreamListener, dummyChannel)))
  {
    NS_RELEASE(ptr);
    printf("Unable to set the output stream for the mime parser...\ncould be failure to create internal libmime data\n");
    return NS_ERROR_UNEXPECTED;
  }

  // Assuming this is an RFC822 message...
  mimeParser->OnStartRequest(nsnull, aURL);

  // Just pump all of the data from the file into libmime...
  while (NS_SUCCEEDED(fileStream->PumpFileStream()))
  {
    PRUint32    len;
    in->GetLength(&len);
    mimeParser->OnDataAvailable(nsnull, aURL, in, 0, len);
  }

  mimeParser->OnStopRequest(nsnull, aURL, NS_OK, nsnull);
  in->Close();
  ptr->mTmpFileSpec->Delete(PR_FALSE);
  NS_RELEASE(ptr);
  return NS_OK;
}

nsresult
nsMsgQuote::QuoteMessage(const PRUnichar *msgURI, PRBool quoteHeaders, nsIStreamListener * aQuoteMsgStreamListener)
{
nsresult  rv;

  if (!msgURI)
    return NS_ERROR_INVALID_ARG;

  mQuoteHeaders = quoteHeaders;
  mStreamListener = aQuoteMsgStreamListener;

  mTmpFileSpec = nsMsgCreateTempFileSpec("nsquot.tmp"); 
	if (!mTmpFileSpec)
    return NS_ERROR_FAILURE;

  NS_NewFileSpecWithSpec(*mTmpFileSpec, &mTmpIFileSpec);
	if (!mTmpIFileSpec)
    return NS_ERROR_FAILURE;

  nsString                convertString(msgURI);
  mURI = convertString.ToNewCString();

  if (!mURI)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = GetMessageServiceFromURI(mURI, &mMessageService);
  if (NS_FAILED(rv) && !mMessageService)
  {
    return rv;
  }

  NS_ADDREF(this);
  nsMsgDeliveryListener *sendListener = new nsMsgDeliveryListener(SaveQuoteMessageCompleteCallback, 
                                                                  nsFileSaveDelivery, this);
  if (!sendListener)
  {
    ReleaseMessageServiceFromURI(mURI, mMessageService);
    mMessageService = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = mMessageService->SaveMessageToDisk(mURI, mTmpIFileSpec, PR_FALSE, sendListener, nsnull);

	if (NS_FAILED(rv))
    return rv;    
  else
    return NS_OK;
}
