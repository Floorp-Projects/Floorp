/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  UtilityWriteCRLF("<?xml-stylesheet href=\"chrome://messenger/skin/messageBody.css\" type=\"text/css\"?>");

  UtilityWrite("<message id=\"");
  UtilityWrite(newValue);
  UtilityWrite("\">");

  mXMLHeaderStarted = PR_TRUE;
  PR_FREEIF(newValue);
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

  nsString  newTagName; newTagName.AssignWithConversion(tagName);
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
  PR_FREEIF(newValue);

  return NS_OK;
}

// Header handling routines.
nsresult
nsMimeXmlEmitter::StartHeader(PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                           const char *outCharset)
{
  mDocHeader = rootMailHeader;
  WriteXMLHeader(msgID);
  UtilityWrite("<mailheader>");

  return NS_OK; 
}

nsresult
nsMimeXmlEmitter::AddHeaderField(const char *field, const char *value)
{
  if ( (!field) || (!value) )
    return NS_OK;

  WriteXMLTag(field, value);
  return NS_OK;
}

nsresult
nsMimeXmlEmitter::EndHeader()
{
  UtilityWrite("</mailheader>");
  return NS_OK; 
}


// Attachment handling routines
nsresult
nsMimeXmlEmitter::StartAttachment(const char *name, const char *contentType, const char *url,
                                  PRBool aNotDownloaded)
{
  char    buf[128];

  ++mAttachCount;

  sprintf(buf, "<mailattachment id=\"%d\">", mAttachCount);
  UtilityWrite(buf);

  AddAttachmentField(HEADER_PARM_FILENAME, name);
  return NS_OK;
}

nsresult
nsMimeXmlEmitter::AddAttachmentField(const char *field, const char *value)
{
  WriteXMLTag(field, value);
  return NS_OK;
}

nsresult
nsMimeXmlEmitter::EndAttachment()
{
  UtilityWrite("</mailattachment>");
  return NS_OK;
}


