/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#ifdef XP_WIN
#undef GetClassName
#endif

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

// NOTE: DEFAULT_SCRIPTABLE_FLAGS and DOM_DEFAULT_SCRIPTABLE_FLAGS
//       are defined in nsIDOMClassInfo.h.

#define DOMCLASSINFO_STANDARD_FLAGS                                           \
  (nsIClassInfo::MAIN_THREAD_ONLY |                                           \
   nsIClassInfo::DOM_OBJECT       |                                           \
   nsIClassInfo::SINGLETON_CLASSINFO)


#ifdef DEBUG
#define NS_DEFINE_CLASSINFO_DATA_DEBUG(_class)                                \
    eDOMClassInfo_##_class##_id,
#else
#define NS_DEFINE_CLASSINFO_DATA_DEBUG(_class)                                \
  // nothing
#endif

#define NS_DEFINE_CLASSINFO_DATA_HELPER(_class, _helper, _flags)              \
  { nullptr,                                                                  \
    XPC_MAKE_CLASS_OPS(_flags),                                               \
    XPC_MAKE_CLASS(#_class, _flags,                                           \
                   &sClassInfoData[eDOMClassInfo_##_class##_id].mClassOps),   \
    _helper::doCreate,                                                        \
    nullptr,                                                                  \
    nullptr,                                                                  \
    nullptr,                                                                  \
    _flags,                                                                   \
    true,                                                                     \
    false,                                                                    \
    NS_DEFINE_CLASSINFO_DATA_DEBUG(_class)                                    \
  },

#define NS_DEFINE_CLASSINFO_DATA(_class, _helper, _flags)                     \
  NS_DEFINE_CLASSINFO_DATA_HELPER(_class, _helper, _flags)

nsIXPConnect *nsDOMClassInfo::sXPConnect = nullptr;
bool nsDOMClassInfo::sIsInitialized = false;


jsid nsDOMClassInfo::sConstructor_id     = JSID_VOID;
jsid nsDOMClassInfo::sWrappedJSObject_id = JSID_VOID;

// Helper to handle torn-down inner windows.
static inline nsresult
SetParentToWindow(nsGlobalWindowInner *win, JSObject **parent)
{
  MOZ_ASSERT(win);
  *parent = win->FastGetGlobalJSObject();

  if (MOZ_UNLIKELY(!*parent)) {
    // The inner window has been torn down. The scope is dying, so don't create
    // any new wrappers.
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
nsDOMClassInfo::DefineStaticJSVals()
{
  AutoJSAPI jsapi;
  if (!jsapi.Init(xpc::UnprivilegedJunkScope())) {
    return NS_ERROR_UNEXPECTED;
  }
  JSContext* cx = jsapi.cx();

#define SET_JSID_TO_STRING(_id, _cx, _str)                              \
  if (JSString *str = ::JS_AtomizeAndPinString(_cx, _str))                             \
      _id = INTERNED_STRING_TO_JSID(_cx, str);                                \
  else                                                                        \
      return NS_ERROR_OUT_OF_MEMORY;

  SET_JSID_TO_STRING(sConstructor_id,     cx, "constructor");
  SET_JSID_TO_STRING(sWrappedJSObject_id, cx, "wrappedJSObject");

  return NS_OK;
}

// static
bool
nsDOMClassInfo::ObjectIsNativeWrapper(JSContext* cx, JSObject* obj)
{
  return xpc::WrapperFactory::IsXrayWrapper(obj) &&
         xpc::AccessCheck::wrapperSubsumes(obj);
}

nsDOMClassInfo::nsDOMClassInfo(nsDOMClassInfoData* aData) : mData(aData)
{
}

NS_IMPL_ADDREF(nsDOMClassInfo)
NS_IMPL_RELEASE(nsDOMClassInfo)

NS_INTERFACE_MAP_BEGIN(nsDOMClassInfo)
  if (aIID.Equals(NS_GET_IID(nsXPCClassInfo)))
    foundInterface = static_cast<nsIClassInfo*>(
                                    static_cast<nsXPCClassInfo*>(this));
  else
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIClassInfo)
NS_INTERFACE_MAP_END


static const JSClass sDOMConstructorProtoClass = {
  "DOM Constructor.prototype", 0
};


#define _DOM_CLASSINFO_MAP_BEGIN(_class, _ifptr, _has_class_if)               \
  {                                                                           \
    nsDOMClassInfoData &d = sClassInfoData[eDOMClassInfo_##_class##_id];      \
    d.mProtoChainInterface = _ifptr;                                          \
    d.mHasClassInterface = _has_class_if;                                     \
    static const nsIID *interface_list[] = {

#define DOM_CLASSINFO_MAP_BEGIN(_class, _interface)                           \
  _DOM_CLASSINFO_MAP_BEGIN(_class, &NS_GET_IID(_interface), true)

#define DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(_class, _interface)               \
  _DOM_CLASSINFO_MAP_BEGIN(_class, &NS_GET_IID(_interface), false)

#define DOM_CLASSINFO_MAP_ENTRY(_if)                                          \
      &NS_GET_IID(_if),

#define DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(_if, _cond)                       \
      (_cond) ? &NS_GET_IID(_if) : nullptr,

#define DOM_CLASSINFO_MAP_END                                                 \
      nullptr                                                                  \
    };                                                                        \
                                                                              \
    /* Compact the interface list */                                          \
    size_t count = ArrayLength(interface_list);                               \
    /* count is the number of array entries, which is one greater than the */ \
    /* number of interfaces due to the terminating null */                    \
    for (size_t i = 0; i < count - 1; ++i) {                                  \
      if (!interface_list[i]) {                                               \
        /* We are moving the element at index i+1 and successors, */          \
        /* so we must move only count - (i+1) elements total. */              \
        memmove(&interface_list[i], &interface_list[i+1],                     \
                sizeof(nsIID*) * (count - (i+1)));                            \
        /* Make sure to examine the new pointer we ended up with at this */   \
        /* slot, since it may be null too */                                  \
        --i;                                                                  \
        --count;                                                              \
      }                                                                       \
    }                                                                         \
                                                                              \
    d.mInterfaces = interface_list;                                           \
  }

nsresult
nsDOMClassInfo::Init()
{
  /* Errors that can trigger early returns are done first,
     otherwise nsDOMClassInfo is left in a half inited state. */
  static_assert(sizeof(uintptr_t) == sizeof(void*),
                "BAD! You'll need to adjust the size of uintptr_t to the "
                "size of a pointer on your platform.");

  NS_ENSURE_TRUE(!sIsInitialized, NS_ERROR_ALREADY_INITIALIZED);

  nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  NS_ADDREF(sXPConnect = nsContentUtils::XPConnect());

  nsCOMPtr<nsIXPCFunctionThisTranslator> elt = new nsEventListenerThisTranslator();
  sXPConnect->SetFunctionThisTranslator(NS_GET_IID(nsIDOMEventListener), elt);

  // Initialize static JSString's
  DefineStaticJSVals();

  sIsInitialized = true;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetInterfaces(uint32_t *aCount, nsIID ***aArray)
{
  uint32_t count = 0;

  while (mData->mInterfaces[count]) {
    count++;
  }

  *aCount = count;

  if (!count) {
    *aArray = nullptr;

    return NS_OK;
  }

  *aArray = static_cast<nsIID **>(moz_xmalloc(count * sizeof(nsIID *)));

  uint32_t i;
  for (i = 0; i < count; i++) {
    *((*aArray) + i) = mData->mInterfaces[i]->Clone();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetScriptableHelper(nsIXPCScriptable **_retval)
{
  nsCOMPtr<nsIXPCScriptable> rval = this;
  rval.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetContractID(nsACString& aContractID)
{
  aContractID.SetIsVoid(true);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetClassDescription(nsACString& aClassDescription)
{
  return GetClassName(aClassDescription);
}

NS_IMETHODIMP
nsDOMClassInfo::GetClassID(nsCID **aClassID)
{
  *aClassID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetClassIDNoAlloc(nsCID *aClassID)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsDOMClassInfo::GetFlags(uint32_t *aFlags)
{
  *aFlags = DOMCLASSINFO_STANDARD_FLAGS;

  return NS_OK;
}

// nsIXPCScriptable

NS_IMETHODIMP
nsDOMClassInfo::GetClassName(nsACString& aClassName)
{
  aClassName.Assign(mData->mClass.name);
  return NS_OK;
}

// virtual
uint32_t
nsDOMClassInfo::GetScriptableFlags()
{
  return mData->mScriptableFlags;
}

// virtual
const js::Class*
nsDOMClassInfo::GetClass()
{
    return &mData->mClass;
}

// virtual
const JSClass*
nsDOMClassInfo::GetJSClass()
{
    return Jsvalify(&mData->mClass);
}

NS_IMETHODIMP
nsDOMClassInfo::PreCreate(nsISupports *nativeObj, JSContext *cx,
                          JSObject *globalObj, JSObject **parentObj)
{
  *parentObj = globalObj;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, bool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, JS::AutoIdVector &properties,
                             bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::NewEnumerate Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Resolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *aObj, jsid aId, bool *resolvedp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);

  if (id != sConstructor_id) {
    *resolvedp = false;
    return NS_OK;
  }

  JS::Rooted<JSObject*> global(cx, ::JS_GetGlobalForObject(cx, obj));

  JS::Rooted<JS::PropertyDescriptor> desc(cx);
  if (!JS_GetPropertyDescriptor(cx, global, mData->mClass.name, &desc)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (desc.object() && !desc.hasGetterOrSetter() && desc.value().isObject()) {
    // If val is not an (non-null) object there either is no
    // constructor for this class, or someone messed with
    // window.classname, just fall through and let the JS engine
    // return the Object constructor.
    if (!::JS_DefinePropertyById(cx, obj, id, desc.value(),
                                 JSPROP_ENUMERATE)) {
      return NS_ERROR_UNEXPECTED;
    }

    *resolvedp = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::Finalize(nsIXPConnectWrappedNative *wrapper, JSFreeOp *fop,
                         JSObject *obj)
{
  NS_WARNING("nsDOMClassInfo::Finalize Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Call(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JSObject *obj, const JS::CallArgs &args, bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::Call Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, const JS::CallArgs &args,
                          bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::Construct Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, JS::Handle<JS::Value> val, bool *bp,
                            bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::HasInstance Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::PostCreatePrototype(JSContext * cx, JSObject * aProto)
{
  // Return NS_OK for now because we can temporarily get hit when
  // people look up "DOMConstructor".
  return NS_OK;
}

// static
nsIClassInfo *
NS_GetDOMClassInfoInstance(nsDOMClassInfoID aID)
{
  return nullptr;
}

// static
void
nsDOMClassInfo::ShutDown()
{
  sConstructor_id     = JSID_VOID;
  sWrappedJSObject_id = JSID_VOID;

  NS_IF_RELEASE(sXPConnect);
  sIsInitialized = false;
}

static nsresult
LookupComponentsShim(JSContext *cx, JS::Handle<JSObject*> global,
                     nsPIDOMWindowInner *win,
                     JS::MutableHandle<JS::PropertyDescriptor> desc);

// static
bool
nsWindowSH::NameStructEnabled(JSContext* aCx, nsGlobalWindowInner *aWin,
                              const nsAString& aName,
                              const nsGlobalNameStruct& aNameStruct)
{
  // DOMConstructor is special: creating its proto does not actually define it
  // as a property on the global.  So we don't want to expose its name either.
  return !aName.EqualsLiteral("DOMConstructor");
}

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

// EventTarget helper

NS_IMETHODIMP
nsEventTargetSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                           JSObject *aGlobalObj, JSObject **parentObj)
{
  JS::Rooted<JSObject*> globalObj(cx, aGlobalObj);
  DOMEventTargetHelper* target = DOMEventTargetHelper::FromSupports(nativeObj);

  nsCOMPtr<nsIScriptGlobalObject> native_parent;
  target->GetParentObject(getter_AddRefs(native_parent));

  *parentObj = native_parent ? native_parent->GetGlobalJSObject() : globalObj;

  return *parentObj ? NS_OK : NS_ERROR_FAILURE;
}

void
nsEventTargetSH::PreserveWrapper(nsISupports *aNative)
{
  DOMEventTargetHelper* target = DOMEventTargetHelper::FromSupports(aNative);
  target->PreserveWrapper(aNative);
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
