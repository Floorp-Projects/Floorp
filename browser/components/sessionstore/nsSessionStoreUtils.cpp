/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSessionStoreUtils.h"

#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventListenerBinding.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"

using namespace mozilla::dom;

namespace {

class DynamicFrameEventFilter final : public nsIDOMEventListener
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(DynamicFrameEventFilter)

  explicit DynamicFrameEventFilter(EventListener* aListener)
    : mListener(aListener)
  { }

  NS_IMETHODIMP HandleEvent(Event* aEvent) override
  {
    if (mListener && TargetInNonDynamicDocShell(aEvent)) {
      mListener->HandleEvent(*aEvent);
    }

    return NS_OK;
  }

private:
  ~DynamicFrameEventFilter() { }

  bool TargetInNonDynamicDocShell(Event* aEvent)
  {
    EventTarget* target = aEvent->GetTarget();
    if (!target) {
      return false;
    }

    nsPIDOMWindowOuter* outer = target->GetOwnerGlobalForBindings();
    if (!outer) {
      return false;
    }

    nsIDocShell* docShell = outer->GetDocShell();
    if (!docShell) {
      return false;
    }

    bool isDynamic = false;
    nsresult rv = docShell->GetCreatedDynamically(&isDynamic);
    return NS_SUCCEEDED(rv) && !isDynamic;
  }

  RefPtr<EventListener> mListener;
};

NS_IMPL_CYCLE_COLLECTION(DynamicFrameEventFilter, mListener)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DynamicFrameEventFilter)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DynamicFrameEventFilter)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DynamicFrameEventFilter)

} // anonymous namespace

NS_IMPL_ISUPPORTS(nsSessionStoreUtils, nsISessionStoreUtils)

NS_IMETHODIMP
nsSessionStoreUtils::ForEachNonDynamicChildFrame(mozIDOMWindowProxy* aWindow,
                                                 nsISessionStoreUtilsFrameCallback* aCallback)
{
  NS_ENSURE_TRUE(aWindow, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsPIDOMWindowOuter> outer = nsPIDOMWindowOuter::From(aWindow);
  NS_ENSURE_TRUE(outer, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocShell> docShell = outer->GetDocShell();
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  int32_t length;
  nsresult rv = docShell->GetChildCount(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  for (int32_t i = 0; i < length; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    docShell->GetChildAt(i, getter_AddRefs(item));
    NS_ENSURE_TRUE(item, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDocShell> childDocShell(do_QueryInterface(item));
    NS_ENSURE_TRUE(childDocShell, NS_ERROR_FAILURE);

    bool isDynamic = false;
    nsresult rv = childDocShell->GetCreatedDynamically(&isDynamic);
    if (NS_SUCCEEDED(rv) && isDynamic) {
      continue;
    }

    int32_t childOffset = 0;
    rv = childDocShell->GetChildOffset(&childOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    aCallback->HandleFrame(item->GetWindow(), childOffset);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSessionStoreUtils::AddDynamicFrameFilteredListener(EventTarget* aTarget,
                                                     const nsAString& aType,
                                                     JS::Handle<JS::Value> aListener,
                                                     bool aUseCapture,
                                                     JSContext* aCx,
                                                     nsISupports** aResult)
{
  if (NS_WARN_IF(!aListener.isObject())) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_ENSURE_TRUE(aTarget, NS_ERROR_NO_INTERFACE);

  JS::Rooted<JSObject*> obj(aCx, &aListener.toObject());
  RefPtr<EventListener> listener =
    new EventListener(aCx, obj, GetIncumbentGlobal());

  nsCOMPtr<nsIDOMEventListener> filter(new DynamicFrameEventFilter(listener));

  nsresult rv = aTarget->AddEventListener(aType, filter, aUseCapture);
  NS_ENSURE_SUCCESS(rv, rv);

  filter.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsSessionStoreUtils::RemoveDynamicFrameFilteredListener(EventTarget* aTarget,
                                                        const nsAString& aType,
                                                        nsISupports* aListener,
                                                        bool aUseCapture)
{
  NS_ENSURE_TRUE(aTarget, NS_ERROR_NO_INTERFACE);

  nsCOMPtr<nsIDOMEventListener> listener = do_QueryInterface(aListener);
  NS_ENSURE_TRUE(listener, NS_ERROR_NO_INTERFACE);

  aTarget->RemoveEventListener(aType, listener, aUseCapture);
  return NS_OK;
}
