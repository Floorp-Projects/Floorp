/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
/*
  This program drives libmime to parse an RFC822 message and output the 
  HTML representation of it.

  The program takes a single parameter: the disk file from which to read.
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
#include "nsIMimeStreamConverter.h"
#include "nsFileStream.h"
#include "nsFileSpec.h"
#include "nsMimeTypes.h"
#include "nsIPref.h"
#include "nsICharsetConverterManager.h"
#include "prprf.h"
#include "nsIMemory.h" // for the CID
#include "msgCore.h"
#include "nsIIOService.h"
#include "nsMimeEmitterCID.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgMailSession.h"
#include "nsMsgMimeCID.h"
#include "nsStreamConverter.h"    // test hack
#include "nsIAllocator.h"

#ifdef XP_PC
#include <windows.h>
#endif

////////////////////////////////////////////////////////////////////////////////////
// THIS IS THE STUFF TO GET THE TEST HARNESS OFF THE GROUND
////////////////////////////////////////////////////////////////////////////////////
#if defined(XP_PC)
#define NETLIB_DLL "netlib.dll"
#define XPCOM_DLL  "xpcom32.dll"
#define MIME_DLL  "mime.dll"
#define PREF_DLL  "xppref32.dll"
#define UNICHAR_DLL  "uconv.dll"
#define EMITTER_DLL  "mimeemitter.dll"    // Hack..but it works???
#elif defined(XP_UNIX) || defined(XP_BEOS)
#define NETLIB_DLL "libnetlib"MOZ_DLL_SUFFIX
#define XPCOM_DLL  "libxpcom"MOZ_DLL_SUFFIX
#define MIME_DLL  "libmime"MOZ_DLL_SUFFIX
#define PREF_DLL  "libpref"MOZ_DLL_SUFFIX
#define UNICHAR_DLL  "libunicharutil"MOZ_DLL_SUFFIX
#define EMITTER_DLL  "libmimeemitter"MOZ_DLL_SUFFIX
#elif defined(XP_MAC)
#define NETLIB_DLL "NETLIB_DLL"
#define XPCOM_DLL  "XPCOM_DLL"
#define MIME_DLL   "MIME_DLL"
#define PREF_DLL  "XPPREF32_DLL"
#define UNICHAR_DLL  "UNICHARUTIL_DLL"
#define EMITTER_DLL  "MIMEEMITTER_DLL"
#endif

// {588595CB-2012-11d3-8EF0-00A024A7D144}
#define CONV_CID  { 0x1e3f79f1, 0x6b6b, 0x11d2, { 0x8a, 0x86, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36 } };
#define CONV_IID  { 0x1e3f79f0, 0x6b6b, 0x11d2, {  0x8a, 0x86, 0x0, 0x60, 0x8, 0x11, 0xa8, 0x36 } }

// CIDs

// I18N
static NS_DEFINE_CID(charsetCID,  CONV_CID);
NS_DEFINE_IID(kConvMeIID,             CONV_IID);
static NS_DEFINE_CID(kAppShellServiceCID,   NS_APPSHELL_SERVICE_CID);

// prefs
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

// xpcom
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kGenericFactoryCID,    NS_GENERICFACTORY_CID);

// netlib definitions....
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);

// Stream converter
static NS_DEFINE_CID(kStreamConverterCID,    NS_MAILNEWS_MIME_STREAM_CONVERTER_CID);

// Emitters converter
static NS_DEFINE_CID(kHtmlEmitterCID, NS_HTML_MIME_EMITTER_CID);
static NS_DEFINE_CID(kXmlEmitterCID, NS_XML_MIME_EMITTER_CID);
static NS_DEFINE_CID(kRawEmitterCID, NS_RAW_MIME_EMITTER_CID);

static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

nsICharsetConverterManager *ccMan = nsnull;

static nsresult
SetupRegistry(void)
{
  // This line should handle all of the registry setup, but just to be paranoid, I am
  // leaving all of the rest of the registrations here as well.
  nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);

  // i18n
  nsComponentManager::RegisterComponent(charsetCID, NULL, NULL, UNICHAR_DLL,  PR_FALSE, PR_FALSE);
  nsresult res = nsServiceManager::GetService(charsetCID, kConvMeIID, (nsISupports **)&ccMan);
  if (NS_FAILED(res)) 
  {
    printf("ERROR at GetService() code=0x%x.\n",res);
    return NS_ERROR_FAILURE;
  }

  // netlib
  // RICHIE nsComponentManager::RegisterComponent(kNetServiceCID,     NULL, NULL, NETLIB_DLL,  PR_FALSE, PR_FALSE);
  
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

  nsComponentManager::RegisterComponent(kHtmlEmitterCID, "RFC822 Parser", "@mozilla.org/messenger/mimeemitter;1?type=text/html", EMITTER_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kXmlEmitterCID,  "RFC822 Parser", "@mozilla.org/messenger/mimeemitter;1?type=raw", EMITTER_DLL,  PR_FALSE, PR_FALSE);
  nsComponentManager::RegisterComponent(kRawEmitterCID,  "RFC822 Parser", "@mozilla.org/messenger/mimeemitter;1?type=text/xml", EMITTER_DLL,  PR_FALSE, PR_FALSE);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// END OF STUFF TO GET THE TEST HARNESS OFF THE GROUND
////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////
// THIS IS THE CLASS THAT WOULD BE IMPLEMENTED BY THE CONSUMER OF THE HTML OUPUT
// FROM LIBMIME. THIS EXAMPLE SIMPLY WRITES THE OUTPUT TO STDOUT()
////////////////////////////////////////////////////////////////////////////////////
class ConsoleOutputStreamListener : public nsIStreamListener
{
public:
    ConsoleOutputStreamListener(void) 
    { 
      NS_INIT_REFCNT(); 
      mComplete = PR_FALSE;
      mIndentCount = 0;
      mInClosingTag = PR_FALSE;
      mOutFormat = nsMimeOutput::nsMimeMessageRaw;
    }

    virtual ~ConsoleOutputStreamListener(void) {}

    // nsISupports interface
    NS_DECL_ISUPPORTS

	NS_IMETHOD OnDataAvailable(nsIChannel * aChannel, 
							 nsISupports    *ctxt, 
                             nsIInputStream *inStr, 
                             PRUint32       sourceOffset, 
                             PRUint32       count);

	NS_IMETHOD OnStartRequest(nsIChannel * aChannel, nsISupports *ctxt) { return NS_OK;}
	NS_IMETHOD OnStopRequest(nsIChannel * aChannel, nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg)
	{
      if ((mOutFormat == nsMimeOutput::nsMimeMessageSplitDisplay) ||
          (mOutFormat == nsMimeOutput::nsMimeMessageBodyDisplay) ||
          (mOutFormat == nsMimeOutput::nsMimeMessagePrintOutput) ||
          (mOutFormat == nsMimeOutput::nsMimeMessageQuoting))
      {
        char *note = "\n<center><hr WIDTH=\"90%\"><br><b>Anything after the above horizontal line is diagnostic output<br>and is not part of the HTML stream!</b></center><pre>\n";
       PR_Write(PR_GetSpecialFD(PR_StandardOutput), note, PL_strlen(note));
      }

      mComplete = PR_TRUE;
      return NS_OK;
    }

    // nsIOutputStream interface

    NS_IMETHOD Flush(void) {
        PR_Sync(PR_GetSpecialFD(PR_StandardOutput));
        return NS_OK;
    }

    nsresult    DoIndent();
    NS_IMETHOD  SetFormat(nsMimeOutputType  aFormat);
    PRBool      CompleteYet()
    {
      return mComplete;
    }

private:
  PRBool              mComplete;
  PRInt32             mIndentCount;
  PRBool              mInClosingTag;
  nsMimeOutputType    mOutFormat;
};

//NS_IMPL_ISUPPORTS2(ConsoleOutputStreamListener, nsIOutputStream, nsIStreamListener);
NS_IMPL_ISUPPORTS(ConsoleOutputStreamListener, NS_GET_IID(nsIStreamListener));

#define TAB_SPACES    2

nsresult
ConsoleOutputStreamListener::SetFormat(nsMimeOutputType  aFormat)
{
  mOutFormat = aFormat;
  return NS_OK;
}

// make the html pretty :-)
nsresult
ConsoleOutputStreamListener::DoIndent()
{
  PR_Write(PR_GetSpecialFD(PR_StandardOutput), MSG_LINEBREAK, MSG_LINEBREAK_LEN);
  if (mIndentCount <= 1)
    return NS_OK;

  for (PRUint32 j=0; j<(PRUint32) ((mIndentCount-1)*TAB_SPACES); j++)
    PR_Write(PR_GetSpecialFD(PR_StandardOutput), " ", 1);
  return NS_OK;
}

NS_IMETHODIMP ConsoleOutputStreamListener::OnDataAvailable(nsIChannel * aChannel, 
							 nsISupports    *ctxt, 
                             nsIInputStream *inStr, 
                             PRUint32       sourceOffset, 
                             PRUint32       count)
{
  PRUint32 i=0;
  PRUint32 aCount = 0;
  char * aBuf = (char *) PR_Malloc(sizeof(char) * (count+1));
  inStr->Read(aBuf, count, &aCount);

  // If raw, don't postprocess...
  if ((mOutFormat == nsMimeOutput::nsMimeMessageRaw) || 
      (mOutFormat == nsMimeOutput::nsMimeMessageXULDisplay) )
  {
    PR_Write(PR_GetSpecialFD(PR_StandardOutput), aBuf, aCount);
    return NS_OK;
  }

  PRBool mDoIndentProcessing = PR_FALSE;
  for (i=0; i<aCount; i++)
  {
    if (mDoIndentProcessing)
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
    }

    // Now, write the HTML character...
    PR_Write(PR_GetSpecialFD(PR_StandardOutput), aBuf+i, 1);

    if (mDoIndentProcessing)
    {
      if (aBuf[i] == '>')
      {
        // Now, write out the correct number or spaces for the indent count...
        if (mInClosingTag)
          DoIndent();
      }
    }
  }

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

  NS_IMETHOD Available(PRUint32 *_retval)
  {
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
  NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval) {return NS_OK;};
  NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking) {return NS_OK;};
  NS_IMETHOD GetObserver(nsIInputStreamObserver * *aObserver) {return NS_OK;};
  NS_IMETHOD SetObserver(nsIInputStreamObserver * aObserver) {return NS_OK;};

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
    return NS_ERROR_FAILURE;
  
  mBufLen = mInFile->read(mBuf, sizeof(mBuf));
  if (mBufLen > 0)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}

NS_IMPL_ISUPPORTS(FileInputStreamImpl, NS_GET_IID(nsIInputStream));

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

    if (*ptr == ':')
      *ptr = '|';

    ++ptr;
  }
}

// Utility to create a nsIURL object...
nsresult 
NewURI(nsIURI** aInstancePtrResult, const char *aSpec)
{  
  nsresult res;

  NS_WITH_SERVICE(nsIIOService, pService, kIOServiceCID, &res);
  if (NS_FAILED(res)) 
    return NS_ERROR_FAILURE;

  res = pService->NewURI(aSpec, nsnull, aInstancePtrResult);
  if (NS_FAILED(res))
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}

// Utility to create a nsIURL object...
nsresult 
NewChannel(nsIChannel **aInstancePtrResult, nsIURI *aURI)
{  
  nsresult res;

  NS_WITH_SERVICE(nsIIOService, pService, kIOServiceCID, &res);
  if (NS_FAILED(res)) 
    return NS_ERROR_FAILURE;

  res = pService->NewChannelFromURI(aURI, aInstancePtrResult);

  if (NS_FAILED(res))
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}


int
main(int argc, char** argv)
{
  nsresult DoFormattingOnly(char *filename);
  nsresult DoRFC822toHTMLConversion(char *filename, int numArgs, int format);
  nsresult rv;
  
  // Do some sanity checking...
  if (argc < 2) 
  {
    fprintf(stderr, "usage: %s <rfc822_disk_file> [<output_format>]\n\n", argv[0]);
    fprintf(stderr, "where output_format is:\n\n19 - indentation formatting\n");
    fprintf(stderr, "  0 - nsMimeMessageSplitDisplay\n");
    fprintf(stderr, "  1 - nsMimeMessageHeaderDisplay\n");
    fprintf(stderr, "  2 - nsMimeMessageBodyDisplay\n");
    fprintf(stderr, "  3 - nsMimeMessageQuoting\n");
    fprintf(stderr, "  4 - nsMimeMessageBodyQuoting\n");
    fprintf(stderr, "  5 - nsMimeMessageRaw\n");
    fprintf(stderr, "  6 - nsMimeMessageDraftOrTemplate\n");
    fprintf(stderr, "  7 - nsMimeMessageEditorTemplate\n");
    fprintf(stderr, "  8 - nsMimeMessageXULDisplay\n");
    fprintf(stderr, "  9 - nsMimeMessagePrintOutput\n");
    fprintf(stderr, "  10 - nsMimeMessageSaveAs\n");
    fprintf(stderr, "  11 - nsMimeMessageSource\n");
    fprintf(stderr, "  12 - nsMimeUnknown\n");
    fprintf(stderr, "  (the numbers are not garanteed to persist. See\n");
    fprintf(stderr, "  <http://lxr.mozilla.org/seamonkey/source/mailnews/mime/public/nsIMimeStreamConverter.idl>\n");
    fprintf(stderr, "  interface nsMimeOutput for most recent info.)\n");
    return 1;
  }
  
  // Setup the registry...
  SetupRegistry();

  // Get an event queue started...
  NS_WITH_SERVICE(nsIEventQueueService, theEventQueueService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) 
    return rv;

  rv = theEventQueueService->CreateThreadEventQueue();
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create thread event queue");
  if (NS_FAILED(rv)) 
    return rv;
 
  // Do the conversion or just process the file!
  if ( (argc >= 3) && (atoi(argv[2]) == 19) )
    DoFormattingOnly(argv[1]);
  else 
  {
    int   format;
    if (argc >= 3)
      format = atoi(argv[2]);
    else
      format = nsMimeOutput::nsMimeMessagePrintOutput;
    DoRFC822toHTMLConversion(argv[1], argc, format);
  }

  // Cleanup stuff necessary...
  NS_IF_RELEASE(ccMan);
  return NS_OK;
}

nsresult
DoRFC822toHTMLConversion(char *filename, int numArgs, int outFormat)
{
  nsresult          rv;
  char              newURL[1024] = ""; // URL for filename
  nsIURI            *theURI = nsnull;
  char              *contentType = nsnull;

  char *opts = PL_strchr(filename, '?');
  char save;
  if (opts)
  {
    save = *opts;
    *opts = '\0';
  }

  nsFilePath      inFilePath(filename, PR_TRUE); // relative path.
  nsFileSpec      mySpec(inFilePath);

  if (!mySpec.Exists())
  {
    printf("Unable to open input file %s\n", filename);
    return NS_ERROR_FAILURE;
  }

  if (opts)
    *opts = save;

  // Create a mime parser (nsIStreamConverter)!
  nsCOMPtr<nsIStreamConverter> mimeParser;
  rv = nsComponentManager::CreateInstance(kStreamConverterCID, 
                                          NULL, NS_GET_IID(nsIStreamConverter), 
                                          (void **) getter_AddRefs(mimeParser)); 
  if (NS_FAILED(rv) || !mimeParser)
  {
    printf("Failed to create MIME stream converter...\n");
    return rv;
  }
  
  nsCOMPtr<nsIStreamListener> out = nsnull;
  ConsoleOutputStreamListener   *ptr = nsnull;
  if (outFormat != nsMimeOutput::nsMimeMessageDraftOrTemplate)
  {
    // Create the consumer output stream.. this will receive all the HTML from libmime
    ptr = new ConsoleOutputStreamListener();
    out = do_QueryInterface(ptr);
    if (!out)
    {
      printf("Failed to create nsIOutputStream\n");
      return NS_ERROR_FAILURE;
    }
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
  PR_snprintf(newURL, sizeof(newURL), "file:///%s", filename);
  FixURL(newURL + 5);
  if (NS_FAILED(NewURI(&theURI, newURL)))
  {
    printf("Unable to open input file\n");
    return NS_ERROR_FAILURE;
  }

  // Set us as the output stream for HTML data from libmime...
  nsCOMPtr<nsIMimeStreamConverter> mimeStream = do_QueryInterface(mimeParser);
  mimeStream->SetMimeOutputType(outFormat);

  nsIChannel    *tChannel = nsnull;

  NewChannel(&tChannel, theURI);
  mimeParser->AsyncConvertData(
                 nsString(NS_ConvertToString("message/rfc822")).GetUnicode(), 
                 nsString(NS_ConvertToString("text/xul")).GetUnicode(),
                 out, tChannel);

//  rv = mimeParser->Init(theURI, out, nsnull);
  if (NS_FAILED(rv) || !mimeParser)
  {
    printf("Unable to set the output stream for the mime parser...\ncould be failure to create internal libmime data\n");
    return NS_ERROR_FAILURE;
  }

  if (contentType)
  {
    printf("Content type for output = %s\n", contentType);
    PR_FREEIF(contentType);
  }

  if (ptr)
    ptr->SetFormat(outFormat);

  // Assuming this is an RFC822 message...
  mimeParser->OnStartRequest(nsnull, theURI);
  // Just pump all of the data from the file into libmime...
  while (NS_SUCCEEDED(in->PumpFileStream()))
  {
    PRUint32    len;
    in->GetLength(&len);
    if (mimeParser->OnDataAvailable(nsnull, theURI, in, 0, len) != NS_OK)
      break;
  }

  mimeParser->OnStopRequest(nsnull, theURI, NS_OK, nsnull);
  NS_RELEASE(theURI);

  return NS_OK;
}

nsresult
DoFormattingOnly(char *filename)
{
  char              newURL[1024] = ""; // URL for filename
  nsIURI            *theURI = nsnull;
  char              *contentType = nsnull;
  int               outFormat;

  outFormat = nsMimeOutput::nsMimeMessagePrintOutput;

  char *opts = PL_strchr(filename, '?');
  char save;
  if (opts)
  {
    save = *opts;
    *opts = '\0';
  }

  nsFilePath      inFilePath(filename, PR_TRUE); // relative path.
  nsFileSpec      mySpec(inFilePath);

  if (!mySpec.Exists())
  {
    printf("Unable to open input file %s\n", filename);
    return NS_ERROR_FAILURE;
  }

  if (opts)
    *opts = save;

  
  nsCOMPtr<nsIStreamListener> out = nsnull;
  ConsoleOutputStreamListener   *ptr = nsnull;
  // Create the consumer output stream.. this will receive all the HTML from libmime
  ptr = new ConsoleOutputStreamListener();
  out = do_QueryInterface(ptr);
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
  if (NS_FAILED(NewURI(&theURI, newURL)))
  {
    printf("Unable to open input file\n");
    return NS_ERROR_FAILURE;
  }

  nsIChannel    *tChannel = nsnull;

  NewChannel(&tChannel, theURI);

  if (ptr)
    ptr->SetFormat(outFormat);

  // Just pump all of the data from the file into formatter...
  while (NS_SUCCEEDED(in->PumpFileStream()))
  {
    PRUint32    len;

    in->GetLength(&len);
    if (out->OnDataAvailable(tChannel, nsnull, in, 0, len) != NS_OK)
      break;
  }

  NS_RELEASE(theURI);
  return NS_OK;
}
