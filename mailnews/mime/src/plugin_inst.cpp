/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include <stdio.h>
#include <string.h>
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"
#include "mimemoz2.h"
#include "nsIMimeEmitter.h"
#include "nsRepository.h"
#include "nscore.h"
#include "nsString.h"

// RICHIE - Need this for FO_NGLAYOUT
#include "net.h"

//
// This should be passed in somehow, but for now, lets just 
// hardcode this stuff.
//
static NS_DEFINE_IID(kINetPluginInstanceIID,  NS_INETPLUGININSTANCE_IID);
static NS_DEFINE_IID(kINetOStreamIID,         NS_INETOSTREAM_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

extern "C" nsresult 
NS_NewMimePluginInstance(MimePluginInstance **aInstancePtrResult)
{
	/* Note this new macro for assertions...they can take 
   * a string describing the assertion */
  nsresult res = NS_OK;
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
    MimePluginInstance* inst = new MimePluginInstance();
    if (inst == NULL)
      return NS_ERROR_OUT_OF_MEMORY;

    res = inst->QueryInterface(nsINetPluginInstance::GetIID(), (void **)aInstancePtrResult);
    if (res != NS_OK)
    {
      *aInstancePtrResult = NULL;
      delete inst;
    }

    return res;
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

NS_METHOD
MimePluginInstance::QueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
  if (NULL == aInstancePtr)
    return NS_ERROR_NULL_POINTER; 
  
  // Always NULL result, in case of failure   
  *aInstancePtr = NULL;   

  if (aIID.Equals(kINetPluginInstanceIID))
    *aInstancePtr = (void*) this; 
  else if (aIID.Equals(kISupportsIID))
    *aInstancePtr = (void*) ((nsISupports*)(nsINetPluginInstance *)this); 
  else if (aIID.Equals(kINetOStreamIID))
    *aInstancePtr = (void *)((nsINetOStream *)this);
  
  if (*aInstancePtr == NULL)
    return NS_NOINTERFACE;
  
  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}

NS_IMPL_ADDREF(MimePluginInstance);
NS_IMPL_RELEASE(MimePluginInstance);

////////////////////////////////////////////////////////////////////////////////
// MimePluginInstance Methods
////////////////////////////////////////////////////////////////////////////////

MimePluginInstance::MimePluginInstance(void)
{
  NS_INIT_REFCNT();  
  mOutStream = NULL;
  mBridgeStream = NULL;
  mTotalRead = 0;
  mOutputFormat = PL_strdup("text/html");
  mEmitter = NULL;
  mWrapperOutput = PR_FALSE;
}

MimePluginInstance::~MimePluginInstance(void)
{
}

NS_METHOD
MimePluginInstance::DetermineOutputFormat(const char *url)
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

NS_METHOD
MimePluginInstance::Initialize(nsINetOStream* stream, const char *stream_name)
{
  int         format_out = FO_NGLAYOUT;
  nsresult    res;

  // I really think there needs to be a way to handle multiple output formats
  // We will need to find an emitter in the repository that supports the appropriate
  // output format.
  //
  mOutStream = stream;
  nsAutoString progID (eOneByte);
  progID = "component://netscape/messenger/mimeemitter;type=";
  progID += mOutputFormat;

  res = nsComponentManager::CreateInstance(progID.GetBuffer(), nsnull,
										   nsIMimeEmitter::GetIID(),
										   (void **) getter_AddRefs(mEmitter));

  if ((NS_FAILED(res)) || (!mEmitter))
  {
	NS_ASSERTION(PR_FALSE, "unable to create the correct converter");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mEmitter->Initialize(stream);
  mBridgeStream = mime_bridge_create_stream(this, mEmitter, nsnull, nsnull, stream_name, format_out);
  if (!mBridgeStream)
  {  
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_METHOD
MimePluginInstance::Destroy(void)
{
  return NS_OK;
}

NS_METHOD
MimePluginInstance::GetMIMEOutput(const char* *result, const char *url)
{
  mURL = PL_strdup(url);
  DetermineOutputFormat(url);
  *result = mOutputFormat;
  return NS_OK;
}

NS_METHOD
MimePluginInstance::Start(void)
{
    return NS_OK;
}

NS_METHOD
MimePluginInstance::Stop(void)
{
    return NS_OK;
}

nsresult MimePluginInstance::Close(void)
{
	nsINetOStream *stream = mOutStream;
  nsresult      rc;

  if (!stream)
    return NS_ERROR_FAILURE;

  rc = stream->Close();
  return rc;
}

nsresult MimePluginInstance::Complete(void)
{

  nsINetOStream *stream = mOutStream;

  if (!stream)
    return NS_ERROR_FAILURE;

  //
  // Now complete the stream!
  //
  mime_display_stream_complete((nsMIMESession *)mBridgeStream);

  // Make sure to do necessary cleanup!
  InternalCleanup();

  return stream->Complete();
}

nsresult MimePluginInstance::InternalCleanup(void)
{
  // 
  // Now complete the emitter and do necessary cleanup!
  //
#ifdef DEBUG_rhp
  printf("TOTAL READ    = %d\n", mTotalRead);
#endif
  if (mEmitter)
  {
    mEmitter->Complete();
  }

  PR_FREEIF(mOutputFormat);
  PR_FREEIF(mURL);
  mime_bridge_destroy_stream(mBridgeStream);

  Close();
  return NS_OK;
}

nsresult MimePluginInstance::Abort(PRInt32 status)
{
	nsINetOStream *stream = mOutStream;

  if (!stream)
    return NS_ERROR_FAILURE;

  mime_display_stream_abort ((nsMIMESession *)mBridgeStream, status);

  // Make sure to do necessary cleanup!
  InternalCleanup();

  return stream->Abort(status);
}

nsresult MimePluginInstance::WriteReady(PRUint32 *aReadyCount)
{
	nsINetOStream *stream = mOutStream;

  if (!stream)
    return NS_ERROR_FAILURE;

  return stream->WriteReady(aReadyCount);
}

nsresult MimePluginInstance::Write(const char* aBuf, PRUint32 aCount, 
                                   PRUint32 *aWriteCount)
{
  nsresult        rc;
	nsINetOStream   *stream = mOutStream;

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

    PR_snprintf(outBuf, sizeof(outBuf), output, mURL, mURL);
    if (mEmitter)
      mEmitter->Write(outBuf, PL_strlen(outBuf), &written);
    mTotalRead += written;
    return -1;
  }

  if (!stream)
    return NS_ERROR_FAILURE;

  mTotalRead += aCount;
  rc = mime_display_stream_write((nsMIMESession *) mBridgeStream, aBuf, aCount);
  // SHERRY - returning the amount read here?
  *aWriteCount = aCount;
  return rc;
}
