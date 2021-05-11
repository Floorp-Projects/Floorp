/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowNamedPropertiesHandler.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsHTMLDocument.h"
#include "nsJSUtils.h"
#include "xpcprivate.h"

namespace mozilla::dom {

static bool ShouldExposeChildWindow(const nsString& aNameBeingResolved,
                                    BrowsingContext* aChild) {
  Element* e = aChild->GetEmbedderElement();
  if (e && e->IsInShadowTree()) {
    return false;
  }

  // If we're same-origin with the child, go ahead and expose it.
  nsPIDOMWindowOuter* child = aChild->GetDOMWindow();
  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(child);
  if (sop && nsContentUtils::SubjectPrincipal()->Equals(sop->GetPrincipal())) {
    return true;
  }

  // If we're not same-origin, expose it _only_ if the name of the browsing
  // context matches the 'name' attribute of the frame element in the parent.
  // The motivations behind this heuristic are worth explaining here.
  //
  // Historically, all UAs supported global named access to any child browsing
  // context (that is to say, window.dolske returns a child frame where either
  // the "name" attribute on the frame element was set to "dolske", or where
  // the child explicitly set window.name = "dolske").
  //
  // This is problematic because it allows possibly-malicious and unrelated
  // cross-origin subframes to pollute the global namespace of their parent in
  // unpredictable ways (see bug 860494). This is also problematic for browser
  // engines like Servo that want to run cross-origin script on different
  // threads.
  //
  // The naive solution here would be to filter out any cross-origin subframes
  // obtained when doing named lookup in global scope. But that is unlikely to
  // be web-compatible, since it will break named access for consumers that do
  // <iframe name="dolske" src="http://cross-origin.com/sadtrombone.html"> and
  // expect to be able to access the cross-origin subframe via named lookup on
  // the global.
  //
  // The optimal behavior would be to do the following:
  // (a) Look for any child browsing context with name="dolske".
  // (b) If the result is cross-origin, null it out.
  // (c) If we have null, look for a frame element whose 'name' attribute is
  //     "dolske".
  //
  // Unfortunately, (c) would require some engineering effort to be performant
  // in Gecko, and probably in other UAs as well. So we go with a simpler
  // approximation of the above. This approximation will only break sites that
  // rely on their cross-origin subframes setting window.name to a known value,
  // which is unlikely to be very common. And while it does introduce a
  // dependency on cross-origin state when doing global lookups, it doesn't
  // allow the child to arbitrarily pollute the parent namespace, and requires
  // cross-origin communication only in a limited set of cases that can be
  // computed independently by the parent.
  return e && e->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                             aNameBeingResolved, eCaseMatters);
}

bool WindowNamedPropertiesHandler::getOwnPropDescriptor(
    JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
    bool /* unused */,
    JS::MutableHandle<Maybe<JS::PropertyDescriptor>> aDesc) const {
  aDesc.reset();

  if (!JSID_IS_STRING(aId)) {
    if (aId.isWellKnownSymbol(JS::SymbolCode::toStringTag)) {
      JS::Rooted<JSString*> toStringTagStr(
          aCx, JS_NewStringCopyZ(aCx, "WindowProperties"));
      if (!toStringTagStr) {
        return false;
      }

      aDesc.set(Some(
          JS::PropertyDescriptor::Data(JS::StringValue(toStringTagStr),
                                       {JS::PropertyAttribute::Configurable})));
      return true;
    }

    // Nothing to do if we're resolving another non-string property.
    return true;
  }

  bool hasOnPrototype;
  if (!HasPropertyOnPrototype(aCx, aProxy, aId, &hasOnPrototype)) {
    return false;
  }
  if (hasOnPrototype) {
    return true;
  }

  nsAutoJSString str;
  if (!str.init(aCx, JSID_TO_STRING(aId))) {
    return false;
  }

  if (str.IsEmpty()) {
    return true;
  }

  // Grab the DOM window.
  nsGlobalWindowInner* win = xpc::WindowGlobalOrNull(aProxy);
  if (win->Length() > 0) {
    RefPtr<BrowsingContext> child = win->GetChildWindow(str);
    if (child && ShouldExposeChildWindow(str, child)) {
      // We found a subframe of the right name. Shadowing via |var foo| in
      // global scope is still allowed, since |var| only looks up |own|
      // properties. But unqualified shadowing will fail, per-spec.
      JS::Rooted<JS::Value> v(aCx);
      if (!ToJSValue(aCx, WindowProxyHolder(std::move(child)), &v)) {
        return false;
      }
      aDesc.set(mozilla::Some(
          JS::PropertyDescriptor::Data(v, {JS::PropertyAttribute::Configurable,
                                           JS::PropertyAttribute::Writable})));
      return true;
    }
  }

  // The rest of this function is for HTML documents only.
  Document* doc = win->GetExtantDoc();
  if (!doc || !doc->IsHTMLOrXHTML()) {
    return true;
  }
  nsHTMLDocument* document = doc->AsHTMLDocument();

  JS::Rooted<JS::Value> v(aCx);
  Element* element = document->GetElementById(str);
  if (element) {
    if (!ToJSValue(aCx, element, &v)) {
      return false;
    }
    aDesc.set(mozilla::Some(
        JS::PropertyDescriptor::Data(v, {JS::PropertyAttribute::Configurable,
                                         JS::PropertyAttribute::Writable})));
    return true;
  }

  ErrorResult rv;
  bool found = document->ResolveName(aCx, str, &v, rv);
  if (rv.MaybeSetPendingException(aCx)) {
    return false;
  }

  if (found) {
    aDesc.set(mozilla::Some(
        JS::PropertyDescriptor::Data(v, {JS::PropertyAttribute::Configurable,
                                         JS::PropertyAttribute::Writable})));
  }
  return true;
}

bool WindowNamedPropertiesHandler::defineProperty(
    JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
    JS::Handle<JS::PropertyDescriptor> aDesc,
    JS::ObjectOpResult& result) const {
  ErrorResult rv;
  rv.ThrowTypeError(
      "Not allowed to define a property on the named properties object.");
  MOZ_ALWAYS_TRUE(rv.MaybeSetPendingException(aCx));
  return false;
}

bool WindowNamedPropertiesHandler::ownPropNames(
    JSContext* aCx, JS::Handle<JSObject*> aProxy, unsigned flags,
    JS::MutableHandleVector<jsid> aProps) const {
  if (!(flags & JSITER_HIDDEN)) {
    // None of our named properties are enumerable.
    return true;
  }

  // Grab the DOM window.
  nsGlobalWindowInner* win = xpc::WindowGlobalOrNull(aProxy);
  nsTArray<nsString> names;
  // The names live on the outer window, which might be null
  nsGlobalWindowOuter* outer = win->GetOuterWindowInternal();
  if (outer) {
    if (BrowsingContext* bc = outer->GetBrowsingContext()) {
      for (const auto& child : bc->Children()) {
        const nsString& name = child->Name();
        if (!name.IsEmpty() && !names.Contains(name)) {
          // Make sure we really would expose it from getOwnPropDescriptor.
          if (ShouldExposeChildWindow(name, child)) {
            names.AppendElement(name);
          }
        }
      }
    }
  }
  if (!AppendNamedPropertyIds(aCx, aProxy, names, false, aProps)) {
    return false;
  }

  names.Clear();
  Document* doc = win->GetExtantDoc();
  if (!doc || !doc->IsHTMLOrXHTML()) {
    // Define to @@toStringTag on this object to keep Object.prototype.toString
    // backwards compatible.
    JS::Rooted<jsid> toStringTagId(aCx, SYMBOL_TO_JSID(JS::GetWellKnownSymbol(
                                            aCx, JS::SymbolCode::toStringTag)));
    return aProps.append(toStringTagId);
  }

  nsHTMLDocument* document = doc->AsHTMLDocument();
  // Document names are enumerable, so we want to get them no matter what flags
  // is.
  document->GetSupportedNames(names);

  JS::RootedVector<jsid> docProps(aCx);
  if (!AppendNamedPropertyIds(aCx, aProxy, names, false, &docProps)) {
    return false;
  }

  JS::Rooted<jsid> toStringTagId(aCx, SYMBOL_TO_JSID(JS::GetWellKnownSymbol(
                                          aCx, JS::SymbolCode::toStringTag)));
  if (!docProps.append(toStringTagId)) {
    return false;
  }

  return js::AppendUnique(aCx, aProps, docProps);
}

bool WindowNamedPropertiesHandler::delete_(JSContext* aCx,
                                           JS::Handle<JSObject*> aProxy,
                                           JS::Handle<jsid> aId,
                                           JS::ObjectOpResult& aResult) const {
  return aResult.failCantDeleteWindowNamedProperty();
}

// Note that this class doesn't need any reserved slots, but SpiderMonkey
// asserts all proxy classes have at least one reserved slot.
static const DOMIfaceAndProtoJSClass WindowNamedPropertiesClass = {
    PROXY_CLASS_DEF("WindowProperties", JSCLASS_IS_DOMIFACEANDPROTOJSCLASS |
                                            JSCLASS_HAS_RESERVED_SLOTS(1)),
    eNamedPropertiesObject,
    false,
    prototypes::id::_ID_Count,
    0,
    &sEmptyNativePropertyHooks,
    "[object WindowProperties]",
    EventTarget_Binding::GetProtoObject};

// static
JSObject* WindowNamedPropertiesHandler::Create(JSContext* aCx,
                                               JS::Handle<JSObject*> aProto) {
  js::ProxyOptions options;
  options.setClass(&WindowNamedPropertiesClass.mBase);

  JS::Rooted<JSObject*> gsp(
      aCx, js::NewProxyObject(aCx, WindowNamedPropertiesHandler::getInstance(),
                              JS::NullHandleValue, aProto, options));
  if (!gsp) {
    return nullptr;
  }

  bool succeeded;
  if (!JS_SetImmutablePrototype(aCx, gsp, &succeeded)) {
    return nullptr;
  }
  MOZ_ASSERT(succeeded,
             "errors making the [[Prototype]] of the named properties object "
             "immutable should have been JSAPI failures, not !succeeded");

  return gsp;
}

}  // namespace mozilla::dom
