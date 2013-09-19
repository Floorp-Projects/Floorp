/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowNamedPropertiesHandler.h"
#include "nsDOMClassInfo.h"
#include "nsGlobalWindow.h"
#include "nsHTMLDocument.h"
#include "nsJSUtils.h"
#include "xpcprivate.h"

namespace mozilla {
namespace dom {

bool
WindowNamedPropertiesHandler::getOwnPropertyDescriptor(JSContext* aCx,
                                                       JS::Handle<JSObject*> aProxy,
                                                       JS::Handle<jsid> aId,
                                                       JS::MutableHandle<JSPropertyDescriptor> aDesc,
                                                       unsigned aFlags)
{
  if (!JSID_IS_STRING(aId)) {
    // Nothing to do if we're resolving a non-string property.
    return true;
  }

  JS::Rooted<JSObject*> global(aCx, JS_GetGlobalForObject(aCx, aProxy));
  nsresult rv =
    nsDOMClassInfo::ScriptSecurityManager()->CheckPropertyAccess(aCx, global,
                                                                 "Window", aId,
                                                                 nsIXPCSecurityManager::ACCESS_GET_PROPERTY);
  if (NS_FAILED(rv)) {
    // The security check failed. The security manager set a JS exception for
    // us.
    return false;
  }

  if (HasPropertyOnPrototype(aCx, aProxy, aId)) {
    return true;
  }

  nsDependentJSString str(aId);

  // Grab the DOM window.
  XPCWrappedNative* wrapper = XPCWrappedNative::Get(global);
  nsCOMPtr<nsPIDOMWindow> piWin = do_QueryWrappedNative(wrapper);
  MOZ_ASSERT(piWin);
  nsGlobalWindow* win = static_cast<nsGlobalWindow*>(piWin.get());
  if (win->GetLength() > 0) {
    nsCOMPtr<nsIDOMWindow> childWin = win->GetChildWindow(str);
    if (childWin) {
      // We found a subframe of the right name. Shadowing via |var foo| in
      // global scope is still allowed, since |var| only looks up |own|
      // properties. But unqualified shadowing will fail, per-spec.
      JS::Rooted<JS::Value> v(aCx);
      if (!WrapObject(aCx, aProxy, childWin, &v)) {
        return false;
      }
      aDesc.object().set(aProxy);
      aDesc.value().set(v);
      aDesc.setAttributes(JSPROP_ENUMERATE);
      return true;
    }
  }

  // The rest of this function is for HTML documents only.
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(win->GetExtantDoc());
  if (!htmlDoc) {
    return true;
  }
  nsHTMLDocument* document = static_cast<nsHTMLDocument*>(htmlDoc.get());

  Element* element = document->GetElementById(str);
  if (element) {
    JS::Rooted<JS::Value> v(aCx);
    if (!WrapObject(aCx, aProxy, element, &v)) {
      return false;
    }
    aDesc.object().set(aProxy);
    aDesc.value().set(v);
    aDesc.setAttributes(JSPROP_ENUMERATE);
    return true;
  }

  nsWrapperCache* cache;
  nsISupports* result = document->ResolveName(str, &cache);
  if (!result) {
    return true;
  }

  JS::Rooted<JS::Value> v(aCx);
  if (!WrapObject(aCx, aProxy, result, cache, nullptr, &v)) {
    return false;
  }
  aDesc.object().set(aProxy);
  aDesc.value().set(v);
  aDesc.setAttributes(JSPROP_ENUMERATE);
  return true;
}

bool
WindowNamedPropertiesHandler::defineProperty(JSContext* aCx,
                                             JS::Handle<JSObject*> aProxy,
                                             JS::Handle<jsid> aId,
                                             JS::MutableHandle<JSPropertyDescriptor> aDesc)
{
  ErrorResult rv;
  rv.ThrowTypeError(MSG_DEFINEPROPERTY_ON_GSP);
  rv.ReportTypeError(aCx);
  return false;
}

bool
WindowNamedPropertiesHandler::getOwnPropertyNames(JSContext* aCx,
                                                  JS::Handle<JSObject*> aProxy,
                                                  JS::AutoIdVector& aProps)
{
  // Grab the DOM window.
  JSObject* global = JS_GetGlobalForObject(aCx, aProxy);
  XPCWrappedNative* wrapper = XPCWrappedNative::Get(global);
  nsCOMPtr<nsPIDOMWindow> piWin = do_QueryWrappedNative(wrapper);
  MOZ_ASSERT(piWin);
  nsGlobalWindow* win = static_cast<nsGlobalWindow*>(piWin.get());
  nsTArray<nsString> names;
  win->GetSupportedNames(names);
  if (!AppendNamedPropertyIds(aCx, aProxy, names, false, aProps)) {
    return false;
  }

  names.Clear();
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(win->GetExtantDoc());
  if (!htmlDoc) {
    return true;
  }
  nsHTMLDocument* document = static_cast<nsHTMLDocument*>(htmlDoc.get());
  document->GetSupportedNames(names);

  JS::AutoIdVector docProps(aCx);
  if (!AppendNamedPropertyIds(aCx, aProxy, names, false, docProps)) {
    return false;
  }

  return js::AppendUnique(aCx, aProps, docProps);
}

bool
WindowNamedPropertiesHandler::delete_(JSContext* aCx,
                                      JS::Handle<JSObject*> aProxy,
                                      JS::Handle<jsid> aId, bool* aBp)
{
  *aBp = false;
  return true;
}

// static
void
WindowNamedPropertiesHandler::Install(JSContext* aCx,
                                      JS::Handle<JSObject*> aProto)
{
  JS::Rooted<JSObject*> protoProto(aCx);
  if (!::JS_GetPrototype(aCx, aProto, &protoProto)) {
    return;
  }

  // Note: since the scope polluter proxy lives on the window's prototype
  // chain, it needs a singleton type to avoid polluting type information
  // for properties on the window.
  JS::Rooted<JSObject*> gsp(aCx);
  gsp = js::NewProxyObject(aCx, WindowNamedPropertiesHandler::getInstance(),
                           JS::NullHandleValue, protoProto,
                           js::GetGlobalForObjectCrossCompartment(aProto),
                           js::ProxyNotCallable,
                           /* singleton = */ true);
  if (!gsp) {
    return;
  }

  // And then set the prototype of the interface prototype object to be the
  // global scope polluter.
  ::JS_SplicePrototype(aCx, aProto, gsp);
}

} // namespace dom
} // namespace mozilla
