/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Johnny Stenback <jst@mozilla.jstenback.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsLSParser.h"
#include "nsLSEvent.h"
#include "nsAutoPtr.h"
#include "nsXMLHttpRequest.h"
#include "nsIDOMLSParserFilter.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"

#include "nsIDOMClassInfo.h"


// QueryInterface implementation for nsLSParser
NS_INTERFACE_MAP_BEGIN(nsLSParser)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMLSParser)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLSParser)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(LSParser)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsLSParser)
NS_IMPL_RELEASE(nsLSParser)

nsLSParser::nsLSParser()
  : mIsAsync(PR_TRUE)
{
}

nsLSParser::~nsLSParser()
{
}

NS_IMETHODIMP
nsLSParser::GetDomConfig(nsIDOMDOMConfiguration * *aDomConfig)
{
  *aDomConfig = nsnull;

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLSParser::GetFilter(nsIDOMLSParserFilter * *aFilter)
{
  *aFilter = mFilter;

  return NS_OK;
}

NS_IMETHODIMP
nsLSParser::SetFilter(nsIDOMLSParserFilter * aFilter)
{
  mFilter = aFilter;

  return NS_OK;
}

NS_IMETHODIMP
nsLSParser::GetAsync(PRBool *aAsync)
{
  *aAsync = mIsAsync;

  return NS_OK;
}

NS_IMETHODIMP
nsLSParser::GetBusy(PRBool *aBusy)
{
  PRInt32 readyState;
  nsresult rv = mXMLHttpRequest->GetReadyState(&readyState);
  NS_ENSURE_SUCCESS(rv, rv);

  *aBusy = readyState == 0 || readyState == 4;

  return NS_OK;
}

class nsLSParserEventListener : public nsIDOMEventListener
{
public:
  nsLSParserEventListener(nsLSParser *aParser)
    : mParser(aParser)
  {
  }

  virtual ~nsLSParserEventListener()
  {
  }

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMEVENTLISTENER

protected:
  nsRefPtr<nsLSParser> mParser;
};


NS_IMPL_ISUPPORTS1(nsLSParserEventListener, nsIDOMEventListener)

NS_IMETHODIMP
nsLSParserEventListener::HandleEvent(nsIDOMEvent *aEvent)
{
  mParser->FireOnLoad();

  return NS_OK;
}


NS_IMETHODIMP
nsLSParser::Parse(nsIDOMLSInput *input, nsIDOMDocument **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLSParser::ParseURI(const nsAString & uri, nsIDOMDocument **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsIDOMEventListener> listener = new nsLSParserEventListener(this);
  NS_ENSURE_TRUE(listener, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;
  rv = mXMLHttpRequest->OpenRequest(NS_LITERAL_CSTRING("GET"),
                                    NS_ConvertUTF16toUTF8(uri),
                                    mIsAsync, EmptyString(), EmptyString());
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mXMLHttpRequest));
  rv = target->AddEventListener(NS_LITERAL_STRING("load"), listener, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mXMLHttpRequest->Send(nsnull);

  if (!mIsAsync && NS_SUCCEEDED(rv)) {
    rv = mXMLHttpRequest->GetResponseXML(_retval);
  }

  return rv;
}

NS_IMETHODIMP
nsLSParser::ParseWithContext(nsIDOMLSInput *input, nsIDOMNode *contextArg,
                             PRUint16 action, nsIDOMNode **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLSParser::Abort()
{
  return mXMLHttpRequest->Abort();
}

NS_IMETHODIMP
nsLSParser::AddEventListener(const nsAString& aType,
                             nsIDOMEventListener *aListener,
                             PRBool aUseCapture)
{
  nsCOMArray<nsIDOMEventListener> *listeners;

  if (aType.EqualsLiteral("ls-load")) {
    listeners = &mLoadListeners;
  } else if (aType.EqualsLiteral("ls-progress")) {
    listeners = &mProgressListeners;
  } else {
    // Not a supported event

    return NS_OK;
  }

  if (listeners->IndexOf(aListener) < 0) {
    listeners->AppendObject(aListener);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLSParser::RemoveEventListener(const nsAString& aType,
                                nsIDOMEventListener *aListener,
                                PRBool aUseCapture)
{
  nsCOMArray<nsIDOMEventListener> *listeners;

  if (aType.EqualsLiteral("ls-load")) {
    listeners = &mLoadListeners;
  } else if (aType.EqualsLiteral("ls-progress")) {
    listeners = &mProgressListeners;
  } else {
    // Not a supported event

    return NS_OK;
  }

  listeners->RemoveObject(aListener);

  return NS_OK;
}

NS_IMETHODIMP
nsLSParser::DispatchEvent(nsIDOMEvent *aEvt, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsLSParser::FireOnLoad()
{
  for (PRInt32 i = 0; i < mLoadListeners.Count(); ++i) {
    nsRefPtr<nsLSParserLoadEvent> event = new nsLSParserLoadEvent(this);

    if (event) {
      mLoadListeners.ObjectAt(i)->HandleEvent(NS_STATIC_CAST(nsLSEvent *,
                                                             event));
    }
  }
}

nsresult
nsLSParser::Init()
{
  mXMLHttpRequest = new nsXMLHttpRequest();

  return mXMLHttpRequest ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
