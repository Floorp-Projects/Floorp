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

#include "stdio.h"
#include "nsMimeRebuffer.h"
#include "nsMimeXmlEmitter.h"
#include "plstr.h"
#include "nsMailHeaders.h"
#include "nscore.h"
#include "nsEscape.h"
#include "prmem.h"
#include "nsEmitterUtils.h"
#include "nsCOMPtr.h"

nsresult NS_NewMimeXmlEmitter(const nsIID& iid, void **result)
{
	nsMimeXmlEmitter *obj = new nsMimeXmlEmitter();
	if (obj)
		return obj->QueryInterface(iid, result);
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

/*
 * nsMimeXmlEmitter definitions....
 */
nsMimeXmlEmitter::nsMimeXmlEmitter()
{
}


nsMimeXmlEmitter::~nsMimeXmlEmitter(void)
{
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

  return nsMimeBaseEmitter::Complete();

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


