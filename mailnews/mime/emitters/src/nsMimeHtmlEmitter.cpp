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
#include "nsMimeHtmlEmitter.h"
#include "plstr.h"
#include "nsMailHeaders.h"
#include "nscore.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsEscape.h"
#include "prmem.h"
#include "nsEmitterUtils.h"

// For the new pref API's
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsresult NS_NewMimeHtmlEmitter(const nsIID& iid, void **result)
{
	nsMimeHtmlEmitter *obj = new nsMimeHtmlEmitter();
	if (obj)
		return obj->QueryInterface(iid, result);
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

/* 
 * The following macros actually implement addref, release and 
 * query interface for our component. 
 */
NS_IMPL_ADDREF(nsMimeHtmlEmitter)
NS_IMPL_RELEASE(nsMimeHtmlEmitter)
NS_IMPL_QUERY_INTERFACE(nsMimeHtmlEmitter, nsIMimeEmitter::GetIID()); /* we need to pass in the interface ID of this interface */

/*
 * nsMimeHtmlEmitter definitions....
 */
nsMimeHtmlEmitter::nsMimeHtmlEmitter()
{
  NS_INIT_REFCNT(); 
  mBufferMgr = NULL;
  mTotalWritten = 0;
  mTotalRead = 0;
  mDocHeader = PR_FALSE;
  mAttachContentType = NULL;

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

nsMimeHtmlEmitter::~nsMimeHtmlEmitter(void)
{
  if (mBufferMgr)
    delete mBufferMgr;

  // Release the prefs service
  if (mPrefs)
    nsServiceManager::ReleaseService(kPrefCID, mPrefs);
}

// Set the output stream for processed data.
nsresult
nsMimeHtmlEmitter::SetPipe(nsIInputStream * aInputStream, nsIOutputStream *outStream)
{
  mInputStream = aInputStream;
  mOutStream = outStream;
  return NS_OK;
}

// Note - these is setup only...you should not write
// anything to the stream since these may be image data
// output streams, etc...
nsresult       
nsMimeHtmlEmitter::Initialize(nsIURI *url, nsIChannel * aChannel)
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
nsMimeHtmlEmitter::SetOutputListener(nsIStreamListener *listener)
{
  mOutListener = listener;

  return NS_OK;
}

// Note - this is teardown only...you should not write
// anything to the stream since these may be image data
// output streams, etc...
nsresult
nsMimeHtmlEmitter::Complete()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_FALSE;
#endif

  // If we are here and still have data to write, we should try
  // to flush it...if we try and fail, we should probably return
  // an error!
  PRUint32      written; 
  if ( (mBufferMgr) && (mBufferMgr->GetSize() > 0))
    Write("", 0, &written);

#ifdef DEBUG_rhp
  if (mLogFile) 
    PR_Close(mLogFile);
#endif

  return NS_OK;
}

// Header handling routines.
nsresult
nsMimeHtmlEmitter::StartHeader(PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                           const char *outCharset)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  mDocHeader = rootMailHeader;

  if (mDocHeader)
  {
    if ( (!headerOnly) && (outCharset) && (*outCharset) )
    {
      UtilityWrite("<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=");
      UtilityWrite(outCharset);
      UtilityWrite("\">");
    }
    UtilityWrite("<BLOCKQUOTE><table BORDER=0>");
  }  
  else
    UtilityWrite("<BLOCKQUOTE><table BORDER=0 BGCOLOR=\"#CCCCCC\" >");
  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::AddHeaderField(const char *field, const char *value)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  if ( (!field) || (!value) )
    return NS_OK;

  //
  // This is a check to see what the pref is for header display. If
  // We should only output stuff that corresponds with that setting.
  //
  if (!EmitThisHeaderForPrefSetting(mHeaderDisplayType, field))
    return NS_OK;

  char  *newValue = nsEscapeHTML(value);
  if (!newValue)
    return NS_OK;

  UtilityWrite("<TR>");

  UtilityWrite("<td>");
  UtilityWrite("<div align=right>");
  UtilityWrite("<B>");

  // Here is where we are going to try to L10N the tagName so we will always
  // get a field name next to an emitted header value. Note: Default will always
  // be the name of the header itself.
  //
  nsString  newTagName(field);
  newTagName.CompressWhitespace(PR_TRUE, PR_TRUE);
  newTagName.ToUpperCase();
  char *upCaseField = newTagName.ToNewCString();

  char *l10nTagName = LocalizeHeaderName(upCaseField, field);
  if ( (!l10nTagName) || (!*l10nTagName) )
    UtilityWrite(field);
  else
  {
    UtilityWrite(l10nTagName);
    PR_FREEIF(l10nTagName);
  }

  // Now write out the actual value itself and move on!
  //
  UtilityWrite(":");
  UtilityWrite("</B>");
  UtilityWrite("</div>");
  UtilityWrite("</td>");

  UtilityWrite("<td>");
  UtilityWrite(newValue);
  UtilityWrite("</td>");

  UtilityWrite("</TR>");

  PR_FREEIF(newValue);
  PR_FREEIF(upCaseField);
  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::EndHeader()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  UtilityWrite("</TABLE></BLOCKQUOTE>");
  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::ProcessContentType(const char *ct)
{
  if (mAttachContentType)
  {
    PR_FREEIF(mAttachContentType);
    mAttachContentType = NULL;
  }
  
  if ( (!ct) || (!*ct) )
    return NS_OK;
  
  char *slash = PL_strchr(ct, '/');
  if (!slash)
    mAttachContentType = PL_strdup(ct);
  else
  {
    PRInt32 size = (PL_strlen(ct) + 4 + 1);
    mAttachContentType = (char *)PR_MALLOC( size );
    if (!mAttachContentType)
      return NS_ERROR_OUT_OF_MEMORY;
    
    memset(mAttachContentType, 0, size);
    PL_strcpy(mAttachContentType, ct);
    
    char *newSlash = PL_strchr(mAttachContentType, '/');
    *newSlash = '\0';
    PL_strcat(mAttachContentType, "%2F");
    PL_strcat(mAttachContentType, (slash + 1));
  }

  return NS_OK;
}

// Attachment handling routines
nsresult
nsMimeHtmlEmitter::StartAttachment(const char *name, const char *contentType, const char *url)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  PR_FREEIF(mAttachContentType);
  mAttachContentType = NULL;
  ProcessContentType(contentType);
  UtilityWrite("<CENTER>");
  UtilityWrite("<table BORDER CELLSPACING=0>");
  UtilityWrite("<tr>");
  UtilityWrite("<td>");

  if (mAttachContentType)
  {
    UtilityWrite("<a href=\"");
    UtilityWrite(url);
    UtilityWrite("&outformat=");
    UtilityWrite(mAttachContentType);
    UtilityWrite("\" target=new>");
  }

  UtilityWrite("<img SRC=\"resource:/res/network/gopher-unknown.gif\" BORDER=0 ALIGN=ABSCENTER>");
  UtilityWrite(name);

  if (mAttachContentType)
    UtilityWrite("</a>");

  UtilityWrite("</td>");
  UtilityWrite("<td>");
  UtilityWrite("<table BORDER=0 BGCOLOR=\"#FFFFCC\">");
  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::AddAttachmentField(const char *field, const char *value)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  // Don't let bad things happen
  if ( (!value) || (!*value) )
    return NS_OK;

  char  *newValue = nsEscapeHTML(value);
  PRBool  linkIt = (!PL_strcmp(HEADER_X_MOZILLA_PART_URL, field));

  //
  // For now, let's not output the long URL field, but when prefs are
  // working this will change.
  //
  if (linkIt)
    return NS_OK;

  UtilityWrite("<TR>");

  UtilityWrite("<td>");
  UtilityWrite("<div align=right>");
  UtilityWrite("<B>");
  UtilityWrite(field);
  UtilityWrite(":");
  UtilityWrite("</B>");
  UtilityWrite("</div>");
  UtilityWrite("</td>");
  UtilityWrite("<td>");

  if (linkIt)
  {
    UtilityWrite("<a href=\"");
    UtilityWrite(value);
    if (mAttachContentType)
    {
      UtilityWrite("&outformat=");
      UtilityWrite(mAttachContentType);
    }
    UtilityWrite("\" target=new>");
  }

  UtilityWrite(newValue);

  if (linkIt)
    UtilityWrite("</a>");

  UtilityWrite("</td>");
  UtilityWrite("</TR>");

  PR_FREEIF(newValue);
  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::EndAttachment()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  PR_FREEIF(mAttachContentType);
  UtilityWrite("</TABLE>");
  UtilityWrite("</td>");
  UtilityWrite("</tr>");

  UtilityWrite("</TABLE>");
  UtilityWrite("</CENTER>");
  UtilityWrite("<BR>");
  return NS_OK;
}

// Attachment handling routines
nsresult
nsMimeHtmlEmitter::StartBody(PRBool bodyOnly, const char *msgID, const char *outCharset)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  if ((bodyOnly) && (outCharset) && (*outCharset))
  {
    UtilityWrite("<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=");
    UtilityWrite(outCharset);
    UtilityWrite("\">");
  }

  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  Write(buf, size, amountWritten);
  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::EndBody()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_FALSE;
#endif

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// These are routines necessary for the C based routines in libmime
// to access the new world streams.
////////////////////////////////////////////////////////////////////////////////
nsresult
nsMimeHtmlEmitter::Write(const char *buf, PRUint32 size, PRUint32 *amountWritten)
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
nsMimeHtmlEmitter::UtilityWrite(const char *buf)
{
  PRInt32     tmpLen = PL_strlen(buf);
  PRUint32    written;

  Write(buf, tmpLen, &written);
#ifdef DEBUG
//  Write("\r\n", 2, &written);
#endif

  return NS_OK;
}
