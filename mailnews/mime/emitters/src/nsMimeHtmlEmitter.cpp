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
 *    Henrik Gemal <gemal@gemal.dk>
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
#include "prtime.h"
#include "nsReadableUtils.h"

// hack: include this to fix opening news attachments.
#include "nsINntpUrl.h"

#include "nsIMimeConverter.h"
#include "nsMsgMimeCID.h"
#include "nsDateTimeFormatCID.h"

static NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);
static NS_DEFINE_CID(kDateTimeFormatCID,    NS_DATETIMEFORMAT_CID);

/*
 * nsMimeHtmlEmitter definitions....
 */
nsMimeHtmlDisplayEmitter::nsMimeHtmlDisplayEmitter() : nsMimeBaseEmitter()
{
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
  for (PRInt32 j=0; j<mHeaderArray->Count(); j++)
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
    headerSink->OnStartHeaders();

  // We are going to iterate over all the known headers,
  // and broadcast them to the header sink. However, we need to 
  // convert our UTF-8 header values into unicode before 
  // broadcasting them....
  nsXPIDLString unicodeHeaderValue;

  for (PRInt32 i=0; i<mHeaderArray->Count(); i++)
  {
    headerInfoType *headerInfo = (headerInfoType *)mHeaderArray->ElementAt(i);
    if ( (!headerInfo) || (!headerInfo->name) || (!(*headerInfo->name)) ||
      (!headerInfo->value) || (!(*headerInfo->value)))
      continue;

    if (headerSink)
    {
      char buffer[128];
      const char * headerValue = headerInfo->value;

      if (nsCRT::strcasecmp("Date", headerInfo->name) == 0)
      {
        nsXPIDLString formattedDate;
        nsresult rv = GenerateDateString(headerInfo->value, getter_Copies(formattedDate));
        if (NS_SUCCEEDED(rv))
          headerSink->HandleHeader(headerInfo->name, formattedDate, bFromNewsgroups);
        // let's try some fancy date formatting...
        PRExplodedTime explode;
        PRTime dateTime;
        PR_ParseTimeString(headerInfo->value, PR_FALSE, &dateTime);

        PR_ExplodeTime( dateTime, PR_LocalTimeParameters, &explode);
        PR_FormatTime(buffer, sizeof(buffer), "%m/%d/%Y %I:%M %p", &explode);
        headerValue = buffer;
      }
      else
      {
        // Convert UTF-8 to UCS2
        unicodeHeaderValue.Adopt(NS_ConvertUTF8toUCS2(headerValue).ToNewUnicode());

        if (NS_SUCCEEDED(rv))
          headerSink->HandleHeader(headerInfo->name, unicodeHeaderValue, bFromNewsgroups);
      }
    }
  }

  if (headerSink)
    headerSink->OnEndHeaders();
  return NS_OK;
}

nsresult nsMimeHtmlDisplayEmitter::GenerateDateString(const char * dateString, PRUnichar ** aDateString)
{
  nsAutoString formattedDateString;
  nsresult rv = NS_OK;

  if (!mDateFormater)
    mDateFormater = do_CreateInstance(kDateTimeFormatCID);

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
    *aDateString = formattedDateString.ToNewUnicode();

  return rv;
}

nsresult
nsMimeHtmlDisplayEmitter::EndHeader()
{
  if (mDocHeader)
  {
    UtilityWriteCRLF("<html>");
    UtilityWriteCRLF("<head>");

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
nsMimeHtmlDisplayEmitter::StartAttachment(const char *name, const char *contentType, const char *url,
                                          PRBool aNotDownloaded)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMsgHeaderSink> headerSink; 
  rv = GetHeaderSink(getter_AddRefs(headerSink));
  
  if (NS_FAILED(rv) || !headerSink)
    return NS_OK;   //If we don't need or cannot broadcast attachment info, just ignore it and return NS_OK
    
  char * escapedUrl = nsEscape(url, url_Path);
  nsXPIDLCString uriString;

  nsCOMPtr<nsIMsgMessageUrl> msgurl (do_QueryInterface(mURL, &rv));
  if (NS_SUCCEEDED(rv))
  {
    // HACK: news urls require us to use the originalSpec. everyone else uses GetURI
    // to get the RDF resource which describes the message.
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
    unicodeHeaderValue.Adopt(NS_ConvertUTF8toUCS2(name).ToNewUnicode());

      // but it's not really a failure if we didn't have a converter in the first place
    if ( !mUnicodeConverter )
      rv = NS_OK;
  }

  headerSink->HandleAttachment(contentType, url /* was escapedUrl */, unicodeHeaderValue, uriString, aNotDownloaded);

  nsCRT::free(escapedUrl);

  return rv;
}

nsresult
nsMimeHtmlDisplayEmitter::AddAttachmentField(const char *field, const char *value)
{
  return NS_OK;
}

nsresult
nsMimeHtmlDisplayEmitter::EndAttachment()
{
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
  UtilityWriteCRLF("</body>");
  UtilityWriteCRLF("</html>");
  return NS_OK;
}


