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
#include "nsMimeXULEmitter.h"
#include "plstr.h"
#include "nsIMimeEmitter.h"
#include "nsMailHeaders.h"
#include "nscore.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsEscape.h"
#include "prmem.h"
#include "nsEmitterUtils.h"
#include "nsFileStream.h"
#include "nsMimeStringResources.h"

// For the new pref API's
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

nsresult NS_NewMimeXULEmitter(const nsIID& iid, void **result)
{
	nsMimeXULEmitter *obj = new nsMimeXULEmitter();
	if (obj)
		return obj->QueryInterface(iid, result);
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

/* 
 * The following macros actually implement addref, release and 
 * query interface for our component. 
 */
NS_IMPL_ADDREF(nsMimeXULEmitter)
NS_IMPL_RELEASE(nsMimeXULEmitter)
NS_IMPL_QUERY_INTERFACE(nsMimeXULEmitter, nsIMimeEmitter::GetIID()); /* we need to pass in the interface ID of this interface */

/*
 * nsMimeXULEmitter definitions....
 */
nsMimeXULEmitter::nsMimeXULEmitter()
{
  NS_INIT_REFCNT(); 

  mBufferMgr = NULL;
  mTotalWritten = 0;
  mTotalRead = 0;
  mDocHeader = PR_FALSE;
  mAttachContentType = NULL;

  // Setup array for attachments
  mAttachCount = 0;
  mAttachArray = new nsVoidArray();
  mCurrentAttachment = nsnull;

  // Vars to handle the body...
  mBody = "";
  mBodyFileSpec = nsnull;
  mBodyStarted = PR_FALSE;

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

nsMimeXULEmitter::~nsMimeXULEmitter(void)
{
  if (mAttachArray)
    delete mAttachArray;
 
  if (mBufferMgr)
    delete mBufferMgr;

  if (mBodyFileSpec)
    delete mBodyFileSpec;

  // Release the prefs service
  if (mPrefs)
    nsServiceManager::ReleaseService(kPrefCID, mPrefs);
}

// Set the output stream for processed data.
NS_IMETHODIMP
nsMimeXULEmitter::SetPipe(nsIInputStream * aInputStream, nsIOutputStream *outStream)
{
  mInputStream = aInputStream;
  mOutStream = outStream;
  return NS_OK;
}

// Note - these is setup only...you should not write
// anything to the stream since these may be image data
// output streams, etc...
nsresult       
nsMimeXULEmitter::Initialize(nsIURI *url, nsIChannel * aChannel)
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
nsMimeXULEmitter::SetOutputListener(nsIStreamListener *listener)
{
  mOutListener = listener;
  return NS_OK;
}

// Attachment handling routines
nsresult
nsMimeXULEmitter::StartAttachment(const char *name, const char *contentType, const char *url)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  // Ok, now we will setup the attachment info 
  mCurrentAttachment = (attachmentInfoType *) PR_NEWZAP(attachmentInfoType);
  if ( (mCurrentAttachment) && mAttachArray)
  {
    ++mAttachCount;

    mCurrentAttachment->displayName = PL_strdup(name);
    mCurrentAttachment->urlSpec = PL_strdup(url);
    mCurrentAttachment->contentType = PL_strdup(contentType);
  }

  return NS_OK;
}

nsresult
nsMimeXULEmitter::AddAttachmentField(const char *field, const char *value)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  // If any fields are of interest, do something here with them...
  return NS_OK;
}

nsresult
nsMimeXULEmitter::EndAttachment()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif
  
  // Ok, add the attachment info to the attachment array...
  if ( (mCurrentAttachment) && (mAttachArray) )
  {
    mAttachArray->AppendElement(mCurrentAttachment);
    mCurrentAttachment = nsnull;
  }

  return NS_OK;
}

// Attachment handling routines
nsresult
nsMimeXULEmitter::StartBody(PRBool bodyOnly, const char *msgID, const char *outCharset)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  mBody.Append("<HTML>");

  mBody.Append("<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=");
  mBody.Append(outCharset);
  mBody.Append("\">");

  mBodyStarted = PR_TRUE;
  return NS_OK;
}

nsresult
nsMimeXULEmitter::WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  mBody.Append(buf, size);
  *amountWritten = size;

  return NS_OK;
}

nsresult
nsMimeXULEmitter::EndBody()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  mBody.Append("</HTML>");
  mBodyStarted = PR_FALSE;

  // Now that the body is complete, we are going to create a temp file and 
  // write it out to disk. The nsFileSpec will be stored in the object for 
  // writing to the output stream on the Complete() call.  
  mBodyFileSpec = nsMsgCreateTempFileSpec("nsMimeBody.html");
  if (!mBodyFileSpec)
    return MIME_ERROR_WRITING_FILE;

  nsOutputFileStream tempOutfile(*mBodyFileSpec);
  if (! tempOutfile.is_open())
    return MIME_ERROR_WRITING_FILE;

  char  *tmpBody = mBody.ToNewCString();
  if (tmpBody)
    PRInt32 n = tempOutfile.write(tmpBody, mBody.Length());
  tempOutfile.close();

  return NS_OK;
}

nsresult
nsMimeXULEmitter::Write(const char *buf, PRUint32 size, PRUint32 *amountWritten)
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
  //
  // Note: if the body has been started, we shouldn't write to the
  // output stream, but rather, just append to the body buffer.
  //
  if (mBodyStarted)
  {
    mBody.Append(buf, size);
    rc = size;
    written = size;
  }
  else
    rc = mOutStream->Write(buf, size, &written);

  *amountWritten = written;
  mTotalWritten += written;

  if (written < size)
    mBufferMgr->IncreaseBuffer(buf+written, (size-written));

  // Only call the listener if we wrote data into the stream.
  if ((!mBodyStarted) && (mOutListener))
    mOutListener->OnDataAvailable(mChannel, mURL, mInputStream, 0, written);

  return rc;
}

nsresult
nsMimeXULEmitter::UtilityWrite(const char *buf)
{
  PRInt32     tmpLen = PL_strlen(buf);
  PRUint32    written;

  Write(buf, tmpLen, &written);
#ifdef DEBUG
//  Write("\r\n", 2, &written);
#endif

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// Ok, these are the routines that will need to be modified to alter the 
// appearance of the XUL display for the message. Everything above this line
// should just be support routines for this output.
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

// This is called at teardown time, but since we have been cacheing lots of 
// good information about the mail message (attachments and body), this is 
// probably the time that we have to do a lot of completion processing...
// such as writing the XUL for the attachments and the XUL for the body
// display
//
nsresult
nsMimeXULEmitter::Complete()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  // Now we can finally write out the attachment information...  
  if (mAttachArray->Count() > 0)
  {
    PRInt32     i;
    
    for (i=0; i<mAttachArray->Count(); i++)
    {
      attachmentInfoType *attachInfo = (attachmentInfoType *)mAttachArray->ElementAt(i);
      if (!attachInfo)
        continue;

      UtilityWrite(attachInfo->displayName);
      UtilityWrite(" ");
    }
  }

  // Now, close out the box for the headers...
  //
  UtilityWrite("</box>");  // For the headers!
  
  // Now we will write out the XUL/IFRAME line for the mBody which
  // we have cached to disk.
  //
  char  *url = nsnull;
  if (mBodyFileSpec)
    url = nsMimePlatformFileToURL(mBodyFileSpec->GetNativePathCString());

  UtilityWrite("<html:iframe id=\"mail-body-frame\" src=\"");
  UtilityWrite(url);
  UtilityWrite("\" flex=\"100%\" width=\"100%\" />");

  PR_FREEIF(url);

  UtilityWrite("</window>");

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

// 
// This should be any of the XUL that has to be part of the header for the
// display. This will include style sheet information, etc...
//
nsresult
nsMimeXULEmitter::WriteXULHeader(const char *msgID)
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

  UtilityWrite("<window ");
  UtilityWrite("xmlns:html=\"http://www.w3.org/TR/REC-html40\" ");
  UtilityWrite("xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" ");
  UtilityWrite("xmlns=\"http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul\" ");
  UtilityWrite("align=\"vertical\"> ");

  mXULHeaderStarted = PR_TRUE;
  PR_FREEIF(newValue);
  return NS_OK;
}

//
// This is a header tag (i.e. tagname: "From", value: rhp@netscape.com. Do the right 
// thing here to display this header in the XUL output.
//
nsresult
nsMimeXULEmitter::WriteXULTag(const char *tagName, const char *value)
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

  delete[] upCaseTag;
  PR_FREEIF(newValue);

  return NS_OK;
}

//
// This is called at the start of the header block for all header information in ANY
// AND ALL MESSAGES (yes, quoted, attached, etc...) So, we need to handle these differently
// if they are the rood message as opposed to quoted message. If rootMailHeader is set to
// PR_TRUE, this is the root document, else it is a header in the body somewhere
//
nsresult
nsMimeXULEmitter::StartHeader(PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                           const char *outCharset)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  mDocHeader = rootMailHeader;

  if (!mDocHeader)
  {
    UtilityWrite("<BLOCKQUOTE><table BORDER=0 BGCOLOR=\"#CCCCCC\" >");
  }
  else
  {
    WriteXULHeader(msgID);

    UtilityWrite("<box align=\"horizontal\">");
    UtilityWrite("<spring flex=\"100%\"/>");
  }

  return NS_OK; 
}

//
// This will be called for every header field...for now, we are calling the WriteXULTag for
// each of these calls.
//
nsresult
nsMimeXULEmitter::AddHeaderField(const char *field, const char *value)
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  if ( (!field) || (!value) )
    return NS_OK;

  WriteXULTag(field, value);
  return NS_OK;
}

//
// This finalizes the header field being parsed.
//
nsresult
nsMimeXULEmitter::EndHeader()
{
#ifdef DEBUG_rhp
  mReallyOutput = PR_TRUE;
#endif

  if (!mDocHeader)
    UtilityWrite("</TABLE></BLOCKQUOTE>");
 
  return NS_OK; 
}

