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
#include "msgCore.h"

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

  // Header cache...
  mHeaderArray = new nsVoidArray();

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
}

nsMimeXULEmitter::~nsMimeXULEmitter(void)
{
  PRInt32 i;

  if (mAttachArray)
  {
    for (i=0; i<mAttachArray->Count(); i++)
    {
      attachmentInfoType *attachInfo = (attachmentInfoType *)mAttachArray->ElementAt(i);
      if (!attachInfo)
        continue;
    
      PR_FREEIF(attachInfo->contentType);
      PR_FREEIF(attachInfo->displayName);
      PR_FREEIF(attachInfo->urlSpec);
    }
    delete mAttachArray;
  }

  if (mHeaderArray)
  {
    for (i=0; i<mHeaderArray->Count(); i++)
    {
      headerInfoType *headerInfo = (headerInfoType *)mHeaderArray->ElementAt(i);
      if (!headerInfo)
        continue;
    
      PR_FREEIF(headerInfo->name);
      PR_FREEIF(headerInfo->value);
    }
    delete mHeaderArray;
  }

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
  // If any fields are of interest, do something here with them...
  return NS_OK;
}

nsresult
nsMimeXULEmitter::EndAttachment()
{
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
  mBody.Append(buf, size);
  *amountWritten = size;

  return NS_OK;
}

nsresult
nsMimeXULEmitter::EndBody()
{
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

  return NS_OK;
}

nsresult
nsMimeXULEmitter::UtilityWriteCRLF(const char *buf)
{
  PRInt32     tmpLen = PL_strlen(buf);
  PRUint32    written;

  Write(buf, tmpLen, &written);
  Write(MSG_LINEBREAK, MSG_LINEBREAK_LEN, &written);

  return NS_OK;
}

nsresult
nsMimeXULEmitter::AddHeaderFieldHTML(const char *field, const char *value)
{
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

//
// Find a cached header! Note: Do NOT free this value!
//
char *
nsMimeXULEmitter::GetHeaderValue(const char *aHeaderName)
{
  PRInt32     i;
  char        *retVal = nsnull;

  if (!mHeaderArray)
    return nsnull;

  for (i=0; i<mHeaderArray->Count(); i++)
  {
    headerInfoType *headerInfo = (headerInfoType *)mHeaderArray->ElementAt(i);
    if ( (!headerInfo) || (!headerInfo->name) || (!(*headerInfo->name)) )
      continue;
    
    if (!PL_strcasecmp(aHeaderName, headerInfo->name))
    {
      retVal = headerInfo->value;
      break;
    }
  }

  return retVal;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// Ok, these are the routines that will need to be modified to alter the 
// appearance of the XUL display for the message. Everything above this line
// should just be support routines for this output.
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

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
    
  UtilityWriteCRLF("<?xml version=\"1.0\"?>");

  if (mHeaderDisplayType == nsMimeHeaderDisplayTypes::MicroHeaders)
    UtilityWriteCRLF("<?xml-stylesheet href=\"chrome://messenger/skin/mailheader-micro.css\" type=\"text/css\"?>");
  else if (mHeaderDisplayType == nsMimeHeaderDisplayTypes::NormalHeaders)
    UtilityWriteCRLF("<?xml-stylesheet href=\"chrome://messenger/skin/mailheader-normal.css\" type=\"text/css\"?>");
  else /* AllHeaders */
    UtilityWriteCRLF("<?xml-stylesheet href=\"chrome://messenger/skin/mailheader-all.css\" type=\"text/css\"?>");

  // Make it look consistent...
  UtilityWriteCRLF("<?xml-stylesheet href=\"chrome://global/skin/\" type=\"text/css\"?>");

  // Output the message ID to make it query-able via the DOM
  UtilityWrite("<message id=\"");
  UtilityWrite(newValue);
  PR_FREEIF(newValue);
  UtilityWriteCRLF("\">");

  // Now, the XUL window!
  UtilityWriteCRLF("<window ");
  UtilityWriteCRLF("xmlns:html=\"http://www.w3.org/TR/REC-html40\" ");
  UtilityWriteCRLF("xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" ");
  UtilityWriteCRLF("xmlns=\"http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul\" ");
  UtilityWriteCRLF("align=\"vertical\"> ");

  // Now, the JavaScript...
  UtilityWriteCRLF("<html:script language=\"javascript\" src=\"chrome://messenger/content/attach.js\"/>");

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
  mDocHeader = rootMailHeader;

  if (!mDocHeader)
  {
    UtilityWriteCRLF("<BLOCKQUOTE><table BORDER=0 BGCOLOR=\"#CCCCCC\" >");
  }
  else
  {
    WriteXULHeader(msgID);
  }

  return NS_OK; 
}

nsresult
nsMimeXULEmitter::DoSpecialSenderProcessing(const char *field, const char *value)
{
  return NS_OK;
}

//
// This will be called for every header field...for now, we are calling the WriteXULTag for
// each of these calls.
//
nsresult
nsMimeXULEmitter::AddHeaderField(const char *field, const char *value)
{
  if ( (!field) || (!value) )
    return NS_OK;

  if ( (mDocHeader) && (!PL_strcmp(field, HEADER_FROM)) )
    DoSpecialSenderProcessing(field, value);

  if (!mDocHeader)
    return AddHeaderFieldHTML(field, value);
  else
  { 
    // This is the main header so we do caching for XUL later!
    // Ok, now we will setup the header info for the header array!
    headerInfoType  *ptr = (headerInfoType *) PR_NEWZAP(headerInfoType);
    if ( (ptr) && mHeaderArray)
    {
      ptr->name = PL_strdup(field);
      ptr->value = PL_strdup(value);
      mHeaderArray->AppendElement(ptr);
    }

    return NS_OK;
  }
}

//
// This finalizes the header field being parsed.
//
nsresult
nsMimeXULEmitter::EndHeader()
{
  if (!mDocHeader)
    UtilityWriteCRLF("</TABLE></BLOCKQUOTE>");
  
  // Nothing to do in the case of the actual Envelope. This will be done
  // on Complete().

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

  // UtilityWrite("<html:div>: </html:div>");
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
// This is called at teardown time, but since we have been cacheing lots of 
// good information about the mail message (headers, attachments and body), this 
// is the time that we have to do a lot of completion processing...
// such as writing the XUL for the attachments and the XUL for the body
// display
//
nsresult
nsMimeXULEmitter::Complete()
{
  // Start off the header as a toolbox!
  UtilityWriteCRLF("<toolbox>");

  // Start with the subject, from date info!
  DumpSubjectFromDate();

  // Continue with the to and cc headers
  DumpToCC();

  // do the rest of the headers, but these will only be written if
  // the user has the "show all headers" pref set
  DumpRestOfHeaders();

  // Now close out the header toolbox
  UtilityWriteCRLF("</toolbox>");

  // Now process the body...
  DumpBody();

  // Finalize the XUL document...
  UtilityWriteCRLF("</window>");

  // Now, just try to check if we still have any data and if so,
  // try to flush it this one final time
  PRUint32      written; 
  if ( (mBufferMgr) && (mBufferMgr->GetSize() > 0))
    Write("", 0, &written);

  return NS_OK;
}


nsresult
nsMimeXULEmitter::DumpAttachmentMenu()
{
  if ( (!mAttachArray) || (mAttachArray->Count() <= 0) )
    return NS_OK;

  char *buttonXUL = PR_smprintf(
                      "<titledbutton src=\"chrome://messenger/skin/attach.gif\" value=\"%d Attachments\" align=\"right\" class=\"popup\" popup=\"attachmentPopup\"/>", 
                      mAttachArray->Count());

  if ( (!buttonXUL) || (!*buttonXUL) )
    return NS_OK;

  UtilityWriteCRLF("<box align=\"horizontal\">");

  UtilityWriteCRLF("<popup id=\"attachmentPopup\">");
  UtilityWriteCRLF("<menu>");

  // Now we can finally write out the attachment information...  
  if (mAttachArray->Count() > 0)
  {
    PRInt32     i;
    
    for (i=0; i<mAttachArray->Count(); i++)
    {
      attachmentInfoType *attachInfo = (attachmentInfoType *)mAttachArray->ElementAt(i);
      if (!attachInfo)
        continue;

      UtilityWrite("<menuitem value=\"");
      UtilityWrite(attachInfo->displayName);
      UtilityWrite("\" onaction=\"OpenAttachURL('");
      UtilityWrite(attachInfo->urlSpec);
      UtilityWriteCRLF("' );\"  />");
    }
  }

  UtilityWriteCRLF("</menu>");
  UtilityWriteCRLF("</popup>");
  UtilityWriteCRLF(buttonXUL);

  UtilityWriteCRLF("</box>");
  PR_FREEIF(buttonXUL);

  return NS_OK;
}

nsresult
nsMimeXULEmitter::DumpAddBookIcon()
{
  UtilityWriteCRLF("<box align=\"horizontal\">");
  UtilityWriteCRLF("<titledbutton src=\"chrome://messenger/skin/addcard.gif\"/>");
  UtilityWriteCRLF("</box>");
  return NS_OK;
}

nsresult
nsMimeXULEmitter::DumpBody()
{
  // Now we will write out the XUL/IFRAME line for the mBody which
  // we have cached to disk.
  //
  char  *url = nsnull;
  if (mBodyFileSpec)
    url = nsMimePlatformFileToURL(mBodyFileSpec->GetNativePathCString());

  UtilityWrite("<html:iframe id=\"mail-body-frame\" src=\"");
  UtilityWrite(url);
  UtilityWriteCRLF("\" border=\"0\" scrolling=\"auto\" resize=\"yes\" width=\"100%\" height=\"100%\"/>");
  PR_FREEIF(url);
  return NS_OK;
}

nsresult
nsMimeXULEmitter::OutputGenericHeader(const char *aHeaderVal)
{
  char      *val = GetHeaderValue(aHeaderVal);
  nsresult  rv;

  if (val)
  {
    //UtilityWriteCRLF("<box align=\"vertical\" style=\"padding: 2px;\">");
    UtilityWriteCRLF("<box style=\"padding: 2px;\">");
    rv = WriteXULTag(aHeaderVal, val);
    UtilityWriteCRLF("</box>");
    return rv;
  }
  else
    return NS_ERROR_FAILURE;
}

nsresult
nsMimeXULEmitter::DumpSubjectFromDate()
{
  UtilityWriteCRLF("<toolbar>");
  UtilityWriteCRLF("<box name=\"header-splitter\" align=\"horizontal\" flex=\"100%\" >");

      UtilityWriteCRLF("<box name=\"header-part1\" align=\"vertical\" flex=\"100%\">");
      UtilityWriteCRLF("<spring flex=\"2\"/>");

          // This is the envelope information
          OutputGenericHeader(HEADER_SUBJECT);
          OutputGenericHeader(HEADER_FROM);
          OutputGenericHeader(HEADER_DATE);

      UtilityWriteCRLF("<spring flex=\"2\"/>");
      UtilityWriteCRLF("</box>");

      UtilityWriteCRLF("<box name=\"header-attachment\" align=\"horizontal\" flex=\"100%\">");
      UtilityWriteCRLF("<spring flex=\"1\"/>");

        // Now the addbook and attachment icons need to be displayed
        DumpAddBookIcon();
        DumpAttachmentMenu();

      UtilityWriteCRLF("<spring flex=\"1\"/>");
      UtilityWriteCRLF("</box>");

  UtilityWriteCRLF("</box>");
  UtilityWriteCRLF("</toolbar>");

  return NS_OK;
}

nsresult
nsMimeXULEmitter::DumpToCC()
{
  UtilityWriteCRLF("<toolbar>");
  UtilityWriteCRLF("<box name=\"header-splitter2\" align=\"horizontal\" flex=\"100%\" >");

    UtilityWriteCRLF("<box name=\"header-part2\" align=\"vertical\" flex=\"100%\" style=\"background-color: #FF0000; \" >");
    UtilityWriteCRLF("<spring flex=\"1\"/>");

          OutputGenericHeader(HEADER_TO);

    UtilityWriteCRLF("<spring flex=\"1\"/>");
    UtilityWriteCRLF("</box>");

    UtilityWriteCRLF("<box name=\"header-part3\" align=\"vertical\" style=\"background-color: #00FF00; \">");
    UtilityWriteCRLF("<spring flex=\"1\"/>");

      OutputGenericHeader(HEADER_CC);

    UtilityWriteCRLF("<spring flex=\"1\"/>");
    UtilityWriteCRLF("</box>");

  UtilityWriteCRLF("</box>");
  UtilityWriteCRLF("</toolbar>");
  return NS_OK;
}

nsresult
nsMimeXULEmitter::DumpRestOfHeaders()
{
  if (mHeaderDisplayType != nsMimeHeaderDisplayTypes::AllHeaders)
    return NS_OK;

  UtilityWriteCRLF("<toolbar>");

  UtilityWriteCRLF("</toolbar>");
  return NS_OK;
}
