/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowNamedPropertiesHandler.h"
#include "mozilla/dom/WindowBinding.h"
#include "nsDOMClassInfo.h"
#include "nsGlobalWindow.h"
#include "nsHTMLDocument.h"
#include "nsJSUtils.h"
#include "xpcprivate.h"

namespace mozilla {
namespace dom {

static bool
ShouldExposeChildWindow(nsString& aNameBeingResolved, nsIDOMWindow *aChild)
{
  // If we're same-origin with the child, go ahead and expose it.
  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aChild);
  NS_ENSURE_TRUE(sop, false);
  if (nsContentUtils::SubjectPrincipal()->Equals(sop->GetPrincipal())) {
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
  nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(aChild);
  NS_ENSURE_TRUE(piWin, false);
  Element* e = piWin->GetFrameElementInternal();
  return e && e->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                             aNameBeingResolved, eCaseMatters);
}

static nsGlobalWindow*
GetWindowFromGlobal(JSObject* aGlobal)
{
  nsGlobalWindow* win;
  if (NS_SUCCEEDED(UNWRAP_OBJECT(Window, aGlobal, win))) {
    return win;
  }
  XPCWrappedNative* wrapper = XPCWrappedNative::Get(aGlobal);
  nsCOMPtr<nsPIDOMWindow> piWin = do_QueryWrappedNative(wrapper);
  MOZ_ASSERT(piWin);
  return static_cast<nsGlobalWindow*>(piWin.get());
}

bool
WindowNamedPropertiesHandler::getOwnPropertyDescriptor(JSContext* aCx,
                                                       JS::Handle<JSObject*> aProxy,
                                                       JS::Handle<jsid> aId,
                                                       JS::MutableHandle<JSPropertyDescriptor> aDesc)
                                                       const
{
  // Note: The infallibleInit call below depends on this check.
  if (!JSID_IS_STRING(aId)) {
    // Nothing to do if we're resolving a non-string property.
    return true;
  }

  JS::Rooted<JSObject*> global(aCx, JS_GetGlobalForObject(aCx, aProxy));
  if (HasPropertyOnPrototype(aCx, aProxy, aId)) {
    return true;
  }

  nsDependentJSString str;
  str.infallibleInit(aId);

  // Grab the DOM window.
  nsGlobalWindow* win = GetWindowFromGlobal(global);
  if (win->Length() > 0) {
    nsCOMPtr<nsIDOMWindow> childWin = win->GetChildWindow(str);
    if (childWin && ShouldExposeChildWindow(str, childWin)) {
      // We found a subframe of the right name. Shadowing via |var foo| in
      // global scope is still allowed, since |var| only looks up |own|
      // properties. But unqualified shadowing will fail, per-spec.
      JS::Rooted<JS::Value> v(aCx);
      if (!WrapObject(aCx, childWin, &v)) {
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
    if (!WrapObject(aCx, element, &v)) {
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
  if (!WrapObject(aCx, result, cache, nullptr, &v)) {
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
                                             JS::MutableHandle<JSPropertyDescriptor> aDesc) const
{
  ErrorResult rv;
  rv.ThrowTypeError(MSG_DEFINEPROPERTY_ON_GSP);
  rv.ReportTypeError(aCx);
  return false;
}

bool
WindowNamedPropertiesHandler::ownPropNames(JSContext* aCx,
                                           JS::Handle<JSObject*> aProxy,
                                           unsigned flags,
                                           JS::AutoIdVector& aProps) const
{
  // Grab the DOM window.
  nsGlobalWindow* win = GetWindowFromGlobal(JS_GetGlobalForObject(aCx, aProxy));
  nsTArray<nsString> names;
  win->GetSupportedNames(names);
  // Filter out the ones we wouldn't expose from getOwnPropertyDescriptor.
  // We iterate backwards so we can remove things from the list easily.
  for (size_t i = names.Length(); i > 0; ) {
    --i; // Now we're pointing at the next name we want to look at
    nsIDOMWindow* childWin = win->GetChildWindow(names[i]);
    if (!childWin || !ShouldExposeChildWindow(names[i], childWin)) {
      names.RemoveElementAt(i);
    }
  }
  if (!AppendNamedPropertyIds(aCx, aProxy, names, false, aProps)) {
    return false;
  }

  names.Clear();
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(win->GetExtantDoc());
  if (!htmlDoc) {
    return true;
  }
  nsHTMLDocument* document = static_cast<nsHTMLDocument*>(htmlDoc.get());
  document->GetSupportedNames(flags, names);

  JS::AutoIdVector docProps(aCx);
  if (!AppendNamedPropertyIds(aCx, aProxy, names, false, docProps)) {
    return false;
  }

  return js::AppendUnique(aCx, aProps, docProps);
}

bool
WindowNamedPropertiesHandler::delete_(JSContext* aCx,
                                      JS::Handle<JSObject*> aProxy,
                                      JS::Handle<jsid> aId, bool* aBp) const
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
  js::ProxyOptions options;
  options.setSingleton(true);
  gsp = js::NewProxyObject(aCx, WindowNamedPropertiesHandler::getInstance(),
                           JS::NullHandleValue, protoProto,
                           js::GetGlobalForObjectCrossCompartment(aProto),
                           options);
  if (!gsp) {
    return;
  }

  // And then set the prototype of the interface prototype object to be the
  // global scope polluter.
  ::JS_SplicePrototype(aCx, aProto, gsp);
}

} // namespace dom
} // namespace mozilla
