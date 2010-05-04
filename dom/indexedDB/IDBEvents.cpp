/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
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

#include "IDBEvents.h"

#include "nsIPrivateDOMEvent.h"

#include "nsDOMClassInfo.h"
#include "nsThreadUtils.h"

#include "IDBDatabaseError.h"

USING_INDEXEDDB_NAMESPACE

namespace {

template<class Class>
inline
nsIDOMEvent*
idomevent_cast(Class* aClassPtr)
{
  return static_cast<nsIDOMEvent*>(static_cast<nsDOMEvent*>(aClassPtr));
}

template<class Class>
inline
nsIDOMEvent*
idomevent_cast(nsRefPtr<Class> aClassAutoPtr)
{
  return idomevent_cast(aClassAutoPtr.get());
}

class EventFiringRunnable : public nsRunnable
{
public:
  EventFiringRunnable(nsIDOMEventTarget* aTarget,
                      nsIDOMEvent* aEvent)
  : mTarget(aTarget), mEvent(aEvent)
  { }

  NS_IMETHOD Run() {
    PRBool dummy;
    return mTarget->DispatchEvent(mEvent, &dummy);
  }

private:
  nsCOMPtr<nsIDOMEventTarget> mTarget;
  nsCOMPtr<nsIDOMEvent> mEvent;
};

} // anonymous namespace

// static
already_AddRefed<nsIDOMEvent>
IDBErrorEvent::Create(PRUint16 aCode)
{
  nsRefPtr<IDBErrorEvent> event(new IDBErrorEvent());
  nsresult rv = event->Init();
  NS_ENSURE_SUCCESS(rv, nsnull);

  event->mError = new IDBDatabaseError(aCode);

  nsCOMPtr<nsIDOMEvent> result(idomevent_cast(event));
  return result.forget();
}

// static
already_AddRefed<nsIRunnable>
IDBErrorEvent::CreateRunnable(nsIDOMEventTarget* aTarget,
                              PRUint16 aCode)
{
  nsCOMPtr<nsIDOMEvent> event(IDBErrorEvent::Create(aCode));
  NS_ENSURE_TRUE(event, nsnull);

  nsCOMPtr<nsIRunnable> runnable(new EventFiringRunnable(aTarget, event));
  return runnable.forget();
}

nsresult
IDBErrorEvent::Init()
{
  nsresult rv = InitEvent(NS_LITERAL_STRING(ERROR_EVT_STR), PR_FALSE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_ADDREF_INHERITED(IDBErrorEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(IDBErrorEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(IDBErrorEvent)
  NS_INTERFACE_MAP_ENTRY(nsIIDBErrorEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBErrorEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

DOMCI_DATA(IDBErrorEvent, IDBErrorEvent)

NS_IMETHODIMP
IDBErrorEvent::GetError(nsIIDBDatabaseError** aError)
{
  nsCOMPtr<nsIIDBDatabaseError> error(mError);
  error.forget(aError);
  return NS_OK;
}

// static
already_AddRefed<nsIDOMEvent>
IDBSuccessEvent::Create(nsIVariant* aResult)
{
  nsRefPtr<IDBSuccessEvent> event(new IDBSuccessEvent());
  nsresult rv = event->Init();
  NS_ENSURE_SUCCESS(rv, nsnull);

  event->mResult = aResult;

  nsCOMPtr<nsIDOMEvent> result(idomevent_cast(event));
  return result.forget();
}

// static
already_AddRefed<nsIRunnable>
IDBSuccessEvent::CreateRunnable(nsIDOMEventTarget* aTarget,
                                nsIVariant* aResult)
{
  nsCOMPtr<nsIDOMEvent> event(IDBSuccessEvent::Create(aResult));
  NS_ENSURE_TRUE(event, nsnull);

  nsCOMPtr<nsIRunnable> runnable(new EventFiringRunnable(aTarget, event));
  return runnable.forget();
}

nsresult
IDBSuccessEvent::Init()
{
  nsresult rv = InitEvent(NS_LITERAL_STRING(SUCCESS_EVT_STR), PR_FALSE,
                          PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_ADDREF_INHERITED(IDBSuccessEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(IDBSuccessEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(IDBSuccessEvent)
  NS_INTERFACE_MAP_ENTRY(nsIIDBSuccessEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBSuccessEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

DOMCI_DATA(IDBSuccessEvent, IDBSuccessEvent)

NS_IMETHODIMP
IDBSuccessEvent::GetResult(nsIVariant** aResult)
{
  nsCOMPtr<nsIVariant> result(mResult);
  result.forget(aResult);
  return NS_OK;
}
