/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Henrik Gemal <mozilla@gemal.dk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include <stdio.h>
#include "nsMimeRebuffer.h"
#include "nsMimeHtmlEmitter.h"
#include "plstr.h"
#include "nsMailHeaders.h"
#include "nscore.h"
#include "nsEmitterUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsEscape.h"
#include "nsIMimeStreamConverter.h"
#include "nsIMsgWindow.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsXPIDLString.h"
#include "nsMimeTypes.h"
#include "prtime.h"
#include "nsReadableUtils.h"
#include "prprf.h"
#include "nsIStringEnumerator.h"
#include "nsStringEnumerator.h"

// hack: include this to fix opening news attachments.
#include "nsINntpUrl.h"

#include "nsIMimeConverter.h"
#include "nsMsgMimeCID.h"
#include "nsDateTimeFormatCID.h"

static NS_DEFINE_CID(kDateTimeFormatCID,    NS_DATETIMEFORMAT_CID);
#define VIEW_ALL_HEADERS 2

/*
 * nsMimeHtmlEmitter definitions....
 */
nsMimeHtmlDisplayEmitter::nsMimeHtmlDisplayEmitter() : nsMimeBaseEmitter()
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
  if (!BroadCastHeadersAndAttachments() || (mFormat == nsMimeOutput::nsMimeMessagePrintOutput))
    return nsMimeBaseEmitter::WriteHeaderFieldHTMLPrefix();
  else
    return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::WriteHeaderFieldHTML(const char *field, const char *value)
{
  if (!BroadCastHeadersAndAttachments() || (mFormat == nsMimeOutput::nsMimeMessagePrintOutput))
    return nsMimeBaseEmitter::WriteHeaderFieldHTML(field, value);
  else
    return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::WriteHeaderFieldHTMLPostfix()
{
  if (!BroadCastHeadersAndAttachments() || (mFormat == nsMimeOutput::nsMimeMessagePrintOutput))
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
        msgurl->GetMsgHeaderSink(getter_AddRefs(mHeaderSink));
        if (!mHeaderSink)  // if the url is not overriding the header sink, then just get the one from the msg window
        {
        nsCOMPtr<nsIMsgWindow> msgWindow;
        msgurl->GetMsgWindow(getter_AddRefs(msgWindow));
        if (msgWindow)
          msgWindow->GetMsgHeaderSink(getter_AddRefs(mHeaderSink));
      }
    }
  }
  }

  *aHeaderSink = mHeaderSink;
  NS_IF_ADDREF(*aHeaderSink);
  return rv;
}

nsresult nsMimeHtmlDisplayEmitter::BroadcastHeaders(nsIMsgHeaderSink * aHeaderSink, PRInt32 aHeaderMode, PRBool aFromNewsgroup)
{
  // two string enumerators to pass out to the header sink
  nsCOMPtr<nsIUTF8StringEnumerator> headerNameEnumerator;
  nsCOMPtr<nsIUTF8StringEnumerator> headerValueEnumerator;

  // CStringArrays which we can pass into the enumerators
  nsCStringArray headerNameArray;
  nsCStringArray headerValueArray;

  nsCAutoString convertedDateString;

  PRBool displayOriginalDate = PR_FALSE;
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pPrefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (pPrefBranch)
    pPrefBranch->GetBoolPref("mailnews.display.original_date", &displayOriginalDate);

  for (PRInt32 i=0; i<mHeaderArray->Count(); i++)
  {
    headerInfoType * headerInfo = (headerInfoType *) mHeaderArray->ElementAt(i);
    if ( (!headerInfo) || (!headerInfo->name) || (!(*headerInfo->name)) || (!headerInfo->value) || (!(*headerInfo->value)))
      continue;

    const char * headerValue = headerInfo->value;

    // optimization: if we aren't in view all header view mode, we only show a small set of the total # of headers.
    // don't waste time sending those out to the UI since the UI is going to ignore them anyway. 
    if (aHeaderMode != VIEW_ALL_HEADERS && (mFormat != nsMimeOutput::nsMimeMessageFilterSniffer)) 
    {
      if (nsCRT::strcasecmp("to", headerInfo->name) && nsCRT::strcasecmp("from", headerInfo->name) &&
          nsCRT::strcasecmp("cc", headerInfo->name) && nsCRT::strcasecmp("newsgroups", headerInfo->name) &&
          nsCRT::strcasecmp("bcc", headerInfo->name) && nsCRT::strcasecmp("followup-to", headerInfo->name) &&
          nsCRT::strcasecmp("reply-to", headerInfo->name) && nsCRT::strcasecmp("subject", headerInfo->name) &&
          nsCRT::strcasecmp("organization", headerInfo->name) && nsCRT::strcasecmp("user-agent", headerInfo->name) &&
          nsCRT::strcasecmp("content-base", headerInfo->name) && 
          nsCRT::strcasecmp("date", headerInfo->name) && nsCRT::strcasecmp("x-mailer", headerInfo->name))
            continue;
    }

    if (!nsCRT::strcasecmp("Date", headerInfo->name) && !displayOriginalDate) 
    {
      GenerateDateString(headerValue, convertedDateString); 
      headerValueArray.AppendCString(convertedDateString);
    }
    else // append the header value as is
      headerValueArray.AppendCString(nsCString(headerValue));

    // XXX: TODO If nsCStringArray were converted over to take nsACStrings instead of nsCStrings, we can just 
    // wrap these strings with a nsDependentCString which would avoid making a duplicate copy of the string
    // like we are doing here....
    headerNameArray.AppendCString(nsCString(headerInfo->name));
  }

  // turn our string arrays into enumerators
  NS_NewUTF8StringEnumerator(getter_AddRefs(headerNameEnumerator), &headerNameArray);
  NS_NewUTF8StringEnumerator(getter_AddRefs(headerValueEnumerator), &headerValueArray);

  aHeaderSink->ProcessHeaders(headerNameEnumerator, headerValueEnumerator, aFromNewsgroup);
  return rv;
}

NS_IMETHODIMP nsMimeHtmlDisplayEmitter::WriteHTMLHeaders()
{
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
 
  PRBool bFromNewsgroups = PR_FALSE;
  for (PRInt32 j=0; j < mHeaderArray->Count(); j++)
  {
    headerInfoType *headerInfo = (headerInfoType *)mHeaderArray->ElementAt(j);
    if (!(headerInfo && headerInfo->name && *headerInfo->name))
      continue;

    if (!nsCRT::strcasecmp("Newsgroups", headerInfo->name))
    {
      bFromNewsgroups = PR_TRUE;
	  break;
    }
  }

  // try to get a header sink if there is one....
  nsCOMPtr<nsIMsgHeaderSink> headerSink; 
  nsresult rv = GetHeaderSink(getter_AddRefs(headerSink));

  if (headerSink)
  {
  PRInt32 viewMode = 0;
  nsCOMPtr<nsIPrefBranch> pPrefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (pPrefBranch)
    rv = pPrefBranch->GetIntPref("mail.show_headers", &viewMode);

    rv = BroadcastHeaders(headerSink, viewMode, bFromNewsgroups);
  } // if header Sink

  return NS_OK;
}

nsresult nsMimeHtmlDisplayEmitter::GenerateDateString(const char * dateString, nsACString &formattedDate)
{
  nsAutoString formattedDateString;
  nsresult rv = NS_OK;

  if (!mDateFormater) {
    mDateFormater = do_CreateInstance(kDateTimeFormatCID, &rv);
    if (NS_FAILED(rv))
      return rv;
  }

  PRTime messageTime;
  PR_ParseTimeString(dateString, PR_FALSE, &messageTime);

  PRTime currentTime = PR_Now();
  PRExplodedTime explodedCurrentTime;
  PR_ExplodeTime(currentTime, PR_LocalTimeParameters, &explodedCurrentTime);
  PRExplodedTime explodedMsgTime;
  PR_ExplodeTime(messageTime, PR_LocalTimeParameters, &explodedMsgTime);

  // if the message is from today, don't show the date, only the time. (i.e. 3:15 pm)
  // if the message is from the last week, show the day of the week.   (i.e. Mon 3:15 pm)
  // in all other cases, show the full date (03/19/01 3:15 pm)
  nsDateFormatSelector dateFormat = kDateFormatShort;
  if (explodedCurrentTime.tm_year == explodedMsgTime.tm_year &&
      explodedCurrentTime.tm_month == explodedMsgTime.tm_month &&
      explodedCurrentTime.tm_mday == explodedMsgTime.tm_mday)
  {
    // same day...
    dateFormat = kDateFormatNone;
  } 
  // the following chunk of code causes us to show a day instead of a number if the message was received
  // within the last 7 days. i.e. Mon 5:10pm. We need to add a preference so folks to can enable this behavior
  // if they want it. 
/*
  else if (LL_CMP(currentTime, >, dateOfMsg))
  {
    PRInt64 microSecondsPerSecond, secondsInDays, microSecondsInDays;
	  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
    LL_UI2L(secondsInDays, 60 * 60 * 24 * 7); // how many seconds in 7 days.....
	  LL_MUL(microSecondsInDays, secondsInDays, microSecondsPerSecond); // turn that into microseconds

    PRInt64 diff;
    LL_SUB(diff, currentTime, dateOfMsg);
    if (LL_CMP(diff, <=, microSecondsInDays)) // within the same week 
      dateFormat = kDateFormatWeekday;
  }
*/

  if (NS_SUCCEEDED(rv)) 
    rv = mDateFormater->FormatPRTime(nsnull /* nsILocale* locale */,
                                      dateFormat,
                                      kTimeFormatNoSeconds,
                                      messageTime,
                                      formattedDateString);

  if (NS_SUCCEEDED(rv))
    CopyUTF16toUTF8(formattedDateString, formattedDate);

  return rv;
}

nsresult
nsMimeHtmlDisplayEmitter::EndHeader()
{
  if (mDocHeader && (mFormat != nsMimeOutput::nsMimeMessageFilterSniffer))
  {
    UtilityWriteCRLF("<html>");
    UtilityWriteCRLF("<head>");

    const char * val = GetHeaderValue(HEADER_SUBJECT); // do not free this value
    if (val)
    {
      char * subject = nsEscapeHTML(val);
      if (subject)
      {
        PRInt32 bufLen = strlen(subject) + 16;
        char *buf = new char[bufLen];
        PR_snprintf(buf, bufLen, "<title>%s</title>", subject);
        UtilityWriteCRLF(buf);
        delete [] buf;
        nsMemory::Free(subject);
      }
    }

    // mscott --> we should refer to the style sheet used in msg display...this one is wrong i think.
    // Stylesheet info!
    UtilityWriteCRLF("<link rel=\"important stylesheet\" href=\"chrome://messenger/skin/messageBody.css\">");

    UtilityWriteCRLF("</head>");
    UtilityWriteCRLF("<body>");
  }

  WriteHTMLHeaders();
  return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::StartAttachment(const char *name,
                                          const char *contentType,
                                          const char *url,
                                          PRBool aNotDownloaded)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgHeaderSink> headerSink; 
  rv = GetHeaderSink(getter_AddRefs(headerSink));
  
  if (NS_SUCCEEDED(rv) && headerSink)
  {    
    char * escapedUrl = nsEscape(url, url_Path);
    nsXPIDLCString uriString;

    nsCOMPtr<nsIMsgMessageUrl> msgurl (do_QueryInterface(mURL, &rv));
    if (NS_SUCCEEDED(rv))
    {
      // HACK: news urls require us to use the originalSpec. Everyone
      // else uses GetURI to get the RDF resource which describes the message.
      nsCOMPtr<nsINntpUrl> nntpUrl (do_QueryInterface(mURL, &rv));
      if (NS_SUCCEEDED(rv) && nntpUrl)
        rv = msgurl->GetOriginalSpec(getter_Copies(uriString));
      else
        rv = msgurl->GetUri(getter_Copies(uriString));
    }

    // we need to convert the attachment name from UTF-8 to unicode before
    // we emit it...
    nsXPIDLString unicodeHeaderValue;

    rv = NS_ERROR_FAILURE;  // use failure to mean that we couldn't decode
    if (mUnicodeConverter)
      rv = mUnicodeConverter->DecodeMimeHeader(name,
                                             getter_Copies(unicodeHeaderValue));

    if (NS_FAILED(rv))
    {
      CopyUTF8toUTF16(name, unicodeHeaderValue);

      // but it's not really a failure if we didn't have a converter
      // in the first place
      if ( !mUnicodeConverter )
        rv = NS_OK;
    }

    headerSink->HandleAttachment(contentType, url /* was escapedUrl */,
                                 unicodeHeaderValue, uriString,
                                 aNotDownloaded);

    nsCRT::free(escapedUrl);
    mSkipAttachment = PR_TRUE;
  }
  else if (mFormat == nsMimeOutput::nsMimeMessagePrintOutput)
  {
    // then we need to deal with the attachments in the body by inserting
    // them into a table..
    rv = StartAttachmentInBody(name, contentType, url);
  }
  else
  {
    // If we don't need or cannot broadcast attachment info, just ignore it
    mSkipAttachment = PR_TRUE;
    rv = NS_OK;
  }

  return rv;
}

// Attachment handling routines
// Ok, we are changing the way we handle these now...It used to be that we output 
// HTML to make a clickable link, etc... but now, this should just be informational
// and only show up during printing
// XXX should they also show up during quoting?
nsresult
nsMimeHtmlDisplayEmitter::StartAttachmentInBody(const char *name, const char *contentType, const char *url)
{
  mSkipAttachment = PR_FALSE;

  if ( (contentType) &&
       ((!strcmp(contentType, APPLICATION_XPKCS7_MIME)) ||
        (!strcmp(contentType, APPLICATION_XPKCS7_SIGNATURE)) ||
        (!strcmp(contentType, TEXT_VCARD)))
     ) {
     mSkipAttachment = PR_TRUE;
     return NS_OK;
  }

  if (!mFirst)
    UtilityWrite("<hr width=\"90%\" size=4>");

  mFirst = PR_FALSE;

  UtilityWrite("<center>");
  UtilityWrite("<table border>");
  UtilityWrite("<tr>");
  UtilityWrite("<td>");

  UtilityWrite("<div align=right class=\"headerdisplayname\" style=\"display:inline;\">");

  UtilityWrite(name);

  UtilityWrite("</div>");

  UtilityWrite("</td>");
  UtilityWrite("<td>");
  UtilityWrite("<table border=0>");
  return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::AddAttachmentField(const char *field, const char *value)
{
  if (mSkipAttachment || BroadCastHeadersAndAttachments())
    return NS_OK;

  // Don't let bad things happen
  if ( !value || !*value )
    return NS_OK;

  // Don't output this ugly header...
  if (!strcmp(field, HEADER_X_MOZILLA_PART_URL))
    return NS_OK;

  char  *newValue = nsEscapeHTML(value);

  UtilityWrite("<tr>");

  UtilityWrite("<td>");
  UtilityWrite("<div align=right class=\"headerdisplayname\" style=\"display:inline;\">");

  UtilityWrite(field);
  UtilityWrite(":");
  UtilityWrite("</div>");
  UtilityWrite("</td>");
  UtilityWrite("<td>");

  UtilityWrite(newValue);

  UtilityWrite("</td>");
  UtilityWrite("</tr>");

  PR_Free(newValue);
  return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::EndAttachment()
{
  if (mSkipAttachment)
    return NS_OK;

  mSkipAttachment = PR_FALSE; // reset it for next attachment round

  if (BroadCastHeadersAndAttachments())
    return NS_OK;

  if (mFormat == nsMimeOutput::nsMimeMessagePrintOutput) {
    UtilityWrite("</table>");
    UtilityWrite("</td>");
    UtilityWrite("</tr>");

    UtilityWrite("</table>");
    UtilityWrite("</center>");
    UtilityWrite("<br>");
  }
  return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::EndAllAttachments()
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgHeaderSink> headerSink; 
  rv = GetHeaderSink(getter_AddRefs(headerSink));
  if (headerSink)
	  headerSink->OnEndAllAttachments();
  return rv;
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
  if (mFormat != nsMimeOutput::nsMimeMessageFilterSniffer)
  {
  UtilityWriteCRLF("</body>");
  UtilityWriteCRLF("</html>");
  }
  nsCOMPtr<nsIMsgHeaderSink> headerSink; 
  nsresult rv = GetHeaderSink(getter_AddRefs(headerSink));
  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl (do_QueryInterface(mURL, &rv));
  if (headerSink)
    headerSink->OnEndMsgHeaders(mailnewsUrl);

  return NS_OK;
}


