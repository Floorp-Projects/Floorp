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
#include "plstr.h"
#include "mimemoz2.h"
#include "nsMimeTypes.h"
#include "nsRepository.h"
#include "nsIURL.h"
#include "nsString.h"
#include "nsINetService.h"
#include "nsIServiceManager.h"

/* 
 * This function will be used by the factory to generate an 
 * mime object class object....
 */
nsresult NS_NewStreamConverter(nsIStreamConverter ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take 
     a string describing the assertion */
	//nsresult result = NS_OK;
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsStreamConverter *obj = new nsStreamConverter();
		if (obj)
			return obj->QueryInterface(nsIStreamConverter::GetIID(), (void**) aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

/* 
 * The following macros actually implement addref, release and 
 * query interface for our component. 
 */
/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsStreamConverter, nsIStreamConverter::GetIID());

/*
 * nsStreamConverter definitions....
 */
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);

nsresult 
NewURL(nsIURI** aInstancePtrResult, const nsString& aSpec)
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

/* 
 * Inherited methods for nsMimeConverter
 */
nsStreamConverter::nsStreamConverter()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();

  // Init member variables...
  mOutStream = nsnull;
  mOutListener = nsnull;

  mBridgeStream = NULL;
  mTotalRead = 0;
  mOutputFormat = PL_strdup("text/html");
  mEmitter = NULL;
  mWrapperOutput = PR_FALSE;
  mURLString = nsnull;
  mURL = nsnull;
}

nsStreamConverter::~nsStreamConverter()
{
  InternalCleanup();
}

NS_METHOD
nsStreamConverter::DetermineOutputFormat(const char *url)
{
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
  
      return NS_OK;
    }
  }

  if (!part)
  {
    if (header)
    {
      char *ptr2 = PL_strcasestr ("only", (header+PL_strlen("?header=")));
      if (ptr2)
      {
        PR_FREEIF(mOutputFormat);
        mOutputFormat = PL_strdup("text/xml");
      }
    }
    else
    {
      mWrapperOutput = PR_TRUE;
      PR_FREEIF(mOutputFormat);
      mOutputFormat = PL_strdup("text/html");
    }
  }
  else
  {
    PR_FREEIF(mOutputFormat);
    mOutputFormat = PL_strdup("text/html");
  }

  return NS_OK;
}

nsresult 
nsStreamConverter::InternalCleanup(void)
{
  // 
  // Now complete the emitter and do necessary cleanup!
  //
  if (mEmitter)
  {
    mEmitter->Complete();
  }

  PR_FREEIF(mOutputFormat);
  if (mBridgeStream)
  {
    mime_bridge_destroy_stream(mBridgeStream);
    mBridgeStream = nsnull;
  }

  return NS_OK;
}

nsresult
nsStreamConverter::SetStreamURL(char *url)
{
  PR_FREEIF(mURLString);
  mURLString = PL_strdup(url);
  if (mBridgeStream)
    return mimeSetNewURL((nsMIMESession *)mBridgeStream, url);
  else
    return NS_OK;
}

//
// Methods for nsIStreamConverter
// 
// This is the output stream where the stream converter will write processed data after 
// conversion. 
// 

// RICHIE - Need this for FO_NGLAYOUT
#include "net.h"

nsresult
nsStreamConverter::SetOutputStream(nsIOutputStream *outStream, char *url)
{
  mEmitter = new nsMimeEmitter2();
  if (!mEmitter)
    return NS_ERROR_OUT_OF_MEMORY;

  // make sure to set these!
  mOutStream = outStream;
  SetStreamURL(url);

  nsString urlSpec(mURLString);
  nsresult rv = NewURL(&mURL, urlSpec);
  if (NS_SUCCEEDED(rv) && mURL)
  {
    mURL->SetSpec(mURLString);
  }

  mEmitter->Initialize(mURL);
  mEmitter->SetOutputStream(outStream);

  mBridgeStream = mime_bridge_create_stream(nsnull, nsnull, this, mEmitter, url, FO_NGLAYOUT);
  if (!mBridgeStream)
  {  
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

// 
// The output listener can be set to allow for the flexibility of having the stream converter 
// directly notify the listener of the output stream for any processed/converter data. If 
// this output listener is not set, the data will be written into the output stream but it is 
// the responsibility of the client of the stream converter to handle the resulting data. 
// 
nsresult
nsStreamConverter::SetOutputListener(nsIStreamListener *outListner)
{
  return mEmitter->SetOutputListener(outListner);
}

// Methods for nsIStreamListener...
/**
* Return information regarding the current URL load.<BR>
* The info structure that is passed in is filled out and returned
* to the caller. 
* 
* This method is currently not called.  
*/
nsresult
nsStreamConverter::GetBindInfo(nsIURI* aURL, nsStreamBindingInfo* aInfo)
{
#ifdef NS_DEBUG
  // printf("nsStreamConverter::GetBindInfo()\n");
#endif

  return NS_OK;
}

/**
* Notify the client that data is available in the input stream.  This
* method is called whenver data is written into the input stream by the
* networking library...<BR><BR>
* 
* @param pIStream  The input stream containing the data.  This stream can
* be either a blocking or non-blocking stream.
* @param length    The amount of data that was just pushed into the stream.
* @return The return value is currently ignored.
*/
nsresult
nsStreamConverter::OnDataAvailable(nsIURI* aURL, nsIInputStream *aIStream, 
                                   PRUint32 aLength)
{
  nsresult        rc;
  PRUint32        readLen = aLength;

  if (!mOutStream)
    return NS_ERROR_FAILURE;

  char *buf = (char *)PR_Malloc(aLength);
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */

  mTotalRead += aLength;
  aIStream->Read(buf, aLength, &readLen);
  rc = mime_display_stream_write((nsMIMESession *) mBridgeStream, buf, readLen);
  PR_FREEIF(buf);
  return rc;
}


// Methods for nsIStreamObserver 
/**
* Notify the observer that the URL has started to load.  This method is
* called only once, at the beginning of a URL load.<BR><BR>
*
* @return The return value is currently ignored.  In the future it may be
* used to cancel the URL load..
*/
nsresult
nsStreamConverter::OnStartBinding(nsIURI* aURL, const char *aContentType)
{
#ifdef NS_DEBUG
  // printf("nsStreamConverter::OnStartBinding() for Content-Type: %s\n", aContentType);
#endif

  return NS_OK;
}

/**
* Notify the observer that progress as occurred for the URL load.<BR>
*/
nsresult
nsStreamConverter::OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
#ifdef NS_DEBUG
  // printf("nsStreamConverter::OnProgress()\n");
#endif

  return NS_OK;
}

/**
* Notify the observer with a status message for the URL load.<BR>
*/
nsresult
nsStreamConverter::OnStatus(nsIURI* aURL, const PRUnichar* aMsg)
{
#ifdef NS_DEBUG
  // printf("nsStreamConverter::OnStatus()\n");
#endif

  return NS_OK;
}

/**
* Notify the observer that the URL has finished loading.  This method is 
* called once when the networking library has finished processing the 
* URL transaction initiatied via the nsINetService::Open(...) call.<BR><BR>
* 
* This method is called regardless of whether the URL loaded successfully.<BR><BR>
* 
* @param status    Status code for the URL load.
* @param msg   A text string describing the error.
* @return The return value is currently ignored.
*/
nsresult
nsStreamConverter::OnStopBinding(nsIURI* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
#ifdef NS_DEBUG
  // printf("nsStreamConverter::OnStopBinding()\n");
#endif

  //
  // Now complete the stream!
  //
  if (mBridgeStream)
    mime_display_stream_complete((nsMIMESession *)mBridgeStream);

  // First close the output stream...
  mOutStream->Close();

  // Make sure to do necessary cleanup!
  InternalCleanup();

  // Time to return...
  return NS_OK;
}
