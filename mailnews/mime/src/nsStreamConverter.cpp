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
#include "nsCOMPtr.h"
#include "stdio.h"
#include "mimecom.h"
#include "modmimee.h"
#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsStreamConverter.h"
#include "comi18n.h"
#include "prmem.h"
#include "prprf.h"
#include "plstr.h"
#include "mimemoz2.h"
#include "nsMimeTypes.h"
#include "nsRepository.h"
#include "nsIURL.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"
#include "nsIAllocator.h"
#include "nsIBuffer.h"
#include "nsMimeStringResources.h"
#include "nsIPref.h"

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
                     nsMimeOutputType    format_out)
{
  if  ( (format_out == nsMimeOutput::nsMimeMessageDraftOrTemplate) ||
        (format_out == nsMimeOutput::nsMimeMessageEditorTemplate) )
    return mime_bridge_create_draft_stream(newEmitter, newPluginObj2, uri, format_out);
  else
    return mime_bridge_create_display_stream(newEmitter, newPluginObj2, uri, format_out);
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
          msd->url_name = PL_strdup(urlString);
          if (!(msd->url_name))
            return NS_ERROR_OUT_OF_MEMORY;

          PR_FREEIF(urlString);
        }
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
    mOutputFormat = PL_strdup("text/html");
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
    format += PL_strlen("?outformat=");
    while (*format == ' ')
      ++format;

    if ((format) && (*format))
    {
      char *ptr;
      PR_FREEIF(mOutputFormat);
      mOutputFormat = PL_strdup(format);
      mOverrideFormat = PL_strdup("raw");
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
            memmove(ptr+1, ptr+3, PL_strlen(ptr+3));
            *(ptr + PL_strlen(ptr+3) + 1) = '\0';
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
      PRInt32 lenOfHeader = PL_strlen("?header=");

      char *ptr2 = PL_strcasestr ("only", (header+lenOfHeader));
      char *ptr3 = PL_strcasestr ("quote", (header+lenOfHeader));
      char *ptr4 = PL_strcasestr ("none", (header+lenOfHeader));
      if (ptr4)
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = PL_strdup("text/html");
        *aNewType = nsMimeOutput::nsMimeMessageBodyDisplay;
      }
      else if (ptr2)
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = PL_strdup("text/xml");
        *aNewType = nsMimeOutput::nsMimeMessageHeaderDisplay;
      }
      else if (ptr3)
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = PL_strdup("text/html");
        *aNewType = nsMimeOutput::nsMimeMessageQuoting;
      }
    }
    else
    {
      // Ok, we are adding a pref to turn on the new XUL header output. If this 
      // pref is true, then we will use the new quoting, otherwise, we default to
      // the old split display quoting.
      //
      nsresult    rv;
      PRBool      mimeXULOutput = PR_TRUE;

      NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
      if (NS_SUCCEEDED(rv) && prefs) 
        rv = prefs->GetBoolPref("mail.mime_xul_output", &mimeXULOutput);

      if (mimeXULOutput)
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = PL_strdup("text/xul");
        *aNewType = nsMimeOutput::nsMimeMessageXULDisplay;
      }
      else
      {
        mWrapperOutput = PR_TRUE;
        PR_FREEIF(mOutputFormat);
        mOutputFormat = PL_strdup("text/html");
      }
    }
  }
  else // this is a part that should just come out raw!
  {
    PR_FREEIF(mOutputFormat);
    mOutputFormat = PL_strdup("raw");
    *aNewType = nsMimeOutput::nsMimeMessageRaw;
  }

  return NS_OK;
}

nsresult 
nsStreamConverter::InternalCleanup(void)
{
  PR_FREEIF(mOutputFormat);
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
  mOutputFormat = PL_strdup("text/html");
  mDoneParsing = PR_FALSE;
  mAlreadyKnowOutputType = PR_FALSE;
}

nsStreamConverter::~nsStreamConverter()
{
  InternalCleanup();
}

/* 
 * This function will be used by the factory to generate an 
 * mime object class object....
 */
nsresult 
NS_NewStreamConverter(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take 
     a string describing the assertion */
	//nsresult result = NS_OK;
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsStreamConverter *obj = new nsStreamConverter();
		if (obj)
			return obj->QueryInterface(aIID, (void**) aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

NS_IMPL_ADDREF(nsStreamConverter)
NS_IMPL_RELEASE(nsStreamConverter)

NS_IMETHODIMP nsStreamConverter::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
	*aInstancePtr = nsnull;

	if (aIID.Equals(nsCOMTypeInfo<nsIStreamConverter2>::GetIID()) || 
		aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIStreamConverter2*, this);
	}              
	else if(aIID.Equals(nsCOMTypeInfo<nsIMimeStreamConverter>::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIMimeStreamConverter*, this);
	}
	else if (aIID.Equals(nsCOMTypeInfo<nsIBufferObserver>::GetIID()))
	{
		*aInstancePtr = NS_STATIC_CAST(nsIBufferObserver*, this);
	}

	if(*aInstancePtr)
	{
		NS_ADDREF_THIS();
		return NS_OK;
	}
	else
		return NS_ERROR_NO_INTERFACE;
}

///////////////////////////////////////////////////////////////
// nsStreamConverter definitions....
///////////////////////////////////////////////////////////////

NS_IMETHODIMP nsStreamConverter::Init(nsIURI *aURI, nsIStreamListener * aOutListener, nsIChannel *aChannel)
{
	nsresult rv = NS_OK;

	mOutListener = aOutListener;

	// mscott --> we need to look at the url and figure out what the correct output type is...
	nsMimeOutputType newType;

	if (NS_FAILED(rv)) return rv;
	
	if (!mAlreadyKnowOutputType)
	{
		nsXPIDLCString urlSpec;
		rv = aURI->GetSpec(getter_Copies(urlSpec));
		DetermineOutputFormat(urlSpec, &newType);
		mAlreadyKnowOutputType = PR_TRUE;
	}
	else
		newType = mOutputType;

	  switch (newType)
	  {
    case nsMimeOutput::nsMimeMessageXULDisplay:
			PR_FREEIF(mOutputFormat);
			mOutputFormat = PL_strdup("text/xul");
      break;

		case nsMimeOutput::nsMimeMessageSplitDisplay:    // the wrapper HTML output to produce the split header/body display
			mWrapperOutput = PR_TRUE;
			PR_FREEIF(mOutputFormat);
			mOutputFormat = PL_strdup("text/html");
			break;
		case nsMimeOutput::nsMimeMessageHeaderDisplay:   // the split header/body display
			PR_FREEIF(mOutputFormat);
			mOutputFormat = PL_strdup("text/xml");
			break;

		case nsMimeOutput::nsMimeMessageBodyDisplay:   // the split header/body display
			PR_FREEIF(mOutputFormat);
			mOutputFormat = PL_strdup("text/html");
			break;

		case nsMimeOutput::nsMimeMessageQuoting:   // all HTML quoted output
			PR_FREEIF(mOutputFormat);
			mOutputFormat = PL_strdup("text/html");
			break;

		case nsMimeOutput::nsMimeMessageRaw:       // the raw RFC822 data (view source) and attachments
			PR_FREEIF(mOutputFormat);
			mOutputFormat = PL_strdup("raw");
			break;

		case nsMimeOutput::nsMimeMessageDraftOrTemplate:       // Loading drafts & templates
			PR_FREEIF(mOutputFormat);
			mOutputFormat = PL_strdup("message/draft");
			break;

		case nsMimeOutput::nsMimeMessageEditorTemplate:       // Loading templates into editor
			PR_FREEIF(mOutputFormat);
			mOutputFormat = PL_strdup("text/html");
			break;
		default:
			NS_ASSERTION(0, "this means I made a mistake in my assumptions");
	  }
	
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
										                         nsIMimeEmitter::GetIID(),
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
	rv = NS_NewPipe(getter_AddRefs(mInputStream), getter_AddRefs(mOutputStream),
                    NS_STREAM_CONVERTER_SEGMENT_SIZE,
                    NS_STREAM_CONVERTER_BUFFER_SIZE, PR_TRUE, this);

	// initialize our emitter
	if (NS_SUCCEEDED(rv) && mEmitter)
	{
	  mEmitter->Initialize(aURI, aChannel);
	  mEmitter->SetPipe(mInputStream, mOutputStream);
	  mEmitter->SetOutputListener(aOutListener);
	}
  
	mBridgeStream = bridge_create_stream(mEmitter, this, aURI, newType);

	if (!mBridgeStream)
		return NS_ERROR_OUT_OF_MEMORY;
	else
		return rv;
}

NS_IMETHODIMP nsStreamConverter::GetContentType(char **aOutputContentType)
{
	if (!aOutputContentType)
		return NS_ERROR_NULL_POINTER;

	// since this method passes a string through an IDL file we need to use nsAllocator to allocate it 
	// and not PL_strdup!
	if (PL_strcasecmp(mOutputFormat, "raw") == 0)
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

  // If this is the first time through and we are supposed to be 
  // outputting the wrapper two pane URL, then do it now.
  if (mWrapperOutput)
  {
    PRUint32    written;
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
      mEmitter->Write(outBuf, PL_strlen(outBuf), &written);
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

  if (mBridgeStream)
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
  	mOutListener->OnStartRequest(aChannel, ctxt);
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
    mOutListener->OnStopRequest(aChannel, ctxt, status, errorMsg);

  // Time to return...
  return NS_OK;
}

NS_IMETHODIMP
nsStreamConverter::OnFull(nsIBuffer* buffer)
{
    return NS_OK;;
}

NS_IMETHODIMP
nsStreamConverter::OnWrite(nsIBuffer* aBuffer, PRUint32 aCount)
{
    return NS_OK;
}

NS_IMETHODIMP
nsStreamConverter::OnEmpty(nsIBuffer* buffer)
{
    return NS_OK;
}
