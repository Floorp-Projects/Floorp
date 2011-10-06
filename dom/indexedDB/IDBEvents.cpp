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

#include "nsIIDBDatabaseException.h"
#include "nsIPrivateDOMEvent.h"

#include "jscntxt.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsDOMException.h"
#include "nsJSON.h"
#include "nsThreadUtils.h"

#include "IDBRequest.h"
#include "IDBTransaction.h"

USING_INDEXEDDB_NAMESPACE

namespace {

class EventFiringRunnable : public nsRunnable
{
public:
  EventFiringRunnable(nsIDOMEventTarget* aTarget,
                      nsIDOMEvent* aEvent)
  : mTarget(aTarget), mEvent(aEvent)
  { }

  NS_IMETHOD Run() {
    bool dummy;
    return mTarget->DispatchEvent(mEvent, &dummy);
  }

private:
  nsCOMPtr<nsIDOMEventTarget> mTarget;
  nsCOMPtr<nsIDOMEvent> mEvent;
};

} // anonymous namespace

already_AddRefed<nsDOMEvent>
mozilla::dom::indexedDB::CreateGenericEvent(const nsAString& aType,
                                            bool aBubblesAndCancelable)
{
  nsRefPtr<nsDOMEvent> event(new nsDOMEvent(nsnull, nsnull));
  nsresult rv = event->InitEvent(aType, aBubblesAndCancelable,
                                 aBubblesAndCancelable);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = event->SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  return event.forget();
}

already_AddRefed<nsIRunnable>
mozilla::dom::indexedDB::CreateGenericEventRunnable(const nsAString& aType,
                                                    nsIDOMEventTarget* aTarget)
{
  nsCOMPtr<nsIDOMEvent> event(CreateGenericEvent(aType));
  NS_ENSURE_TRUE(event, nsnull);

  nsCOMPtr<nsIRunnable> runnable(new EventFiringRunnable(aTarget, event));
  return runnable.forget();
}

// static
already_AddRefed<nsIDOMEvent>
IDBVersionChangeEvent::CreateInternal(const nsAString& aType,
                                      const nsAString& aVersion)
{
  nsRefPtr<IDBVersionChangeEvent> event(new IDBVersionChangeEvent());

  nsresult rv = event->InitEvent(aType, PR_FALSE, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = event->SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, nsnull);

  event->mVersion = aVersion;

  nsDOMEvent* result;
  event.forget(&result);
  return result;
}

// static
already_AddRefed<nsIRunnable>
IDBVersionChangeEvent::CreateRunnableInternal(const nsAString& aType,
                                              const nsAString& aVersion,
                                              nsIDOMEventTarget* aTarget)
{
  nsCOMPtr<nsIDOMEvent> event = CreateInternal(aType, aVersion);
  NS_ENSURE_TRUE(event, nsnull);

  nsCOMPtr<nsIRunnable> runnable(new EventFiringRunnable(aTarget, event));
  return runnable.forget();
}

NS_IMPL_ADDREF_INHERITED(IDBVersionChangeEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(IDBVersionChangeEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(IDBVersionChangeEvent)
  NS_INTERFACE_MAP_ENTRY(nsIIDBVersionChangeEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBVersionChangeEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

DOMCI_DATA(IDBVersionChangeEvent, IDBVersionChangeEvent)

NS_IMETHODIMP
IDBVersionChangeEvent::GetVersion(nsAString& aVersion)
{
  aVersion.Assign(mVersion);
  return NS_OK;
}
