/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessCheck.h"
#include "js/Proxy.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/RemoteObjectProxy.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "xpcprivate.h"

namespace mozilla::dom {

/**
 * RemoteOuterWindowProxy is the proxy handler for the WindowProxy objects for
 * Window objects that live in a different process.
 *
 * RemoteOuterWindowProxy holds a BrowsingContext, which is cycle collected.
 * This reference is declared to the cycle collector via NoteChildren().
 */

class RemoteOuterWindowProxy
    : public RemoteObjectProxy<BrowsingContext,
                               Window_Binding::sCrossOriginProperties> {
 public:
  using Base = RemoteObjectProxy;

  constexpr RemoteOuterWindowProxy()
      : RemoteObjectProxy(prototypes::id::Window) {}

  // Standard internal methods
  bool getOwnPropertyDescriptor(
      JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
      JS::MutableHandle<Maybe<JS::PropertyDescriptor>> aDesc) const final;
  bool ownPropertyKeys(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                       JS::MutableHandleVector<jsid> aProps) const final;

  // SpiderMonkey extensions
  bool getOwnEnumerablePropertyKeys(
      JSContext* cx, JS::Handle<JSObject*> proxy,
      JS::MutableHandleVector<jsid> props) const final;

  void NoteChildren(JSObject* aProxy,
                    nsCycleCollectionTraversalCallback& aCb) const override {
    CycleCollectionNoteChild(aCb,
                             static_cast<BrowsingContext*>(GetNative(aProxy)),
                             "JS::GetPrivate(obj)");
  }
};

static const RemoteOuterWindowProxy sSingleton;

// Give RemoteOuterWindowProxy 2 reserved slots, like the other wrappers,
// so JSObject::swap can swap it with CrossCompartmentWrappers without requiring
// malloc.
template <>
const JSClass RemoteOuterWindowProxy::Base::sClass =
    PROXY_CLASS_DEF("Proxy", JSCLASS_HAS_RESERVED_SLOTS(2));

bool GetRemoteOuterWindowProxy(JSContext* aCx, BrowsingContext* aContext,
                               JS::Handle<JSObject*> aTransplantTo,
                               JS::MutableHandle<JSObject*> aRetVal) {
  MOZ_ASSERT(!aContext->GetDocShell(),
             "Why are we creating a RemoteOuterWindowProxy?");

  sSingleton.GetProxyObject(aCx, aContext, aTransplantTo, aRetVal);
  return !!aRetVal;
}

BrowsingContext* GetBrowsingContext(JSObject* aProxy) {
  MOZ_ASSERT(IsRemoteObjectProxy(aProxy, prototypes::id::Window));
  return static_cast<BrowsingContext*>(
      RemoteObjectProxyBase::GetNative(aProxy));
}

static bool WrapResult(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                       BrowsingContext* aResult, JS::PropertyAttributes attrs,
                       JS::MutableHandle<Maybe<JS::PropertyDescriptor>> aDesc) {
  JS::Rooted<JS::Value> v(aCx);
  if (!ToJSValue(aCx, WindowProxyHolder(aResult), &v)) {
    return false;
  }

  aDesc.set(Some(JS::PropertyDescriptor::Data(v, attrs)));
  return true;
}

bool RemoteOuterWindowProxy::getOwnPropertyDescriptor(
    JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
    JS::MutableHandle<Maybe<JS::PropertyDescriptor>> aDesc) const {
  BrowsingContext* bc = GetBrowsingContext(aProxy);
  uint32_t index = GetArrayIndexFromId(aId);
  if (IsArrayIndex(index)) {
    Span<RefPtr<BrowsingContext>> children = bc->Children();
    if (index < children.Length()) {
      return WrapResult(aCx, aProxy, children[index],
                        {JS::PropertyAttribute::Configurable,
                         JS::PropertyAttribute::Enumerable},
                        aDesc);
    }
    return ReportCrossOriginDenial(aCx, aId, "access"_ns);
  }

  bool ok = CrossOriginGetOwnPropertyHelper(aCx, aProxy, aId, aDesc);
  if (!ok || aDesc.isSome()) {
    return ok;
  }

  // We don't need the "print" hack that nsOuterWindowProxy has, because pdf
  // documents are placed in a process based on their principal before the PDF
  // viewer changes principals around, so are always same-process with things
  // that are same-origin with their original principal and won't reach this
  // code in the cases when "print" should be accessible.

  if (JSID_IS_STRING(aId)) {
    nsAutoJSString str;
    if (!str.init(aCx, JSID_TO_STRING(aId))) {
      return false;
    }

    for (BrowsingContext* child : bc->Children()) {
      if (child->NameEquals(str)) {
        return WrapResult(aCx, aProxy, child,
                          {JS::PropertyAttribute::Configurable}, aDesc);
      }
    }
  }

  return CrossOriginPropertyFallback(aCx, aProxy, aId, aDesc);
}

bool AppendIndexedPropertyNames(JSContext* aCx, BrowsingContext* aContext,
                                JS::MutableHandleVector<jsid> aIndexedProps) {
  int32_t length = aContext->Children().Length();
  if (!aIndexedProps.reserve(aIndexedProps.length() + length)) {
    return false;
  }

  for (int32_t i = 0; i < length; ++i) {
    aIndexedProps.infallibleAppend(INT_TO_JSID(i));
  }
  return true;
}

bool RemoteOuterWindowProxy::ownPropertyKeys(
    JSContext* aCx, JS::Handle<JSObject*> aProxy,
    JS::MutableHandleVector<jsid> aProps) const {
  BrowsingContext* bc = GetBrowsingContext(aProxy);

  // https://html.spec.whatwg.org/multipage/window-object.html#windowproxy-ownpropertykeys:crossoriginownpropertykeys-(-o-)
  // step 3 to 5
  if (!AppendIndexedPropertyNames(aCx, bc, aProps)) {
    return false;
  }

  // https://html.spec.whatwg.org/multipage/window-object.html#windowproxy-ownpropertykeys:crossoriginownpropertykeys-(-o-)
  // step 7
  return RemoteObjectProxy::ownPropertyKeys(aCx, aProxy, aProps);
}

bool RemoteOuterWindowProxy::getOwnEnumerablePropertyKeys(
    JSContext* aCx, JS::Handle<JSObject*> aProxy,
    JS::MutableHandleVector<jsid> aProps) const {
  return AppendIndexedPropertyNames(aCx, GetBrowsingContext(aProxy), aProps);
}

}  // namespace mozilla::dom
