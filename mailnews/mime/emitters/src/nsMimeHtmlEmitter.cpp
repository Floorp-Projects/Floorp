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
#include "nsIMsgWindow.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsXPIDLString.h"
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
  if ( (contentType) &&
        ((!nsCRT::strcmp(contentType, APPLICATION_XPKCS7_MIME)) ||
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

  UtilityWrite("<CENTER>");
  UtilityWrite("<TABLE BORDER>");
  UtilityWrite("<tr>");
  UtilityWrite("<TD>");

  UtilityWrite("<CENTER>");
  UtilityWrite("<DIV align=right CLASS=\"headerdisplayname\">");

  UtilityWrite(name);

  UtilityWrite("</DIV>");
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


/////////////////////////////////////////////////////////////////////////////////////////////

nsresult NS_NewMimeHtmlDisplayEmitter(const nsIID& iid, void **result)
{
	nsMimeHtmlDisplayEmitter *obj = new nsMimeHtmlDisplayEmitter();
	if (obj)
		return obj->QueryInterface(iid, result);
	else
		return NS_ERROR_OUT_OF_MEMORY;
}


/*
 * nsMimeHtmlEmitter definitions....
 */
nsMimeHtmlDisplayEmitter::nsMimeHtmlDisplayEmitter()
{
  mFormat = nsMimeOutput::nsMimeMessageBodyQuoting;
  mFirst = PR_TRUE;
}

nsMimeHtmlDisplayEmitter::~nsMimeHtmlDisplayEmitter(void)
{
}

nsresult 
nsMimeHtmlDisplayEmitter::WriteHeaderFieldHTMLPrefix()
{
  return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::WriteHeaderFieldHTML(const char *field, const char *value)
{
  return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::WriteHeaderFieldHTMLPostfix()
{
  return NS_OK;
}


nsresult
nsMimeHtmlDisplayEmitter::GetHeaderSink(nsIMsgHeaderSink ** aHeaderSink)
{
  nsresult rv = NS_OK;
  if (!mHeaderSink)
  {
    nsCOMPtr<nsIURI> uri;
    mChannel->GetURI(getter_AddRefs(uri));
    if (uri)
    {
      nsCOMPtr<nsIMsgMailNewsUrl> msgurl (do_QueryInterface(uri));
      if (msgurl)
      {
        nsCOMPtr<nsIMsgWindow> msgWindow;
        msgurl->GetMsgWindow(getter_AddRefs(msgWindow));
        if (msgWindow)
          msgWindow->GetMsgHeaderSink(getter_AddRefs(mHeaderSink));
      }
    }
  }

  *aHeaderSink = mHeaderSink;
  NS_IF_ADDREF(*aHeaderSink);
  return rv;
}

nsresult nsMimeHtmlDisplayEmitter::WriteHTMLHeaders()
{
  // try to get a header sink if there is one....
  nsCOMPtr<nsIMsgHeaderSink> headerSink; 
  nsresult rv = GetHeaderSink(getter_AddRefs(headerSink));

  if (headerSink)
    headerSink->OnStartHeaders();

  for (PRInt32 i=0; i<mHeaderArray->Count(); i++)
  {
    headerInfoType *headerInfo = (headerInfoType *)mHeaderArray->ElementAt(i);
    if ( (!headerInfo) || (!headerInfo->name) || (!(*headerInfo->name)) ||
      (!headerInfo->value) || (!(*headerInfo->value)))
      continue;

    if (headerSink)
      headerSink->HandleHeader(headerInfo->name, headerInfo->value);
  }

  DumpAttachmentMenu();

  if (headerSink)
    headerSink->OnEndHeaders();
  return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::EndHeader()
{
  WriteHTMLHeaders();
  return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::DumpAttachmentMenu()
{
  nsresult rv;

  nsCOMPtr<nsIMsgHeaderSink> headerSink; 
  GetHeaderSink(getter_AddRefs(headerSink));

  if ( (!mAttachArray) || (mAttachArray->Count() <= 0) )
    return NS_OK;

  nsCAutoString escapedName;
  nsCAutoString escapedUrl;
  nsCOMPtr<nsIMsgMessageUrl> messageUrl;
  nsXPIDLCString uriString;

  // Now we can finally write out the attachment information...  
  PRInt32     i;
  for (i=0; i<mAttachArray->Count(); i++)
  {
    attachmentInfoType *attachInfo = (attachmentInfoType *)mAttachArray->ElementAt(i);
    if (!attachInfo)
       continue;

    escapedName = nsEscape(attachInfo->displayName, url_Path);
    escapedUrl = nsEscape(attachInfo->urlSpec, url_Path);
     
    nsCOMPtr<nsIMsgMessageUrl> messageUrl = do_QueryInterface(mURL, &rv);
    if (NS_SUCCEEDED(rv))
    {
      rv = messageUrl->GetURI(getter_Copies(uriString));
    }

    if (headerSink)
      headerSink->HandleAttachment(escapedUrl, attachInfo->displayName, uriString);
  } // for each attachment

    // now broadcast to the display emitter sink 

  return NS_OK;
}

// Attachment handling routines
// Ok, we are changing the way we handle these now...It used to be that we output 
// HTML to make a clickable link, etc... but now, this should just be informational
// and only show up in quoting
//
nsresult
nsMimeHtmlDisplayEmitter::StartAttachment(const char *name, const char *contentType, const char *url)
{

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgHeaderSink> headerSink; 
  rv = GetHeaderSink(getter_AddRefs(headerSink));
  
  if (headerSink)
  {
    nsCAutoString escapedUrl;
    nsXPIDLCString uriString;
    escapedUrl = nsEscape(url, url_Path);

    nsCOMPtr<nsIMsgMessageUrl> messageUrl = do_QueryInterface(mURL, &rv);
    if (NS_SUCCEEDED(rv))
      rv = messageUrl->GetURI(getter_Copies(uriString));
    headerSink->HandleAttachment(escapedUrl, name, uriString);
  }

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

  return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::AddAttachmentField(const char *field, const char *value)
{
  if (mSkipAttachment)
    return NS_OK;
  return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::EndAttachment()
{
  if (mSkipAttachment)
    return NS_OK;
   return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
  Write(buf, size, amountWritten);
  return NS_OK;
}
