/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIURL.h"
#include "nsIEventQueueService.h"
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
#include "nsIIOService.h"
#include "nsMsgMimeCID.h"
#include "nsMsgCompose.h"
#include "nsMsgMailNewsUrl.h"
#include "nsIImapUrl.h"
#include "nsIMailboxUrl.h"
#include "nsINntpUrl.h"
#include "nsMsgNewsCID.h"
#include "nsMsgLocalCID.h"
#include "nsMsgBaseCID.h"
#include "nsMsgImapCID.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kIStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kImapUrlCID, NS_IMAPURL_CID);
static NS_DEFINE_CID(kCMailboxUrl, NS_MAILBOXURL_CID);
static NS_DEFINE_CID(kCNntpUrlCID, NS_NNTPURL_CID);
static NS_DEFINE_CID(kStreamConverterCID,    NS_MAILNEWS_MIME_STREAM_CONVERTER_CID);


NS_IMPL_ISUPPORTS(nsMsgQuoteListener, nsCOMTypeInfo<nsIMimeStreamConverterListener>::GetIID())

nsMsgQuoteListener::nsMsgQuoteListener() :
	mMsgQuote(nsnull)
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();
}

nsMsgQuoteListener::~nsMsgQuoteListener()
{
}

void nsMsgQuoteListener::SetMsgQuote(nsMsgQuote * msgQuote)
{
	mMsgQuote = msgQuote;
}

nsresult nsMsgQuoteListener::OnHeadersReady(nsIMimeHeaders * headers)
{

	printf("RECEIVE CALLBACK: OnHeadersReady\n");
	
	if (mMsgQuote && mMsgQuote->mStreamListener)
	{
		QuotingOutputStreamListener * quoting;
		if (NS_SUCCEEDED(mMsgQuote->mStreamListener->QueryInterface(QuotingOutputStreamListener::GetIID(), (void**)&quoting)) &&
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

  mURI = nsnull;
  mMessageService = nsnull;
  mQuoteHeaders = PR_FALSE;
  mQuoteListener = nsnull;
}

nsMsgQuote::~nsMsgQuote()
{
  if (mMessageService)
  {
    ReleaseMessageServiceFromURI(mURI, mMessageService);
    mMessageService = nsnull;
  }

  PR_FREEIF(mURI);
  NS_IF_RELEASE(mQuoteListener);
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgQuote, nsCOMTypeInfo<nsIMsgQuote>::GetIID());

/* this function will be used by the factory to generate an Message Compose Fields Object....*/
nsresult 
NS_NewMsgQuote(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMsgQuote *pQuote = new nsMsgQuote();
		if (pQuote)
			return pQuote->QueryInterface(aIID, aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}


nsresult 
nsMsgQuote::CreateStartupUrl(char *uri, nsIURI** aUrl)
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (!uri || !*uri || !aUrl) return rv;
    *aUrl = nsnull;
    if (PL_strncasecmp(uri, "imap", 4) == 0)
    {
        nsCOMPtr<nsIImapUrl> imapUrl;
        rv = nsComponentManager::CreateInstance(kImapUrlCID, nsnull,
                                                nsIImapUrl::GetIID(),
                                                getter_AddRefs(imapUrl));
        if (NS_SUCCEEDED(rv) && imapUrl)
            rv = imapUrl->QueryInterface(nsCOMTypeInfo<nsIURI>::GetIID(),
                                         (void**) aUrl);
    }
    else if (PL_strncasecmp(uri, "mailbox", 7) == 0)
    {
        nsCOMPtr<nsIMailboxUrl> mailboxUrl;
        rv = nsComponentManager::CreateInstance(kCMailboxUrl, nsnull,
                                                nsIMailboxUrl::GetIID(),
                                                getter_AddRefs(mailboxUrl));
        if (NS_SUCCEEDED(rv) && mailboxUrl)
            rv = mailboxUrl->QueryInterface(nsCOMTypeInfo<nsIURI>::GetIID(),
                                            (void**) aUrl);
    }
    else if (PL_strncasecmp(uri, "news", 4) == 0)
    {
        nsCOMPtr<nsINntpUrl> nntpUrl;
        rv = nsComponentManager::CreateInstance(kCNntpUrlCID, nsnull,
                                                nsINntpUrl::GetIID(),
                                                getter_AddRefs(nntpUrl));
        if (NS_SUCCEEDED(rv) && nntpUrl)
            rv = nntpUrl->QueryInterface(nsCOMTypeInfo<nsIURI>::GetIID(),
                                         (void**) aUrl);
    }
    if (*aUrl)
        (*aUrl)->SetSpec(uri);
    return rv;
}

nsresult
nsMsgQuote::QuoteMessage(const PRUnichar *msgURI, PRBool quoteHeaders, nsIStreamListener * aQuoteMsgStreamListener)
{
nsresult  rv;

  if (!msgURI)
    return NS_ERROR_INVALID_ARG;

  mQuoteHeaders = quoteHeaders;
  mStreamListener = aQuoteMsgStreamListener;

  nsString                convertString(msgURI);

  if (quoteHeaders)
      convertString += "?header=quote";
  else
      convertString += "?header=quotebody";

  mURI = convertString.ToNewCString();

  if (!mURI)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = GetMessageServiceFromURI(mURI, &mMessageService);
  if (NS_FAILED(rv) && !mMessageService)
  {
    return rv;
  }

  // NS_ADDREF(this);
  AddRef();

  NS_WITH_SERVICE(nsIStreamConverterService, streamConverterService, 
                  kIStreamConverterServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsAutoString from, to;
  from = "message/rfc822";
  to = "text/xul";

  nsCOMPtr<nsIURI> aURL;
  rv = CreateStartupUrl(mURI, getter_AddRefs(aURL));

  mQuoteChannel = null_nsCOMPtr();
  NS_WITH_SERVICE(nsIIOService, netService, kIOServiceCID, &rv);
  rv = netService->NewInputStreamChannel(aURL, nsnull, nsnull,
                                         getter_AddRefs(mQuoteChannel));

  NS_ASSERTION(!mQuoteListener, "Oops quote listener exists\n");

  if (mQuoteListener)
      delete mQuoteListener;
      
  mQuoteListener = new nsMsgQuoteListener();
  if (mQuoteListener)
  {
      NS_ADDREF(mQuoteListener);
      mQuoteListener->SetMsgQuote(this);
  }
  nsCOMPtr<nsISupports> quoteSupport;
  rv = QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(),
                      getter_AddRefs(quoteSupport));
  
  nsCOMPtr<nsIStreamListener> convertedListener;
  rv = streamConverterService->AsyncConvertData(from.GetUnicode(),
                                                to.GetUnicode(),
                                                mStreamListener,
                                                quoteSupport,
                                      getter_AddRefs(convertedListener));
  if (NS_SUCCEEDED(rv))
      rv = mMessageService->DisplayMessage(mURI, convertedListener, nsnull,
                                           nsnull);
  ReleaseMessageServiceFromURI(mURI, mMessageService);
  mMessageService = nsnull;
  Release();

	if (NS_FAILED(rv))
    return rv;    
  else
    return NS_OK;
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
