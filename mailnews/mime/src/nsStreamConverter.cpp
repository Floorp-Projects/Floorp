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
#include "nsCOMPtr.h"
#include "stdio.h"
#include "mimecom.h"
#include "modmimee.h"
#include "nscore.h"
#include "nsStreamConverter.h"
#include "comi18n.h"
#include "prmem.h"
#include "prprf.h"
#include "prlog.h"
#include "plstr.h"
#include "mimemoz2.h"
#include "nsMimeTypes.h"
#include "nsRepository.h"
#include "nsIURL.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsIAllocator.h"
#include "nsIPipe.h"
#include "nsMimeStringResources.h"
#include "nsIPref.h"
#include "nsNetUtil.h"
#include "nsIMsgQuote.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
#include "mozITXTToHTMLConv.h"

#define PREF_MAIL_DISPLAY_GLYPH "mail.display_glyph"
#define PREF_MAIL_DISPLAY_STRUCT "mail.display_struct"

#define PREF_MAIL_MIME_XUL_OUTPUT "mail.mime_xul_output"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

////////////////////////////////////////////////////////////////
// Bridge routines for new stream converter XP-COM interface 
////////////////////////////////////////////////////////////////

// RICHIE - should live in the mimedrft.h header file!
extern "C" void  *
mime_bridge_create_draft_stream(nsIMimeEmitter      *newEmitter,
                                nsStreamConverter   *newPluginObj2,
                                nsIURI              *uri,
                                nsMimeOutputType    format_out);

extern "C" void  *
bridge_create_stream(nsIMimeEmitter      *newEmitter,
                     nsStreamConverter   *newPluginObj2,
                     nsIURI              *uri,
                     nsMimeOutputType    format_out,
		                 PRUint32		         whattodo,
                     nsIChannel          *aChannel)
{
  if  ( (format_out == nsMimeOutput::nsMimeMessageDraftOrTemplate) ||
        (format_out == nsMimeOutput::nsMimeMessageEditorTemplate) )
    return mime_bridge_create_draft_stream(newEmitter, newPluginObj2, uri, format_out);
  else 
    return mime_bridge_create_display_stream(newEmitter, newPluginObj2, uri, format_out, whattodo,
                                             aChannel);
}

void
bridge_destroy_stream(void *newStream)
{
  nsMIMESession     *stream = (nsMIMESession *)newStream;
  if (!stream)
    return;
  
  PR_FREEIF(stream);
}

void          
bridge_set_output_type(void *bridgeStream, nsMimeOutputType aType)
{
  nsMIMESession *session = (nsMIMESession *)bridgeStream;

  if (session)
  {
    struct mime_stream_data *msd = (struct mime_stream_data *)session->data_object;
    if (msd)
      msd->format_out = aType;     // output format type
  }
}

nsresult
bridge_new_new_uri(void *bridgeStream, nsIURI *aURI)
{
  nsMIMESession *session = (nsMIMESession *)bridgeStream;

  if (session)
  {
    struct mime_stream_data *msd = (struct mime_stream_data *)session->data_object;
    if (msd)
    {
      char *urlString;
      if (NS_SUCCEEDED(aURI->GetSpec(&urlString)))
      {
        if ((urlString) && (*urlString))
        {
          PR_FREEIF(msd->url_name);
          msd->url_name = nsCRT::strdup(urlString);
          if (!(msd->url_name))
            return NS_ERROR_OUT_OF_MEMORY;

          PR_FREEIF(urlString);
        }
      }
    }
  }

  return NS_OK;
}

static int
mime_headers_callback ( void *closure, MimeHeaders *headers )
{
  struct mime_stream_data *msd = (struct mime_stream_data *)closure;

  PR_ASSERT ( msd && headers );
  
  if ( !msd || ! headers ) 
    return 0;

  PR_ASSERT ( msd->headers == NULL );
  msd->headers = MimeHeaders_copy ( headers );

  return 0;
}

nsresult          
bridge_set_mime_stream_converter_listener(void *bridgeStream, nsIMimeStreamConverterListener* listener)
{
  nsMIMESession *session = (nsMIMESession *)bridgeStream;

  if (session)
  {
    struct mime_stream_data *msd = (struct mime_stream_data *)session->data_object;
    if (msd)
    {
      if (listener)
      {
        msd->options->caller_need_root_headers = PR_TRUE;
        msd->options->decompose_headers_info_fn = mime_headers_callback;
      }
      else
      {
        msd->options->caller_need_root_headers = PR_FALSE;
        msd->options->decompose_headers_info_fn = nsnull;
      }
    }
  }
  return NS_OK;
}



//
// Utility routines needed by this interface...
//
nsresult
nsStreamConverter::DetermineOutputFormat(const char *url,  nsMimeOutputType *aNewType)
{
  // Default to html the entire document...
	*aNewType = nsMimeOutput::nsMimeMessageQuoting;

  // Do sanity checking...
  if ( (!url) || (!*url) )
  {
    mOutputFormat = nsCRT::strdup("text/html");
    return NS_OK;
  }

  char *format = PL_strcasestr(url, "?outformat=");
  char *part   = PL_strcasestr(url, "?part=");
  char *header = PL_strcasestr(url, "?header=");

  if (!format) format = PL_strcasestr(url, "&outformat=");
  if (!part) part = PL_strcasestr(url, "&part=");
  if (!header) header = PL_strcasestr(url, "&header=");

  // First, did someone pass in a desired output format. They will be able to
  // pass in any content type (i.e. image/gif, text/html, etc...but the "/" will
  // have to be represented via the "%2F" value
  if (format)
  {
    format += nsCRT::strlen("?outformat=");
    while (*format == ' ')
      ++format;

    if ((format) && (*format))
    {
      char *ptr;
      PR_FREEIF(mOutputFormat);
      mOutputFormat = nsCRT::strdup(format);
      mOverrideFormat = nsCRT::strdup("raw");
      ptr = mOutputFormat;
      do
      {
        if ( (*ptr == '?') || (*ptr == '&') || 
             (*ptr == ';') || (*ptr == ' ') )
        {
          *ptr = '\0';
          break;
        }
        else if (*ptr == '%')
        {
          if ( (*(ptr+1) == '2') &&
               ( (*(ptr+2) == 'F') || (*(ptr+2) == 'f') )
              )
          {
            *ptr = '/';
            memmove(ptr+1, ptr+3, nsCRT::strlen(ptr+3));
            *(ptr + nsCRT::strlen(ptr+3) + 1) = '\0';
            ptr += 3;
          }
        }
      } while (*ptr++);
  
      // Don't muck with this data!
      *aNewType = nsMimeOutput::nsMimeMessageRaw;
      return NS_OK;
    }
  }

  if (!part)
  {
    if (header)
    {
      PRInt32 lenOfHeader = nsCRT::strlen("?header=");

      char *ptr2 = PL_strcasestr ("only", (header+lenOfHeader));
      char *ptr3 = PL_strcasestr ("quote", (header+lenOfHeader));
      char *ptr4 = PL_strcasestr ("quotebody", (header+lenOfHeader));
      char *ptr5 = PL_strcasestr ("none", (header+lenOfHeader));
      char *ptr6 = PL_strcasestr ("print", (header+lenOfHeader));
      char *ptr7 = PL_strcasestr ("saveas", (header+lenOfHeader));
      char *ptr8 = PL_strcasestr ("src", (header+lenOfHeader));
      if (ptr5)
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = nsCRT::strdup("text/html");
        *aNewType = nsMimeOutput::nsMimeMessageBodyDisplay;
      }
      else if (ptr2)
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = nsCRT::strdup("text/xml");
        *aNewType = nsMimeOutput::nsMimeMessageHeaderDisplay;
      }
      else if (ptr3)
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = nsCRT::strdup("text/html");
        *aNewType = nsMimeOutput::nsMimeMessageQuoting;
      }
      else if (ptr4)
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = nsCRT::strdup("text/html");
        *aNewType = nsMimeOutput::nsMimeMessageBodyQuoting;
      }
      else if (ptr6)
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = nsCRT::strdup("text/html");
        *aNewType = nsMimeOutput::nsMimeMessagePrintOutput;
      }
      else if (ptr7)
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = nsCRT::strdup("text/html");
        *aNewType = nsMimeOutput::nsMimeMessageSaveAs;
      }
      else if (ptr8)
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = nsCRT::strdup("text/plain");
        *aNewType = nsMimeOutput::nsMimeMessageSource;
      }
    }
    else
    {
      // okay, we are just doing a regular message display url
      // since we didn't have any special extensions after the url...
      // we need to know if we need to use the text/xul or text/html emitter.
      // check the desired output content type that was passed into AsyncConvertData.

      if (mDesiredOutputType && nsCRT::strcasecmp(mDesiredOutputType, "text/xul") == 0)
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = nsCRT::strdup("text/xul");
        *aNewType = nsMimeOutput::nsMimeMessageXULDisplay;
      }
      else
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = nsCRT::strdup("text/html");
        *aNewType = nsMimeOutput::nsMimeMessageBodyDisplay;
      }
    }
  }
  else // this is a part that should just come out raw!
  {
    PR_FREEIF(mOutputFormat);
    mOutputFormat = nsCRT::strdup("raw");
    *aNewType = nsMimeOutput::nsMimeMessageRaw;
  }

  return NS_OK;
}

nsresult 
nsStreamConverter::InternalCleanup(void)
{
  PR_FREEIF(mOutputFormat);
  nsCRT::free(mDesiredOutputType);
  if (mBridgeStream)
  {
    bridge_destroy_stream(mBridgeStream);
    mBridgeStream = nsnull;
  }

  PR_FREEIF(mOverrideFormat);
  return NS_OK;
}

/* 
 * Inherited methods for nsMimeConverter
 */
nsStreamConverter::nsStreamConverter()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();

  // Init member variables...
  mOverrideFormat = nsnull;

  mWrapperOutput = PR_FALSE;
  mBridgeStream = NULL;
  mTotalRead = 0;
  mOutputFormat = nsCRT::strdup("text/html");
  mDoneParsing = PR_FALSE;
  mAlreadyKnowOutputType = PR_FALSE;
  mMimeStreamConverterListener = nsnull;
  mForwardInline = PR_FALSE;
}

nsStreamConverter::~nsStreamConverter()
{
  InternalCleanup();
}


NS_IMPL_ISUPPORTS4(nsStreamConverter, nsIStreamListener, nsIStreamObserver, nsIStreamConverter, nsIMimeStreamConverter)

///////////////////////////////////////////////////////////////
// nsStreamConverter definitions....
///////////////////////////////////////////////////////////////

NS_IMETHODIMP nsStreamConverter::Init(nsIURI *aURI, nsIStreamListener * aOutListener, nsIChannel *aChannel)
{
  nsresult rv = NS_OK;
  
  mOutListener = aOutListener;
  mDesiredOutputType = nsnull;
  
  // mscott --> we need to look at the url and figure out what the correct output type is...
  nsMimeOutputType newType;
  
  if (!mAlreadyKnowOutputType)
  {
    nsXPIDLCString urlSpec;
    rv = aURI->GetSpec(getter_Copies(urlSpec));
    DetermineOutputFormat(urlSpec, &newType);
    mAlreadyKnowOutputType = PR_TRUE;
  }
  else
    newType = mOutputType;
  
  mOutputType = newType;  
  switch (newType)
  {
  case nsMimeOutput::nsMimeMessageXULDisplay:
    PR_FREEIF(mOutputFormat);
    mOutputFormat = nsCRT::strdup("text/xul");
    break;
		case nsMimeOutput::nsMimeMessageSplitDisplay:    // the wrapper HTML output to produce the split header/body display
      mWrapperOutput = PR_TRUE;
      PR_FREEIF(mOutputFormat);
      mOutputFormat = nsCRT::strdup("text/html");
      break;
    case nsMimeOutput::nsMimeMessageHeaderDisplay:   // the split header/body display
      PR_FREEIF(mOutputFormat);
      mOutputFormat = nsCRT::strdup("text/xml");
      break;
    case nsMimeOutput::nsMimeMessageBodyDisplay:   // the split header/body display
      PR_FREEIF(mOutputFormat);
      mOutputFormat = nsCRT::strdup("text/html");
      break;
      
    case nsMimeOutput::nsMimeMessageQuoting:   		// all HTML quoted output
    case nsMimeOutput::nsMimeMessageSaveAs:   		// Save as operation
    case nsMimeOutput::nsMimeMessageBodyQuoting: 	// only HTML body quoted output
    case nsMimeOutput::nsMimeMessagePrintOutput:  // all Printing output
      PR_FREEIF(mOutputFormat);
      mOutputFormat = nsCRT::strdup("text/html");
      break;
      
    case nsMimeOutput::nsMimeMessageRaw:       // the raw RFC822 data (view source) and attachments
      PR_FREEIF(mOutputFormat);
      mOutputFormat = nsCRT::strdup("raw");
      break;
      
    case nsMimeOutput::nsMimeMessageSource:    // the raw RFC822 data (view source) and attachments
      PR_FREEIF(mOutputFormat);
      PR_FREEIF(mOverrideFormat);
      mOutputFormat = nsCRT::strdup("text/plain");
      mOverrideFormat = nsCRT::strdup("raw");
      break;
      
    case nsMimeOutput::nsMimeMessageDraftOrTemplate:       // Loading drafts & templates
      PR_FREEIF(mOutputFormat);
      mOutputFormat = nsCRT::strdup("message/draft");
      break;
      
    case nsMimeOutput::nsMimeMessageEditorTemplate:       // Loading templates into editor
      PR_FREEIF(mOutputFormat);
      mOutputFormat = nsCRT::strdup("text/html");
      break;
    default:
      NS_ASSERTION(0, "this means I made a mistake in my assumptions");
  }
  
  
	// the following output channel stream is used to fake the content type for people who later
	// call into us..
  rv = NS_NewInputStreamChannel(getter_AddRefs(mOutgoingChannel), aURI, nsnull, mOutputFormat, -1);
  if (NS_FAILED(rv)) 
    return rv;
  
  // Set system principal for this document, which will be dynamically generated 
  NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
  if (NS_FAILED(rv)) 
    return rv;
  nsCOMPtr<nsIPrincipal> principal;
  if (NS_FAILED(rv = securityManager->GetSystemPrincipal(getter_AddRefs(principal))))
    return rv;
  nsCOMPtr<nsISupports> owner = do_QueryInterface(principal);
  if (NS_FAILED(rv = mOutgoingChannel->SetOwner(owner)))
    return rv;
  
  // We will first find an appropriate emitter in the repository that supports 
  // the requested output format...note, the special exceptions are nsMimeMessageDraftOrTemplate
  // or nsMimeMessageEditorTemplate where we don't need any emitters
  //
  
  if ( (newType != nsMimeOutput::nsMimeMessageDraftOrTemplate) && 
    (newType != nsMimeOutput::nsMimeMessageEditorTemplate) )
  {
    nsCAutoString progID = "component://netscape/messenger/mimeemitter;type=";
    if (mOverrideFormat)
      progID += mOverrideFormat;
    else
      progID += mOutputFormat;
    
    rv = nsComponentManager::CreateInstance(progID, nsnull,
      NS_GET_IID(nsIMimeEmitter),
      (void **) getter_AddRefs(mEmitter));
    if ((NS_FAILED(rv)) || (!mEmitter))
    {
#ifdef DEBUG_rhp
      printf("Unable to create the correct converter!\n");
#endif
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  
  SetStreamURI(aURI);
  // now we want to create a pipe which we'll use for converting the data...
  nsCOMPtr<nsIPipeObserver> pipeObserver = do_QueryInterface(mEmitter);
  rv = NS_NewPipe(getter_AddRefs(mInputStream), getter_AddRefs(mOutputStream),
    pipeObserver,
    NS_STREAM_CONVERTER_SEGMENT_SIZE,
    NS_STREAM_CONVERTER_BUFFER_SIZE);
  
  if (NS_SUCCEEDED(rv))
  {
    mInputStream->SetNonBlocking(PR_TRUE);
    mOutputStream->SetNonBlocking(PR_TRUE);
  }
  
  // initialize our emitter
  if (NS_SUCCEEDED(rv) && mEmitter)
  {
    mEmitter->Initialize(aURI, mOutgoingChannel, newType);
    mEmitter->SetPipe(mInputStream, mOutputStream);
    mEmitter->SetOutputListener(aOutListener);
  }
  
  PRUint32 whattodo = mozITXTToHTMLConv::kURLs;
  PRBool enable_emoticons = PR_TRUE;
  PRBool enable_structs = PR_TRUE;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_SUCCEEDED(rv) && prefs) 
  {
    rv = prefs->GetBoolPref(PREF_MAIL_DISPLAY_GLYPH,&enable_emoticons);
    if (NS_FAILED(rv) || enable_emoticons) 
    {
    	whattodo = whattodo | mozITXTToHTMLConv::kGlyphSubstitution;
    }
    rv = prefs->GetBoolPref(PREF_MAIL_DISPLAY_STRUCT,&enable_structs);
    if (NS_FAILED(rv) || enable_structs) 
    {
    	whattodo = whattodo | mozITXTToHTMLConv::kStructPhrase;
    }
  }

  // initialize our emitter
	if (NS_SUCCEEDED(rv) && mEmitter)
	{
	  mEmitter->Initialize(aURI, mOutgoingChannel, newType);
	  mEmitter->SetPipe(mInputStream, mOutputStream);
	  mEmitter->SetOutputListener(aOutListener);
	}

  if (mOutputType == nsMimeOutput::nsMimeMessageSource)
    return NS_OK;
  else
  {
    mBridgeStream = bridge_create_stream(mEmitter, this, aURI, newType, whattodo, mOutgoingChannel);
    if (!mBridgeStream)
      return NS_ERROR_OUT_OF_MEMORY;
    else
    {
      //Do we need to setup an Mime Stream Converter Listener?
      if (mMimeStreamConverterListener)
        bridge_set_mime_stream_converter_listener((nsMIMESession *)mBridgeStream, mMimeStreamConverterListener);
      return rv;
      
      PRUint32 whattodo = mozITXTToHTMLConv::kURLs;
      PRBool enable_emoticons = PR_TRUE;
      PRBool enable_structs = PR_TRUE;
  
      NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
      if (NS_SUCCEEDED(rv) && prefs) 
      {
        rv = prefs->GetBoolPref(PREF_MAIL_DISPLAY_GLYPH,&enable_emoticons);
        if (NS_FAILED(rv) || enable_emoticons) 
        {
  	      whattodo = whattodo | mozITXTToHTMLConv::kGlyphSubstitution;
        }
        rv = prefs->GetBoolPref(PREF_MAIL_DISPLAY_STRUCT,&enable_structs);
        if (NS_FAILED(rv) || enable_structs) 
        {
        	whattodo = whattodo | mozITXTToHTMLConv::kStructPhrase;
        }
      }      
    }
  }
}

NS_IMETHODIMP nsStreamConverter::GetContentType(char **aOutputContentType)
{
	if (!aOutputContentType)
		return NS_ERROR_NULL_POINTER;

	// since this method passes a string through an IDL file we need to use nsAllocator to allocate it 
	// and not nsCRT::strdup!
	if (nsCRT::strcasecmp(mOutputFormat, "raw") == 0)
		*aOutputContentType = (char *) nsAllocator::Clone(UNKNOWN_CONTENT_TYPE, nsCRT::strlen(UNKNOWN_CONTENT_TYPE) + 1);
	else
		*aOutputContentType = (char *) nsAllocator::Clone(mOutputFormat, nsCRT::strlen(mOutputFormat) + 1);
	return NS_OK;
}

// 
// This is the type of output operation that is being requested by libmime. The types
// of output are specified by nsIMimeOutputType enum
// 
nsresult 
nsStreamConverter::SetMimeOutputType(nsMimeOutputType aType)
{
  mAlreadyKnowOutputType = PR_TRUE;
  mOutputType = aType;
  if (mBridgeStream)
    bridge_set_output_type(mBridgeStream, aType);
  return NS_OK;
}

NS_IMETHODIMP nsStreamConverter::GetMimeOutputType(nsMimeOutputType *aOutFormat)
{
	nsresult rv = NS_OK;
	if (aOutFormat)
		*aOutFormat = mOutputType;
	else
		rv = NS_ERROR_NULL_POINTER;

	return rv;
}

// 
// This is needed by libmime for MHTML link processing...this is the URI associated
// with this input stream
// 
nsresult 
nsStreamConverter::SetStreamURI(nsIURI *aURI)
{
  mURI = aURI;
  if (mBridgeStream)
    return bridge_new_new_uri((nsMIMESession *)mBridgeStream, aURI);
  else
    return NS_OK;
}

nsresult
nsStreamConverter::SetMimeHeadersListener(nsIMimeStreamConverterListener *listener)
{
   mMimeStreamConverterListener = listener;
   bridge_set_mime_stream_converter_listener((nsMIMESession *)mBridgeStream, listener);
   return NS_OK;
}

NS_IMETHODIMP
nsStreamConverter::SetForwardInline(PRBool forwardInline)
{
  mForwardInline = forwardInline;
  return NS_OK;
}

NS_IMETHODIMP
nsStreamConverter::GetForwardInline(PRBool *result)
{
  if (!result) return NS_ERROR_NULL_POINTER;
  *result = mForwardInline;
  return NS_OK;
}

NS_IMETHODIMP
nsStreamConverter::GetIdentity(nsIMsgIdentity * *aIdentity)
{
  if (!aIdentity) return NS_ERROR_NULL_POINTER;
  /*
	We don't have an identity for the local folders account,
    we will return null but it is not an error!
  */
  	*aIdentity = mIdentity;
  	NS_IF_ADDREF(*aIdentity);
  return NS_OK;
}

NS_IMETHODIMP
nsStreamConverter::SetIdentity(nsIMsgIdentity * aIdentity)
{
	mIdentity = aIdentity;
	return NS_OK;
}


/////////////////////////////////////////////////////////////////////////////
// Methods for nsIStreamListener...
/////////////////////////////////////////////////////////////////////////////
//
// Notify the client that data is available in the input stream.  This
// method is called whenver data is written into the input stream by the
// networking library...
//
nsresult 
nsStreamConverter::OnDataAvailable(nsIChannel * /* aChannel */, nsISupports    *ctxt, 
                                   nsIInputStream *aIStream, 
                                   PRUint32       sourceOffset, 
                                   PRUint32       aLength)
{
  nsresult        rc=NS_OK;     // should this be an error instead?
  PRUint32        readLen = aLength;
  PRUint32        written;

  // If this is the first time through and we are supposed to be 
  // outputting the wrapper two pane URL, then do it now.
  if (mWrapperOutput)
  {
    char        outBuf[1024];
char *output = "\
<HTML>\
<FRAMESET ROWS=\"30%%,70%%\">\
<FRAME NAME=messageHeader SRC=\"%s?header=only\">\
<FRAME NAME=messageBody SRC=\"%s?header=none\">\
</FRAMESET>\
</HTML>";

    char *url = nsnull;
    if (NS_FAILED(mURI->GetSpec(&url)))
      return NS_ERROR_FAILURE;
  
    PR_snprintf(outBuf, sizeof(outBuf), output, url, url);
    PR_FREEIF(url);
    if (mEmitter)
      mEmitter->Write(outBuf, nsCRT::strlen(outBuf), &written);
    mTotalRead += written;

    // RICHIE - will this stop the stream???? Not sure.    
    return NS_ERROR_FAILURE;
  }

  char *buf = (char *)PR_Malloc(aLength);
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */

  mTotalRead += aLength;
  readLen = aLength;
  aIStream->Read(buf, aLength, &readLen);

  if (mOutputType == nsMimeOutput::nsMimeMessageSource)
  {
    rc = NS_OK;
    if (mEmitter)
      rc = mEmitter->Write(buf, readLen, &written);
  }
  else if (mBridgeStream)
  {
    nsMIMESession   *tSession = (nsMIMESession *) mBridgeStream;
    rc = tSession->put_block((nsMIMESession *)mBridgeStream, buf, readLen);
  }

  PR_FREEIF(buf);
  if (NS_FAILED(rc))
    mDoneParsing = PR_TRUE;
  return rc;
}

/////////////////////////////////////////////////////////////////////////////
// Methods for nsIStreamObserver 
/////////////////////////////////////////////////////////////////////////////
//
// Notify the observer that the URL has started to load.  This method is
// called only once, at the beginning of a URL load.
//
nsresult 
nsStreamConverter::OnStartRequest(nsIChannel * aChannel, nsISupports *ctxt)
{
#ifdef DEBUG_rhp
    printf("nsStreamConverter::OnStartRequest()\n");
#endif

	// forward the start rquest to any listeners
  if (mOutListener)
  	mOutListener->OnStartRequest(mOutgoingChannel, ctxt);
	return NS_OK;
}

//
// Notify the observer that the URL has finished loading.  This method is 
// called once when the networking library has finished processing the 
//
nsresult 
nsStreamConverter::OnStopRequest(nsIChannel * aChannel, nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg)
{
#ifdef DEBUG_rhp
    printf("nsStreamConverter::OnStopRequest()\n");
#endif

  //
  // Now complete the stream!
  //
  if (mBridgeStream)
  {
    nsMIMESession   *tSession = (nsMIMESession *) mBridgeStream;
    
    if (mMimeStreamConverterListener)
    {
      struct mime_stream_data *msd = (struct mime_stream_data *)tSession->data_object;
      if (msd)
      {
        static NS_DEFINE_CID(kCIMimeHeadersCID, NS_IMIMEHEADERS_CID);
        nsresult rv;
        nsCOMPtr<nsIMimeHeaders> mimeHeaders;
        
        rv = nsComponentManager::CreateInstance(kCIMimeHeadersCID, 
          nsnull, NS_GET_IID(nsIMimeHeaders), 
          (void **) getter_AddRefs(mimeHeaders)); 
        
        if (NS_SUCCEEDED(rv))
        {
          if (msd->headers)
            mimeHeaders->Initialize(msd->headers->all_headers);
          mMimeStreamConverterListener->OnHeadersReady(mimeHeaders);
        }
        else
          mMimeStreamConverterListener->OnHeadersReady(nsnull);
      }
    }
    
    tSession->complete((nsMIMESession *)mBridgeStream);
  }

  // 
  // Now complete the emitter and do necessary cleanup!
  //
  if (mEmitter)    
  {
    mEmitter->Complete();
  }

  // First close the output stream...
  if (mOutputStream)
    mOutputStream->Close();

  // Make sure to do necessary cleanup!
  InternalCleanup();

  // forward on top request to any listeners
  if (mOutListener)
    mOutListener->OnStopRequest(mOutgoingChannel, ctxt, status, errorMsg);
    

  mAlreadyKnowOutputType = PR_FALSE;

  // since we are done converting data, lets close all the objects we own...
  // this helps us fix some circular ref counting problems we are running into...
  Close(); 

  // Time to return...
  return NS_OK;
}

nsresult nsStreamConverter::Close()
{
	mOutgoingChannel = null_nsCOMPtr();
	mEmitter = null_nsCOMPtr();
	mOutListener = null_nsCOMPtr();
	return NS_OK;
}

// nsIStreamConverter implementation

// No syncronous conversion at this time.
NS_IMETHODIMP nsStreamConverter::Convert(nsIInputStream *aFromStream,
                          const PRUnichar *aFromType,
                          const PRUnichar *aToType,
                          nsISupports *aCtxt, nsIInputStream **_retval) 
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// Stream converter service calls this to initialize the actual stream converter (us).
NS_IMETHODIMP nsStreamConverter::AsyncConvertData(const PRUnichar *aFromType, const PRUnichar *aToType,
                                   nsIStreamListener *aListener, nsISupports *aCtxt) 
{
	nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgQuote> aMsgQuote = do_QueryInterface(aCtxt, &rv);
	nsCOMPtr<nsIChannel> aChannel;
  
  if (aMsgQuote)
  {
    nsCOMPtr<nsIMimeStreamConverterListener> quoteListener;
    rv = aMsgQuote->GetQuoteListener(getter_AddRefs(quoteListener));
    if (quoteListener)
      SetMimeHeadersListener(quoteListener);
    rv = aMsgQuote->GetQuoteChannel(getter_AddRefs(aChannel));
  }
  else
  {
    aChannel = do_QueryInterface(aCtxt, &rv);
  }

  if (aToType)
      mDesiredOutputType = nsCRT::strdup(aToType);

	NS_ASSERTION(aChannel && NS_SUCCEEDED(rv), "mailnews mime converter has to have the channel passed in...");
	if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIURI> aUri;
	aChannel->GetURI(getter_AddRefs(aUri));

	return Init(aUri, aListener, aChannel);
}

