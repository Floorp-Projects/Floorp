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
#include "nsCOMPtr.h"
#include "nsIURL.h"
#include "nsIEventQueueService.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIStreamConverter.h"
#include "nsFileStream.h"
#include "nsFileSpec.h"
#include "nsMimeTypes.h"
#include "nsIPref.h"
#include "nsICharsetConverterManager.h"
#include "prprf.h"
#include "nsMsgQuote.h" 
#include "nsINetService.h"

//
// Implementation...
//
nsMsgQuote::nsMsgQuote()
{
	NS_INIT_REFCNT();
}

nsMsgQuote::~nsMsgQuote()
{
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgQuote, nsIMsgQuote::GetIID());

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

// net service definitions....
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);

// stream converter
static NS_DEFINE_CID(kStreamConverterCID,    NS_STREAM_CONVERTER_CID);

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
    NS_BASE_STREAM_EOF;
  
  mBufLen = mInFile->read(mBuf, sizeof(mBuf));
  if (mBufLen > 0)
    return NS_OK;
  else
    return NS_BASE_STREAM_EOF;
}

NS_IMPL_ISUPPORTS(FileInputStreamImpl, nsIInputStream::GetIID());

////////////////////////////////////////////////////////////////////////////////////
// End of FileInputStreamImpl()
////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////

// Utility to make back slashes forward slashes...
void
FixURL(char *url)
{
  char *ptr = url;
  while (*ptr != '\0')
  {
    if (*ptr == '\\')
      *ptr = '/';
    ++ptr;
  }
}

// Utility to create a nsIURL object...
nsresult 
NewURL(nsIURL** aInstancePtrResult, const nsString& aSpec)
{  
  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  
  nsINetService *inet = nsnull;
  nsresult rv = nsServiceManager::GetService(kNetServiceCID, nsINetService::GetIID(),
                                             (nsISupports **)&inet);
  if (rv != NS_OK) 
    return rv;

  rv = inet->CreateURL(aInstancePtrResult, aSpec, nsnull, nsnull, nsnull);
  nsServiceManager::ReleaseService(kNetServiceCID, inet);
  return rv;
}

nsresult
nsMsgQuote::QuoteMessage(const PRUnichar *msgURI, nsIOutputStream *outStream)
{
  char *filename = "n:\\mhtml\\enzo.eml";

  nsFilePath      inFilePath(filename, PR_TRUE); // relative path.
  nsFileSpec      mySpec(inFilePath);
  nsresult        rv;
  char            newURL[1024] = ""; // URL for filename
  nsIURL          *aURL = nsnull;

  if (!mySpec.Exists())
  {
    printf("Unable to open input file %s\n", filename);
    return NS_ERROR_FAILURE;
  }

  // Create a mime parser (nsIStreamConverter)!
  nsCOMPtr<nsIStreamConverter> mimeParser;
  rv = nsComponentManager::CreateInstance(kStreamConverterCID, 
                                          NULL, nsIStreamConverter::GetIID(), 
                                          (void **) getter_AddRefs(mimeParser)); 
  if (NS_FAILED(rv) || !mimeParser)
  {
    printf("Failed to create MIME stream converter...\n");
    return rv;
  }
  
  // This is the producer stream that will deliver data from the disk file...
  nsCOMPtr<FileInputStreamImpl> in = do_QueryInterface(new FileInputStreamImpl());
  if (!in )
  {
    printf("Failed to create nsIInputStream\n");
    return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(in->OpenDiskFile(mySpec)))
  {
    printf("Unable to open input file %s\n", filename);
    return NS_ERROR_FAILURE;
  }
  
  // Create an nsIURL object needed for stream IO...
  PR_snprintf(newURL, sizeof(newURL), "file://%s", filename);
  FixURL(newURL);
  if (NS_FAILED(NewURL(&aURL, nsString(newURL))))
  {
    printf("Unable to open input file\n");
    return NS_ERROR_FAILURE;
  }

  // Set us as the output stream for HTML data from libmime...
  if (NS_FAILED(mimeParser->SetOutputStream(outStream, newURL)))
  {
    printf("Unable to set the output stream for the mime parser...\ncould be failure to create internal libmime data\n");
    return NS_ERROR_FAILURE;
  }

  // Assuming this is an RFC822 message...
  mimeParser->OnStartBinding(aURL, MESSAGE_RFC822);

  // Just pump all of the data from the file into libmime...
  while (NS_SUCCEEDED(in->PumpFileStream()))
  {
    PRUint32    len;
    in->GetLength(&len);
    mimeParser->OnDataAvailable(aURL, in, len);
  }

  mimeParser->OnStopBinding(aURL, NS_OK, nsnull);
  NS_RELEASE(aURL);
  return NS_OK;
}
