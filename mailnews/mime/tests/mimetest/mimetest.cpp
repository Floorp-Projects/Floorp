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
#include "nsIEventQueueService.h"
#include "nsIInputStream.h"
#include "nsINetService.h"
#include "nsINetService.h"
#include "nsIOutputStream.h"
#include "nsIGenericFactory.h"
#include "nsIPostToServer.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFXMLDataSource.h"
#include "nsIRDFDocument.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIRDFXMLSource.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsDOMCID.h"    // for NS_SCRIPT_NAMESET_REGISTRY_CID
#include "nsLayoutCID.h" // for NS_NAMESPACEMANAGER_CID
#include "nsRDFCID.h"
#include "nsRDFCID.h"
#include "nsIComponentManager.h"
#include "prthread.h"
#include "plevent.h"
#include "plstr.h"
#include "nsParserCIID.h"
#include "nsIStreamConverter.h"
#include "nsFileStream.h"
#include "nsFileSpec.h"
#include "nsMimeTypes.h"
#include "nsIPref.h"
#include "nsICharsetConverterManager.h"

#ifdef XP_PC
#include "windows.h"
#endif 

#if defined(XP_PC)
#define DOM_DLL    "jsdom.dll"
#define LAYOUT_DLL "raptorhtml.dll"
#define NETLIB_DLL "netlib.dll"
#define PARSER_DLL "raptorhtmlpars.dll"
#define RDF_DLL    "rdf.dll"
#define XPCOM_DLL  "xpcom32.dll"
#define MIME_DLL  "mime.dll"
#define PREF_DLL  "xppref32.dll"
#define UNICHAR_DLL  "uconv.dll"
#elif defined(XP_UNIX)
#define DOM_DLL    "libjsdom"MOZ_DLL_SUFFIX
#define LAYOUT_DLL "libraptorhtml"MOZ_DLL_SUFFIX
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define PARSER_DLL "libraptorhtmlpars"MOZ_DLL_SUFFIX
#define RDF_DLL    "librdf"MOZ_DLL_SUFFIX
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#define MIME_DLL  "libmime.dll"MOZ_DLL_SUFFIX
#define PREF_DLL  "libxppref32"MOZ_DLL_SUFFIX
#define UNICHAR_DLL  "libunicharutil"MOZ_DLL_SUFFIX
#elif defined(XP_MAC)
#define DOM_DLL    "DOM_DLL"
#define LAYOUT_DLL "LAYOUT_DLL"
#define NETLIB_DLL "NETLIB_DLL"
#define PARSER_DLL "PARSER_DLL"
#define RDF_DLL    "RDF_DLL"
#define XPCOM_DLL  "XPCOM_DLL"
#define MIME_DLL   "MIME_DLL"
#define PREF_DLL  "XPPREF32_DLL"
#define UNICHAR_DLL  "UNICHARUTIL_DLL"
#endif

// {588595CB-2012-11d3-8EF0-00A024A7D144}
#define CONV_CID  \
    { 0x1e3f79f1, 0x6b6b, 0x11d2,   \
    { 0x8a, 0x86, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36 } };

#define CONV_IID \
    { 0x1e3f79f0, 0x6b6b, 0x11d2,   \
    {  0x8a, 0x86, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36 } }



////////////////////////////////////////////////////////////////////////
// CIDs

static NS_DEFINE_CID(charsetCID,  CONV_CID);

// prefs
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

// rdf
static NS_DEFINE_CID(kRDFServiceCID,        NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID,  NS_RDFXMLDATASOURCE_CID);

// xpcom
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kGenericFactoryCID,    NS_GENERICFACTORY_CID);

static NS_DEFINE_CID(kStreamConverterCID,    NS_STREAM_CONVERTER_CID);

/*
 * nsStreamConverter definitions....
 */
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID);

////////////////////////////////////////////////////////////////////////
// IIDs

NS_DEFINE_IID(kIEventQueueServiceIID,  NS_IEVENTQUEUESERVICE_IID);
NS_DEFINE_IID(kIOutputStreamIID,       NS_IOUTPUTSTREAM_IID);
NS_DEFINE_IID(kIInputStreamIID,       NS_IINPUTSTREAM_IID);
NS_DEFINE_IID(kIRDFXMLDataSourceIID,   NS_IRDFXMLDATASOURCE_IID);
NS_DEFINE_IID(kIRDFServiceIID,         NS_IRDFSERVICE_IID);
NS_DEFINE_IID(kIRDFXMLSourceIID,       NS_IRDFXMLSOURCE_IID);

NS_DEFINE_IID(kConvMeIID,             CONV_IID);

#include "nsIAllocator.h" // for the CID

nsICharsetConverterManager *ccMan;

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
  nsComponentManager::RegisterComponent(kEventQueueServiceCID,     NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kEventQueueCID,            NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kGenericFactoryCID,        NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kAllocatorCID,             NULL, NULL, XPCOM_DLL,  PR_FALSE, PR_FALSE);

  // prefs
  nsComponentManager::RegisterComponent(kPrefCID,             NULL, NULL, PREF_DLL,  PR_FALSE, PR_FALSE);

  // mime
  nsComponentManager::RegisterComponent(kStreamConverterCID,             NULL, NULL, MIME_DLL,  PR_FALSE, PR_FALSE);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////

class ConsoleOutputStreamImpl : public nsIOutputStream
{
public:
    ConsoleOutputStreamImpl(void) { NS_INIT_REFCNT(); }
    virtual ~ConsoleOutputStreamImpl(void) {}

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIBaseStream interface
    NS_IMETHOD Close(void) {
        return NS_OK;
    }

    // nsIOutputStream interface
    NS_IMETHOD Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount) {
        PR_Write(PR_GetSpecialFD(PR_StandardOutput), aBuf, aCount);
        *aWriteCount = aCount;
        return NS_OK;
    }

    NS_IMETHOD Flush(void) {
        PR_Sync(PR_GetSpecialFD(PR_StandardOutput));
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(ConsoleOutputStreamImpl, kIOutputStreamIID);

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

NS_IMPL_ISUPPORTS(FileInputStreamImpl, kIInputStreamIID);

////////////////////////////////////////////////////////////////////////

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

#include "nsSpecialSystemDirectory.h"
nsresult
AutoregisterComponents()
{
  // Autoregister components to populate registry
  // All this logic is in nsSpecialFileSpec in apprunner.
  // But hey this is viewer a different app.

  nsresult rv = NS_ERROR_FAILURE;

  nsSpecialSystemDirectory sysdir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
#ifdef XP_MAC
  sysdir += "Components";
#else
  sysdir += "components";
#endif /* XP_MAC */
  nsNSPRPath componentsDir(sysdir);
  const char *componentsDirPath = (const char *) componentsDir;
  if (componentsDirPath != NULL)
  {
    rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, componentsDirPath);
  }

  return rv;
}

int
main(int argc, char** argv)
{
  nsresult rv;
  
  if (argc < 2) 
  {
    fprintf(stderr, "usage: %s <rfc822_disk_file>\n", argv[0]);
    return 1;
  }
  
  SetupRegistry();
//  AutoregisterComponents();

  // Get an event queue started...
  NS_WITH_SERVICE(nsIEventQueueService, theEventQueueService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = theEventQueueService->CreateThreadEventQueue();
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create thread event queue");
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIStreamConverter> mimeParser;
  rv = nsComponentManager::CreateInstance(kStreamConverterCID, 
                                          NULL, nsIStreamConverter::GetIID(), 
                                          (void **) getter_AddRefs(mimeParser)); 
  if (NS_FAILED(rv) || !mimeParser)
  {
    printf("Failed to create MIME stream converter...\n");
    return rv;
  }
  
  // And this should receive all the HTML from libmime
  nsCOMPtr<nsIOutputStream> out = do_QueryInterface(new ConsoleOutputStreamImpl());
  if (! out)
  {
    printf("Failed to create nsIOutputStream\n");
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  nsFilePath      inFilePath(argv[1], PR_TRUE); // relative path.
  const char      *pathAsString = (const char*)argv[1];
  nsFileSpec      mySpec(inFilePath);
 
  // And this should receive all the HTML from libmime
  nsCOMPtr<FileInputStreamImpl> in = do_QueryInterface(new FileInputStreamImpl());
  if (! in )
  {
    printf("Failed to create nsIInputStream\n");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (NS_FAILED(in->OpenDiskFile(mySpec)))
  {
    printf("Unable to open input file %s\n", argv[1]);
    return -1;
  }
  
  // Create an nsIURL object needed for stream IO...
  char newURL[512];
  PR_snprintf(newURL, sizeof(newURL), "file://%s", argv[1]);
  FixURL(newURL);
  nsIURL  *aURL = nsnull;
  if (NS_FAILED(NewURL(&aURL, nsString(newURL))))
  {
    printf("Unable to open input file\n");
    return -1;
  }

  // Set us as the output stream for HTML data from libmime...
  if (NS_FAILED(mimeParser->SetOutputStream(out, newURL)))
  {
    printf("Unable to set the output stream for the mime parser...\ncould be failure to create internal data\n");
    return -1;
  }

  mimeParser->OnStartBinding(aURL, MESSAGE_RFC822);
  while (NS_SUCCEEDED(in->PumpFileStream()))
  {
    PRUint32    len;
    in->GetLength(&len);
    mimeParser->OnDataAvailable(aURL, in, len);
  }

  mimeParser->OnStopBinding(aURL, NS_OK, nsnull);

  return NS_OK;
}

// SetOutputListener(nsIStreamListener *outListner); 

