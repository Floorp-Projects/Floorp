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
#include "nsCOMPtr.h"
#include "stdio.h"
#include "nsMimeRebuffer.h"
#include "nsMimeXmlEmitter.h"
#include "plstr.h"
#include "nsIMimeEmitter.h"
#include "nsMailHeaders.h"
#include "nscore.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsEscape.h"
#include "prmem.h"
#include "nsEmitterUtils.h"

// For the new pref API's
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsresult NS_NewMimeXmlEmitter(const nsIID& iid, void **result)
{
	nsMimeXmlEmitter *obj = new nsMimeXmlEmitter();
	if (obj)
		return obj->QueryInterface(iid, result);
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

/* 
 * The following macros actually implement addref, release and 
 * query interface for our component. 
 */
NS_IMPL_ADDREF(nsMimeXmlEmitter)
NS_IMPL_RELEASE(nsMimeXmlEmitter)
NS_IMPL_QUERY_INTERFACE(nsMimeXmlEmitter, nsIMimeEmitter::GetIID()); /* we need to pass in the interface ID of this interface */

/*
 * nsMimeXmlEmitter definitions....
 */
nsMimeXmlEmitter::nsMimeXmlEmitter()
{
  NS_INIT_REFCNT(); 

  mBufferMgr = NULL;
  mTotalWritten = 0;
  mTotalRead = 0;
  mDocHeader = PR_FALSE;
  mAttachContentType = NULL;
  mAttachCount = 0;

  mInputStream = nsnull;
  mOutStream = nsnull;
  mOutListener = nsnull;
  mURL = nsnull;
  mHeaderDisplayType = nsMimeHeaderDisplayTypes::NormalHeaders;

  nsresult rv = nsServiceManager::GetService(kPrefCID, nsIPref::GetIID(), (nsISupports**)&(mPrefs));
  if (! (mPrefs && NS_SUCCEEDED(rv)))
    return;

  if ((mPrefs && NS_SUCCEEDED(rv)))
    mPrefs->GetIntPref("mail.show_headers", &mHeaderDisplayType);

#ifdef DEBUG_rhp
  mLogFile = NULL;    /* Temp file to put generated HTML into. */
  mReallyOutput = PR_FALSE;
#endif
}


nsMimeXmlEmitter::~nsMimeXmlEmitter(void)
{
  if (mBufferMgr)
    delete mBufferMgr;

  // Release the prefs service
  if (mPrefs)
    nsServiceManager::ReleaseService(kPrefCID, mPrefs);
}

// Set the output stream for processed data.
NS_IMETHODIMP
nsMimeXmlEmitter::SetPipe(nsIInputStream * aInputStream, nsIOutputStream *outStream)
{
  mInputStream = aInputStream;
  mOutStream = outStream;
  return NS_OK;
}

// Note - these is setup only...you should not write
// anything to the stream since these may be image data
// output streams, etc...
nsresult       
nsMimeXmlEmitter::Initialize(nsIURI *url, nsIChannel * aChannel)
{
  // set the url
  mURL = url;
  mChannel = aChannel;

  // Create rebuffering object
  mBufferMgr = new MimeRebuffer();

  // Counters for output stream
  mTotalWritten = 0;
  mTotalRead = 0;

#ifdef DEBUG_rhp
  PR_Delete("C:\\email.html");
  // mLogFile = PR_Open("C:\\email.html", PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE, 493);
#endif /* DEBUG */

  return NS_OK;
}

nsresult
nsMimeXmlEmitter::SetOutputListener(nsIStreamListener *listener)
{
  mOutListener = listener;

  return NS_OK;
}

// Note - this is teardown only...you should not write
// anything to the stream since these may be image data
// output streams, etc...
nsresult
nsMimeXmlEmitter::Complete()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  char  buf[16];

  // Now write out the total count of attachments for this message
  UtilityWrite("<mailattachcount>");
  sprintf(buf, "%d", mAttachCount);
  UtilityWrite(buf);
  UtilityWrite("</mailattachcount>");

  UtilityWrite("</message>");

  // If we are here and still have data to write, we should try
  // to flush it...if we try and fail, we should probably return
  // an error!
  PRUint32      written; 
  if ( (mBufferMgr) && (mBufferMgr->GetSize() > 0))
    Write("", 0, &written);

#ifdef DEBUG_rhp
  printf("TOTAL WRITTEN = %d\n", mTotalWritten);
  printf("LEFTOVERS     = %d\n", mBufferMgr->GetSize());
#endif

#ifdef DEBUG_rhp
  if (mLogFile) 
    PR_Close(mLogFile);
#endif

  return NS_OK;
}

nsresult
nsMimeXmlEmitter::WriteXMLHeader(const char *msgID)
{
  if ( (!msgID) || (!*msgID) )
    msgID = "none";
    
  char  *newValue = nsEscapeHTML(msgID);
  if (!newValue)
    return NS_ERROR_OUT_OF_MEMORY;
    
  UtilityWrite("<?xml version=\"1.0\"?>");

  if (mHeaderDisplayType == nsMimeHeaderDisplayTypes::MicroHeaders)
    UtilityWrite("<?xml-stylesheet href=\"chrome://messenger/skin/mailheader-micro.css\" type=\"text/css\"?>");
  else if (mHeaderDisplayType == nsMimeHeaderDisplayTypes::NormalHeaders)
    UtilityWrite("<?xml-stylesheet href=\"chrome://messenger/skin/mailheader-normal.css\" type=\"text/css\"?>");
  else /* AllHeaders */
    UtilityWrite("<?xml-stylesheet href=\"chrome://messenger/skin/mailheader-all.css\" type=\"text/css\"?>");

  UtilityWrite("<message id=\"");
  UtilityWrite(newValue);
  UtilityWrite("\">");

  mXMLHeaderStarted = PR_TRUE;
  nsCRT::free(newValue);
  return NS_OK;
}

nsresult
nsMimeXmlEmitter::WriteXMLTag(const char *tagName, const char *value)
{
  if ( (!value) || (!*value) )
    return NS_OK;

  char  *upCaseTag = NULL;
  char  *newValue = nsEscapeHTML(value);
  if (!newValue) 
    return NS_OK;

  nsString  newTagName(tagName);
  newTagName.CompressWhitespace(PR_TRUE, PR_TRUE);

  newTagName.ToUpperCase();
  upCaseTag = newTagName.ToNewCString();

  UtilityWrite("<header field=\"");
  UtilityWrite(upCaseTag);
  UtilityWrite("\">");

  // Here is where we are going to try to L10N the tagName so we will always
  // get a field name next to an emitted header value. Note: Default will always
  // be the name of the header itself.
  //
  UtilityWrite("<headerdisplayname>");
  char *l10nTagName = LocalizeHeaderName(upCaseTag, tagName);
  if ( (!l10nTagName) || (!*l10nTagName) )
    UtilityWrite(tagName);
  else
  {
    UtilityWrite(l10nTagName);
    PR_FREEIF(l10nTagName);
  }

  UtilityWrite(": ");
  UtilityWrite("</headerdisplayname>");

  // Now write out the actual value itself and move on!
  //
  UtilityWrite(newValue);
  UtilityWrite("</header>");

  nsCRT::free(upCaseTag);
  nsCRT::free(newValue);

  return NS_OK;
}

// Header handling routines.
nsresult
nsMimeXmlEmitter::StartHeader(PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                           const char *outCharset)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  mDocHeader = rootMailHeader;
  WriteXMLHeader(msgID);
  UtilityWrite("<mailheader>");

  return NS_OK; 
}

nsresult
nsMimeXmlEmitter::AddHeaderField(const char *field, const char *value)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  if ( (!field) || (!value) )
    return NS_OK;

  WriteXMLTag(field, value);
  return NS_OK;
}

nsresult
nsMimeXmlEmitter::EndHeader()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  UtilityWrite("</mailheader>");
  return NS_OK; 
}


// Attachment handling routines
nsresult
nsMimeXmlEmitter::StartAttachment(const char *name, const char *contentType, const char *url)
{
  char    buf[128];
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  ++mAttachCount;

  sprintf(buf, "<mailattachment id=\"%d\">", mAttachCount);
  UtilityWrite(buf);

  AddAttachmentField(HEADER_PARM_FILENAME, name);
  return NS_OK;
}

nsresult
nsMimeXmlEmitter::AddAttachmentField(const char *field, const char *value)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  WriteXMLTag(field, value);
  return NS_OK;
}

nsresult
nsMimeXmlEmitter::EndAttachment()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  UtilityWrite("</mailattachment>");
  return NS_OK;
}

// Attachment handling routines
nsresult
nsMimeXmlEmitter::StartBody(PRBool bodyOnly, const char *msgID, const char *outCharset)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  // For now, we are going to just eat the message body
  /**
  if (bodyOnly && !mXMLHeaderStarted)
    WriteXMLHeader(msgID);

  UtilityWrite("<mailbody>");
  ***/
  return NS_OK;
}

nsresult
nsMimeXmlEmitter::WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  // For now, we are going to just eat the message body
  //Write(buf, size, amountWritten);
  return NS_OK;
}

nsresult
nsMimeXmlEmitter::EndBody()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  // For now, we are going to just eat the message body
  // UtilityWrite("</mailbody>");
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// These are routines necessary for the C based routines in libmime
// to access the new world streams.
////////////////////////////////////////////////////////////////////////////////
nsresult
nsMimeXmlEmitter::Write(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
  unsigned int        written = 0;
  PRUint32            rc = 0;
  PRUint32            needToWrite;

#ifdef DEBUG_rhp
  if ((mLogFile) && (mReallyOutput))
    PR_Write(mLogFile, buf, size);
#endif

  //
  // Make sure that the buffer we are "pushing" into has enough room
  // for the write operation. If not, we have to buffer, return, and get
  // it on the next time through
  //
  *amountWritten = 0;

  needToWrite = mBufferMgr->GetSize();
  // First, handle any old buffer data...
  if (needToWrite > 0)
  {
    rc += mOutStream->Write(mBufferMgr->GetBuffer(), 
                            mBufferMgr->GetSize(), &written);
    mTotalWritten += written;
    mBufferMgr->ReduceBuffer(written);
    mOutListener->OnDataAvailable(mChannel, mURL, mInputStream, 0, written);

    *amountWritten = written;

    // if we couldn't write all the old data, buffer the new data
    // and return
    if (mBufferMgr->GetSize() > 0)
    {
      mBufferMgr->IncreaseBuffer(buf, size);
      return NS_OK;
    }
  }


  // if we get here, we are dealing with new data...try to write
  // and then do the right thing...
  rc = mOutStream->Write(buf, size, &written);
  *amountWritten = written;
  mTotalWritten += written;

  if (written < size)
    mBufferMgr->IncreaseBuffer(buf+written, (size-written));

  if (mOutListener)
    mOutListener->OnDataAvailable(mChannel, mURL, mInputStream, 0, written);

  return rc;
}

nsresult
nsMimeXmlEmitter::UtilityWrite(const char *buf)
{
  PRInt32     tmpLen = PL_strlen(buf);
  PRUint32    written;

  Write(buf, tmpLen, &written);
#ifdef DEBUG
//  Write("\r\n", 2, &written);
#endif

  return NS_OK;
}
