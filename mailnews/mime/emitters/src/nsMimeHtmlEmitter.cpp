/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */
#include "nsCOMPtr.h"
#include "stdio.h"
#include "nsMimeRebuffer.h"
#include "nsMimeHtmlEmitter.h"
#include "plstr.h"
#include "nsMailHeaders.h"
#include "nscore.h"
#include "nsEmitterUtils.h"
#include "nsEscape.h"
#include "nsIMimeStreamConverter.h"
#include "nsMimeTypes.h"

nsresult NS_NewMimeHtmlEmitter(const nsIID& iid, void **result)
{
	nsMimeHtmlEmitter *obj = new nsMimeHtmlEmitter();
	if (obj)
		return obj->QueryInterface(iid, result);
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

/*
 * nsMimeHtmlEmitter definitions....
 */
nsMimeHtmlEmitter::nsMimeHtmlEmitter()
{
  mFormat = nsMimeOutput::nsMimeMessageBodyQuoting;
  mFirst = PR_TRUE;
  mSkipAttachment = PR_FALSE;
}

nsMimeHtmlEmitter::~nsMimeHtmlEmitter(void)
{
}

nsresult
nsMimeHtmlEmitter::EndHeader()
{
  if (mDocHeader)
  {
    // Stylesheet info!
    UtilityWriteCRLF("<LINK REL=\"STYLESHEET\" HREF=\"chrome://messenger/skin/mailheader.css\">");

    // Make it look consistent...
    UtilityWriteCRLF("<LINK REL=\"STYLESHEET\" HREF=\"chrome://global/skin\">");
  }
 
  WriteHTMLHeaders();
  return NS_OK;
}

// Attachment handling routines
// Ok, we are changing the way we handle these now...It used to be that we output 
// HTML to make a clickable link, etc... but now, this should just be informational
// and only show up in quoting
//
nsresult
nsMimeHtmlEmitter::StartAttachment(const char *name, const char *contentType, const char *url)
{
  if (  (contentType) &&
        ( (!nsCRT::strcmp(contentType, APPLICATION_XPKCS7_MIME)) ||
          (!nsCRT::strcmp(contentType, APPLICATION_XPKCS7_SIGNATURE)) ||
          (!nsCRT::strcmp(contentType, TEXT_VCARD))
        )
     )
  {
    mSkipAttachment = PR_TRUE;
    return NS_OK;
  }
  else
    mSkipAttachment = PR_FALSE;

  if (mFirst)
    UtilityWrite("<HR WIDTH=\"90%\" SIZE=4>");

  mFirst = PR_FALSE;

  UtilityWrite("<CENTER BORDER=1>");
  UtilityWrite("<TABLE>");
  UtilityWrite("<tr>");
  UtilityWrite("<TD>");

  UtilityWrite("<DIV align=right CLASS=\"headerdisplayname\">");

  UtilityWrite(name);

  UtilityWrite("</DIV>");

  UtilityWrite("</TD>");
  UtilityWrite("<TD>");
  UtilityWrite("<TABLE BORDER=1>");
  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::AddAttachmentField(const char *field, const char *value)
{
  if (mSkipAttachment)
    return NS_OK;

  // Don't let bad things happen
  if ( (!value) || (!*value) )
    return NS_OK;

  // Don't output this ugly header...
  if (!nsCRT::strcmp(field, HEADER_X_MOZILLA_PART_URL))
    return NS_OK;

  char  *newValue = nsEscapeHTML(value);

  UtilityWrite("<TR>");

  UtilityWrite("<TD>");
  UtilityWrite("<DIV align=right CLASS=\"headerdisplayname\">");

  UtilityWrite(field);
  UtilityWrite(":");
  UtilityWrite("</DIV>");
  UtilityWrite("</TD>");
  UtilityWrite("<TD>");

  UtilityWrite(newValue);

  UtilityWrite("</TD>");
  UtilityWrite("</TR>");

  PR_FREEIF(newValue);
  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::EndAttachment()
{
  if (mSkipAttachment)
    return NS_OK;

  UtilityWrite("</TABLE>");
  UtilityWrite("</TD>");
  UtilityWrite("</tr>");

  UtilityWrite("</TABLE>");
  UtilityWrite("</CENTER>");
  UtilityWrite("<BR>");
  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
  Write(buf, size, amountWritten);
  return NS_OK;
}
