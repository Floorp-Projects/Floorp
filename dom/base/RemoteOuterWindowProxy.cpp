/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessCheck.h"
#include "js/Proxy.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/RemoteObjectProxy.h"
#include "mozilla/dom/WindowBinding.h"
#include "xpcprivate.h"

namespace mozilla {
namespace dom {

/**
 * RemoteOuterWindowProxy is the proxy handler for the WindowProxy objects for
 * Window objects that live in a different process.
 *
 * RemoteOuterWindowProxy holds a BrowsingContext, which is cycle collected.
 * However, RemoteOuterWindowProxy only holds BrowsingContexts that don't have a
 * reference to a docshell, so there's no need to declare the edge from
 * RemoteOuterWindowProxy to its BrowsingContext to the cycle collector.
 *
 * FIXME Verify that this is correct:
 *       https://bugzilla.mozilla.org/show_bug.cgi?id=1516350.
 */

class RemoteOuterWindowProxy
    : public RemoteObjectProxy<BrowsingContext,
                               Window_Binding::sCrossOriginAttributes,
                               Window_Binding::sCrossOriginMethods> {
 public:
  constexpr RemoteOuterWindowProxy()
      : RemoteObjectProxy(prototypes::id::Window) {}

  // Standard internal methods
  bool getOwnPropertyDescriptor(
      JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
      JS::MutableHandle<JS::PropertyDescriptor> aDesc) const final;
  bool ownPropertyKeys(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                       JS::AutoIdVector& aProps) const final;

  // SpiderMonkey extensions
  bool getOwnEnumerablePropertyKeys(JSContext* cx, JS::Handle<JSObject*> proxy,
                                    JS::AutoIdVector& props) const final;
  void finalize(JSFreeOp* aFop, JSObject* aProxy) const final;
  const char* className(JSContext* aCx,
                        JS::Handle<JSObject*> aProxy) const final;
};

static const RemoteOuterWindowProxy sSingleton;

// Give RemoteOuterWindowProxyClass 2 reserved slots, like the other wrappers,
// so JSObject::swap can swap it with CrossCompartmentWrappers without requiring
// malloc.
const js::Class RemoteOuterWindowProxyClass =
    PROXY_CLASS_DEF("Proxy", JSCLASS_HAS_RESERVED_SLOTS(2));

bool GetRemoteOuterWindowProxy(JSContext* aCx, BrowsingContext* aContext,
                               JS::MutableHandle<JSObject*> aRetVal) {
  MOZ_ASSERT(!aContext->GetDocShell(),
             "Why are we creating a RemoteOuterWindowProxy?");

  xpc::CompartmentPrivate* priv =
      xpc::CompartmentPrivate::Get(JS::CurrentGlobalOrNull(aCx));
  xpc::CompartmentPrivate::RemoteProxyMap& map = priv->GetRemoteProxyMap();
  auto result = map.lookupForAdd(aContext);
  if (result) {
    aRetVal.set(result->value());
    return true;
  }

  JS::Rooted<JSObject*> obj(
      aCx, sSingleton.CreateProxyObject(aCx, aContext,
                                        &RemoteOuterWindowProxyClass));
  if (!obj) {
    return false;
  }
  NS_ADDREF(aContext);

  if (!map.add(result, aContext, obj)) {
    JS_ReportOutOfMemory(aCx);
    return false;
  }

  aRetVal.set(obj);
  return true;
}

static BrowsingContext* GetBrowsingContext(JSObject* aProxy) {
  MOZ_ASSERT(IsRemoteObjectProxy(aProxy, prototypes::id::Window));
  return static_cast<BrowsingContext*>(
      RemoteObjectProxyBase::GetNative(aProxy));
}

bool WrapResult(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                BrowsingContext* aResult, unsigned attrs,
                JS::MutableHandle<JS::PropertyDescriptor> aDesc) {
  JS::Rooted<JS::Value> v(aCx);
  if (!ToJSValue(aCx, WindowProxyHolder(aResult), &v)) {
    return false;
  }
  aDesc.setDataDescriptor(v, attrs);
  aDesc.object().set(aProxy);
  return true;
}

bool RemoteOuterWindowProxy::getOwnPropertyDescriptor(
    JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
    JS::MutableHandle<JS::PropertyDescriptor> aDesc) const {
  BrowsingContext* bc = GetBrowsingContext(aProxy);
  uint32_t index = GetArrayIndexFromId(aId);
  if (IsArrayIndex(index)) {
    const BrowsingContext::Children& children = bc->GetChildren();
    if (index < children.Length()) {
      return WrapResult(aCx, aProxy, children[index],
                        JSPROP_READONLY | JSPROP_ENUMERATE, aDesc);
    }
    return ReportCrossOriginDenial(aCx, aId, NS_LITERAL_CSTRING("access"));
  }

  bool ok = CrossOriginGetOwnPropertyHelper(aCx, aProxy, aId, aDesc);
  if (!ok || aDesc.object()) {
    return ok;
  }

  if (JSID_IS_STRING(aId)) {
    nsAutoJSString str;
    if (!str.init(aCx, JSID_TO_STRING(aId))) {
      return false;
    }

    for (BrowsingContext* child : bc->GetChildren()) {
      if (child->NameEquals(str)) {
        return WrapResult(aCx, aProxy, child, JSPROP_READONLY, aDesc);
      }
    }
  }

  return CrossOriginPropertyFallback(aCx, aProxy, aId, aDesc);
}

bool AppendIndexedPropertyNames(JSContext* aCx, BrowsingContext* aContext,
                                JS::AutoIdVector& aIndexedProps) {
  int32_t length = aContext->GetChildren().Length();
  if (!aIndexedProps.reserve(aIndexedProps.length() + length)) {
    return false;
  }

  for (int32_t i = 0; i < length; ++i) {
    aIndexedProps.infallibleAppend(INT_TO_JSID(i));
  }
  return true;
}

bool RemoteOuterWindowProxy::ownPropertyKeys(JSContext* aCx,
                                             JS::Handle<JSObject*> aProxy,
                                             JS::AutoIdVector& aProps) const {
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
    JS::AutoIdVector& aProps) const {
  return AppendIndexedPropertyNames(aCx, GetBrowsingContext(aProxy), aProps);
}

void RemoteOuterWindowProxy::finalize(JSFreeOp* aFop, JSObject* aProxy) const {
  BrowsingContext* bc = GetBrowsingContext(aProxy);
  RefPtr<BrowsingContext> self(dont_AddRef(bc));
}

const char* RemoteOuterWindowProxy::className(
    JSContext* aCx, JS::Handle<JSObject*> aProxy) const {
  MOZ_ASSERT(js::IsProxy(aProxy));

  return "Object";
}

}  // namespace dom
}  // namespace mozilla
