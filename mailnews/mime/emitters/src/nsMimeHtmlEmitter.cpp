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

  nsCRT::free(newValue);
  nsCRT::free(upCaseField);
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

  UtilityWrite("<CENTER>");
  UtilityWrite("<img SRC=\"resource:/chrome/editor/skin/default/images/ED_NewFile.gif\" BORDER=0 ALIGN=ABSCENTER>");
  UtilityWrite(name);
  UtilityWrite("</CENTER>");

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

  nsCRT::free(newValue);
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


