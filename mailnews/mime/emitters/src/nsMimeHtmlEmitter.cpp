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
}

nsMimeHtmlEmitter::~nsMimeHtmlEmitter(void)
{
}

// Header handling routines.
nsresult
nsMimeHtmlEmitter::StartHeader(PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                           const char *outCharset)
{
  mDocHeader = rootMailHeader;

  if (mDocHeader)
  {
    UtilityWrite("<TABLE BORDER=0>");
  }  
  else
    UtilityWrite("<TABLE BORDER=0>");
  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::AddHeaderField(const char *field, const char *value)
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

  UtilityWrite("<TD>");
  UtilityWrite("<DIV align=right>");
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
  UtilityWrite("</DIV>");
  UtilityWrite("</TD>");

  UtilityWrite("<TD>");
  UtilityWrite(newValue);
  UtilityWrite("</TD>");

  UtilityWrite("</TR>");

  PR_FREEIF(newValue);
  nsCRT::free(upCaseField);
  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::EndHeader()
{
  UtilityWrite("</TABLE></BLOCKQUOTE>");
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
  UtilityWrite("<CENTER>");
  UtilityWrite("<TABLE BORDER CELLSPACING=0>");
  UtilityWrite("<tr>");
  UtilityWrite("<TD>");

  UtilityWrite("<CENTER>");
  UtilityWrite("<B>Attachment: </B>");
  UtilityWrite(name);
  UtilityWrite("</CENTER>");

  UtilityWrite("</TD>");
  UtilityWrite("<TD>");
  UtilityWrite("<TABLE BORDER=0>");
  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::AddAttachmentField(const char *field, const char *value)
{
  // Don't let bad things happen
  if ( (!value) || (!*value) )
    return NS_OK;

  char  *newValue = nsEscapeHTML(value);

  UtilityWrite("<TR>");

  UtilityWrite("<TD>");
  UtilityWrite("<DIV align=right>");
  UtilityWrite("<B>");
  UtilityWrite(field);
  UtilityWrite(":");
  UtilityWrite("</B>");
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
  UtilityWrite("</TABLE>");
  UtilityWrite("</TD>");
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
  return NS_OK;
}

nsresult
nsMimeHtmlEmitter::WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
  Write(buf, size, amountWritten);
  return NS_OK;
}
