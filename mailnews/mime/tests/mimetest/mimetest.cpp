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
/*
  This program drives libmime to parse an RFC822 message and output the 
  HTML representation of it.

  The program takes a single parameter: the disk file from which to read.
 */
#include "nsCOMPtr.h"
#include "nsIURL.h"
#include "nsIEventQueueService.h"
#include "nsINetService.h"
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
#include "nsIAllocator.h" // for the CID
#include "msgCore.h"

////////////////////////////////////////////////////////////////////////////////////
// THIS IS THE STUFF TO GET THE TEST HARNESS OFF THE GROUND
////////////////////////////////////////////////////////////////////////////////////
#if defined(XP_PC)
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#define MIME_DLL  "mime.dll"
#define PREF_DLL  "xppref32.dll"
#define UNICHAR_DLL  "uconv.dll"
#elif defined(XP_UNIX) || defined(XP_BEOS)
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#define MIME_DLL  "libmime"MOZ_DLL_SUFFIX
#define PREF_DLL  "libpref"MOZ_DLL_SUFFIX
#define UNICHAR_DLL  "libunicharutil"MOZ_DLL_SUFFIX
#elif defined(XP_MAC)
#define NETLIB_DLL "NETLIB_DLL"
#define XPCOM_DLL  "XPCOM_DLL"
#define MIME_DLL   "MIME_DLL"
#define PREF_DLL  "XPPREF32_DLL"
#define UNICHAR_DLL  "UNICHARUTIL_DLL"
#endif

// {588595CB-2012-11d3-8EF0-00A024A7D144}
#define CONV_CID  { 0x1e3f79f1, 0x6b6b, 0x11d2, { 0x8a, 0x86, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36 } };
#define CONV_IID  { 0x1e3f79f0, 0x6b6b, 0x11d2, {  0x8a, 0x86, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36 } }

// CIDs

// I18N
static NS_DEFINE_CID(charsetCID,  CONV_CID);
NS_DEFINE_IID(kConvMeIID,             CONV_IID);

// prefs
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

// xpcom
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kGenericFactoryCID,    NS_GENERICFACTORY_CID);

// netlib definitions....
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);

// Stream converter
static NS_DEFINE_CID(kStreamConverterCID,    NS_STREAM_CONVERTER_CID);

nsICharsetConverterManager *ccMan = nsnull;

static nsresult
SetupRegistry(void)
{
  // i18n
  nsComponentManager::RegisterComponent(charsetCID, NULL, NULL, UNICHAR_DLL,  PR_FALSE, PR_FALSE);
  nsresult res = nsServiceManager::GetService(charsetCID, kConvMeIID, (nsISupports **)&ccMan);
  if (NS_FAILED(res)) 
  {
    printf("ERROR at GetService() code=0x%x.\n",res);
    return NS_ERROR_FAILURE;
  }

  // netlib
  nsComponentManager::RegisterComponent(kNetServiceCID,     NULL, NULL, NETLIB_DLL,  PR_FALSE, PR_FALSE);
  
  // xpcom
  static NS_DEFINE_CID(kAllocatorCID,  NS_ALLOCATOR_CID);
  static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);
  nsComponentManager::RegisterComponent(kEventQueueServiceCID, NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kEventQueueCID,        NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kGenericFactoryCID,    NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kAllocatorCID,         NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);

  // prefs
  nsComponentManager::RegisterComponent(kPrefCID,              NULL, NULL, PREF_DLL,  PR_FALSE, PR_FALSE);

  // mime
  nsComponentManager::RegisterComponent(kStreamConverterCID,   NULL, NULL, MIME_DLL,  PR_FALSE, PR_FALSE);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// END OF STUFF TO GET THE TEST HARNESS OFF THE GROUND
////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////
// THIS IS THE CLASS THAT WOULD BE IMPLEMENTED BY THE CONSUMER OF THE HTML OUPUT
// FROM LIBMIME. THIS EXAMPLE SIMPLY WRITES THE OUTPUT TO STDOUT()
////////////////////////////////////////////////////////////////////////////////////
class ConsoleOutputStreamImpl : public nsIOutputStream
{
public:
    ConsoleOutputStreamImpl(void) 
    { 
      NS_INIT_REFCNT(); 
      mIndentCount = 0;
      mInClosingTag = PR_FALSE;
    }

    virtual ~ConsoleOutputStreamImpl(void) {}

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIBaseStream interface
    NS_IMETHOD Close(void) 
    {
      char *note = "<center><hr WIDTH=\"90%\"><br><b>Anything after the above horizontal line is diagnostic output<br>and is not part of the HTML stream!</b></center>";
      PR_Write(PR_GetSpecialFD(PR_StandardOutput), note, PL_strlen(note));

      return NS_OK;
    }

    // nsIOutputStream interface
    NS_IMETHOD Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount);

    NS_IMETHOD Flush(void) {
        PR_Sync(PR_GetSpecialFD(PR_StandardOutput));
        return NS_OK;
    }

    nsresult  DoIndent();

private:
  PRInt32   mIndentCount;
  PRBool    mInClosingTag;
};
NS_IMPL_ISUPPORTS(ConsoleOutputStreamImpl, nsIOutputStream::GetIID());

#define TAB_SPACES    2

// make the html pretty :-)
nsresult
ConsoleOutputStreamImpl::DoIndent()
{
  PR_Write(PR_GetSpecialFD(PR_StandardOutput), MSG_LINEBREAK, MSG_LINEBREAK_LEN);
  if (mIndentCount <= 1)
    return NS_OK;

  for (PRUint32 j=0; j<(PRUint32) ((mIndentCount-1)*TAB_SPACES); j++)
    PR_Write(PR_GetSpecialFD(PR_StandardOutput), " ", 1);
  return NS_OK;
}

nsresult
ConsoleOutputStreamImpl::Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount)
{
  PRUint32 i=0;

  for (i=0; i<aCount; i++)
  {
    // First, check if we are in a new level of html...
    if (aBuf[i] == '<')
    {
      if ( (i < aCount) && (aBuf[i+1] == '/') )
      {
        --mIndentCount;
        mInClosingTag = PR_TRUE;
      }
      else
      {
        ++mIndentCount;
        DoIndent();
        mInClosingTag = PR_FALSE;
      }

      if (mIndentCount < 0)
        mIndentCount = 0;
    }

    // Now, write the HTML character...
    PR_Write(PR_GetSpecialFD(PR_StandardOutput), aBuf+i, 1);

    if (aBuf[i] == '>')
    {
      // Now, write out the correct number or spaces for the indent count...
      if (mInClosingTag)
        DoIndent();
    }
  }

  *aWriteCount = aCount;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// END OF CONSUMER STREAM
////////////////////////////////////////////////////////////////////////////////////

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
NewURL(nsIURI** aInstancePtrResult, const nsString& aSpec)
{  
  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  
  nsINetService *inet = nsnull;
  nsresult rv = nsServiceManager::GetService(kNetServiceCID, nsCOMTypeInfo<nsINetService>::GetIID(),
                                             (nsISupports **)&inet);
  if (rv != NS_OK) 
    return rv;

  rv = inet->CreateURL(aInstancePtrResult, aSpec, nsnull, nsnull, nsnull);
  nsServiceManager::ReleaseService(kNetServiceCID, inet);
  return rv;
}

int
main(int argc, char** argv)
{
  nsresult DoRFC822toHTMLConversion(char *filename);
  nsresult rv;
  
  // Do some sanity checking...
  if (argc < 2) 
  {
    fprintf(stderr, "usage: %s <rfc822_disk_file>\n", argv[0]);
    return 1;
  }
  
  // Setup the registry...
  SetupRegistry();

  // Get an event queue started...
  NS_WITH_SERVICE(nsIEventQueueService, theEventQueueService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = theEventQueueService->CreateThreadEventQueue();
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create thread event queue");
  if (NS_FAILED(rv)) return rv;

  DoRFC822toHTMLConversion(argv[1]);

  // Cleanup stuff necessary...
  NS_IF_RELEASE(ccMan);
  return NS_OK;
}

nsresult
DoRFC822toHTMLConversion(char *filename)
{
  nsFilePath      inFilePath(filename, PR_TRUE); // relative path.
  nsFileSpec      mySpec(inFilePath);
  nsresult        rv;
  char            newURL[1024] = ""; // URL for filename
  nsIURI          *aURL = nsnull;

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
  
  // Create the consumer output stream.. this will receive all the HTML from libmime
  ConsoleOutputStreamImpl   *ptr = new ConsoleOutputStreamImpl();
  nsCOMPtr<nsIOutputStream> out = do_QueryInterface(ptr);
  if (!out)
  {
    printf("Failed to create nsIOutputStream\n");
    return NS_ERROR_FAILURE;
  }
 
  // This is the producer stream that will deliver data from the disk file...
  FileInputStreamImpl   *ptr2 = new FileInputStreamImpl();
  nsCOMPtr<FileInputStreamImpl> in = do_QueryInterface(ptr2);
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
  
  // Create an nsIURI object needed for stream IO...
  PR_snprintf(newURL, sizeof(newURL), "file://%s", filename);
  FixURL(newURL);
  if (NS_FAILED(NewURL(&aURL, nsString(newURL))))
  {
    printf("Unable to open input file\n");
    return NS_ERROR_FAILURE;
  }

  // Set us as the output stream for HTML data from libmime...
  if (NS_FAILED(mimeParser->SetOutputStream(out, newURL)))
  {
    printf("Unable to set the output stream for the mime parser...\ncould be failure to create internal libmime data\n");
    return NS_ERROR_FAILURE;
  }

  // Assuming this is an RFC822 message...
  mimeParser->OnStartRequest(aURL, (const char *) MESSAGE_RFC822);

  // Just pump all of the data from the file into libmime...
  while (NS_SUCCEEDED(in->PumpFileStream()))
  {
    PRUint32    len;
    in->GetLength(&len);
    mimeParser->OnDataAvailable(aURL, in, len);
  }

  mimeParser->OnStopRequest(aURL, NS_OK, nsnull);
  NS_RELEASE(aURL);
  return NS_OK;
}
