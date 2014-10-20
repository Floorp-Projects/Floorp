/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CustomEvent.h"
#include "mozilla/dom/CustomEventBinding.h"

#include "mozilla/dom/BindingUtils.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"

using namespace mozilla;
using namespace mozilla::dom;

CustomEvent::CustomEvent(mozilla::dom::EventTarget* aOwner,
                         nsPresContext* aPresContext,
                         mozilla::WidgetEvent* aEvent)
: Event(aOwner, aPresContext, aEvent)
{
}

CustomEvent::~CustomEvent() {}

NS_IMPL_CYCLE_COLLECTION_CLASS(CustomEvent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CustomEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDetail)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CustomEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDetail)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(CustomEvent, Event)
NS_IMPL_RELEASE_INHERITED(CustomEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(CustomEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCustomEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

already_AddRefed<CustomEvent>
CustomEvent::Constructor(const GlobalObject& aGlobal,
                         const nsAString& aType,
                         const CustomEventInit& aParam,
                         ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<CustomEvent> e = new CustomEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  JS::Rooted<JS::Value> detail(aGlobal.Context(), aParam.mDetail);
  e->InitCustomEvent(aGlobal.Context(), aType, aParam.mBubbles, aParam.mCancelable, detail, aRv);
  e->SetTrusted(trusted);
  return e.forget();
}

JSObject*
CustomEvent::WrapObjectInternal(JSContext* aCx)
{
  return mozilla::dom::CustomEventBinding::Wrap(aCx, this);
}

NS_IMETHODIMP
CustomEvent::InitCustomEvent(const nsAString& aType,
                             bool aCanBubble,
                             bool aCancelable,
                             nsIVariant* aDetail)
{
  nsresult rv = Event::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);
  mDetail = aDetail;
  return NS_OK;
}

void
CustomEvent::InitCustomEvent(JSContext* aCx,
                             const nsAString& aType,
                             bool aCanBubble,
                             bool aCancelable,
                             JS::Handle<JS::Value> aDetail,
                             ErrorResult& aRv)
{
  nsCOMPtr<nsIVariant> detail;
  if (nsIXPConnect* xpc = nsContentUtils::XPConnect()) {
    xpc->JSToVariant(aCx, aDetail, getter_AddRefs(detail));
  }

  if (!detail) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  aRv = InitCustomEvent(aType, aCanBubble, aCancelable, detail);
}

NS_IMETHODIMP
CustomEvent::GetDetail(nsIVariant** aDetail)
{
  NS_IF_ADDREF(*aDetail = mDetail);
  return NS_OK;
}

void
CustomEvent::GetDetail(JSContext* aCx,
                       JS::MutableHandle<JS::Value> aRetval)
{
  if (!mDetail) {
    aRetval.setNull();
    return;
  }

  VariantToJsval(aCx, mDetail, aRetval);
}

nsresult
NS_NewDOMCustomEvent(nsIDOMEvent** aInstancePtrResult,
                     mozilla::dom::EventTarget* aOwner,
                     nsPresContext* aPresContext,
                     mozilla::WidgetEvent* aEvent)
{
  CustomEvent* it = new CustomEvent(aOwner, aPresContext, aEvent);
  NS_ADDREF(it);
  *aInstancePtrResult = static_cast<Event*>(it);
  return NS_OK;
}
