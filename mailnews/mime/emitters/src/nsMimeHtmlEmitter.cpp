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

#include "nsIMimeConverter.h"
#include "nsMsgMimeCID.h"

static NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);

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
  mFirst = PR_TRUE;
  mSkipAttachment = PR_FALSE; 
}

nsMimeHtmlDisplayEmitter::~nsMimeHtmlDisplayEmitter(void)
{
}

nsresult nsMimeHtmlDisplayEmitter::Init()
{
  return NS_OK;
}

PRBool nsMimeHtmlDisplayEmitter::BroadCastHeadersAndAttachments()
{
  // try to get a header sink if there is one....
  nsCOMPtr<nsIMsgHeaderSink> headerSink; 
  nsresult rv = GetHeaderSink(getter_AddRefs(headerSink));
  if (NS_SUCCEEDED(rv) && headerSink && mDocHeader)
    return PR_TRUE;
  else
    return PR_FALSE;
}

nsresult 
nsMimeHtmlDisplayEmitter::WriteHeaderFieldHTMLPrefix()
{
  if (!BroadCastHeadersAndAttachments() || nsMimeOutput::nsMimeMessagePrintOutput)
    return nsMimeBaseEmitter::WriteHeaderFieldHTMLPrefix();
  else
    return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::WriteHeaderFieldHTML(const char *field, const char *value)
{
  if (!BroadCastHeadersAndAttachments() || nsMimeOutput::nsMimeMessagePrintOutput)
    return nsMimeBaseEmitter::WriteHeaderFieldHTML(field, value);
  else
    return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::WriteHeaderFieldHTMLPostfix()
{
  if (!BroadCastHeadersAndAttachments() || nsMimeOutput::nsMimeMessagePrintOutput)
    return nsMimeBaseEmitter::WriteHeaderFieldHTMLPostfix();
  else
    return NS_OK;
}


nsresult
nsMimeHtmlDisplayEmitter::GetHeaderSink(nsIMsgHeaderSink ** aHeaderSink)
{
  nsresult rv = NS_OK;
  if ( (mChannel) && (!mHeaderSink) )
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
  if (mDocHeader)
  {
    UtilityWriteCRLF("<HTML>");
    UtilityWriteCRLF("<HEAD>");

    // mscott --> we should refer to the style sheet used in msg display...this one is wrong i think.
    // Stylesheet info!
    UtilityWriteCRLF("<LINK REL=\"IMPORTANT STYLESHEET\" HREF=\"chrome://messenger/skin/mailheader.css\">");

    UtilityWriteCRLF("</HEAD>");
    UtilityWriteCRLF("<BODY>");
  }

  // if we aren't broadcasting headers OR printing...just do whatever
  // our base class does...
  if (mFormat == nsMimeOutput::nsMimeMessagePrintOutput)
  {
    return nsMimeBaseEmitter::WriteHTMLHeaders();
  }
  else if (!BroadCastHeadersAndAttachments() || !mDocHeader)
  {
    // This needs to be here to correct the output format if we are
    // not going to broadcast headers to the XUL document.
    if (mFormat == nsMimeOutput::nsMimeMessageBodyDisplay)
      mFormat = nsMimeOutput::nsMimeMessagePrintOutput;

    return nsMimeBaseEmitter::WriteHTMLHeaders();
  }
  else
    mFirstHeaders = PR_FALSE;
 
  // try to get a header sink if there is one....
  nsCOMPtr<nsIMsgHeaderSink> headerSink; 
  nsresult rv = GetHeaderSink(getter_AddRefs(headerSink));

  if (headerSink)
    headerSink->OnStartHeaders();

  // We are going to iterate over all the known headers,
  // and broadcast them to the header sink. However, we need to 
  // convert our UTF-8 header values into unicode before 
  // broadcasting them....
  nsXPIDLString unicodeHeaderValue;
  nsAutoString charset ("UTF-8");

  for (PRInt32 i=0; i<mHeaderArray->Count(); i++)
  {
    headerInfoType *headerInfo = (headerInfoType *)mHeaderArray->ElementAt(i);
    if ( (!headerInfo) || (!headerInfo->name) || (!(*headerInfo->name)) ||
      (!headerInfo->value) || (!(*headerInfo->value)))
      continue;

    if (headerSink)
    {
        // this interface for DecodeMimePartIIStr requires us to pass in nsStrings by reference
        // we should remove the nsString requirements from the interface....
        if (mUnicodeConverter)
  			  rv = mUnicodeConverter->DecodeMimePartIIStr(headerInfo->value, charset, getter_Copies(unicodeHeaderValue));
        else {
          nsAutoString headerValue(headerInfo->value);
          *((PRUnichar **)getter_Copies(unicodeHeaderValue)) =
            nsXPIDLString::Copy(headerValue.GetUnicode());
        }
        if (NS_SUCCEEDED(rv))
          headerSink->HandleHeader(headerInfo->name, unicodeHeaderValue);
    }
  }

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
nsMimeHtmlDisplayEmitter::StartAttachment(const char *name, const char *contentType, const char *url)
{

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgHeaderSink> headerSink; 
  rv = GetHeaderSink(getter_AddRefs(headerSink));
  
  if (headerSink)
  {
    char * escapedUrl = nsEscape(url, url_Path);
    nsXPIDLCString uriString;

    nsCOMPtr<nsIMsgMessageUrl> msgurl (do_QueryInterface(mURL, &rv));
    if (NS_SUCCEEDED(rv))
      rv = msgurl->GetURI(getter_Copies(uriString));

    // we need to convert the attachment name from UTF-8 to unicode before
    // we emit it...
    nsXPIDLString unicodeHeaderValue;
    nsAutoString charset ("UTF-8");
  
    if (mUnicodeConverter)
  	  rv = mUnicodeConverter->DecodeMimePartIIStr(name, charset,
                                                  getter_Copies(unicodeHeaderValue));
    else {
      nsAutoString attachmentName (name);
      *((PRUnichar **)getter_Copies(unicodeHeaderValue)) =
        nsXPIDLString::Copy(attachmentName.GetUnicode());
    }

    if (NS_SUCCEEDED(rv))
      headerSink->HandleAttachment(escapedUrl, unicodeHeaderValue, uriString);
    nsCRT::free(escapedUrl);
    mSkipAttachment = PR_TRUE;
  }
  else
    // then we need to deal with the attachments in the body by inserting them into a table..
    return StartAttachmentInBody(name, contentType, url);

  return rv;
}

// Attachment handling routines
// Ok, we are changing the way we handle these now...It used to be that we output 
// HTML to make a clickable link, etc... but now, this should just be informational
// and only show up in quoting
//
nsresult
nsMimeHtmlDisplayEmitter::StartAttachmentInBody(const char *name, const char *contentType, const char *url)
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

  if (!mFirst)
    UtilityWrite("<HR WIDTH=\"90%\" SIZE=4>");

  mFirst = PR_FALSE;

  UtilityWrite("<CENTER>");
  UtilityWrite("<TABLE BORDER>");
  UtilityWrite("<tr>");
  UtilityWrite("<TD>");

  UtilityWrite("<CENTER>");
  UtilityWrite("<DIV align=right CLASS=\"headerdisplayname\" style=\"display:inline;\">");

  UtilityWrite(name);

  UtilityWrite("</DIV>");
  UtilityWrite("</CENTER>");

  UtilityWrite("</TD>");
  UtilityWrite("<TD>");
  UtilityWrite("<TABLE BORDER=0>");
  return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::AddAttachmentField(const char *field, const char *value)
{
  if (mSkipAttachment || BroadCastHeadersAndAttachments())
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
  UtilityWrite("<DIV align=right CLASS=\"headerdisplayname\" style=\"display:inline;\">");

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
nsMimeHtmlDisplayEmitter::EndAttachment()
{
  mSkipAttachment = PR_FALSE;
  if (BroadCastHeadersAndAttachments())
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
nsMimeHtmlDisplayEmitter::WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten)
{
  Write(buf, size, amountWritten);
  return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::EndBody()
{
  UtilityWriteCRLF("</BODY>");
  UtilityWriteCRLF("</HTML>");
  return NS_OK;
}


