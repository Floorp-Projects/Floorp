/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIURL.h"
#include "nsIEventQueueService.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIStreamConverter.h"
#include "nsIChannel.h"
#include "nsIMimeStreamConverter.h"
#include "nsFileStream.h"
#include "nsFileSpec.h"
#include "nsMimeTypes.h"
#include "nsICharsetConverterManager.h"
#include "prprf.h"
#include "nsMsgCreate.h" 
#include "nsMsgCompUtils.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#include "nsMsgDeliveryListener.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgIdentity.h"
#include "nsIEnumerator.h"
#include "nsMsgBaseCID.h"
#include "nsMsgCopy.h"
#include "nsNetUtil.h"
#include "nsMsgMimeCID.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsMsgBaseCID.h"


//
// Implementation...
//
nsMsgDraft::nsMsgDraft()
{
  mURI = nsnull;
  mMessageService = nsnull;
  mOutType = nsMimeOutput::nsMimeMessageDraftOrTemplate;
  mAddInlineHeaders = PR_FALSE;
}

nsMsgDraft::~nsMsgDraft()
{
    mMessageService = nsnull;

  PR_FREEIF(mURI);
}

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS1(nsMsgDraft, nsIMsgDraft)

nsresult    
nsMsgDraft::ProcessDraftOrTemplateOperation(const char *msgURI, nsMimeOutputType aOutType, 
                                            nsIMsgIdentity * identity, const char *originalMsgURI, nsIMsgWindow *aMsgWindow)
{
  nsresult  rv;

  mOutType = aOutType;

  if (!msgURI)
    return NS_ERROR_INVALID_ARG;

  mURI = nsCRT::strdup(msgURI);

  if (!mURI)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = GetMessageServiceFromURI(mURI, getter_AddRefs(mMessageService));
  if (NS_FAILED(rv) && !mMessageService)
  {
    return rv;
  }

  NS_ADDREF(this);

  // Now, we can create a mime parser (nsIStreamConverter)!
  nsCOMPtr<nsIStreamConverter> mimeParser;
  mimeParser = do_CreateInstance(NS_MAILNEWS_MIME_STREAM_CONVERTER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
  {
    Release();
    mMessageService = nsnull;
#ifdef NS_DEBUG
    printf("Failed to create MIME stream converter...\n");
#endif
    return rv;
  }
  
  // Set us as the output stream for HTML data from libmime...
  nsCOMPtr<nsIMimeStreamConverter> mimeConverter = do_QueryInterface(mimeParser);
  if (mimeConverter)
  {
    mimeConverter->SetMimeOutputType(mOutType);  // Set the type of output for libmime
    mimeConverter->SetForwardInline(mAddInlineHeaders);
    mimeConverter->SetIdentity(identity);
    mimeConverter->SetOriginalMsgURI(originalMsgURI);
  }

  nsCOMPtr<nsIStreamListener> convertedListener = do_QueryInterface(mimeParser);
  if (!convertedListener)
  {
    Release();
    mMessageService = nsnull;
#ifdef NS_DEBUG
    printf("Unable to get the nsIStreamListener interface from libmime\n");
#endif
    return NS_ERROR_UNEXPECTED;
  }  

  nsCOMPtr<nsIURI> aURL;
  rv = mMessageService->GetUrlForUri(mURI, getter_AddRefs(aURL), aMsgWindow);
  if (aURL)
    aURL->SetSpec(nsDependentCString(mURI));

  // if we are forwarding a message and that message used a charset over ride
  // then use that over ride charset instead of the charset specified in the message
  nsXPIDLCString mailCharset;
  if (aMsgWindow)
  {
    PRBool charsetOverride;
    if (NS_SUCCEEDED(aMsgWindow->GetCharsetOverride(&charsetOverride)) && charsetOverride)
    {
      if (NS_SUCCEEDED(aMsgWindow->GetMailCharacterSet(getter_Copies(mailCharset))))
      {
        nsCOMPtr<nsIMsgI18NUrl> i18nUrl(do_QueryInterface(aURL));
        if (i18nUrl)
          (void) i18nUrl->SetCharsetOverRide(mailCharset.get());
      }
    }
  }

  nsCOMPtr<nsIChannel> dummyChannel;
  rv = NS_NewInputStreamChannel(getter_AddRefs(dummyChannel), aURL, nsnull);
  if (NS_FAILED(mimeParser->AsyncConvertData(nsnull, nsnull, nsnull, dummyChannel)))
  {
    Release();
    mMessageService = nsnull;
#ifdef NS_DEBUG
    printf("Unable to set the output stream for the mime parser...\ncould be failure to create internal libmime data\n");
#endif

    return NS_ERROR_UNEXPECTED;
  }

  // Now, just plug the two together and get the hell out of the way!
  rv = mMessageService->DisplayMessage(mURI, convertedListener, aMsgWindow, nsnull, mailCharset, nsnull);

  mMessageService = nsnull;
  Release();

	if (NS_FAILED(rv))
    return rv;    
  else
    return NS_OK;
}

nsresult
nsMsgDraft::OpenDraftMsg(const char *msgURI, const char *originalMsgURI, nsIMsgIdentity * identity,
                         PRBool addInlineHeaders, nsIMsgWindow *aMsgWindow)
{
  // We should really never get here, but if we do, just return 
  // with an error
  if (!msgURI)
    return NS_ERROR_FAILURE;
  
  mAddInlineHeaders = addInlineHeaders;
  return ProcessDraftOrTemplateOperation(msgURI, nsMimeOutput::nsMimeMessageDraftOrTemplate, 
                                         identity, originalMsgURI, aMsgWindow);
}

nsresult
nsMsgDraft::OpenEditorTemplate(const char *msgURI, nsIMsgIdentity * identity, nsIMsgWindow *aMsgWindow)
{
  return ProcessDraftOrTemplateOperation(msgURI, nsMimeOutput::nsMimeMessageEditorTemplate, 
                                         identity, nsnull, aMsgWindow);
}

