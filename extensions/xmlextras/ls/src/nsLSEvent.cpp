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

#include "nsLSEvent.h"
#include "nsAutoPtr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"

#include "nsIDOMClassInfo.h"

#include "prtime.h"


// QueryInterface implementation for nsLSEvent
NS_IMPL_ISUPPORTS1(nsLSEvent, nsIDOMEvent)

nsLSEvent::nsLSEvent()
  : mTime(PR_Now())
{
}

nsLSEvent::~nsLSEvent()
{
}

NS_IMETHODIMP
nsLSEvent::GetType(nsAString & aType)
{
  return GetTypeInternal(aType);
}

NS_IMETHODIMP
nsLSEvent::GetTarget(nsIDOMEventTarget * *aTarget)
{
  return GetTargetInternal(aTarget);
}

NS_IMETHODIMP
nsLSEvent::GetCurrentTarget(nsIDOMEventTarget * *aCurrentTarget)
{
  return GetTargetInternal(aCurrentTarget);
}

NS_IMETHODIMP
nsLSEvent::GetEventPhase(PRUint16 *aEventPhase)
{
  *aEventPhase = AT_TARGET;

  return NS_OK;
}

NS_IMETHODIMP
nsLSEvent::GetBubbles(PRBool *aBubbles)
{
  *aBubbles = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsLSEvent::GetCancelable(PRBool *aCancelable)
{
  *aCancelable = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsLSEvent::GetTimeStamp(PRUint64 *aTimeStamp)
{
  // XXX: We want to return in ms, right?
  LL_DIV(*aTimeStamp, mTime, PR_MSEC_PER_SEC);

  return NS_OK;
}

NS_IMETHODIMP
nsLSEvent::StopPropagation()
{
  return NS_OK;
}

NS_IMETHODIMP
nsLSEvent::PreventDefault()
{
  return NS_OK;
}

NS_IMETHODIMP
nsLSEvent::InitEvent(const nsAString & eventTypeArg, PRBool canBubbleArg,
                     PRBool cancelableArg)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// QueryInterface implementation for nsLSParserLoadEvent
NS_INTERFACE_MAP_BEGIN(nsLSParserLoadEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsLSEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEvent, nsLSEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLSLoadEvent)
  NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(LSLoadEvent)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsLSParserLoadEvent)
NS_IMPL_RELEASE(nsLSParserLoadEvent)


nsresult
nsLSParserLoadEvent::GetTargetInternal(nsIDOMEventTarget **aTarget)
{
  NS_ADDREF(*aTarget = mParser.get());

  return NS_OK;
}

nsresult
nsLSParserLoadEvent::GetTypeInternal(nsAString& aType)
{
  aType.AssignLiteral("ls-load");

  return NS_OK;
}

NS_IMETHODIMP
nsLSParserLoadEvent::GetNewDocument(nsIDOMDocument **aNewDocument)
{
  return mParser->XMLHttpRequest()->GetResponseXML(aNewDocument);
}

NS_IMETHODIMP
nsLSParserLoadEvent::GetInput(nsIDOMLSInput **aInput)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

