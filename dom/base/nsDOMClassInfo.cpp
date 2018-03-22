/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

// JavaScript includes
#include "jsapi.h"
#include "jsfriendapi.h"
#include "WrapperFactory.h"
#include "AccessCheck.h"
#include "XrayWrapper.h"

#include "xpcpublic.h"
#include "xpcprivate.h"
#include "xpc_make_class.h"
#include "XPCWrapper.h"

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/RegisterBindings.h"

#include "nscore.h"
#include "nsDOMClassInfo.h"
#include "nsIDOMClassInfo.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsICategoryManager.h"
#include "nsIComponentRegistrar.h"
#include "nsXPCOM.h"
#include "nsISimpleEnumerator.h"
#include "nsISupportsPrimitives.h"
#include "nsIXPConnect.h"
#include "xptcall.h"
#include "nsTArray.h"

// General helper includes
#include "nsGlobalWindow.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsContentUtils.h"
#include "nsIDOMGlobalPropertyInitializer.h"
#include "mozilla/Attributes.h"
#include "mozilla/Telemetry.h"

// Window scriptable helper includes
#include "nsScriptNameSpaceManager.h"

// DOM base includes
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"

// DOM core includes
#include "nsError.h"

// Event related includes
#include "nsIDOMEventTarget.h"

// CSS related includes
#include "nsMemory.h"

// includes needed for the prototype chain interfaces

#include "nsIEventListenerService.h"

#include "mozilla/dom/TouchEvent.h"

#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/HTMLCollectionBinding.h"

#include "nsDebug.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Likely.h"
#include "nsIInterfaceInfoManager.h"

#ifdef MOZ_TIME_MANAGER
#include "TimeManager.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

bool nsDOMClassInfo::sIsInitialized = false;

// static
nsresult
nsDOMClassInfo::Init()
{
  /* Errors that can trigger early returns are done first,
     otherwise nsDOMClassInfo is left in a half inited state. */
  static_assert(sizeof(uintptr_t) == sizeof(void*),
                "BAD! You'll need to adjust the size of uintptr_t to the "
                "size of a pointer on your platform.");

  NS_ENSURE_TRUE(!sIsInitialized, NS_ERROR_ALREADY_INITIALIZED);

  nsCOMPtr<nsIXPCFunctionThisTranslator> elt = new nsEventListenerThisTranslator();
  nsContentUtils::XPConnect()->SetFunctionThisTranslator(NS_GET_IID(nsIDOMEventListener), elt);

  sIsInitialized = true;

  return NS_OK;
}

// static
void
nsDOMClassInfo::ShutDown()
{
  sIsInitialized = false;
}

static nsresult
LookupComponentsShim(JSContext *cx, JS::Handle<JSObject*> global,
                     nsPIDOMWindowInner *win,
                     JS::MutableHandle<JS::PropertyDescriptor> desc);

#ifdef RELEASE_OR_BETA
#define USE_CONTROLLERS_SHIM
#endif

#ifdef USE_CONTROLLERS_SHIM
static const JSClass ControllersShimClass = {
    "Controllers", 0
};
static const JSClass XULControllersShimClass = {
    "XULControllers", 0
};
#endif

// static
nsresult
nsWindowSH::GlobalResolve(nsGlobalWindowInner *aWin, JSContext *cx,
                          JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                          JS::MutableHandle<JS::PropertyDescriptor> desc)
{
  if (id == XPCJSRuntime::Get()->GetStringID(XPCJSContext::IDX_COMPONENTS)) {
    return LookupComponentsShim(cx, obj, aWin->AsInner(), desc);
  }

#ifdef USE_CONTROLLERS_SHIM
  // Note: We use |obj| rather than |aWin| to get the principal here, because
  // this is called during Window setup when the Document isn't necessarily
  // hooked up yet.
  if ((id == XPCJSRuntime::Get()->GetStringID(XPCJSContext::IDX_CONTROLLERS) ||
       id == XPCJSRuntime::Get()->GetStringID(XPCJSContext::IDX_CONTROLLERS_CLASS)) &&
      !xpc::IsXrayWrapper(obj) &&
      !nsContentUtils::IsSystemPrincipal(nsContentUtils::ObjectPrincipal(obj)))
  {
    if (aWin->GetDoc()) {
      aWin->GetDoc()->WarnOnceAbout(nsIDocument::eWindow_Cc_ontrollers);
    }
    const JSClass* clazz;
    if (id == XPCJSRuntime::Get()->GetStringID(XPCJSContext::IDX_CONTROLLERS)) {
      clazz = &XULControllersShimClass;
    } else {
      clazz = &ControllersShimClass;
    }
    MOZ_ASSERT(JS_IsGlobalObject(obj));
    JS::Rooted<JSObject*> shim(cx, JS_NewObject(cx, clazz));
    if (NS_WARN_IF(!shim)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    FillPropertyDescriptor(desc, obj, JS::ObjectValue(*shim), /* readOnly = */ false);
    return NS_OK;
  }
#endif

  nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  // Note - Our only caller is nsGlobalWindow::DoResolve, which checks that
  // JSID_IS_STRING(id) is true.
  nsAutoJSString name;
  if (!name.init(cx, JSID_TO_STRING(id))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  const char16_t *class_name = nullptr;
  const nsGlobalNameStruct *name_struct =
    nameSpaceManager->LookupName(name, &class_name);

  if (!name_struct) {
    return NS_OK;
  }

  // The class_name had better match our name
  MOZ_ASSERT(name.Equals(class_name));

  NS_ENSURE_TRUE(class_name, NS_ERROR_UNEXPECTED);

  nsresult rv = NS_OK;

  if (name_struct->mType == nsGlobalNameStruct::eTypeProperty) {
    // Before defining a global property, check for a named subframe of the
    // same name. If it exists, we don't want to shadow it.
    if (nsCOMPtr<nsPIDOMWindowOuter> childWin = aWin->GetChildWindow(name)) {
      return NS_OK;
    }

    nsCOMPtr<nsISupports> native(do_CreateInstance(name_struct->mCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    JS::Rooted<JS::Value> prop_val(cx, JS::UndefinedValue()); // Property value.

    nsCOMPtr<nsIDOMGlobalPropertyInitializer> gpi(do_QueryInterface(native));
    if (gpi) {
      rv = gpi->Init(aWin->AsInner(), &prop_val);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (prop_val.isPrimitive() && !prop_val.isNull()) {
      rv = nsContentUtils::WrapNative(cx, native, &prop_val, true);
    }

    NS_ENSURE_SUCCESS(rv, rv);

    if (!JS_WrapValue(cx, &prop_val)) {
      return NS_ERROR_UNEXPECTED;
    }

    FillPropertyDescriptor(desc, obj, prop_val, false);

    return NS_OK;
  }

  return rv;
}

struct InterfaceShimEntry {
  const char *geckoName;
  const char *domName;
};

// We add shims from Components.interfaces.nsIDOMFoo to window.Foo for each
// interface that has interface constants that sites might be getting off
// of Ci.
const InterfaceShimEntry kInterfaceShimMap[] =
{ { "nsIXMLHttpRequest", "XMLHttpRequest" },
  { "nsIDOMDOMException", "DOMException" },
  { "nsIDOMNode", "Node" },
  { "nsIDOMCSSPrimitiveValue", "CSSPrimitiveValue" },
  { "nsIDOMCSSRule", "CSSRule" },
  { "nsIDOMCSSValue", "CSSValue" },
  { "nsIDOMEvent", "Event" },
  { "nsIDOMNSEvent", "Event" },
  { "nsIDOMKeyEvent", "KeyEvent" },
  { "nsIDOMMouseEvent", "MouseEvent" },
  { "nsIDOMMouseScrollEvent", "MouseScrollEvent" },
  { "nsIDOMMutationEvent", "MutationEvent" },
  { "nsIDOMSimpleGestureEvent", "SimpleGestureEvent" },
  { "nsIDOMUIEvent", "UIEvent" },
  { "nsIDOMHTMLMediaElement", "HTMLMediaElement" },
  { "nsIDOMOfflineResourceList", "OfflineResourceList" },
  { "nsIDOMRange", "Range" },
  { "nsIDOMSVGLength", "SVGLength" },
  // Think about whether Ci.nsINodeFilter can just go away for websites!
  { "nsIDOMNodeFilter", "NodeFilter" },
  { "nsIDOMXPathResult", "XPathResult" } };

static nsresult
LookupComponentsShim(JSContext *cx, JS::Handle<JSObject*> global,
                     nsPIDOMWindowInner *win,
                     JS::MutableHandle<JS::PropertyDescriptor> desc)
{
  // Keep track of how often this happens.
  Telemetry::Accumulate(Telemetry::COMPONENTS_SHIM_ACCESSED_BY_CONTENT, true);

  // Warn once.
  nsCOMPtr<nsIDocument> doc = win->GetExtantDoc();
  if (doc) {
    doc->WarnOnceAbout(nsIDocument::eComponents, /* asError = */ true);
  }

  // Create a fake Components object.
  AssertSameCompartment(cx, global);
  JS::Rooted<JSObject*> components(cx, JS_NewPlainObject(cx));
  NS_ENSURE_TRUE(components, NS_ERROR_OUT_OF_MEMORY);

  // Create a fake interfaces object.
  JS::Rooted<JSObject*> interfaces(cx, JS_NewPlainObject(cx));
  NS_ENSURE_TRUE(interfaces, NS_ERROR_OUT_OF_MEMORY);
  bool ok =
    JS_DefineProperty(cx, components, "interfaces", interfaces,
                      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);

  // Define a bunch of shims from the Ci.nsIDOMFoo to window.Foo for DOM
  // interfaces with constants.
  for (uint32_t i = 0; i < ArrayLength(kInterfaceShimMap); ++i) {

    // Grab the names from the table.
    const char *geckoName = kInterfaceShimMap[i].geckoName;
    const char *domName = kInterfaceShimMap[i].domName;

    // Look up the appopriate interface object on the global.
    JS::Rooted<JS::Value> v(cx, JS::UndefinedValue());
    ok = JS_GetProperty(cx, global, domName, &v);
    NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
    if (!v.isObject()) {
      NS_WARNING("Unable to find interface object on global");
      continue;
    }

    // Define the shim on the interfaces object.
    ok = JS_DefineProperty(cx, interfaces, geckoName, v,
                           JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);
    NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  }

  FillPropertyDescriptor(desc, global, JS::ObjectValue(*components), false);

  return NS_OK;
}

// nsIDOMEventListener::HandleEvent() 'this' converter helper

NS_INTERFACE_MAP_BEGIN(nsEventListenerThisTranslator)
  NS_INTERFACE_MAP_ENTRY(nsIXPCFunctionThisTranslator)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsEventListenerThisTranslator)
NS_IMPL_RELEASE(nsEventListenerThisTranslator)


NS_IMETHODIMP
nsEventListenerThisTranslator::TranslateThis(nsISupports *aInitialThis,
                                             nsISupports **_retval)
{
  nsCOMPtr<nsIDOMEvent> event(do_QueryInterface(aInitialThis));
  NS_ENSURE_TRUE(event, NS_ERROR_UNEXPECTED);

  nsCOMPtr<EventTarget> target = event->InternalDOMEvent()->GetCurrentTarget();
  target.forget(_retval);
  return NS_OK;
}
