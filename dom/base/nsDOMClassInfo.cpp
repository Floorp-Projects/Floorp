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
#include "nsIDOMConstructor.h"

// DOM core includes
#include "nsError.h"
#include "nsIDOMXULButtonElement.h"
#include "nsIDOMXULCheckboxElement.h"
#include "nsIDOMXULPopupElement.h"

// Event related includes
#include "nsIDOMEventTarget.h"

// CSS related includes
#include "nsIDOMCSSRule.h"
#include "nsMemory.h"

// includes needed for the prototype chain interfaces

#include "nsIEventListenerService.h"
#include "nsIMessageManager.h"

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

#define NS_DEFINE_CLASSINFO_DATA_HELPER(_class, _helper, _flags,              \
                                        _chromeOnly, _allowXBL)               \
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
    _chromeOnly,                                                              \
    _allowXBL,                                                                \
    false,                                                                    \
    NS_DEFINE_CLASSINFO_DATA_DEBUG(_class)                                    \
  },

#define NS_DEFINE_CLASSINFO_DATA(_class, _helper, _flags)                     \
  NS_DEFINE_CLASSINFO_DATA_HELPER(_class, _helper, _flags, false, false)

#define NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA(_class, _helper, _flags)         \
  NS_DEFINE_CLASSINFO_DATA_HELPER(_class, _helper, _flags, true, false)

#define NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(_class, _helper, _flags)          \
  NS_DEFINE_CLASSINFO_DATA_HELPER(_class, _helper, _flags, true, true)


// This list of NS_DEFINE_CLASSINFO_DATA macros is what gives the DOM
// classes their correct behavior when used through XPConnect. The
// arguments that are passed to NS_DEFINE_CLASSINFO_DATA are
//
// 1. Class name as it should appear in JavaScript, this name is also
//    used to find the id of the class in nsDOMClassInfo
//    (i.e. e<classname>_id)
// 2. Scriptable helper class
// 3. nsIClassInfo/nsIXPCScriptable flags (i.e. for GetScriptableFlags)

static nsDOMClassInfoData sClassInfoData[] = {
  // Base classes

  NS_DEFINE_CLASSINFO_DATA(DOMPrototype, nsDOMConstructorSH,
                           DOM_BASE_SCRIPTABLE_FLAGS |
                           XPC_SCRIPTABLE_WANT_PRECREATE |
                           XPC_SCRIPTABLE_WANT_RESOLVE |
                           XPC_SCRIPTABLE_WANT_HASINSTANCE |
                           XPC_SCRIPTABLE_DONT_ENUM_QUERY_INTERFACE)
  NS_DEFINE_CLASSINFO_DATA(DOMConstructor, nsDOMConstructorSH,
                           DOM_BASE_SCRIPTABLE_FLAGS |
                           XPC_SCRIPTABLE_WANT_PRECREATE |
                           XPC_SCRIPTABLE_WANT_RESOLVE |
                           XPC_SCRIPTABLE_WANT_HASINSTANCE |
                           XPC_SCRIPTABLE_WANT_CALL |
                           XPC_SCRIPTABLE_WANT_CONSTRUCT |
                           XPC_SCRIPTABLE_DONT_ENUM_QUERY_INTERFACE)

  // Misc Core related classes

  NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA(ContentFrameMessageManager,
                                       nsMessageManagerSH<nsEventTargetSH>,
                                       DOM_DEFAULT_SCRIPTABLE_FLAGS |
                                       XPC_SCRIPTABLE_WANT_ENUMERATE |
                                       XPC_SCRIPTABLE_IS_GLOBAL_OBJECT)
  NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA(ContentProcessMessageManager,
                                       nsMessageManagerSH<nsDOMGenericSH>,
                                       DOM_DEFAULT_SCRIPTABLE_FLAGS |
                                       XPC_SCRIPTABLE_WANT_ENUMERATE |
                                       XPC_SCRIPTABLE_IS_GLOBAL_OBJECT)
  NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA(ChromeMessageBroadcaster, nsDOMGenericSH,
                                       DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA(ChromeMessageSender, nsDOMGenericSH,
                                       DOM_DEFAULT_SCRIPTABLE_FLAGS)


  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(XULControlElement, nsDOMGenericSH,
                                      DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(XULLabeledControlElement, nsDOMGenericSH,
                                      DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(XULButtonElement, nsDOMGenericSH,
                                      DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(XULCheckboxElement, nsDOMGenericSH,
                                      DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(XULPopupElement, nsDOMGenericSH,
                                      DOM_DEFAULT_SCRIPTABLE_FLAGS)
};

nsIXPConnect *nsDOMClassInfo::sXPConnect = nullptr;
bool nsDOMClassInfo::sIsInitialized = false;


jsid nsDOMClassInfo::sConstructor_id     = JSID_VOID;
jsid nsDOMClassInfo::sWrappedJSObject_id = JSID_VOID;

static const JSClass *sObjectClass = nullptr;

/**
 * Set our JSClass pointer for the Object class
 */
static void
FindObjectClass(JSContext* cx, JSObject* aGlobalObject)
{
  NS_ASSERTION(!sObjectClass,
               "Double set of sObjectClass");
  JS::Rooted<JSObject*> obj(cx), proto(cx, aGlobalObject);
  do {
    obj = proto;
    js::GetObjectProto(cx, obj, &proto);
  } while (proto);

  sObjectClass = js::GetObjectJSClass(obj);
}

// Helper to handle torn-down inner windows.
static inline nsresult
SetParentToWindow(nsGlobalWindow *win, JSObject **parent)
{
  MOZ_ASSERT(win);
  MOZ_ASSERT(win->IsInnerWindow());
  *parent = win->FastGetGlobalJSObject();

  if (MOZ_UNLIKELY(!*parent)) {
    // The inner window has been torn down. The scope is dying, so don't create
    // any new wrappers.
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// static

nsISupports *
nsDOMClassInfo::GetNative(nsIXPConnectWrappedNative *wrapper, JSObject *obj)
{
  return wrapper ? wrapper->Native() : static_cast<nsISupports*>(js::GetObjectPrivate(obj));
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


static const char *
CutPrefix(const char *aName) {
  static const char prefix_nsIDOM[] = "nsIDOM";
  static const char prefix_nsI[]    = "nsI";

  if (strncmp(aName, prefix_nsIDOM, sizeof(prefix_nsIDOM) - 1) == 0) {
    return aName + sizeof(prefix_nsIDOM) - 1;
  }

  if (strncmp(aName, prefix_nsI, sizeof(prefix_nsI) - 1) == 0) {
    return aName + sizeof(prefix_nsI) - 1;
  }

  return aName;
}

// static
nsresult
nsDOMClassInfo::RegisterClassProtos(int32_t aClassInfoID)
{
  nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);
  bool found_old;

  const nsIID *primary_iid = sClassInfoData[aClassInfoID].mProtoChainInterface;

  if (!primary_iid || primary_iid == &NS_GET_IID(nsISupports)) {
    return NS_OK;
  }

  nsCOMPtr<nsIInterfaceInfoManager>
    iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
  NS_ENSURE_TRUE(iim, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIInterfaceInfo> if_info;
  bool first = true;

  iim->GetInfoForIID(primary_iid, getter_AddRefs(if_info));

  while (if_info) {
    const nsIID *iid = nullptr;

    if_info->GetIIDShared(&iid);
    NS_ENSURE_TRUE(iid, NS_ERROR_UNEXPECTED);

    if (iid->Equals(NS_GET_IID(nsISupports))) {
      break;
    }

    const char *name = nullptr;
    if_info->GetNameShared(&name);
    NS_ENSURE_TRUE(name, NS_ERROR_UNEXPECTED);

    nameSpaceManager->RegisterClassProto(CutPrefix(name), iid, &found_old);

    if (first) {
      first = false;
    } else if (found_old) {
      break;
    }

    nsCOMPtr<nsIInterfaceInfo> tmp(if_info);
    tmp->GetParent(getter_AddRefs(if_info));
  }

  return NS_OK;
}

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

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(DOMPrototype, nsIDOMDOMConstructor)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMConstructor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DOMConstructor, nsIDOMDOMConstructor)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMConstructor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ContentFrameMessageManager, nsISupports)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageListenerManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageSender)
    DOM_CLASSINFO_MAP_ENTRY(nsISyncMessageSender)
    DOM_CLASSINFO_MAP_ENTRY(nsIContentFrameMessageManager)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ContentProcessMessageManager, nsISupports)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageListenerManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageSender)
    DOM_CLASSINFO_MAP_ENTRY(nsISyncMessageSender)
    DOM_CLASSINFO_MAP_ENTRY(nsIContentProcessMessageManager)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ChromeMessageBroadcaster, nsISupports)
    DOM_CLASSINFO_MAP_ENTRY(nsIFrameScriptLoader)
    DOM_CLASSINFO_MAP_ENTRY(nsIProcessScriptLoader)
    DOM_CLASSINFO_MAP_ENTRY(nsIGlobalProcessScriptLoader)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageListenerManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageBroadcaster)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ChromeMessageSender, nsISupports)
    DOM_CLASSINFO_MAP_ENTRY(nsIFrameScriptLoader)
    DOM_CLASSINFO_MAP_ENTRY(nsIProcessScriptLoader)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageListenerManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageSender)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XULControlElement, nsIDOMXULControlElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULControlElement)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XULLabeledControlElement, nsIDOMXULLabeledControlElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULLabeledControlElement)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XULButtonElement, nsIDOMXULButtonElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULButtonElement)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XULCheckboxElement, nsIDOMXULCheckboxElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULCheckboxElement)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XULPopupElement, nsIDOMXULPopupElement)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULPopupElement)
  DOM_CLASSINFO_MAP_END

  static_assert(MOZ_ARRAY_LENGTH(sClassInfoData) == eDOMClassInfoIDCount,
                "The number of items in sClassInfoData doesn't match the "
                "number of nsIDOMClassInfo ID's, this is bad! Fix it!");

#ifdef DEBUG
  for (size_t i = 0; i < eDOMClassInfoIDCount; i++) {
    if (!sClassInfoData[i].mConstructorFptr ||
        sClassInfoData[i].mDebugID != i) {
      MOZ_CRASH("Class info data out of sync, you forgot to update "
                "nsDOMClassInfo.h and nsDOMClassInfo.cpp! Fix this, "
                "mozilla will not work without this fixed!");
    }
  }

  for (size_t i = 0; i < eDOMClassInfoIDCount; i++) {
    if (!sClassInfoData[i].mInterfaces) {
      MOZ_CRASH("Class info data without an interface list! Fix this, "
                "mozilla will not work without this fixed!");
     }
   }
#endif

  // Initialize static JSString's
  DefineStaticJSVals();

  int32_t i;

  for (i = 0; i < eDOMClassInfoIDCount; ++i) {
    if (i == eDOMClassInfo_DOMPrototype_id) {
      continue;
    }

    nsDOMClassInfoData& data = sClassInfoData[i];
    nameSpaceManager->RegisterClassName(data.mClass.name, i, data.mChromeOnly,
                                        data.mAllowXBL, &data.mNameUTF16);
  }

  for (i = 0; i < eDOMClassInfoIDCount; ++i) {
    RegisterClassProtos(i);
  }

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
  NS_ENSURE_TRUE(*aArray, NS_ERROR_OUT_OF_MEMORY);

  uint32_t i;
  for (i = 0; i < count; i++) {
    nsIID *iid = static_cast<nsIID *>(nsMemory::Clone(mData->mInterfaces[i],
                                                         sizeof(nsIID)));

    if (!iid) {
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(i, *aArray);

      return NS_ERROR_OUT_OF_MEMORY;
    }

    *((*aArray) + i) = iid;
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
nsDOMClassInfo::GetContractID(char **aContractID)
{
  *aContractID = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::GetClassDescription(char **aClassDescription)
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
nsDOMClassInfo::GetClassName(char **aClassName)
{
  *aClassName = NS_strdup(mData->mClass.name);

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
nsDOMClassInfo::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, JS::Value *vp,
                            bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::GetProperty Don't call me!");

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, JS::Value *vp,
                            bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::SetProperty Don't call me!");

  return NS_ERROR_UNEXPECTED;
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
                                 JSPROP_ENUMERATE,
                                 JS_STUBGETTER, JS_STUBSETTER)) {
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

static nsresult
ResolvePrototype(nsIXPConnect *aXPConnect, nsGlobalWindow *aWin, JSContext *cx,
                 JS::Handle<JSObject*> obj, const char16_t *name,
                 const nsDOMClassInfoData *ci_data,
                 const nsGlobalNameStruct *name_struct,
                 nsScriptNameSpaceManager *nameSpaceManager,
                 JSObject *dot_prototype,
                 JS::MutableHandle<JS::PropertyDescriptor> ctorDesc);

NS_IMETHODIMP
nsDOMClassInfo::PostCreatePrototype(JSContext * cx, JSObject * aProto)
{
  JS::Rooted<JSObject*> proto(cx, aProto);

  // This is called before any other location that requires
  // sObjectClass, so compute it here. We assume that nobody has had a
  // chance to monkey around with proto's prototype chain before this.
  if (!sObjectClass) {
    FindObjectClass(cx, proto);
    NS_ASSERTION(sObjectClass && !strcmp(sObjectClass->name, "Object"),
                 "Incorrect object class!");
  }

#ifdef DEBUG
    JS::Rooted<JSObject*> proto2(cx);
    JS_GetPrototype(cx, proto, &proto2);
    NS_ASSERTION(proto2 && JS_GetClass(proto2) == sObjectClass,
                 "Hmm, somebody did something evil?");
#endif

#ifdef DEBUG
  if (mData->mHasClassInterface && mData->mProtoChainInterface &&
      mData->mProtoChainInterface != &NS_GET_IID(nsISupports)) {
    nsCOMPtr<nsIInterfaceInfoManager>
      iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));

    if (iim) {
      nsCOMPtr<nsIInterfaceInfo> if_info;
      iim->GetInfoForIID(mData->mProtoChainInterface,
                         getter_AddRefs(if_info));

      if (if_info) {
        nsXPIDLCString name;
        if_info->GetName(getter_Copies(name));
        NS_ASSERTION(nsCRT::strcmp(CutPrefix(name), mData->mClass.name) == 0,
                     "Class name and proto chain interface name mismatch!");
      }
    }
  }
#endif

  // Make prototype delegation work correctly. Consider if a site sets
  // HTMLElement.prototype.foopy = function () { ... } Now, calling
  // document.body.foopy() needs to ensure that looking up foopy on
  // document.body's prototype will find the right function.
  JS::Rooted<JSObject*> global(cx, ::JS_GetGlobalForObject(cx, proto));

  // Only do this if the global object is a window.
  // XXX Is there a better way to check this?
  nsISupports *globalNative = XPConnect()->GetNativeOfWrapper(cx, global);
  nsCOMPtr<nsPIDOMWindowInner> piwin = do_QueryInterface(globalNative);
  if (!piwin) {
    return NS_OK;
  }

  nsGlobalWindow *win = nsGlobalWindow::Cast(piwin);
  if (win->IsClosedOrClosing()) {
    return NS_OK;
  }

  // If the window is in a different compartment than the global object, then
  // it's likely that global is a sandbox object whose prototype is a window.
  // Don't do anything in this case.
  if (win->FastGetGlobalJSObject() &&
      js::GetObjectCompartment(global) != js::GetObjectCompartment(win->FastGetGlobalJSObject())) {
    return NS_OK;
  }

  // Don't overwrite a property set by content.
  bool contentDefinedProperty;
  if (!::JS_AlreadyHasOwnUCProperty(cx, global, reinterpret_cast<const char16_t*>(mData->mNameUTF16),
                                    NS_strlen(mData->mNameUTF16),
                                    &contentDefinedProperty)) {
    return NS_ERROR_FAILURE;
  }

  nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_OK);

  JS::Rooted<JS::PropertyDescriptor> desc(cx);
  nsresult rv = ResolvePrototype(sXPConnect, win, cx, global, mData->mNameUTF16,
                                 mData, nullptr, nameSpaceManager, proto,
                                 &desc);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!contentDefinedProperty && desc.object() && !desc.value().isUndefined()) {
    desc.attributesRef() |= JSPROP_RESOLVING;
    if (!JS_DefineUCProperty(cx, global, mData->mNameUTF16,
                             NS_strlen(mData->mNameUTF16), desc)) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
}

// static
nsIClassInfo *
NS_GetDOMClassInfoInstance(nsDOMClassInfoID aID)
{
  if (aID >= eDOMClassInfoIDCount) {
    NS_ERROR("Bad ID!");

    return nullptr;
  }

  nsresult rv = RegisterDOMNames();
  NS_ENSURE_SUCCESS(rv, nullptr);

  if (!sClassInfoData[aID].mCachedClassInfo) {
    nsDOMClassInfoData& data = sClassInfoData[aID];

    data.mCachedClassInfo = data.mConstructorFptr(&data);
    NS_ENSURE_TRUE(data.mCachedClassInfo, nullptr);

    NS_ADDREF(data.mCachedClassInfo);
  }

  return sClassInfoData[aID].mCachedClassInfo;
}

// static
void
nsDOMClassInfo::ShutDown()
{
  if (sClassInfoData[0].mConstructorFptr) {
    uint32_t i;

    for (i = 0; i < eDOMClassInfoIDCount; i++) {
      NS_IF_RELEASE(sClassInfoData[i].mCachedClassInfo);
    }
  }

  sConstructor_id     = JSID_VOID;
  sWrappedJSObject_id = JSID_VOID;

  NS_IF_RELEASE(sXPConnect);
  sIsInitialized = false;
}

static nsresult
BaseStubConstructor(nsIWeakReference* aWeakOwner,
                    const nsGlobalNameStruct *name_struct, JSContext *cx,
                    JS::Handle<JSObject*> obj, const JS::CallArgs &args)
{
  MOZ_ASSERT(obj);
  MOZ_ASSERT(cx == nsContentUtils::GetCurrentJSContext());

  nsresult rv;
  nsCOMPtr<nsISupports> native;
  if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    rv = NS_ERROR_NOT_AVAILABLE;
  } else {
    MOZ_ASSERT(name_struct->mType ==
               nsGlobalNameStruct::eTypeExternalConstructor);
    native = do_CreateInstance(name_struct->mCID, &rv);
  }
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to create the object");
    return rv;
  }

  js::AssertSameCompartment(cx, obj);
  return nsContentUtils::WrapNative(cx, native, args.rval(), true);
}

static nsresult
DefineInterfaceConstants(JSContext *cx, JS::Handle<JSObject*> obj, const nsIID *aIID)
{
  nsCOMPtr<nsIInterfaceInfoManager>
    iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
  NS_ENSURE_TRUE(iim, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIInterfaceInfo> if_info;

  nsresult rv = iim->GetInfoForIID(aIID, getter_AddRefs(if_info));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && if_info, rv);

  uint16_t constant_count;

  if_info->GetConstantCount(&constant_count);

  if (!constant_count) {
    return NS_OK;
  }

  nsCOMPtr<nsIInterfaceInfo> parent_if_info;

  rv = if_info->GetParent(getter_AddRefs(parent_if_info));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && parent_if_info, rv);

  uint16_t parent_constant_count, i;
  parent_if_info->GetConstantCount(&parent_constant_count);

  JS::Rooted<JS::Value> v(cx);
  for (i = parent_constant_count; i < constant_count; i++) {
    nsXPIDLCString name;
    rv = if_info->GetConstant(i, &v, getter_Copies(name));
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv), rv);

    if (!::JS_DefineProperty(cx, obj, name, v,
                             JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT,
                             JS_STUBGETTER, JS_STUBSETTER)) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
}

class nsDOMConstructor final : public nsIDOMDOMConstructor
{
protected:
  nsDOMConstructor(const char16_t* aName,
                   bool aIsConstructable,
                   nsPIDOMWindowInner* aOwner)
    : mClassName(aName),
      mConstructable(aIsConstructable),
      mWeakOwner(do_GetWeakReference(aOwner))
  {
  }

  ~nsDOMConstructor() {}

public:

  static nsresult Create(const char16_t* aName,
                         const nsGlobalNameStruct* aNameStruct,
                         nsPIDOMWindowInner* aOwner,
                         nsDOMConstructor** aResult);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDOMCONSTRUCTOR

  nsresult PreCreate(JSContext *cx, JSObject *globalObj, JSObject **parentObj);

  nsresult Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JS::Handle<JSObject*> obj, const JS::CallArgs &args,
                     bool *_retval);

  nsresult HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JS::Handle<JSObject*> obj, const JS::Value &val, bool *bp,
                       bool *_retval);

  nsresult ResolveInterfaceConstants(JSContext *cx, JS::Handle<JSObject*> obj);

private:
  const nsGlobalNameStruct *GetNameStruct()
  {
    if (!mClassName) {
      NS_ERROR("Can't get name");
      return nullptr;
    }

    const nsGlobalNameStruct *nameStruct;
#ifdef DEBUG
    nsresult rv =
#endif
      GetNameStruct(nsDependentString(mClassName), &nameStruct);

    NS_ASSERTION(NS_FAILED(rv) || nameStruct, "Name isn't in hash.");

    return nameStruct;
  }

  static nsresult GetNameStruct(const nsAString& aName,
                                const nsGlobalNameStruct **aNameStruct)
  {
    *aNameStruct = nullptr;

    nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
    if (!nameSpaceManager) {
      NS_ERROR("Can't get namespace manager.");
      return NS_ERROR_UNEXPECTED;
    }

    *aNameStruct = nameSpaceManager->LookupName(aName);

    // Return NS_OK here, aName just isn't a DOM class but nothing failed.
    return NS_OK;
  }

  static bool IsConstructable(const nsGlobalNameStruct *aNameStruct)
  {
    return aNameStruct->mType == nsGlobalNameStruct::eTypeExternalConstructor;
  }

  const char16_t*   mClassName;
  const bool mConstructable;
  nsWeakPtr          mWeakOwner;
};

//static
nsresult
nsDOMConstructor::Create(const char16_t* aName,
                         const nsGlobalNameStruct* aNameStruct,
                         nsPIDOMWindowInner* aOwner,
                         nsDOMConstructor** aResult)
{
  *aResult = nullptr;
  // Prevent creating a constructor if aOwner is inner window which doesn't have
  // an outer window. If the outer window doesn't have an inner window or the
  // caller can't access the outer window's current inner window then try to use
  // the owner (so long as it is, in fact, an inner window). If that doesn't
  // work then prevent creation also.
  nsPIDOMWindowOuter* outerWindow = aOwner->GetOuterWindow();
  nsPIDOMWindowInner* currentInner =
    outerWindow ? outerWindow->GetCurrentInnerWindow() : aOwner;
  if (!currentInner ||
      (aOwner != currentInner &&
       !nsContentUtils::CanCallerAccess(currentInner) &&
       !(currentInner = aOwner)->IsInnerWindow())) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  bool constructable = aNameStruct && IsConstructable(aNameStruct);

  *aResult = new nsDOMConstructor(aName, constructable, currentInner);
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMPL_ADDREF(nsDOMConstructor)
NS_IMPL_RELEASE(nsDOMConstructor)
NS_INTERFACE_MAP_BEGIN(nsDOMConstructor)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMConstructor)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
#ifdef DEBUG
    {
      const nsGlobalNameStruct *name_struct = GetNameStruct();
      NS_ASSERTION(!name_struct ||
                   mConstructable == IsConstructable(name_struct),
                   "Can't change constructability dynamically!");
    }
#endif
    foundInterface =
      NS_GetDOMClassInfoInstance(mConstructable ?
                                 eDOMClassInfo_DOMConstructor_id :
                                 eDOMClassInfo_DOMPrototype_id);
    if (!foundInterface) {
      *aInstancePtr = nullptr;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else
NS_INTERFACE_MAP_END

nsresult
nsDOMConstructor::PreCreate(JSContext *cx, JSObject *globalObj, JSObject **parentObj)
{
  nsCOMPtr<nsPIDOMWindowInner> owner(do_QueryReferent(mWeakOwner));
  if (!owner) {
    // Can't do anything.
    return NS_OK;
  }

  nsGlobalWindow *win = nsGlobalWindow::Cast(owner);
  return SetParentToWindow(win, parentObj);
}

nsresult
nsDOMConstructor::Construct(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                            JS::Handle<JSObject*> obj, const JS::CallArgs &args,
                            bool *_retval)
{
  MOZ_ASSERT(obj);

  const nsGlobalNameStruct *name_struct = GetNameStruct();
  NS_ENSURE_TRUE(name_struct, NS_ERROR_FAILURE);

  if (!IsConstructable(name_struct)) {
    // ignore return value, we return false anyway
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  return BaseStubConstructor(mWeakOwner, name_struct, cx, obj, args);
}

nsresult
nsDOMConstructor::HasInstance(nsIXPConnectWrappedNative *wrapper,
                              JSContext * cx, JS::Handle<JSObject*> obj,
                              const JS::Value &v, bool *bp, bool *_retval)

{
  // No need to look these up in the hash.
  *bp = false;
  if (v.isPrimitive()) {
    return NS_OK;
  }

  JS::Rooted<JSObject*> dom_obj(cx, v.toObjectOrNull());
  NS_ASSERTION(dom_obj, "nsDOMConstructor::HasInstance couldn't get object");

  // This might not be the right object, if there are wrappers. Unwrap if we can.
  JSObject *wrapped_obj = js::CheckedUnwrap(dom_obj, /* stopAtWindowProxy = */ false);
  if (wrapped_obj)
      dom_obj = wrapped_obj;

  const JSClass *dom_class = JS_GetClass(dom_obj);
  if (!dom_class) {
    NS_ERROR("nsDOMConstructor::HasInstance can't get class.");
    return NS_ERROR_UNEXPECTED;
  }

  const nsGlobalNameStruct *name_struct;
  nsresult rv = GetNameStruct(NS_ConvertASCIItoUTF16(dom_class->name), &name_struct);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!name_struct) {
    // This isn't a normal DOM object, see if this constructor lives on its
    // prototype chain.
    JS::Rooted<JS::PropertyDescriptor> desc(cx);
    if (!JS_GetPropertyDescriptor(cx, obj, "prototype", &desc)) {
      return NS_ERROR_UNEXPECTED;
    }

    if (!desc.object() || desc.hasGetterOrSetter() || !desc.value().isObject()) {
      return NS_OK;
    }

    JS::Rooted<JSObject*> dot_prototype(cx, &desc.value().toObject());

    JS::Rooted<JSObject*> proto(cx, dom_obj);
    JSAutoCompartment ac(cx, proto);

    if (!JS_WrapObject(cx, &dot_prototype)) {
      return NS_ERROR_UNEXPECTED;
    }

    for (;;) {
      if (!JS_GetPrototype(cx, proto, &proto)) {
        return NS_ERROR_UNEXPECTED;
      }
      if (!proto) {
        break;
      }
      if (proto == dot_prototype) {
        *bp = true;
        break;
      }
    }

    return NS_OK;
  }

  if (name_struct->mType != nsGlobalNameStruct::eTypeClassConstructor) {
    // Doesn't have DOM interfaces.
    return NS_OK;
  }

  const nsGlobalNameStruct *class_name_struct = GetNameStruct();
  NS_ENSURE_TRUE(class_name_struct, NS_ERROR_FAILURE);

  if (name_struct == class_name_struct) {
    *bp = true;

    return NS_OK;
  }

  const nsIID *class_iid;
  if (class_name_struct->mType == nsGlobalNameStruct::eTypeClassProto) {
    class_iid = &class_name_struct->mIID;
  } else if (class_name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    class_iid =
      sClassInfoData[class_name_struct->mDOMClassInfoID].mProtoChainInterface;
  } else {
    *bp = false;

    return NS_OK;
  }

  NS_ASSERTION(name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor,
               "The constructor was set up with a struct of the wrong type.");

  const nsDOMClassInfoData *ci_data;
  if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor &&
      name_struct->mDOMClassInfoID >= 0) {
    ci_data = &sClassInfoData[name_struct->mDOMClassInfoID];
  } else {
    ci_data = nullptr;
  }

  nsCOMPtr<nsIInterfaceInfoManager>
    iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
  if (!iim) {
    NS_ERROR("nsDOMConstructor::HasInstance can't get interface info mgr.");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIInterfaceInfo> if_info;
  uint32_t count = 0;
  const nsIID* class_interface;
  while ((class_interface = ci_data->mInterfaces[count++])) {
    if (class_iid->Equals(*class_interface)) {
      *bp = true;

      return NS_OK;
    }

    iim->GetInfoForIID(class_interface, getter_AddRefs(if_info));
    if (!if_info) {
      NS_ERROR("nsDOMConstructor::HasInstance can't get interface info.");
      return NS_ERROR_UNEXPECTED;
    }

    if_info->HasAncestor(class_iid, bp);

    if (*bp) {
      return NS_OK;
    }
  }

  return NS_OK;
}

nsresult
nsDOMConstructor::ResolveInterfaceConstants(JSContext *cx, JS::Handle<JSObject*> obj)
{
  const nsGlobalNameStruct *class_name_struct = GetNameStruct();
  if (!class_name_struct)
    return NS_ERROR_UNEXPECTED;

  const nsIID *class_iid;
  if (class_name_struct->mType == nsGlobalNameStruct::eTypeClassProto) {
    class_iid = &class_name_struct->mIID;
  } else if (class_name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    class_iid =
      sClassInfoData[class_name_struct->mDOMClassInfoID].mProtoChainInterface;
  } else {
    return NS_OK;
  }

  nsresult rv = DefineInterfaceConstants(cx, obj, class_iid);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMConstructor::ToString(nsAString &aResult)
{
  aResult.AssignLiteral("[object ");
  aResult.Append(mClassName);
  aResult.Append(char16_t(']'));

  return NS_OK;
}


static nsresult
GetXPCProto(nsIXPConnect *aXPConnect, JSContext *cx, nsGlobalWindow *aWin,
            const nsGlobalNameStruct *aNameStruct,
            JS::MutableHandle<JSObject*> aProto)
{
  NS_ASSERTION(aNameStruct->mType == nsGlobalNameStruct::eTypeClassConstructor,
               "Wrong type!");

  int32_t id = aNameStruct->mDOMClassInfoID;
  MOZ_ASSERT(id >= 0, "Negative DOM classinfo?!?");

  nsDOMClassInfoID ci_id = (nsDOMClassInfoID)id;

  nsCOMPtr<nsIClassInfo> ci = NS_GetDOMClassInfoInstance(ci_id);
  NS_ENSURE_TRUE(ci, NS_ERROR_UNEXPECTED);

  nsresult rv =
    aXPConnect->GetWrappedNativePrototype(cx, aWin->GetGlobalJSObject(), ci, aProto.address());
  NS_ENSURE_SUCCESS(rv, rv);

  return JS_WrapObject(cx, aProto) ? NS_OK : NS_ERROR_FAILURE;
}

// Either ci_data must be non-null or name_struct must be non-null and of type
// eTypeClassProto.
static nsresult
ResolvePrototype(nsIXPConnect *aXPConnect, nsGlobalWindow *aWin, JSContext *cx,
                 JS::Handle<JSObject*> obj, const char16_t *name,
                 const nsDOMClassInfoData *ci_data,
                 const nsGlobalNameStruct *name_struct,
                 nsScriptNameSpaceManager *nameSpaceManager,
                 JSObject* aDot_prototype,
                 JS::MutableHandle<JS::PropertyDescriptor> ctorDesc)
{
  JS::Rooted<JSObject*> dot_prototype(cx, aDot_prototype);
  NS_ASSERTION(ci_data ||
               (name_struct &&
                name_struct->mType == nsGlobalNameStruct::eTypeClassProto),
               "Wrong type or missing ci_data!");

  RefPtr<nsDOMConstructor> constructor;
  nsresult rv = nsDOMConstructor::Create(name, name_struct, aWin->AsInner(),
                                         getter_AddRefs(constructor));
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JS::Value> v(cx);

  js::AssertSameCompartment(cx, obj);
  rv = nsContentUtils::WrapNative(cx, constructor,
                                  &NS_GET_IID(nsIDOMDOMConstructor), &v,
                                  false);
  NS_ENSURE_SUCCESS(rv, rv);

  FillPropertyDescriptor(ctorDesc, obj, 0, v);
  // And make sure we wrap the value into the right compartment.  Note that we
  // do this with ctorDesc.value(), not with v, because we need v to be in the
  // right compartment (that of the reflector of |constructor|) below.
  if (!JS_WrapValue(cx, ctorDesc.value())) {
    return NS_ERROR_UNEXPECTED;
  }

  JS::Rooted<JSObject*> class_obj(cx, &v.toObject());

  const nsIID *primary_iid = &NS_GET_IID(nsISupports);

  if (!ci_data) {
    primary_iid = &name_struct->mIID;
  }
  else if (ci_data->mProtoChainInterface) {
    primary_iid = ci_data->mProtoChainInterface;
  }

  nsCOMPtr<nsIInterfaceInfo> if_info;
  nsCOMPtr<nsIInterfaceInfo> parent;
  const char *class_parent_name = nullptr;

  if (!primary_iid->Equals(NS_GET_IID(nsISupports))) {
    JSAutoCompartment ac(cx, class_obj);

    rv = DefineInterfaceConstants(cx, class_obj, primary_iid);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIInterfaceInfoManager>
      iim(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
    NS_ENSURE_TRUE(iim, NS_ERROR_NOT_AVAILABLE);

    iim->GetInfoForIID(primary_iid, getter_AddRefs(if_info));
    NS_ENSURE_TRUE(if_info, NS_ERROR_UNEXPECTED);

    const nsIID *iid = nullptr;

    if (ci_data && !ci_data->mHasClassInterface) {
      if_info->GetIIDShared(&iid);
    } else {
      if_info->GetParent(getter_AddRefs(parent));
      NS_ENSURE_TRUE(parent, NS_ERROR_UNEXPECTED);

      parent->GetIIDShared(&iid);
    }

    if (iid) {
      if (!iid->Equals(NS_GET_IID(nsISupports))) {
        if (ci_data && !ci_data->mHasClassInterface) {
          // If the class doesn't have a class interface the primary
          // interface is the interface that should be
          // constructor.prototype.__proto__.

          if_info->GetNameShared(&class_parent_name);
        } else {
          // If the class does have a class interface (or there's no
          // real class for this name) then the parent of the
          // primary interface is what we want on
          // constructor.prototype.__proto__.

          NS_ASSERTION(parent, "Whoa, this is bad, null parent here!");

          parent->GetNameShared(&class_parent_name);
        }
      }
    }
  }

  {
    JS::Rooted<JSObject*> winobj(cx, aWin->FastGetGlobalJSObject());

    JS::Rooted<JSObject*> proto(cx);

    if (class_parent_name) {
      JSAutoCompartment ac(cx, winobj);

      JS::Rooted<JS::PropertyDescriptor> desc(cx);
      if (!JS_GetPropertyDescriptor(cx, winobj, CutPrefix(class_parent_name), &desc)) {
        return NS_ERROR_UNEXPECTED;
      }

      if (desc.object() && !desc.hasGetterOrSetter() && desc.value().isObject()) {
        JS::Rooted<JSObject*> obj(cx, &desc.value().toObject());
        if (!JS_GetPropertyDescriptor(cx, obj, "prototype", &desc)) {
          return NS_ERROR_UNEXPECTED;
        }

        if (desc.object() && !desc.hasGetterOrSetter() && desc.value().isObject()) {
          proto = &desc.value().toObject();
        }
      }
    }

    if (dot_prototype) {
      JSAutoCompartment ac(cx, dot_prototype);
      JS::Rooted<JSObject*> xpc_proto_proto(cx);
      if (!::JS_GetPrototype(cx, dot_prototype, &xpc_proto_proto)) {
        return NS_ERROR_UNEXPECTED;
      }

      if (proto &&
          (!xpc_proto_proto ||
           JS_GetClass(xpc_proto_proto) == sObjectClass)) {
        if (!JS_WrapObject(cx, &proto) ||
            !JS_SetPrototype(cx, dot_prototype, proto)) {
          return NS_ERROR_UNEXPECTED;
        }
      }
    } else {
      JSAutoCompartment ac(cx, winobj);
      if (!proto) {
        proto = JS_GetObjectPrototype(cx, winobj);
      }
      dot_prototype = ::JS_NewObjectWithUniqueType(cx,
                                                   &sDOMConstructorProtoClass,
                                                   proto);
      NS_ENSURE_TRUE(dot_prototype, NS_ERROR_OUT_OF_MEMORY);
    }
  }

  v.setObject(*dot_prototype);

  JSAutoCompartment ac(cx, class_obj);

  // Per ECMA, the prototype property is {DontEnum, DontDelete, ReadOnly}
  if (!JS_WrapValue(cx, &v) ||
      !JS_DefineProperty(cx, class_obj, "prototype", v,
                         JSPROP_PERMANENT | JSPROP_READONLY,
                         JS_STUBGETTER, JS_STUBSETTER)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

static bool
OldBindingConstructorEnabled(const nsGlobalNameStruct *aStruct,
                             nsGlobalWindow *aWin, JSContext *cx)
{
  MOZ_ASSERT(aStruct->mType == nsGlobalNameStruct::eTypeProperty ||
             aStruct->mType == nsGlobalNameStruct::eTypeClassConstructor);

  // Don't expose chrome only constructors to content windows.
  if (aStruct->mChromeOnly) {
    bool expose;
    if (aStruct->mAllowXBL) {
      expose = IsChromeOrXBL(cx, nullptr);
    } else {
      expose = nsContentUtils::IsSystemPrincipal(aWin->GetPrincipal());
    }

    if (!expose) {
      return false;
    }
  }

  return true;
}

static nsresult
LookupComponentsShim(JSContext *cx, JS::Handle<JSObject*> global,
                     nsPIDOMWindowInner *win,
                     JS::MutableHandle<JS::PropertyDescriptor> desc);

// static
bool
nsWindowSH::NameStructEnabled(JSContext* aCx, nsGlobalWindow *aWin,
                              const nsAString& aName,
                              const nsGlobalNameStruct& aNameStruct)
{
  // DOMConstructor is special: creating its proto does not actually define it
  // as a property on the global.  So we don't want to expose its name either.
  if (aName.EqualsLiteral("DOMConstructor")) {
    return false;
  }
  const nsGlobalNameStruct* nameStruct = &aNameStruct;
  return (nameStruct->mType != nsGlobalNameStruct::eTypeProperty &&
          nameStruct->mType != nsGlobalNameStruct::eTypeClassConstructor) ||
         OldBindingConstructorEnabled(nameStruct, aWin, aCx);
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
nsWindowSH::GlobalResolve(nsGlobalWindow *aWin, JSContext *cx,
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
      aWin->GetDoc()->WarnOnceAbout(nsIDocument::eWindow_Controllers);
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

  if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    if (!OldBindingConstructorEnabled(name_struct, aWin, cx)) {
      return NS_OK;
    }

    // Create the XPConnect prototype for our classinfo, PostCreateProto will
    // set up the prototype chain.  This will go ahead and define things on the
    // actual window's global.
    JS::Rooted<JSObject*> dot_prototype(cx);
    rv = GetXPCProto(nsDOMClassInfo::sXPConnect, cx, aWin, name_struct,
                     &dot_prototype);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(dot_prototype);

    bool isXray = xpc::WrapperFactory::IsXrayWrapper(obj);
    MOZ_ASSERT_IF(obj != aWin->GetGlobalJSObject(), isXray);
    if (!isXray) {
      // GetXPCProto already defined the property for us
      FillPropertyDescriptor(desc, obj, JS::UndefinedValue(), false);
      return NS_OK;
    }

    // This is the Xray case.  Look up the constructor object for this
    // prototype.
    return ResolvePrototype(nsDOMClassInfo::sXPConnect, aWin, cx, obj,
                            class_name,
                            &sClassInfoData[name_struct->mDOMClassInfoID],
                            name_struct, nameSpaceManager, dot_prototype,
                            desc);
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeClassProto) {
    // We don't have a XPConnect prototype object, let ResolvePrototype create
    // one.
    return ResolvePrototype(nsDOMClassInfo::sXPConnect, aWin, cx, obj,
                            class_name, nullptr,
                            name_struct, nameSpaceManager, nullptr, desc);
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructor) {
    RefPtr<nsDOMConstructor> constructor;
    rv = nsDOMConstructor::Create(class_name, name_struct, aWin->AsInner(),
                                  getter_AddRefs(constructor));
    NS_ENSURE_SUCCESS(rv, rv);

    JS::Rooted<JS::Value> val(cx);
    js::AssertSameCompartment(cx, obj);
    rv = nsContentUtils::WrapNative(cx, constructor,
                                    &NS_GET_IID(nsIDOMDOMConstructor), &val,
                                    true);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(val.isObject(), "Why didn't we get a JSObject?");

    FillPropertyDescriptor(desc, obj, 0, val);

    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeProperty) {
    if (!OldBindingConstructorEnabled(name_struct, aWin, cx))
      return NS_OK;

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
                      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY,
                      JS_STUBGETTER, JS_STUBSETTER);
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
                           JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY,
                           JS_STUBGETTER, JS_STUBSETTER);
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

NS_IMETHODIMP
nsDOMConstructorSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                              JSObject *aGlobalObj, JSObject **parentObj)
{
  JS::Rooted<JSObject*> globalObj(cx, aGlobalObj);
  nsDOMConstructor *wrapped = static_cast<nsDOMConstructor *>(nativeObj);

#ifdef DEBUG
  {
    nsCOMPtr<nsIDOMDOMConstructor> is_constructor =
      do_QueryInterface(nativeObj);
    NS_ASSERTION(is_constructor, "How did we not get a constructor?");
  }
#endif

  return wrapped->PreCreate(cx, globalObj, parentObj);
}

NS_IMETHODIMP
nsDOMConstructorSH::Resolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *aObj, jsid aId, bool *resolvedp,
                            bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  // For regular DOM constructors, we have our interface constants defined on
  // us by nsWindowSH::GlobalResolve. However, XrayWrappers can't see these
  // interface constants (as they look like expando properties) so we have to
  // specially resolve those constants here, but only for Xray wrappers.
  if (!ObjectIsNativeWrapper(cx, obj)) {
    return NS_OK;
  }

  JS::Rooted<JSObject*> nativePropsObj(cx, xpc::XrayUtils::GetNativePropertiesObject(cx, obj));
  nsDOMConstructor *wrapped =
    static_cast<nsDOMConstructor *>(wrapper->Native());
  nsresult rv = wrapped->ResolveInterfaceConstants(cx, nativePropsObj);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now re-lookup the ID to see if we should report back that we resolved the
  // looked-for constant. Note that we don't have to worry about infinitely
  // recurring back here because the Xray wrapper's holder object doesn't call
  // Resolve hooks.
  bool found;
  if (!JS_HasPropertyById(cx, nativePropsObj, id, &found)) {
    *_retval = false;
    return NS_OK;
  }

  if (found) {
    *resolvedp = true;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMConstructorSH::Call(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *aObj, const JS::CallArgs &args, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  MOZ_ASSERT(obj);

  nsDOMConstructor *wrapped =
    static_cast<nsDOMConstructor *>(wrapper->Native());

#ifdef DEBUG
  {
    nsCOMPtr<nsIDOMDOMConstructor> is_constructor =
      do_QueryWrappedNative(wrapper);
    NS_ASSERTION(is_constructor, "How did we not get a constructor?");
  }
#endif

  return wrapped->Construct(wrapper, cx, obj, args, _retval);
}

NS_IMETHODIMP
nsDOMConstructorSH::Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                              JSObject *aObj, const JS::CallArgs &args, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  MOZ_ASSERT(obj);

  nsDOMConstructor *wrapped =
    static_cast<nsDOMConstructor *>(wrapper->Native());

#ifdef DEBUG
  {
    nsCOMPtr<nsIDOMDOMConstructor> is_constructor =
      do_QueryWrappedNative(wrapper);
    NS_ASSERTION(is_constructor, "How did we not get a constructor?");
  }
#endif

  return wrapped->Construct(wrapper, cx, obj, args, _retval);
}

NS_IMETHODIMP
nsDOMConstructorSH::HasInstance(nsIXPConnectWrappedNative *wrapper,
                                JSContext *cx, JSObject *aObj, JS::Handle<JS::Value> val,
                                bool *bp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  nsDOMConstructor *wrapped =
    static_cast<nsDOMConstructor *>(wrapper->Native());

#ifdef DEBUG
  {
    nsCOMPtr<nsIDOMDOMConstructor> is_constructor =
      do_QueryWrappedNative(wrapper);
    NS_ASSERTION(is_constructor, "How did we not get a constructor?");
  }
#endif

  return wrapped->HasInstance(wrapper, cx, obj, val, bp, _retval);
}

// nsContentFrameMessageManagerSH

template<typename Super>
NS_IMETHODIMP
nsMessageManagerSH<Super>::Resolve(nsIXPConnectWrappedNative* wrapper,
                                   JSContext* cx, JSObject* obj_,
                                   jsid id_, bool* resolvedp,
                                   bool* _retval)
{
  JS::Rooted<JSObject*> obj(cx, obj_);
  JS::Rooted<jsid> id(cx, id_);

  *_retval = SystemGlobalResolve(cx, obj, id, resolvedp);
  NS_ENSURE_TRUE(*_retval, NS_ERROR_FAILURE);

  if (*resolvedp) {
    return NS_OK;
  }

  return Super::Resolve(wrapper, cx, obj, id, resolvedp, _retval);
}

template<typename Super>
NS_IMETHODIMP
nsMessageManagerSH<Super>::Enumerate(nsIXPConnectWrappedNative* wrapper,
                                     JSContext* cx, JSObject* obj_,
                                     bool* _retval)
{
  JS::Rooted<JSObject*> obj(cx, obj_);

  *_retval = SystemGlobalEnumerate(cx, obj);
  NS_ENSURE_TRUE(*_retval, NS_ERROR_FAILURE);

  // Don't call up to our superclass, since neither nsDOMGenericSH nor
  // nsEventTargetSH have WANT_ENUMERATE.
  return NS_OK;
}
