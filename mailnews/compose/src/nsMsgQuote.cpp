/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsIURL.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIStreamConverter.h"
#include "nsIStreamConverterService.h"
#include "nsIMimeStreamConverter.h"
#include "nsMimeTypes.h"
#include "nsIPref.h"
#include "nsICharsetConverterManager.h"
#include "prprf.h"
#include "nsMsgQuote.h" 
#include "nsMsgCompUtils.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#include "nsMsgDeliveryListener.h"
#include "nsNetUtil.h"
#include "nsMsgMimeCID.h"
#include "nsMsgCompCID.h"
#include "nsMsgCompose.h"
#include "nsMsgMailNewsUrl.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kIStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kStreamConverterCID,    NS_MAILNEWS_MIME_STREAM_CONVERTER_CID);
static NS_DEFINE_CID(kMsgQuoteListenerCID, NS_MSGQUOTELISTENER_CID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);


NS_IMPL_ISUPPORTS2(nsMsgQuoteListener, nsIMimeStreamConverterListener, nsIMsgQuoteListener)

nsMsgQuoteListener::nsMsgQuoteListener() :
	mMsgQuote(nsnull)
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();
}

nsMsgQuoteListener::~nsMsgQuoteListener()
{
}

NS_IMETHODIMP nsMsgQuoteListener::SetMsgQuote(nsIMsgQuote * msgQuote)
{
	mMsgQuote = msgQuote;
  return NS_OK;
}

NS_IMETHODIMP nsMsgQuoteListener::GetMsgQuote(nsIMsgQuote ** aMsgQuote)
{
  nsresult rv = NS_OK;
  if (aMsgQuote)
  {
    *aMsgQuote = mMsgQuote;
    NS_IF_ADDREF(*aMsgQuote);
  }
  else
    rv = NS_ERROR_NULL_POINTER;

  return rv;
}

nsresult nsMsgQuoteListener::OnHeadersReady(nsIMimeHeaders * headers)
{

	printf("RECEIVE CALLBACK: OnHeadersReady\n");
	nsCOMPtr<nsIStreamListener> aStreamListener;
    if (mMsgQuote)
    mMsgQuote->GetStreamListener(getter_AddRefs(aStreamListener));
	if (aStreamListener)
	{
		QuotingOutputStreamListener * quoting;
		if (NS_SUCCEEDED(aStreamListener->QueryInterface(NS_GET_IID(QuotingOutputStreamListener), (void**)&quoting)) &&
			quoting)
		{
	  	quoting->SetMimeHeaders(headers);
			NS_RELEASE(quoting);			
		}
		else
			return NS_ERROR_FAILURE;
/* ducarroz: Impossible to compile the COMPtr version of this code !!!!
   		nsCOMPtr<QuotingOutputStreamListener> quoting (do_QueryInterface(streamListener));
  		if (quoting)
  		  	quoting->SetMimeHeaders(headers);
*/
	}
	return NS_OK;
}

//
// Implementation...
//
nsMsgQuote::nsMsgQuote()
{
	NS_INIT_REFCNT();
  mQuoteHeaders = PR_FALSE;
  mQuoteListener = nsnull;
}

nsMsgQuote::~nsMsgQuote()
{
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgQuote, NS_GET_IID(nsIMsgQuote));

NS_IMETHODIMP nsMsgQuote::GetStreamListener(nsIStreamListener ** aStreamListener)
{
  nsresult rv = NS_OK;
  if (aStreamListener)
  {
    *aStreamListener = mStreamListener;
    NS_IF_ADDREF(*aStreamListener);
  }
  else
    rv = NS_ERROR_NULL_POINTER;

  return rv;
}

nsresult
nsMsgQuote::QuoteMessage(const PRUnichar *msgURI, PRBool quoteHeaders, nsIStreamListener * aQuoteMsgStreamListener,
                         const PRUnichar * aMsgCharSet)
{
  nsresult  rv;

  if (!msgURI)
    return NS_ERROR_INVALID_ARG;
  nsCAutoString aMsgUri; aMsgUri.AssignWithConversion(msgURI);

  mQuoteHeaders = quoteHeaders;
  mStreamListener = aQuoteMsgStreamListener;

  // first, convert the rdf msg uri into a url that represents the message...
  nsIMsgMessageService * msgService = nsnull;
  rv = GetMessageServiceFromURI(aMsgUri, &msgService);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIURI> aURL;
  rv = msgService->GetUrlForUri(aMsgUri, getter_AddRefs(aURL), nsnull);
  if (NS_FAILED(rv)) return rv;

  // now we want to append some quote specific information to the 
  // end of the url spec. 
  nsXPIDLCString urlSpec;
  aURL->GetSpec(getter_Copies(urlSpec));
  nsCAutoString modifiedUrlSpec(urlSpec);

  PRBool bAutoQuote = PR_TRUE;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
  if (NS_SUCCEEDED(rv))
    prefs->GetBoolPref("mail.auto_quote", &bAutoQuote);

  if (! bAutoQuote) /* We don't need to quote the message body but we still need to extract the headers */
    modifiedUrlSpec += "?header=only";
  else if (quoteHeaders)
      modifiedUrlSpec += "?header=quote";
  else
      modifiedUrlSpec += "?header=quotebody";

  aURL->SetSpec((const char *) modifiedUrlSpec);

  // if we were given a non empty charset, then use it
  if (aMsgCharSet && *aMsgCharSet)
  {
    nsCOMPtr<nsIMsgI18NUrl> i18nUrl (do_QueryInterface(aURL));
    if (i18nUrl)
      i18nUrl->SetCharsetOverRide(aMsgCharSet);
  }

  rv = nsComponentManager::CreateInstance(kMsgQuoteListenerCID, nsnull, NS_GET_IID(nsIMsgQuoteListener), getter_AddRefs(mQuoteListener));
  if (NS_FAILED(rv)) return rv;
  mQuoteListener->SetMsgQuote(this);

  nsCOMPtr<nsISupports> quoteSupport = do_QueryInterface(this);

  // now we want to create a necko channel for this url and we want to open it
  mQuoteChannel = null_nsCOMPtr();
  NS_WITH_SERVICE(nsIIOService, netService, kIOServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  rv = netService->NewChannelFromURI(aURL, getter_AddRefs(mQuoteChannel));
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsISupports> ctxt = do_QueryInterface(aURL);

  NS_WITH_SERVICE(nsIStreamConverterService, streamConverterService, kIStreamConverterServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIStreamListener> convertedListener;
  rv = streamConverterService->AsyncConvertData(NS_LITERAL_STRING("message/rfc822").get(),
                                                NS_LITERAL_STRING("text/xul").get(),
                                                mStreamListener,
                                                quoteSupport,
                                                getter_AddRefs(convertedListener));
  if (NS_FAILED(rv)) return rv;

  //  now try to open the channel passing in our display consumer as the listener 
  rv = mQuoteChannel->AsyncRead(convertedListener, ctxt);

  ReleaseMessageServiceFromURI(aMsgUri, msgService);
  return rv;
}

NS_IMETHODIMP
nsMsgQuote::GetQuoteListener(nsIMimeStreamConverterListener** aQuoteListener)
{
    if (!aQuoteListener || !mQuoteListener)
        return NS_ERROR_NULL_POINTER;
    *aQuoteListener = mQuoteListener;
    NS_ADDREF(*aQuoteListener);
    return NS_OK;
}

NS_IMETHODIMP
nsMsgQuote::GetQuoteChannel(nsIChannel** aQuoteChannel)
{
    if (!aQuoteChannel || !mQuoteChannel)
        return NS_ERROR_NULL_POINTER;
    *aQuoteChannel = mQuoteChannel;
    NS_ADDREF(*aQuoteChannel);
    return NS_OK;
}
