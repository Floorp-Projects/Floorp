/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
// On top because they include basictypes.h:
#include "mozilla/dom/SmsFilter.h"

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
#include "XPCWrapper.h"

#include "mozilla/dom/RegisterBindings.h"

#include "nscore.h"
#include "nsDOMClassInfo.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "nsICategoryManager.h"
#include "nsIComponentRegistrar.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIXPConnect.h"
#include "nsIXPCSecurityManager.h"
#include "xptcall.h"
#include "nsTArray.h"
#include "nsDOMEventTargetHelper.h"
#include "nsDocument.h" // nsDOMStyleSheetList
#include "nsDOMBlobBuilder.h"

// General helper includes
#include "nsGlobalWindow.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsIDOMWindowUtils.h"
#include "nsIDOMGlobalPropertyInitializer.h"
#include "nsLocation.h"
#include "mozilla/Attributes.h"
#include "mozilla/Telemetry.h"

// Window scriptable helper includes
#include "nsIDocShell.h"
#include "nsIScriptExternalNameSet.h"
#include "nsJSUtils.h"
#include "nsScriptNameSpaceManager.h"
#include "nsIJSNativeInitializer.h"
#include "nsJSEnvironment.h"

// DOM base includes
#include "nsIDOMLocation.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMJSWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMConstructor.h"

// DOM core includes
#include "nsError.h"
#include "nsIDOMUserDataHandler.h"
#include "nsIDOMXPathNamespace.h"
#include "nsIDOMXULButtonElement.h"
#include "nsIDOMXULCheckboxElement.h"
#include "nsIDOMXULPopupElement.h"

// Event related includes
#include "nsIDOMEventTarget.h"

// CSS related includes
#include "nsCSSRules.h"
#include "nsIDOMStyleSheetList.h"
#include "nsIDOMCSSRule.h"
#include "nsICSSRuleList.h"
#include "nsAutoPtr.h"
#include "nsMemory.h"

// Tranformiix
#include "nsIXSLTProcessor.h"
#include "nsIXSLTProcessorPrivate.h"

// includes needed for the prototype chain interfaces
#include "nsIDOMCSSCharsetRule.h"
#include "nsIDOMCSSImportRule.h"
#include "nsIDOMCSSMediaRule.h"
#include "nsIDOMCSSFontFaceRule.h"
#include "nsIDOMCSSMozDocumentRule.h"
#include "nsIDOMCSSSupportsRule.h"
#include "nsIDOMMozCSSKeyframeRule.h"
#include "nsIDOMMozCSSKeyframesRule.h"
#include "nsIDOMCSSPageRule.h"
#include "nsIDOMCSSStyleRule.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIControllers.h"
#include "nsIBoxObject.h"
#ifdef MOZ_XUL
#include "nsITreeSelection.h"
#include "nsITreeContentView.h"
#include "nsITreeView.h"
#include "nsIXULTemplateBuilder.h"
#include "nsITreeColumns.h"
#endif
#include "nsIDOMXPathExpression.h"
#include "nsIDOMNSXPathExpression.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsIDOMXPathResult.h"

#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGNumber.h"

// Storage includes
#include "nsIDOMStorage.h"
#include "nsPIDOMStorage.h"

// Drag and drop
#include "nsIDOMFile.h"
#include "nsDOMBlobBuilder.h" // nsDOMMultipartFile

#include "nsIEventListenerService.h"
#include "nsIMessageManager.h"

#include "mozilla/dom/TouchEvent.h"

#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/HTMLCollectionBinding.h"

#include "nsIDOMMobileMessageManager.h"
#include "nsIDOMMozSmsMessage.h"
#include "nsIDOMMozMmsMessage.h"
#include "nsIDOMSmsFilter.h"
#include "nsIDOMSmsSegmentInfo.h"
#include "nsIDOMMozMobileMessageThread.h"

#ifdef MOZ_B2G_RIL
#include "nsIDOMIccManager.h"
#include "nsIDOMMobileConnection.h"
#endif // MOZ_B2G_RIL

#ifdef MOZ_B2G_FM
#include "FMRadio.h"
#endif

#include "nsIDOMGlobalObjectConstructor.h"
#include "nsIDOMLockedFile.h"
#include "nsDebug.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Likely.h"
#include "WindowNamedPropertiesHandler.h"
#include "nsIInterfaceInfoManager.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/WindowBinding.h"

#ifdef MOZ_TIME_MANAGER
#include "TimeManager.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

static NS_DEFINE_CID(kDOMSOF_CID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

// NOTE: DEFAULT_SCRIPTABLE_FLAGS and DOM_DEFAULT_SCRIPTABLE_FLAGS
//       are defined in nsIDOMClassInfo.h.

#define WINDOW_SCRIPTABLE_FLAGS                                               \
 (nsIXPCScriptable::WANT_PRECREATE |                                          \
  nsIXPCScriptable::WANT_POSTCREATE |                                         \
  nsIXPCScriptable::WANT_FINALIZE |                                           \
  nsIXPCScriptable::WANT_ENUMERATE |                                          \
  nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE |                               \
  nsIXPCScriptable::IS_GLOBAL_OBJECT |                                        \
  nsIXPCScriptable::WANT_OUTER_OBJECT)

#define ARRAY_SCRIPTABLE_FLAGS                                                \
  (DOM_DEFAULT_SCRIPTABLE_FLAGS       |                                       \
   nsIXPCScriptable::WANT_GETPROPERTY |                                       \
   nsIXPCScriptable::WANT_ENUMERATE)

#define EVENTTARGET_SCRIPTABLE_FLAGS                                          \
  (DOM_DEFAULT_SCRIPTABLE_FLAGS       |                                       \
   nsIXPCScriptable::WANT_ADDPROPERTY)

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

/**
 * To generate the bitmap for a class that we're sure doesn't implement any of
 * the interfaces in DOMCI_CASTABLE_INTERFACES.
 */
#define DOMCI_DATA_NO_CLASS(_dom_class)                                       \
const uint32_t kDOMClassInfo_##_dom_class##_interfaces =                      \
  0;

DOMCI_DATA_NO_CLASS(ContentFrameMessageManager)
DOMCI_DATA_NO_CLASS(ChromeMessageBroadcaster)
DOMCI_DATA_NO_CLASS(ChromeMessageSender)

DOMCI_DATA_NO_CLASS(DOMPrototype)
DOMCI_DATA_NO_CLASS(DOMConstructor)

DOMCI_DATA_NO_CLASS(UserDataHandler)
DOMCI_DATA_NO_CLASS(XPathNamespace)
DOMCI_DATA_NO_CLASS(XULControlElement)
DOMCI_DATA_NO_CLASS(XULLabeledControlElement)
DOMCI_DATA_NO_CLASS(XULButtonElement)
DOMCI_DATA_NO_CLASS(XULCheckboxElement)
DOMCI_DATA_NO_CLASS(XULPopupElement)

#define NS_DEFINE_CLASSINFO_DATA_HELPER(_class, _helper, _flags,              \
                                        _chromeOnly, _allowXBL)               \
  { #_class,                                                                  \
    nullptr,                                                                  \
    { _helper::doCreate },                                                    \
    nullptr,                                                                  \
    nullptr,                                                                  \
    nullptr,                                                                  \
    _flags,                                                                   \
    true,                                                                     \
    0,                                                                        \
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

  // The Window class lets you QI into interfaces that are not in the
  // flattened set (i.e. nsIXPCScriptable::CLASSINFO_INTERFACES_ONLY
  // is not set), because of this make sure all scriptable interfaces
  // that are implemented by nsGlobalWindow can securely be exposed
  // to JS.


  NS_DEFINE_CLASSINFO_DATA(Window, nsWindowSH,
                           DEFAULT_SCRIPTABLE_FLAGS |
                           WINDOW_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(Location, nsLocationSH,
                           ((DOM_DEFAULT_SCRIPTABLE_FLAGS |
                             nsIXPCScriptable::WANT_ADDPROPERTY) &
                            ~nsIXPCScriptable::ALLOW_PROP_MODS_TO_PROTOTYPE))

  NS_DEFINE_CLASSINFO_DATA(DOMPrototype, nsDOMConstructorSH,
                           DOM_BASE_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_PRECREATE |
                           nsIXPCScriptable::WANT_NEWRESOLVE |
                           nsIXPCScriptable::WANT_HASINSTANCE |
                           nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE)
  NS_DEFINE_CLASSINFO_DATA(DOMConstructor, nsDOMConstructorSH,
                           DOM_BASE_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_PRECREATE |
                           nsIXPCScriptable::WANT_NEWRESOLVE |
                           nsIXPCScriptable::WANT_HASINSTANCE |
                           nsIXPCScriptable::WANT_CALL |
                           nsIXPCScriptable::WANT_CONSTRUCT |
                           nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE)

  // Misc Core related classes

  // CSS classes
  NS_DEFINE_CLASSINFO_DATA(CSSStyleRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSCharsetRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSImportRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSMediaRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSNameSpaceRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSRuleList, nsCSSRuleListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(StyleSheetList, nsStyleSheetListSH,
                           ARRAY_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSStyleSheet, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // XUL classes
#ifdef MOZ_XUL
  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(XULCommandDispatcher, nsDOMGenericSH,
                                      DOM_DEFAULT_SCRIPTABLE_FLAGS)
#endif
  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(XULControllers, nsNonDOMObjectSH,
                                      DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(BoxObject, nsDOMGenericSH,
                                      DEFAULT_SCRIPTABLE_FLAGS)
#ifdef MOZ_XUL
  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(TreeSelection, nsDOMGenericSH,
                                      DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(TreeContentView, nsDOMGenericSH,
                                      DEFAULT_SCRIPTABLE_FLAGS)
#endif

  // DOM Chrome Window class.
  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(ChromeWindow, nsWindowSH,
                                      DEFAULT_SCRIPTABLE_FLAGS |
                                      WINDOW_SCRIPTABLE_FLAGS)

#ifdef MOZ_XUL
  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(XULTemplateBuilder, nsDOMGenericSH,
                                      DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(XULTreeBuilder, nsDOMGenericSH,
                                      DEFAULT_SCRIPTABLE_FLAGS)
#endif

#ifdef MOZ_XUL
  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(TreeColumn, nsDOMGenericSH,
                                      DEFAULT_SCRIPTABLE_FLAGS)
#endif

  NS_DEFINE_CLASSINFO_DATA(CSSMozDocumentRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(CSSSupportsRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // other SVG classes
  NS_DEFINE_CLASSINFO_DATA(SVGLength, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(SVGNumber, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(WindowUtils, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(XSLTProcessor, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(XPathExpression, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(XPathNSResolver, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(XPathResult, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  // WhatWG Storage

  // mrbkap says we don't need WANT_ADDPROPERTY on Storage objects
  // since a call to addProperty() is always followed by a call to
  // setProperty(), except in the case when a getter or setter is set
  // for a property. But we don't care about getters or setters here.
  NS_DEFINE_CLASSINFO_DATA(Storage, nsStorage2SH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS |
                           nsIXPCScriptable::WANT_NEWRESOLVE |
                           nsIXPCScriptable::WANT_GETPROPERTY |
                           nsIXPCScriptable::WANT_SETPROPERTY |
                           nsIXPCScriptable::WANT_DELPROPERTY |
                           nsIXPCScriptable::DONT_ENUM_STATIC_PROPS |
                           nsIXPCScriptable::WANT_NEWENUMERATE)

  NS_DEFINE_CLASSINFO_DATA(Blob, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(File, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(ModalContentWindow, nsWindowSH,
                           DEFAULT_SCRIPTABLE_FLAGS |
                           WINDOW_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozMobileMessageManager, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozSmsMessage, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozMmsMessage, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozSmsFilter, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozSmsSegmentInfo, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(MozMobileMessageThread, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

#ifdef MOZ_B2G_RIL
  NS_DEFINE_CLASSINFO_DATA(MozMobileConnection, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
#endif

  NS_DEFINE_CLASSINFO_DATA(CSSFontFaceRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(EventListenerInfo, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA(ContentFrameMessageManager, nsEventTargetSH,
                                       DOM_DEFAULT_SCRIPTABLE_FLAGS |
                                       nsIXPCScriptable::IS_GLOBAL_OBJECT)
  NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA(ChromeMessageBroadcaster, nsDOMGenericSH,
                                       DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CHROME_ONLY_CLASSINFO_DATA(ChromeMessageSender, nsDOMGenericSH,
                                       DOM_DEFAULT_SCRIPTABLE_FLAGS)


  NS_DEFINE_CLASSINFO_DATA(MozCSSKeyframeRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(MozCSSKeyframesRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CLASSINFO_DATA(CSSPageRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

#ifdef MOZ_B2G_RIL
  NS_DEFINE_CLASSINFO_DATA(MozIccManager, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)
#endif

  NS_DEFINE_CLASSINFO_DATA(LockedFile, nsEventTargetSH,
                           EVENTTARGET_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(CSSFontFeatureValuesRule, nsDOMGenericSH,
                           DOM_DEFAULT_SCRIPTABLE_FLAGS)

  NS_DEFINE_CHROME_XBL_CLASSINFO_DATA(UserDataHandler, nsDOMGenericSH,
                                      DOM_DEFAULT_SCRIPTABLE_FLAGS)
  NS_DEFINE_CLASSINFO_DATA(XPathNamespace, nsDOMGenericSH,
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

#define NS_DEFINE_CONTRACT_CTOR(_class, _contract_id)                           \
  static nsresult                                                               \
  _class##Ctor(nsISupports** aInstancePtrResult)                                \
  {                                                                             \
    nsresult rv = NS_OK;                                                        \
    nsCOMPtr<nsISupports> native = do_CreateInstance(_contract_id, &rv);        \
    native.forget(aInstancePtrResult);                                          \
    return rv;                                                                  \
  }

NS_DEFINE_CONTRACT_CTOR(XSLTProcessor,
                        "@mozilla.org/document-transformer;1?type=xslt")

#undef NS_DEFINE_CONTRACT_CTOR

struct nsConstructorFuncMapData
{
  int32_t mDOMClassInfoID;
  nsDOMConstructorFunc mConstructorFunc;
};

#define NS_DEFINE_CONSTRUCTOR_FUNC_DATA(_class, _func)                        \
  { eDOMClassInfo_##_class##_id, _func },

static const nsConstructorFuncMapData kConstructorFuncMap[] =
{
  NS_DEFINE_CONSTRUCTOR_FUNC_DATA(Blob, nsDOMMultipartFile::NewBlob)
  NS_DEFINE_CONSTRUCTOR_FUNC_DATA(File, nsDOMMultipartFile::NewFile)
  NS_DEFINE_CONSTRUCTOR_FUNC_DATA(MozSmsFilter, SmsFilter::NewSmsFilter)
  NS_DEFINE_CONSTRUCTOR_FUNC_DATA(XSLTProcessor, XSLTProcessorCtor)
};
#undef NS_DEFINE_CONSTRUCTOR_FUNC_DATA

nsIXPConnect *nsDOMClassInfo::sXPConnect = nullptr;
nsIScriptSecurityManager *nsDOMClassInfo::sSecMan = nullptr;
bool nsDOMClassInfo::sIsInitialized = false;


jsid nsDOMClassInfo::sLocation_id        = JSID_VOID;
jsid nsDOMClassInfo::sConstructor_id     = JSID_VOID;
jsid nsDOMClassInfo::sLength_id          = JSID_VOID;
jsid nsDOMClassInfo::sItem_id            = JSID_VOID;
jsid nsDOMClassInfo::sNamedItem_id       = JSID_VOID;
jsid nsDOMClassInfo::sEnumerate_id       = JSID_VOID;
jsid nsDOMClassInfo::sTop_id             = JSID_VOID;
jsid nsDOMClassInfo::sDocument_id        = JSID_VOID;
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

static inline JSString *
IdToString(JSContext *cx, jsid id)
{
  if (JSID_IS_STRING(id))
    return JSID_TO_STRING(id);
  JS::Rooted<JS::Value> idval(cx);
  if (!::JS_IdToValue(cx, id, &idval))
    return nullptr;
  return JS::ToString(cx, idval);
}

static inline nsresult
WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
           nsWrapperCache *cache, const nsIID* aIID, JS::MutableHandle<JS::Value> vp,
           bool aAllowWrapping)
{
  if (!native) {
    vp.setNull();

    return NS_OK;
  }

  JSObject *wrapper = xpc_FastGetCachedWrapper(cache, scope, vp);
  if (wrapper) {
    return NS_OK;
  }

  return nsDOMClassInfo::XPConnect()->WrapNativeToJSVal(cx, scope, native,
                                                        cache, aIID,
                                                        aAllowWrapping, vp);
}

static inline nsresult
WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
           const nsIID* aIID, bool aAllowWrapping, JS::MutableHandle<JS::Value> vp)
{
  return WrapNative(cx, scope, native, nullptr, aIID, vp, aAllowWrapping);
}

// Same as the WrapNative above, but use these if aIID is nsISupports' IID.
static inline nsresult
WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
           bool aAllowWrapping, JS::MutableHandle<JS::Value> vp)
{
  return WrapNative(cx, scope, native, nullptr, nullptr, vp, aAllowWrapping);
}

static inline nsresult
WrapNative(JSContext *cx, JSObject *scope, nsISupports *native,
           nsWrapperCache *cache, bool aAllowWrapping,
           JS::MutableHandle<JS::Value> vp)
{
  return WrapNative(cx, scope, native, cache, nullptr, vp, aAllowWrapping);
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
nsDOMClassInfo::DefineStaticJSVals(JSContext *cx)
{
#define SET_JSID_TO_STRING(_id, _cx, _str)                                    \
  if (JSString *str = ::JS_InternString(_cx, _str))                           \
      _id = INTERNED_STRING_TO_JSID(_cx, str);                                \
  else                                                                        \
      return NS_ERROR_OUT_OF_MEMORY;

  SET_JSID_TO_STRING(sLocation_id,        cx, "location");
  SET_JSID_TO_STRING(sConstructor_id,     cx, "constructor");
  SET_JSID_TO_STRING(sLength_id,          cx, "length");
  SET_JSID_TO_STRING(sItem_id,            cx, "item");
  SET_JSID_TO_STRING(sNamedItem_id,       cx, "namedItem");
  SET_JSID_TO_STRING(sEnumerate_id,       cx, "enumerateProperties");
  SET_JSID_TO_STRING(sTop_id,             cx, "top");
  SET_JSID_TO_STRING(sDocument_id,        cx, "document");
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

nsDOMClassInfo::~nsDOMClassInfo()
{
  if (IS_EXTERNAL(mData->mCachedClassInfo)) {
    // Some compilers don't like delete'ing a const nsDOMClassInfo*
    nsDOMClassInfoData* data = const_cast<nsDOMClassInfoData*>(mData);
    delete static_cast<nsExternalDOMClassInfoData*>(data);
  }
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
  "DOM Constructor.prototype", 0,
  JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nullptr
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

// static
nsresult
nsDOMClassInfo::RegisterExternalClasses()
{
  nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIComponentRegistrar> registrar;
  nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(registrar));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsICategoryManager> cm =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISimpleEnumerator> e;
  rv = cm->EnumerateCategory(JAVASCRIPT_DOM_CLASS, getter_AddRefs(e));
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString contractId;
  nsAutoCString categoryEntry;
  nsCOMPtr<nsISupports> entry;

  while (NS_SUCCEEDED(e->GetNext(getter_AddRefs(entry)))) {
    nsCOMPtr<nsISupportsCString> category(do_QueryInterface(entry));

    if (!category) {
      NS_WARNING("Category entry not an nsISupportsCString!");
      continue;
    }

    rv = category->GetData(categoryEntry);

    cm->GetCategoryEntry(JAVASCRIPT_DOM_CLASS, categoryEntry.get(),
                         getter_Copies(contractId));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCID *cid;
    rv = registrar->ContractIDToCID(contractId, &cid);
    if (NS_FAILED(rv)) {
      NS_WARNING("Bad contract id registered with the script namespace manager");
      continue;
    }

    rv = nameSpaceManager->RegisterExternalClassName(categoryEntry.get(), *cid);
    nsMemory::Free(cid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return nameSpaceManager->RegisterExternalInterfaces(true);
}

#define _DOM_CLASSINFO_MAP_BEGIN(_class, _ifptr, _has_class_if, _disabled)    \
  {                                                                           \
    nsDOMClassInfoData &d = sClassInfoData[eDOMClassInfo_##_class##_id];      \
    d.mProtoChainInterface = _ifptr;                                          \
    d.mHasClassInterface = _has_class_if;                                     \
    d.mInterfacesBitmap = kDOMClassInfo_##_class##_interfaces;                \
    d.mDisabled = _disabled;                                                  \
    static const nsIID *interface_list[] = {

#define DOM_CLASSINFO_MAP_BEGIN(_class, _interface)                           \
  _DOM_CLASSINFO_MAP_BEGIN(_class, &NS_GET_IID(_interface), true, false)

#define DOM_CLASSINFO_MAP_BEGIN_MAYBE_DISABLE(_class, _interface, _disable)   \
  _DOM_CLASSINFO_MAP_BEGIN(_class, &NS_GET_IID(_interface), true, _disable)

#define DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(_class, _interface)               \
  _DOM_CLASSINFO_MAP_BEGIN(_class, &NS_GET_IID(_interface), false, false)

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

#ifdef MOZ_B2G
#define DOM_CLASSINFO_WINDOW_MAP_ENTRIES                                       \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindow)                                        \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindowB2G)                                     \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMJSWindow)                                      \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)                                   \
  DOM_CLASSINFO_MAP_ENTRY(nsIInlineEventHandlers)                              \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindowPerformance)                             \
  DOM_CLASSINFO_MAP_ENTRY(nsIInterfaceRequestor)                               \
  DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsITouchEventReceiver,                   \
                                      TouchEvent::PrefEnabled())
#else // !MOZ_B2G
#define DOM_CLASSINFO_WINDOW_MAP_ENTRIES                                       \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindow)                                        \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMJSWindow)                                      \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)                                   \
  DOM_CLASSINFO_MAP_ENTRY(nsIInlineEventHandlers)                              \
  DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindowPerformance)                             \
  DOM_CLASSINFO_MAP_ENTRY(nsIInterfaceRequestor)                               \
  DOM_CLASSINFO_MAP_CONDITIONAL_ENTRY(nsITouchEventReceiver,                   \
                                      TouchEvent::PrefEnabled())
#endif // MOZ_B2G

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

  nsresult rv = CallGetService(nsIXPConnect::GetCID(), &sXPConnect);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPCFunctionThisTranslator> elt = new nsEventListenerThisTranslator();
  sXPConnect->SetFunctionThisTranslator(NS_GET_IID(nsIDOMEventListener), elt);

  nsCOMPtr<nsIScriptSecurityManager> sm =
    do_GetService("@mozilla.org/scriptsecuritymanager;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  sSecMan = sm;
  NS_ADDREF(sSecMan);

  AutoSafeJSContext cx;

  DOM_CLASSINFO_MAP_BEGIN(Window, nsIDOMWindow)
    DOM_CLASSINFO_WINDOW_MAP_ENTRIES
#ifdef MOZ_WEBSPEECH
    DOM_CLASSINFO_MAP_ENTRY(nsISpeechSynthesisGetter)
#endif
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(WindowUtils, nsIDOMWindowUtils)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMWindowUtils)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Location, nsIDOMLocation)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLocation)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(DOMPrototype, nsIDOMDOMConstructor)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMConstructor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(DOMConstructor, nsIDOMDOMConstructor)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMDOMConstructor)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSStyleRule, nsIDOMCSSStyleRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSStyleRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSCharsetRule, nsIDOMCSSCharsetRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSCharsetRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSImportRule, nsIDOMCSSImportRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSImportRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSMediaRule, nsIDOMCSSMediaRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSMediaRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(CSSNameSpaceRule, nsIDOMCSSRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSRuleList, nsIDOMCSSRuleList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSRuleList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(StyleSheetList, nsIDOMStyleSheetList)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStyleSheetList)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSStyleSheet, nsIDOMCSSStyleSheet)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSStyleSheet)
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_XUL
  DOM_CLASSINFO_MAP_BEGIN(XULCommandDispatcher, nsIDOMXULCommandDispatcher)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXULCommandDispatcher)
  DOM_CLASSINFO_MAP_END
#endif

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XULControllers, nsIControllers)
    DOM_CLASSINFO_MAP_ENTRY(nsIControllers)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(BoxObject, nsIBoxObject)
    DOM_CLASSINFO_MAP_ENTRY(nsIBoxObject)
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_XUL
  DOM_CLASSINFO_MAP_BEGIN(TreeSelection, nsITreeSelection)
    DOM_CLASSINFO_MAP_ENTRY(nsITreeSelection)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(TreeContentView, nsITreeContentView)
    DOM_CLASSINFO_MAP_ENTRY(nsITreeContentView)
    DOM_CLASSINFO_MAP_ENTRY(nsITreeView)
  DOM_CLASSINFO_MAP_END
#endif

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ChromeWindow, nsIDOMWindow)
    DOM_CLASSINFO_WINDOW_MAP_ENTRIES
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMChromeWindow)
#ifdef MOZ_WEBSPEECH
    DOM_CLASSINFO_MAP_ENTRY(nsISpeechSynthesisGetter)
#endif
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_XUL
  DOM_CLASSINFO_MAP_BEGIN(XULTemplateBuilder, nsIXULTemplateBuilder)
    DOM_CLASSINFO_MAP_ENTRY(nsIXULTemplateBuilder)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XULTreeBuilder, nsIXULTreeBuilder)
    DOM_CLASSINFO_MAP_ENTRY(nsIXULTreeBuilder)
    DOM_CLASSINFO_MAP_ENTRY(nsIXULTemplateBuilder)
    DOM_CLASSINFO_MAP_ENTRY(nsITreeView)
  DOM_CLASSINFO_MAP_END
#endif

#ifdef MOZ_XUL
  DOM_CLASSINFO_MAP_BEGIN(TreeColumn, nsITreeColumn)
    DOM_CLASSINFO_MAP_ENTRY(nsITreeColumn)
  DOM_CLASSINFO_MAP_END
#endif

  DOM_CLASSINFO_MAP_BEGIN(CSSMozDocumentRule, nsIDOMCSSMozDocumentRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSMozDocumentRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSSupportsRule, nsIDOMCSSSupportsRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSSupportsRule)
  DOM_CLASSINFO_MAP_END

  // The SVG document

  // other SVG classes
  DOM_CLASSINFO_MAP_BEGIN(SVGLength, nsIDOMSVGLength)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGLength)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(SVGNumber, nsIDOMSVGNumber)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMSVGNumber)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XSLTProcessor, nsIXSLTProcessor)
    DOM_CLASSINFO_MAP_ENTRY(nsIXSLTProcessor)
    DOM_CLASSINFO_MAP_ENTRY(nsIXSLTProcessorPrivate)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XPathExpression, nsIDOMXPathExpression)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXPathExpression)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMNSXPathExpression)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XPathNSResolver, nsIDOMXPathNSResolver)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXPathNSResolver)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(XPathResult, nsIDOMXPathResult)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXPathResult)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Storage, nsIDOMStorage)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMStorage)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(Blob, nsIDOMBlob)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMBlob)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(File, nsIDOMFile)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMBlob)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMFile)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ModalContentWindow, nsIDOMWindow)
    DOM_CLASSINFO_WINDOW_MAP_ENTRIES
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMModalContentWindow)
#ifdef MOZ_WEBSPEECH
    DOM_CLASSINFO_MAP_ENTRY(nsISpeechSynthesisGetter)
#endif
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozMobileMessageManager, nsIDOMMozMobileMessageManager)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozMobileMessageManager)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozSmsMessage, nsIDOMMozSmsMessage)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozSmsMessage)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozMmsMessage, nsIDOMMozMmsMessage)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozMmsMessage)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozSmsFilter, nsIDOMMozSmsFilter)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozSmsFilter)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozSmsSegmentInfo, nsIDOMMozSmsSegmentInfo)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozSmsSegmentInfo)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozMobileMessageThread, nsIDOMMozMobileMessageThread)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozMobileMessageThread)
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_B2G_RIL
  DOM_CLASSINFO_MAP_BEGIN(MozMobileConnection, nsIDOMMozMobileConnection)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozMobileConnection)
     DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END
#endif // MOZ_B2G_RIL

  DOM_CLASSINFO_MAP_BEGIN(CSSFontFaceRule, nsIDOMCSSFontFaceRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSFontFaceRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(EventListenerInfo, nsIEventListenerInfo)
    DOM_CLASSINFO_MAP_ENTRY(nsIEventListenerInfo)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ContentFrameMessageManager, nsISupports)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageListenerManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageSender)
    DOM_CLASSINFO_MAP_ENTRY(nsISyncMessageSender)
    DOM_CLASSINFO_MAP_ENTRY(nsIContentFrameMessageManager)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ChromeMessageBroadcaster, nsISupports)
    DOM_CLASSINFO_MAP_ENTRY(nsIFrameScriptLoader)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageListenerManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageBroadcaster)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(ChromeMessageSender, nsISupports)
    DOM_CLASSINFO_MAP_ENTRY(nsIProcessChecker)
    DOM_CLASSINFO_MAP_ENTRY(nsIFrameScriptLoader)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageListenerManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIMessageSender)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozCSSKeyframeRule, nsIDOMMozCSSKeyframeRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozCSSKeyframeRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(MozCSSKeyframesRule, nsIDOMMozCSSKeyframesRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozCSSKeyframesRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSPageRule, nsIDOMCSSPageRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSPageRule)
  DOM_CLASSINFO_MAP_END

#ifdef MOZ_B2G_RIL
  DOM_CLASSINFO_MAP_BEGIN(MozIccManager, nsIDOMMozIccManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMMozIccManager)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
  DOM_CLASSINFO_MAP_END

#endif

  DOM_CLASSINFO_MAP_BEGIN(LockedFile, nsIDOMLockedFile)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMLockedFile)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN(CSSFontFeatureValuesRule, nsIDOMCSSFontFeatureValuesRule)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMCSSFontFeatureValuesRule)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(UserDataHandler, nsIDOMUserDataHandler)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMUserDataHandler)
  DOM_CLASSINFO_MAP_END

  DOM_CLASSINFO_MAP_BEGIN_NO_CLASS_IF(XPathNamespace, nsIDOMXPathNamespace)
    DOM_CLASSINFO_MAP_ENTRY(nsIDOMXPathNamespace)
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
    if (!sClassInfoData[i].u.mConstructorFptr ||
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
  DefineStaticJSVals(cx);

  int32_t i;

  for (i = 0; i < eDOMClassInfoIDCount; ++i) {
    nsDOMClassInfoData& data = sClassInfoData[i];
    nameSpaceManager->RegisterClassName(data.mName, i, data.mChromeOnly,
                                        data.mAllowXBL, data.mDisabled,
                                        &data.mNameUTF16);
  }

  for (i = 0; i < eDOMClassInfoIDCount; ++i) {
    RegisterClassProtos(i);
  }

  RegisterExternalClasses();

  // Register new DOM bindings
  mozilla::dom::Register(nameSpaceManager);

  sIsInitialized = true;

  return NS_OK;
}

// static
int32_t
nsDOMClassInfo::GetArrayIndexFromId(JSContext *cx, JS::Handle<jsid> id, bool *aIsNumber)
{
  if (aIsNumber) {
    *aIsNumber = false;
  }

  int i;
  if (JSID_IS_INT(id)) {
      i = JSID_TO_INT(id);
  } else {
      JS::Rooted<JS::Value> idval(cx);
      double array_index;
      if (!::JS_IdToValue(cx, id, &idval) ||
          !JS::ToNumber(cx, idval, &array_index) ||
          !::JS_DoubleIsInt32(array_index, &i)) {
        return -1;
      }
  }

  if (aIsNumber) {
    *aIsNumber = true;
  }

  return i;
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

  *aArray = static_cast<nsIID **>(nsMemory::Alloc(count * sizeof(nsIID *)));
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
nsDOMClassInfo::GetHelperForLanguage(uint32_t language, nsISupports **_retval)
{
  if (language == nsIProgrammingLanguage::JAVASCRIPT) {
    *_retval = static_cast<nsIXPCScriptable *>(this);

    NS_ADDREF(*_retval);
  } else {
    *_retval = nullptr;
  }

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
nsDOMClassInfo::GetImplementationLanguage(uint32_t *aImplLanguage)
{
  *aImplLanguage = nsIProgrammingLanguage::CPLUSPLUS;

  return NS_OK;
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
  *aClassName = NS_strdup(mData->mName);

  return NS_OK;
}

// virtual
uint32_t
nsDOMClassInfo::GetScriptableFlags()
{
  return mData->mScriptableFlags;
}

NS_IMETHODIMP
nsDOMClassInfo::PreCreate(nsISupports *nativeObj, JSContext *cx,
                          JSObject *globalObj, JSObject **parentObj)
{
  *parentObj = globalObj;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::Create(nsIXPConnectWrappedNative *wrapper,
                       JSContext *cx, JSObject *obj)
{
  NS_WARNING("nsDOMClassInfo::Create Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::PostCreate(nsIXPConnectWrappedNative *wrapper,
                           JSContext *cx, JSObject *obj)
{
  NS_WARNING("nsDOMClassInfo::PostCreate Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::PostTransplant(nsIXPConnectWrappedNative *wrapper,
                               JSContext *cx, JSObject *obj)
{
  MOZ_CRASH("nsDOMClassInfo::PostTransplant Don't call me!");
}

NS_IMETHODIMP
nsDOMClassInfo::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, jsval *vp,
                            bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::AddProperty Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::DelProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::DelProperty Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsDOMClassInfo::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, jsval *vp,
                            bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::GetProperty Don't call me!");

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::SetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *obj, jsid id, jsval *vp,
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
nsDOMClassInfo::NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                             JSContext *cx, JSObject *obj, uint32_t enum_op,
                             jsval *statep, jsid *idp, bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::NewEnumerate Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

nsresult
nsDOMClassInfo::ResolveConstructor(JSContext *cx, JSObject *aObj,
                                   JSObject **objp)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<JSObject*> global(cx, ::JS_GetGlobalForObject(cx, obj));

  JS::Rooted<JS::Value> val(cx);
  if (!::JS_LookupProperty(cx, global, mData->mName, &val)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!JSVAL_IS_PRIMITIVE(val)) {
    // If val is not an (non-null) object there either is no
    // constructor for this class, or someone messed with
    // window.classname, just fall through and let the JS engine
    // return the Object constructor.

    if (!::JS_DefinePropertyById(cx, obj, sConstructor_id, val, JS_PropertyStub,
                                 JS_StrictPropertyStub, JSPROP_ENUMERATE)) {
      return NS_ERROR_UNEXPECTED;
    }

    *objp = obj;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                           JSObject *obj, jsid id, uint32_t flags,
                           JSObject **objp, bool *_retval)
{
  if (id == sConstructor_id) {
    return ResolveConstructor(cx, obj, objp);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMClassInfo::Convert(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                        JSObject *obj, uint32_t type, jsval *vp,
                        bool *_retval)
{
  NS_WARNING("nsDOMClassInfo::Convert Don't call me!");

  return NS_ERROR_UNEXPECTED;
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
nsDOMClassInfo::OuterObject(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                            JSObject * obj, JSObject * *_retval)
{
  NS_WARNING("nsDOMClassInfo::OuterObject Don't call me!");

  return NS_ERROR_UNEXPECTED;
}

static nsresult
GetExternalClassInfo(nsScriptNameSpaceManager *aNameSpaceManager,
                     const nsString &aName,
                     const nsGlobalNameStruct *aStruct,
                     const nsGlobalNameStruct **aResult)
{
  NS_ASSERTION(aStruct->mType ==
                 nsGlobalNameStruct::eTypeExternalClassInfoCreator,
               "Wrong type!");

  nsresult rv;
  nsCOMPtr<nsIDOMCIExtension> creator(do_CreateInstance(aStruct->mCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMScriptObjectFactory> sof(do_GetService(kDOMSOF_CID));
  NS_ENSURE_TRUE(sof, NS_ERROR_FAILURE);

  rv = creator->RegisterDOMCI(NS_ConvertUTF16toUTF8(aName).get(), sof);
  NS_ENSURE_SUCCESS(rv, rv);

  const nsGlobalNameStruct *name_struct = aNameSpaceManager->LookupName(aName);
  if (name_struct &&
      name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
    *aResult = name_struct;
  }
  else {
    NS_ERROR("Couldn't get the DOM ClassInfo data.");

    *aResult = nullptr;
  }

  return NS_OK;
}


static nsresult
ResolvePrototype(nsIXPConnect *aXPConnect, nsGlobalWindow *aWin, JSContext *cx,
                 JS::Handle<JSObject*> obj, const char16_t *name,
                 const nsDOMClassInfoData *ci_data,
                 const nsGlobalNameStruct *name_struct,
                 nsScriptNameSpaceManager *nameSpaceManager,
                 JSObject *dot_prototype,
                 JS::MutableHandle<JSPropertyDescriptor> ctorDesc);

NS_IMETHODIMP
nsDOMClassInfo::PostCreatePrototype(JSContext * cx, JSObject * aProto)
{
  uint32_t flags = (mData->mScriptableFlags & DONT_ENUM_STATIC_PROPS)
                   ? 0
                   : JSPROP_ENUMERATE;

  uint32_t count = 0;
  while (mData->mInterfaces[count]) {
    count++;
  }

  JS::Rooted<JSObject*> proto(cx, aProto);
  if (!xpc::DOM_DefineQuickStubs(cx, proto, flags, count, mData->mInterfaces)) {
    JS_ClearPendingException(cx);
  }

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
        NS_ASSERTION(nsCRT::strcmp(CutPrefix(name), mData->mName) == 0,
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
  nsCOMPtr<nsPIDOMWindow> piwin = do_QueryInterface(globalNative);
  if (!piwin) {
    return NS_OK;
  }

  nsGlobalWindow *win = nsGlobalWindow::FromSupports(globalNative);
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

  if (win->IsOuterWindow()) {
    // XXXjst: Do security checks here when we remove the security
    // checks on the inner window.

    win = win->GetCurrentInnerWindowInternal();

    if (!win || !(global = win->GetGlobalJSObject()) ||
        win->IsClosedOrClosing()) {
      return NS_OK;
    }
  }

  // Don't overwrite a property set by content.
  bool contentDefinedProperty;
  if (!::JS_AlreadyHasOwnUCProperty(cx, global, reinterpret_cast<const jschar*>(mData->mNameUTF16),
                                    NS_strlen(mData->mNameUTF16),
                                    &contentDefinedProperty)) {
    return NS_ERROR_FAILURE;
  }

  nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_OK);

  JS::Rooted<JSPropertyDescriptor> desc(cx);
  nsresult rv = ResolvePrototype(sXPConnect, win, cx, global, mData->mNameUTF16,
                                 mData, nullptr, nameSpaceManager, proto,
                                 &desc);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!contentDefinedProperty && desc.object() && !desc.value().isUndefined() &&
      !JS_DefineUCProperty(cx, global, mData->mNameUTF16,
                           NS_strlen(mData->mNameUTF16),
                           desc.value(), desc.getter(), desc.setter(),
                           desc.attributes())) {
    return NS_ERROR_UNEXPECTED;
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

  if (!nsDOMClassInfo::sIsInitialized) {
    nsresult rv = nsDOMClassInfo::Init();

    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  if (!sClassInfoData[aID].mCachedClassInfo) {
    nsDOMClassInfoData& data = sClassInfoData[aID];

    data.mCachedClassInfo = data.u.mConstructorFptr(&data);
    NS_ENSURE_TRUE(data.mCachedClassInfo, nullptr);

    NS_ADDREF(data.mCachedClassInfo);
  }

  NS_ASSERTION(!IS_EXTERNAL(sClassInfoData[aID].mCachedClassInfo),
               "This is bad, internal class marked as external!");

  return sClassInfoData[aID].mCachedClassInfo;
}

// static
nsIClassInfo *
nsDOMClassInfo::GetClassInfoInstance(nsDOMClassInfoData* aData)
{
  NS_ASSERTION(IS_EXTERNAL(aData->mCachedClassInfo)
               || !aData->mCachedClassInfo,
               "This is bad, external class marked as internal!");

  if (!aData->mCachedClassInfo) {
    if (aData->u.mExternalConstructorFptr) {
      aData->mCachedClassInfo =
        aData->u.mExternalConstructorFptr(aData->mName);
    } else {
      aData->mCachedClassInfo = nsDOMGenericSH::doCreate(aData);
    }
    NS_ENSURE_TRUE(aData->mCachedClassInfo, nullptr);

    NS_ADDREF(aData->mCachedClassInfo);
    aData->mCachedClassInfo = MARK_EXTERNAL(aData->mCachedClassInfo);
  }

  return GET_CLEAN_CI_PTR(aData->mCachedClassInfo);
}


// static
void
nsDOMClassInfo::ShutDown()
{
  if (sClassInfoData[0].u.mConstructorFptr) {
    uint32_t i;

    for (i = 0; i < eDOMClassInfoIDCount; i++) {
      NS_IF_RELEASE(sClassInfoData[i].mCachedClassInfo);
    }
  }

  sLocation_id        = JSID_VOID;
  sConstructor_id     = JSID_VOID;
  sLength_id          = JSID_VOID;
  sItem_id            = JSID_VOID;
  sEnumerate_id       = JSID_VOID;
  sTop_id             = JSID_VOID;
  sDocument_id        = JSID_VOID;
  sWrappedJSObject_id = JSID_VOID;

  NS_IF_RELEASE(sXPConnect);
  NS_IF_RELEASE(sSecMan);
  sIsInitialized = false;
}

// Window helper

NS_IMETHODIMP
nsWindowSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                      JSObject *globalObj, JSObject **parentObj)
{
  // Normally ::PreCreate() is used to give XPConnect the parent
  // object for the object that's being wrapped, this parent object is
  // set as the parent of the wrapper and it's also used to find the
  // right scope for the object being wrapped. Now, in the case of the
  // global object the wrapper shouldn't have a parent but we supply
  // one here anyway (the global object itself) and this will be used
  // by XPConnect only to find the right scope, once the scope is
  // found XPConnect will find the existing wrapper (which always
  // exists since it's created on window construction), since an
  // existing wrapper is found the parent we supply here is ignored
  // after the wrapper is found.

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(nativeObj));
  NS_ASSERTION(sgo, "nativeObj not a global object!");

  nsGlobalWindow *win = nsGlobalWindow::FromSupports(nativeObj);
  NS_ASSERTION(win->IsInnerWindow(), "Should be inner window.");

  // We sometimes get a disconnected window during file api test. :-(
  if (!win->GetOuterWindowInternal())
    return NS_ERROR_FAILURE;

  // If we're bootstrapping, we don't have a JS object yet.
  if (win->GetOuterWindowInternal()->IsCreatingInnerWindow())
    return NS_OK;

  return SetParentToWindow(win, parentObj);
}

NS_IMETHODIMP
nsWindowSH::PostCreatePrototype(JSContext* aCx, JSObject* aProto)
{
  JS::Rooted<JSObject*> proto(aCx, aProto);

  nsresult rv = nsDOMClassInfo::PostCreatePrototype(aCx, proto);
  NS_ENSURE_SUCCESS(rv, rv);

  // We should probably move this into the CreateInterfaceObjects for Window
  // once it is on WebIDL bindings.
  WindowNamedPropertiesHandler::Install(aCx, proto);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::PostCreate(nsIXPConnectWrappedNative *wrapper,
                       JSContext *cx, JSObject *obj)
{
  JS::Rooted<JSObject*> window(cx, obj);

#ifdef DEBUG
  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryWrappedNative(wrapper));

  NS_ASSERTION(sgo && sgo->GetGlobalJSObject() == nullptr,
               "Multiple wrappers created for global object!");
#endif

  const NativeProperties* windowProperties =
    WindowBinding::sNativePropertyHooks->mNativeProperties.regular;
  const NativeProperties* eventTargetProperties =
    EventTargetBinding::sNativePropertyHooks->mNativeProperties.regular;

  return DefineWebIDLBindingPropertiesOnXPCObject(cx, window, windowProperties, true) &&
         DefineWebIDLBindingPropertiesOnXPCObject(cx, window, eventTargetProperties, true) ?
         NS_OK : NS_ERROR_FAILURE;
}

struct ResolveGlobalNameClosure
{
  JSContext* cx;
  JS::Handle<JSObject*> obj;
  bool* retval;
};

static PLDHashOperator
ResolveGlobalName(const nsAString& aName, void* aClosure)
{
  ResolveGlobalNameClosure* closure =
    static_cast<ResolveGlobalNameClosure*>(aClosure);
  JS::Rooted<JS::Value> dummy(closure->cx);
  bool ok = JS_LookupUCProperty(closure->cx, closure->obj,
                                aName.BeginReading(), aName.Length(),
                                &dummy);
  if (!ok) {
    *closure->retval = false;
    return PL_DHASH_STOP;
  }
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsWindowSH::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                      JSObject *aObj, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  if (!xpc::WrapperFactory::IsXrayWrapper(obj)) {
    *_retval = JS_EnumerateStandardClasses(cx, obj);
    if (!*_retval) {
      return NS_OK;
    }

    // Now resolve everything from the namespace manager
    nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
    if (!nameSpaceManager) {
      NS_ERROR("Can't get namespace manager.");
      return NS_ERROR_UNEXPECTED;
    }
    ResolveGlobalNameClosure closure = { cx, obj, _retval };
    nameSpaceManager->EnumerateGlobalNames(ResolveGlobalName, &closure);
  }

  return NS_OK;
}

static nsDOMConstructorFunc
FindConstructorFunc(const nsDOMClassInfoData *aDOMClassInfoData)
{
  for (uint32_t i = 0; i < ArrayLength(kConstructorFuncMap); ++i) {
    if (&sClassInfoData[kConstructorFuncMap[i].mDOMClassInfoID] ==
        aDOMClassInfoData) {
      return kConstructorFuncMap[i].mConstructorFunc;
    }
  }
  return nullptr;
}

static nsresult
BaseStubConstructor(nsIWeakReference* aWeakOwner,
                    const nsGlobalNameStruct *name_struct, JSContext *cx,
                    JS::Handle<JSObject*> obj, const JS::CallArgs &args)
{
  MOZ_ASSERT(obj);

  nsresult rv;
  nsCOMPtr<nsISupports> native;
  if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    const nsDOMClassInfoData* ci_data =
      &sClassInfoData[name_struct->mDOMClassInfoID];
    nsDOMConstructorFunc func = FindConstructorFunc(ci_data);
    if (func) {
      rv = func(getter_AddRefs(native));
    } else {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
  } else if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructor) {
    native = do_CreateInstance(name_struct->mCID, &rv);
  } else if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    native = do_CreateInstance(name_struct->mAlias->mCID, &rv);
  } else {
    native = do_CreateInstance(*name_struct->mData->mConstructorCID, &rv);
  }
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to create the object");
    return rv;
  }

  nsCOMPtr<nsIJSNativeInitializer> initializer(do_QueryInterface(native));
  nsCOMPtr<nsIDOMGlobalObjectConstructor> constructor(do_QueryInterface(native));
  if (initializer || constructor) {
    // Initialize object using the current inner window, but only if
    // the caller can access it.
    nsCOMPtr<nsPIDOMWindow> owner = do_QueryReferent(aWeakOwner);
    nsPIDOMWindow* outerWindow = owner ? owner->GetOuterWindow() : nullptr;
    nsPIDOMWindow* currentInner =
      outerWindow ? outerWindow->GetCurrentInnerWindow() : nullptr;
    if (!currentInner ||
        (owner != currentInner &&
         !nsContentUtils::CanCallerAccess(currentInner))) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    if (initializer) {
      rv = initializer->Initialize(currentInner, cx, obj, args);
      if (NS_FAILED(rv)) {
        return rv;
      }
    } else {
      nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS = do_QueryInterface(native);

      JS::Rooted<JSObject*> thisObject(cx, wrappedJS->GetJSObject());
      if (!thisObject) {
        return NS_ERROR_UNEXPECTED;
      }

      nsCxPusher pusher;
      pusher.Push(cx);

      JSAutoCompartment ac(cx, thisObject);

      JS::Rooted<JS::Value> funval(cx);
      if (!JS_GetProperty(cx, thisObject, "constructor", &funval) ||
          !funval.isObject()) {
        return NS_ERROR_UNEXPECTED;
      }

      // Check if the object is even callable.
      NS_ENSURE_STATE(JS_ObjectIsCallable(cx, &funval.toObject()));
      {
        // wrap parameters in the target compartment
        // we also pass in the calling window as the first argument
        unsigned argc = args.length() + 1;
        JS::AutoValueVector argv(cx);
        if (!argv.resize(argc)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }

        nsCOMPtr<nsIDOMWindow> currentWin(do_GetInterface(currentInner));
        rv = WrapNative(cx, obj, currentWin, &NS_GET_IID(nsIDOMWindow),
                        true, argv.handleAt(0));
        if (!JS_WrapValue(cx, argv.handleAt(0)))
          return NS_ERROR_FAILURE;

        for (size_t i = 1; i < argc; ++i) {
          argv[i] = args[i - 1];
          if (!JS_WrapValue(cx, argv.handleAt(i)))
            return NS_ERROR_FAILURE;
        }

        JS::Rooted<JS::Value> frval(cx);
        bool ret = JS_CallFunctionValue(cx, thisObject, funval, argv, &frval);

        if (!ret) {
          return NS_ERROR_FAILURE;
        }
      }
    }
  }

  return WrapNative(cx, obj, native, true, args.rval());
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

  for (i = parent_constant_count; i < constant_count; i++) {
    const nsXPTConstant *c = nullptr;

    rv = if_info->GetConstant(i, &c);
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && c, rv);

    uint16_t type = c->GetType().TagPart();

    jsval v;
    switch (type) {
      case nsXPTType::T_I8:
      case nsXPTType::T_U8:
      {
        v = INT_TO_JSVAL(c->GetValue()->val.u8);
        break;
      }
      case nsXPTType::T_I16:
      case nsXPTType::T_U16:
      {
        v = INT_TO_JSVAL(c->GetValue()->val.u16);
        break;
      }
      case nsXPTType::T_I32:
      {
        v = JS_NumberValue(c->GetValue()->val.i32);
        break;
      }
      case nsXPTType::T_U32:
      {
        v = JS_NumberValue(c->GetValue()->val.u32);
        break;
      }
      default:
      {
#ifdef DEBUG
        NS_ERROR("Non-numeric constant found in interface.");
#endif
        continue;
      }
    }

    if (!::JS_DefineProperty(cx, obj, c->GetName(), v,
                             JS_PropertyStub, JS_StrictPropertyStub,
                             JSPROP_ENUMERATE | JSPROP_READONLY |
                             JSPROP_PERMANENT)) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  return NS_OK;
}

class nsDOMConstructor MOZ_FINAL : public nsIDOMDOMConstructor
{
protected:
  nsDOMConstructor(const char16_t* aName,
                   bool aIsConstructable,
                   nsPIDOMWindow* aOwner)
    : mClassName(aName),
      mConstructable(aIsConstructable),
      mWeakOwner(do_GetWeakReference(aOwner))
  {
  }

public:

  static nsresult Create(const char16_t* aName,
                         const nsDOMClassInfoData* aData,
                         const nsGlobalNameStruct* aNameStruct,
                         nsPIDOMWindow* aOwner,
                         nsDOMConstructor** aResult);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDOMCONSTRUCTOR

  nsresult PreCreate(JSContext *cx, JSObject *globalObj, JSObject **parentObj);

  nsresult Construct(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                     JS::Handle<JSObject*> obj, const JS::CallArgs &args,
                     bool *_retval);

  nsresult HasInstance(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JS::Handle<JSObject*> obj, const jsval &val, bool *bp,
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

  static bool IsConstructable(const nsDOMClassInfoData *aData)
  {
    if (IS_EXTERNAL(aData->mCachedClassInfo)) {
      const nsExternalDOMClassInfoData* data =
        static_cast<const nsExternalDOMClassInfoData*>(aData);
      return data->mConstructorCID != nullptr;
    }

    return FindConstructorFunc(aData);
  }
  static bool IsConstructable(const nsGlobalNameStruct *aNameStruct)
  {
    return
      (aNameStruct->mType == nsGlobalNameStruct::eTypeClassConstructor &&
       IsConstructable(&sClassInfoData[aNameStruct->mDOMClassInfoID])) ||
      (aNameStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfo &&
       IsConstructable(aNameStruct->mData)) ||
      aNameStruct->mType == nsGlobalNameStruct::eTypeExternalConstructor ||
      aNameStruct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias;
  }

  const char16_t*   mClassName;
  const bool mConstructable;
  nsWeakPtr          mWeakOwner;
};

//static
nsresult
nsDOMConstructor::Create(const char16_t* aName,
                         const nsDOMClassInfoData* aData,
                         const nsGlobalNameStruct* aNameStruct,
                         nsPIDOMWindow* aOwner,
                         nsDOMConstructor** aResult)
{
  *aResult = nullptr;
  // Prevent creating a constructor if aOwner is inner window which doesn't have
  // an outer window. If the outer window doesn't have an inner window or the
  // caller can't access the outer window's current inner window then try to use
  // the owner (so long as it is, in fact, an inner window). If that doesn't
  // work then prevent creation also.
  nsPIDOMWindow* outerWindow = aOwner->GetOuterWindow();
  nsPIDOMWindow* currentInner =
    outerWindow ? outerWindow->GetCurrentInnerWindow() : aOwner;
  if (!currentInner ||
      (aOwner != currentInner &&
       !nsContentUtils::CanCallerAccess(currentInner) &&
       !(currentInner = aOwner)->IsInnerWindow())) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  bool constructable = aNameStruct ?
                         IsConstructable(aNameStruct) :
                         IsConstructable(aData);

  *aResult = new nsDOMConstructor(aName, constructable, currentInner);
  NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
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
  nsCOMPtr<nsPIDOMWindow> owner(do_QueryReferent(mWeakOwner));
  if (!owner) {
    // Can't do anything.
    return NS_OK;
  }

  nsGlobalWindow *win = static_cast<nsGlobalWindow *>(owner.get());
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
                              const jsval &v, bool *bp, bool *_retval)

{
  // No need to look these up in the hash.
  *bp = false;
  if (JSVAL_IS_PRIMITIVE(v)) {
    return NS_OK;
  }

  JS::Rooted<JSObject*> dom_obj(cx, v.toObjectOrNull());
  NS_ASSERTION(dom_obj, "nsDOMConstructor::HasInstance couldn't get object");

  // This might not be the right object, if there are wrappers. Unwrap if we can.
  JSObject *wrapped_obj = js::CheckedUnwrap(dom_obj, /* stopAtOuter = */ false);
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
    JS::Rooted<JS::Value> val(cx);
    if (!JS_GetProperty(cx, obj, "prototype", &val)) {
      return NS_ERROR_UNEXPECTED;
    }

    if (JSVAL_IS_PRIMITIVE(val)) {
      return NS_OK;
    }

    JS::Rooted<JSObject*> dot_prototype(cx, val.toObjectOrNull());

    JS::Rooted<JSObject*> proto(cx, dom_obj);
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

  if (name_struct->mType != nsGlobalNameStruct::eTypeClassConstructor &&
      name_struct->mType != nsGlobalNameStruct::eTypeExternalClassInfo &&
      name_struct->mType != nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    // Doesn't have DOM interfaces.
    return NS_OK;
  }

  const nsGlobalNameStruct *class_name_struct = GetNameStruct();
  NS_ENSURE_TRUE(class_name_struct, NS_ERROR_FAILURE);

  if (name_struct == class_name_struct) {
    *bp = true;

    return NS_OK;
  }

  nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
  NS_ASSERTION(nameSpaceManager, "Can't get namespace manager?");

  const nsIID *class_iid;
  if (class_name_struct->mType == nsGlobalNameStruct::eTypeInterface ||
      class_name_struct->mType == nsGlobalNameStruct::eTypeClassProto) {
    class_iid = &class_name_struct->mIID;
  } else if (class_name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    class_iid =
      sClassInfoData[class_name_struct->mDOMClassInfoID].mProtoChainInterface;
  } else if (class_name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
    class_iid = class_name_struct->mData->mProtoChainInterface;
  } else if (class_name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    const nsGlobalNameStruct* alias_struct =
      nameSpaceManager->GetConstructorProto(class_name_struct);
    if (!alias_struct) {
      NS_ERROR("Couldn't get constructor prototype.");
      return NS_ERROR_UNEXPECTED;
    }

    if (alias_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
      class_iid =
        sClassInfoData[alias_struct->mDOMClassInfoID].mProtoChainInterface;
    } else if (alias_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
      class_iid = alias_struct->mData->mProtoChainInterface;
    } else {
      NS_ERROR("Expected eTypeClassConstructor or eTypeExternalClassInfo.");
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    *bp = false;

    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    name_struct = nameSpaceManager->GetConstructorProto(name_struct);
    if (!name_struct) {
      NS_ERROR("Couldn't get constructor prototype.");
      return NS_ERROR_UNEXPECTED;
    }
  }

  NS_ASSERTION(name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor ||
               name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo,
               "The constructor was set up with a struct of the wrong type.");

  const nsDOMClassInfoData *ci_data = nullptr;
  if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor &&
      name_struct->mDOMClassInfoID >= 0) {
    ci_data = &sClassInfoData[name_struct->mDOMClassInfoID];
  } else if (name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
    ci_data = name_struct->mData;
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
  if (class_name_struct->mType == nsGlobalNameStruct::eTypeInterface ||
      class_name_struct->mType == nsGlobalNameStruct::eTypeClassProto) {
    class_iid = &class_name_struct->mIID;
  } else if (class_name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    class_iid =
      sClassInfoData[class_name_struct->mDOMClassInfoID].mProtoChainInterface;
  } else if (class_name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
    class_iid = class_name_struct->mData->mProtoChainInterface;
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
            nsIXPConnectJSObjectHolder **aProto)
{
  NS_ASSERTION(aNameStruct->mType ==
                 nsGlobalNameStruct::eTypeClassConstructor ||
               aNameStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfo,
               "Wrong type!");

  nsCOMPtr<nsIClassInfo> ci;
  if (aNameStruct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    int32_t id = aNameStruct->mDOMClassInfoID;
    NS_ABORT_IF_FALSE(id >= 0, "Negative DOM classinfo?!?");

    nsDOMClassInfoID ci_id = (nsDOMClassInfoID)id;

    ci = NS_GetDOMClassInfoInstance(ci_id);

    // In most cases we want to find the wrapped native prototype in
    // aWin's scope and use that prototype for
    // ClassName.prototype. But in the case where we're setting up
    // "Window.prototype" or "ChromeWindow.prototype" we want to do
    // the look up in aWin's outer window's scope since the inner
    // window's wrapped native prototype comes from the outer
    // window's scope.
    if (ci_id == eDOMClassInfo_Window_id ||
        ci_id == eDOMClassInfo_ModalContentWindow_id ||
        ci_id == eDOMClassInfo_ChromeWindow_id) {
      nsGlobalWindow *scopeWindow = aWin->GetOuterWindowInternal();

      if (scopeWindow) {
        aWin = scopeWindow;
      }
    }
  }
  else {
    ci = nsDOMClassInfo::GetClassInfoInstance(aNameStruct->mData);
  }
  NS_ENSURE_TRUE(ci, NS_ERROR_UNEXPECTED);

  nsresult rv =
    aXPConnect->GetWrappedNativePrototype(cx, aWin->GetGlobalJSObject(), ci,
                                          aProto);
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JSObject*> proto_obj(cx, (*aProto)->GetJSObject());
  if (!JS_WrapObject(cx, &proto_obj)) {
    return NS_ERROR_FAILURE;
  }

  NS_IF_RELEASE(*aProto);
  return aXPConnect->HoldObject(cx, proto_obj, aProto);
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
                 JS::MutableHandle<JSPropertyDescriptor> ctorDesc)
{
  JS::Rooted<JSObject*> dot_prototype(cx, aDot_prototype);
  NS_ASSERTION(ci_data ||
               (name_struct &&
                name_struct->mType == nsGlobalNameStruct::eTypeClassProto),
               "Wrong type or missing ci_data!");

  nsRefPtr<nsDOMConstructor> constructor;
  nsresult rv = nsDOMConstructor::Create(name, ci_data, name_struct, aWin,
                                         getter_AddRefs(constructor));
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JS::Value> v(cx);

  rv = WrapNative(cx, obj, constructor, &NS_GET_IID(nsIDOMDOMConstructor),
                  false, &v);
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

      JS::Rooted<JS::Value> val(cx);
      if (!JS_LookupProperty(cx, winobj, CutPrefix(class_parent_name), &val)) {
        return NS_ERROR_UNEXPECTED;
      }

      if (val.isObject()) {
        JS::Rooted<JSObject*> obj(cx, &val.toObject());
        if (!JS_LookupProperty(cx, obj, "prototype", &val)) {
          return NS_ERROR_UNEXPECTED;
        }

        if (val.isObject()) {
          proto = &val.toObject();
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
                                                   proto,
                                                   winobj);
      NS_ENSURE_TRUE(dot_prototype, NS_ERROR_OUT_OF_MEMORY);
    }
  }

  v = OBJECT_TO_JSVAL(dot_prototype);

  JSAutoCompartment ac(cx, class_obj);

  // Per ECMA, the prototype property is {DontEnum, DontDelete, ReadOnly}
  if (!JS_WrapValue(cx, &v) ||
      !JS_DefineProperty(cx, class_obj, "prototype", v,
                         JS_PropertyStub, JS_StrictPropertyStub,
                         JSPROP_PERMANENT | JSPROP_READONLY)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

static bool
OldBindingConstructorEnabled(const nsGlobalNameStruct *aStruct,
                             nsGlobalWindow *aWin, JSContext *cx)
{
  MOZ_ASSERT(aStruct->mType == nsGlobalNameStruct::eTypeClassConstructor ||
             aStruct->mType == nsGlobalNameStruct::eTypeExternalClassInfo);

  // Don't expose chrome only constructors to content windows.
  if (aStruct->mChromeOnly &&
      (aStruct->mAllowXBL ? !IsChromeOrXBL(cx, nullptr) :
       !nsContentUtils::IsSystemPrincipal(aWin->GetPrincipal()))) {
    return false;
  }

  // Don't expose CSSSupportsRule unless @supports processing is enabled.
  if (aStruct->mDOMClassInfoID == eDOMClassInfo_CSSSupportsRule_id) {
    if (!CSSSupportsRule::PrefEnabled()) {
      return false;
    }
  }

  // Don't expose CSSFontFeatureValuesRule unless the pref is enabled
  if (aStruct->mDOMClassInfoID == eDOMClassInfo_CSSFontFeatureValuesRule_id) {
    return nsCSSFontFeatureValuesRule::PrefEnabled();
  }

  return true;
}

static nsresult
LookupComponentsShim(JSContext *cx, JS::Handle<JSObject*> global,
                     nsPIDOMWindow *win,
                     JS::MutableHandle<JSPropertyDescriptor> desc);

// static
nsresult
nsWindowSH::GlobalResolve(nsGlobalWindow *aWin, JSContext *cx,
                          JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                          JS::MutableHandle<JSPropertyDescriptor> desc)
{
  if (id == XPCJSRuntime::Get()->GetStringID(XPCJSRuntime::IDX_COMPONENTS)) {
    return LookupComponentsShim(cx, obj, aWin, desc);
  }

  nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  nsDependentJSString name(id);

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

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfoCreator) {
    rv = GetExternalClassInfo(nameSpaceManager, name, name_struct,
                              &name_struct);
    if (NS_FAILED(rv) || !name_struct) {
      return rv;
    }
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeNewDOMBinding ||
      name_struct->mType == nsGlobalNameStruct::eTypeInterface ||
      name_struct->mType == nsGlobalNameStruct::eTypeClassProto ||
      name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
    // Lookup new DOM bindings.
    DefineInterface getOrCreateInterfaceObject =
      name_struct->mDefineDOMInterface;
    if (getOrCreateInterfaceObject) {
      if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor &&
          !OldBindingConstructorEnabled(name_struct, aWin, cx)) {
        return NS_OK;
      }

      ConstructorEnabled* checkEnabledForScope = name_struct->mConstructorEnabled;
      // We do the enabled check on the current compartment of cx, but for the
      // actual object we pass in the underlying object in the Xray case.  That
      // way the callee can decide whether to allow access based on the caller
      // or the window being touched.
      JS::Rooted<JSObject*> global(cx,
        js::CheckedUnwrap(obj, /* stopAtOuter = */ false));
      if (!global) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }
      if (checkEnabledForScope && !checkEnabledForScope(cx, global)) {
        return NS_OK;
      }

      // The DOM constructor resolve machinery interacts with Xrays in tricky
      // ways, and there are some asymmetries that are important to understand.
      //
      // In the regular (non-Xray) case, we only want to resolve constructors
      // once (so that if they're deleted, they don't reappear). We do this by
      // stashing the constructor in a slot on the global, such that we can see
      // during resolve whether we've created it already. This is rather
      // memory-intensive, so we don't try to maintain these semantics when
      // manipulating a global over Xray (so the properties just re-resolve if
      // they've been deleted).
      //
      // Unfortunately, there's a bit of an impedance-mismatch between the Xray
      // and non-Xray machinery. The Xray machinery wants an API that returns a
      // JSPropertyDescriptor, so that the resolve hook doesn't have to get
      // snared up with trying to define a property on the Xray holder. At the
      // same time, the DefineInterface callbacks are set up to define things
      // directly on the global.  And re-jiggering them to return property
      // descriptors is tricky, because some DefineInterface callbacks define
      // multiple things (like the Image() alias for HTMLImageElement).
      //
      // So the setup is as-follows:
      //
      // * The resolve function takes a JSPropertyDescriptor, but in the
      //   non-Xray case, callees may define things directly on the global, and
      //   set the value on the property descriptor to |undefined| to indicate
      //   that there's nothing more for the caller to do. We assert against
      //   this behavior in the Xray case.
      //
      // * We make sure that we do a non-Xray resolve first, so that all the
      //   slots are set up. In the Xray case, this means unwrapping and doing
      //   a non-Xray resolve before doing the Xray resolve.
      //
      // This all could use some grand refactoring, but for now we just limp
      // along.
      if (xpc::WrapperFactory::IsXrayWrapper(obj)) {
        JS::Rooted<JSObject*> interfaceObject(cx);
        {
          JSAutoCompartment ac(cx, global);
          interfaceObject = getOrCreateInterfaceObject(cx, global, id, false);
        }
        if (NS_WARN_IF(!interfaceObject)) {
          return NS_ERROR_FAILURE;
        }
        if (!JS_WrapObject(cx, &interfaceObject)) {
          return NS_ERROR_FAILURE;
        }

        FillPropertyDescriptor(desc, obj, 0, JS::ObjectValue(*interfaceObject));
      } else {
        JS::Rooted<JSObject*> interfaceObject(cx,
          getOrCreateInterfaceObject(cx, obj, id, true));
        if (NS_WARN_IF(!interfaceObject)) {
          return NS_ERROR_FAILURE;
        }
        // We've already defined the property.  We indicate this to the caller
        // by filling a property descriptor with JS::UndefinedValue() as the
        // value.  We still have to fill in a property descriptor, though, so
        // that the caller knows the property is in fact on this object.  It
        // doesn't matter what we pass for the "readonly" argument here.
        FillPropertyDescriptor(desc, obj, JS::UndefinedValue(), false);
      }

      return NS_OK;
    }
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeInterface) {
    // We're resolving a name of a DOM interface for which there is no
    // direct DOM class, create a constructor object...
    nsRefPtr<nsDOMConstructor> constructor;
    rv = nsDOMConstructor::Create(class_name,
                                  nullptr,
                                  name_struct,
                                  static_cast<nsPIDOMWindow*>(aWin),
                                  getter_AddRefs(constructor));
    NS_ENSURE_SUCCESS(rv, rv);

    JS::Rooted<JS::Value> v(cx);
    rv = WrapNative(cx, obj, constructor, &NS_GET_IID(nsIDOMDOMConstructor),
                    false, &v);
    NS_ENSURE_SUCCESS(rv, rv);

    JS::Rooted<JSObject*> class_obj(cx, &v.toObject());

    // ... and define the constants from the DOM interface on that
    // constructor object.

    {
      JSAutoCompartment ac(cx, class_obj);
      rv = DefineInterfaceConstants(cx, class_obj, &name_struct->mIID);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (!JS_WrapValue(cx, &v)) {
      return NS_ERROR_UNEXPECTED;
    }

    FillPropertyDescriptor(desc, obj, 0, v);
    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor ||
      name_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
    if (!OldBindingConstructorEnabled(name_struct, aWin, cx)) {
      return NS_OK;
    }

    // Create the XPConnect prototype for our classinfo, PostCreateProto will
    // set up the prototype chain.  This will go ahead and define things on the
    // actual window's global.
    nsCOMPtr<nsIXPConnectJSObjectHolder> proto_holder;
    rv = GetXPCProto(sXPConnect, cx, aWin, name_struct,
                     getter_AddRefs(proto_holder));
    NS_ENSURE_SUCCESS(rv, rv);
    bool isXray = xpc::WrapperFactory::IsXrayWrapper(obj);
    MOZ_ASSERT_IF(obj != aWin->GetGlobalJSObject(), isXray);
    if (!isXray) {
      // GetXPCProto already defined the property for us
      FillPropertyDescriptor(desc, obj, JS::UndefinedValue(), false);
      return NS_OK;
    }

    // This is the Xray case.  Look up the constructor object for this
    // prototype.
    JS::Rooted<JSObject*> dot_prototype(cx, proto_holder->GetJSObject());
    NS_ENSURE_STATE(dot_prototype);

    const nsDOMClassInfoData *ci_data;
    if (name_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
      ci_data = &sClassInfoData[name_struct->mDOMClassInfoID];
    } else {
      ci_data = name_struct->mData;
    }

    return ResolvePrototype(sXPConnect, aWin, cx, obj, class_name, ci_data,
                            name_struct, nameSpaceManager, dot_prototype,
                            desc);
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeClassProto) {
    // We don't have a XPConnect prototype object, let ResolvePrototype create
    // one.
    return ResolvePrototype(sXPConnect, aWin, cx, obj, class_name, nullptr,
                            name_struct, nameSpaceManager, nullptr, desc);
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructorAlias) {
    const nsGlobalNameStruct *alias_struct =
      nameSpaceManager->GetConstructorProto(name_struct);
    NS_ENSURE_TRUE(alias_struct, NS_ERROR_UNEXPECTED);

    // We need to use the XPConnect prototype for the DOM class that this
    // constructor is an alias for (for example for Image we need the prototype
    // for HTMLImageElement).
    nsCOMPtr<nsIXPConnectJSObjectHolder> proto_holder;
    rv = GetXPCProto(sXPConnect, cx, aWin, alias_struct,
                     getter_AddRefs(proto_holder));
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject* dot_prototype = proto_holder->GetJSObject();
    NS_ENSURE_STATE(dot_prototype);

    const nsDOMClassInfoData *ci_data;
    if (alias_struct->mType == nsGlobalNameStruct::eTypeClassConstructor) {
      ci_data = &sClassInfoData[alias_struct->mDOMClassInfoID];
    } else if (alias_struct->mType == nsGlobalNameStruct::eTypeExternalClassInfo) {
      ci_data = alias_struct->mData;
    } else {
      return NS_ERROR_UNEXPECTED;
    }

    return ResolvePrototype(sXPConnect, aWin, cx, obj, class_name, ci_data,
                            name_struct, nameSpaceManager, nullptr, desc);
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeExternalConstructor) {
    nsRefPtr<nsDOMConstructor> constructor;
    rv = nsDOMConstructor::Create(class_name, nullptr, name_struct,
                                  static_cast<nsPIDOMWindow*>(aWin),
                                  getter_AddRefs(constructor));
    NS_ENSURE_SUCCESS(rv, rv);

    JS::Rooted<JS::Value> val(cx);
    rv = WrapNative(cx, obj, constructor, &NS_GET_IID(nsIDOMDOMConstructor),
                    true, &val);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(val.isObject(), "Why didn't we get a JSObject?");

    FillPropertyDescriptor(desc, obj, 0, val);

    return NS_OK;
  }

  if (name_struct->mType == nsGlobalNameStruct::eTypeProperty) {
    if (name_struct->mChromeOnly &&
        (name_struct->mAllowXBL ? !IsChromeOrXBL(cx, nullptr) :
         !nsContentUtils::IsCallerChrome()))
      return NS_OK;

    // Before defining a global property, check for a named subframe of the
    // same name. If it exists, we don't want to shadow it.
    nsCOMPtr<nsIDOMWindow> childWin = aWin->GetChildWindow(name);
    if (childWin)
      return NS_OK;

    nsCOMPtr<nsISupports> native(do_CreateInstance(name_struct->mCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    JS::Rooted<JS::Value> prop_val(cx, JS::UndefinedValue()); // Property value.

    nsCOMPtr<nsIDOMGlobalPropertyInitializer> gpi(do_QueryInterface(native));
    if (gpi) {
      rv = gpi->Init(aWin, &prop_val);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (JSVAL_IS_PRIMITIVE(prop_val) && !JSVAL_IS_NULL(prop_val)) {
      JSObject *scope;

      if (aWin->IsOuterWindow()) {
        nsGlobalWindow *inner = aWin->GetCurrentInnerWindowInternal();
        NS_ENSURE_TRUE(inner, NS_ERROR_UNEXPECTED);

        scope = inner->GetGlobalJSObject();
      } else {
        scope = aWin->GetGlobalJSObject();
      }

      rv = WrapNative(cx, scope, native, true, &prop_val);
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

template<class Interface>
static nsresult
LocationSetterGuts(JSContext *cx, JSObject *obj, JS::MutableHandle<JS::Value> vp)
{
  // This function duplicates some of the logic in XPC_WN_HelperSetProperty
  obj = js::CheckedUnwrap(obj, /* stopAtOuter = */ false);
  if (!IS_WN_REFLECTOR(obj))
      return NS_ERROR_XPC_BAD_CONVERT_JS;
  XPCWrappedNative *wrapper = XPCWrappedNative::Get(obj);

  // The error checks duplicate code in THROW_AND_RETURN_IF_BAD_WRAPPER
  NS_ENSURE_TRUE(!wrapper || wrapper->IsValid(), NS_ERROR_XPC_HAS_BEEN_SHUTDOWN);

  nsCOMPtr<Interface> xpcomObj = do_QueryWrappedNative(wrapper, obj);
  NS_ENSURE_TRUE(xpcomObj, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMLocation> location;
  nsresult rv = xpcomObj->GetLocation(getter_AddRefs(location));
  NS_ENSURE_SUCCESS(rv, rv);

  // Grab the value we're being set to before we stomp on |vp|
  JS::Rooted<JSString*> val(cx, JS::ToString(cx, vp));
  NS_ENSURE_TRUE(val, NS_ERROR_UNEXPECTED);

  // Make sure |val| stays alive below
  JS::Anchor<JSString *> anchor(val);

  // We have to wrap location into vp before null-checking location, to
  // avoid assigning the wrong thing into the slot.
  rv = WrapNative(cx, JS::CurrentGlobalOrNull(cx), location,
                  &NS_GET_IID(nsIDOMLocation), true, vp);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!location) {
    // Make this a no-op
    return NS_OK;
  }

  nsDependentJSString depStr;
  NS_ENSURE_TRUE(depStr.init(cx, val), NS_ERROR_UNEXPECTED);

  return location->SetHref(depStr);
}

template<class Interface>
static bool
LocationSetter(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, bool strict,
               JS::MutableHandle<JS::Value> vp)
{
  nsresult rv = LocationSetterGuts<Interface>(cx, obj, vp);
  if (NS_FAILED(rv)) {
    xpc::Throw(cx, rv);
    return false;
  }

  return true;
}

static bool
LocationSetterUnwrapper(JSContext *cx, JS::Handle<JSObject*> obj_, JS::Handle<jsid> id,
                        bool strict, JS::MutableHandle<JS::Value> vp)
{
  JS::Rooted<JSObject*> obj(cx, obj_);

  JSObject *wrapped = XPCWrapper::UnsafeUnwrapSecurityWrapper(obj);
  if (wrapped) {
    obj = wrapped;
  }

  return LocationSetter<nsIDOMWindow>(cx, obj, id, strict, vp);
}

struct InterfaceShimEntry {
  const char *geckoName;
  const char *domName;
};

// We add shims from Components.interfaces.nsIDOMFoo to window.Foo for each
// interface that has interface constants that sites might be getting off
// of Ci.
const InterfaceShimEntry kInterfaceShimMap[] =
{ { "nsIDOMFileReader", "FileReader" },
  { "nsIXMLHttpRequest", "XMLHttpRequest" },
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
  { "nsIDOMMediaError", "MediaError" },
  { "nsIDOMOfflineResourceList", "OfflineResourceList" },
  { "nsIDOMRange", "Range" },
  { "nsIDOMSVGLength", "SVGLength" },
  { "nsIDOMNodeFilter", "NodeFilter" },
  { "nsIDOMXPathNamespace", "XPathNamespace" },
  { "nsIDOMXPathResult", "XPathResult" } };

static nsresult
LookupComponentsShim(JSContext *cx, JS::Handle<JSObject*> global,
                     nsPIDOMWindow *win,
                     JS::MutableHandle<JSPropertyDescriptor> desc)
{
  // Keep track of how often this happens.
  Telemetry::Accumulate(Telemetry::COMPONENTS_SHIM_ACCESSED_BY_CONTENT, true);

  // Warn once.
  nsCOMPtr<nsIDocument> doc = win->GetExtantDoc();
  if (doc) {
    doc->WarnOnceAbout(nsIDocument::eComponents, /* asError = */ true);
  }

  // Create a fake Components object.
  JS::Rooted<JSObject*> components(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), global));
  NS_ENSURE_TRUE(components, NS_ERROR_OUT_OF_MEMORY);

  // Create a fake interfaces object.
  JS::Rooted<JSObject*> interfaces(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), global));
  NS_ENSURE_TRUE(interfaces, NS_ERROR_OUT_OF_MEMORY);
  bool ok =
    JS_DefineProperty(cx, components, "interfaces", JS::ObjectValue(*interfaces),
                      JS_PropertyStub, JS_StrictPropertyStub,
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
                           JS_PropertyStub, JS_StrictPropertyStub,
                           JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);
    NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  }

  FillPropertyDescriptor(desc, global, JS::ObjectValue(*components), false);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *obj_, jsid id_, uint32_t flags,
                       JSObject **objp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, obj_);
  JS::Rooted<jsid> id(cx, id_);

  if (!JSID_IS_STRING(id)) {
    return NS_OK;
  }

  MOZ_ASSERT(*_retval == true); // guaranteed by XPC_WN_Helper_NewResolve

  nsGlobalWindow *win = nsGlobalWindow::FromWrapper(wrapper);
  MOZ_ASSERT(win->IsInnerWindow());

  // Don't resolve standard classes on XrayWrappers, only resolve them if we're
  // resolving on the real global object.
  bool isXray = xpc::WrapperFactory::IsXrayWrapper(obj);
  if (!isXray) {
    bool did_resolve = false;
    if (!JS_ResolveStandardClass(cx, obj, id, &did_resolve)) {
      // Return NS_OK to avoid stomping over the exception that was passed
      // down from the ResolveStandardClass call.
      *_retval = false;
      return NS_OK;
    }

    if (did_resolve) {
      *objp = obj;
      return NS_OK;
    }
  }

  // WebIDL quickstubs handle location for us, but Xrays don't see those.  So if
  // we're an Xray, we have to resolve stuff here to make "window.location =
  // someString" work.
  if (sLocation_id == id && isXray) {
    // This must be done even if we're just getting the value of
    // window.location (i.e. no checking flags & JSRESOLVE_ASSIGNING
    // here) since we must define window.location to prevent the
    // getter from being overriden (for security reasons).

    nsCOMPtr<nsIDOMLocation> location;
    nsresult rv = win->GetLocation(getter_AddRefs(location));
    NS_ENSURE_SUCCESS(rv, rv);

    // Make sure we wrap the location object in the window's scope.
    JS::Rooted<JSObject*> scope(cx, wrapper->GetJSObject());

    JS::Rooted<JS::Value> v(cx);
    rv = WrapNative(cx, scope, location, &NS_GET_IID(nsIDOMLocation), true, &v);
    NS_ENSURE_SUCCESS(rv, rv);

    bool ok = JS_WrapValue(cx, &v) &&
                JS_DefinePropertyById(cx, obj, id, v, JS_PropertyStub,
                                      LocationSetterUnwrapper,
                                      JSPROP_PERMANENT | JSPROP_ENUMERATE);

    if (!ok) {
      return NS_ERROR_FAILURE;
    }

    *objp = obj;

    return NS_OK;
  }

  // WebIDL quickstubs handle "top" for us, but Xrays don't see those.  So if
  // we're an Xray and we want "top" to be JSPROP_PERMANENT, we need to resolve
  // it here.
  if (sTop_id == id && isXray) {
    nsCOMPtr<nsIDOMWindow> top;
    nsresult rv = win->GetScriptableTop(getter_AddRefs(top));
    NS_ENSURE_SUCCESS(rv, rv);

    JS::Rooted<JS::Value> v(cx);
    rv = WrapNative(cx, obj, top, &NS_GET_IID(nsIDOMWindow), true, &v);
    NS_ENSURE_SUCCESS(rv, rv);

    // Hold on to the top window object as a global property so we
    // don't need to worry about losing expando properties etc.
    if (!JS_DefinePropertyById(cx, obj, id, v, JS_PropertyStub, JS_StrictPropertyStub,
                               JSPROP_READONLY | JSPROP_PERMANENT |
                               JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }
    *objp = obj;

    return NS_OK;
  }

  if (isXray) {
    // We promise to resolve on the underlying object first.  That will create
    // the actual interface object if needed and store it in a data structure
    // hanging off the global.  Then our second call will wrap up in an Xray as
    // needed.  We do things this way because we use the existence of the
    // object in that data structure as a flag that indicates that its name
    // (and any relevant named constructor names) has been resolved before;
    // this allows us to avoid re-resolving in the Xray case if the property is
    // deleted by page script.
    JS::Rooted<JSObject*> global(cx,
      js::UncheckedUnwrap(obj, /* stopAtOuter = */ false));
    JSAutoCompartment ac(cx, global);
    JS::Rooted<JSPropertyDescriptor> desc(cx);
    if (!win->DoNewResolve(cx, global, id, &desc)) {
      return NS_ERROR_FAILURE;
    }
    // If we have an object here, that means we resolved the property.
    // But if the value is undefined, that means that GlobalResolve
    // also already defined it, so we don't have to.
    if (desc.object() && !desc.value().isUndefined() &&
        !JS_DefinePropertyById(cx, global, id, desc.value(),
                               desc.getter(), desc.setter(),
                               desc.attributes())) {
      return NS_ERROR_FAILURE;
    }
  }

  JS::Rooted<JSPropertyDescriptor> desc(cx);
  if (!win->DoNewResolve(cx, obj, id, &desc)) {
    return NS_ERROR_FAILURE;
  }
  if (desc.object()) {
    // If we have an object here, that means we resolved the property.
    // But if the value is undefined, that means that GlobalResolve
    // also already defined it, so we don't have to.  Note that in the
    // Xray case we should never see undefined.
    MOZ_ASSERT_IF(isXray, !desc.value().isUndefined());
    if (!desc.value().isUndefined() &&
        !JS_DefinePropertyById(cx, obj, id, desc.value(),
                               desc.getter(), desc.setter(),
                               desc.attributes())) {
      return NS_ERROR_FAILURE;
    }

    *objp = obj;
    return NS_OK;
  }

  if (!(flags & JSRESOLVE_ASSIGNING) && sDocument_id == id) {
    nsCOMPtr<nsIDocument> document = win->GetDoc();
    JS::Rooted<JS::Value> v(cx);
    nsresult rv = WrapNative(cx, JS::CurrentGlobalOrNull(cx), document, document,
                             &NS_GET_IID(nsIDOMDocument), &v, false);
    NS_ENSURE_SUCCESS(rv, rv);

    // nsIDocument::WrapObject will handle defining the property.
    *objp = obj;

    // NB: We need to do this for any Xray wrapper.
    if (xpc::WrapperFactory::IsXrayWrapper(obj)) {
      *_retval = JS_WrapValue(cx, &v) &&
                 JS_DefineProperty(cx, obj, "document", v,
                                   JS_PropertyStub, JS_StrictPropertyStub,
                                   JSPROP_READONLY | JSPROP_ENUMERATE);
      if (!*_retval) {
        return NS_ERROR_UNEXPECTED;
      }
    }

    return NS_OK;
  }

  return nsDOMGenericSH::NewResolve(wrapper, cx, obj, id, flags, objp,
                                    _retval);
}

NS_IMETHODIMP
nsWindowSH::Finalize(nsIXPConnectWrappedNative *wrapper, JSFreeOp *fop,
                     JSObject *obj)
{
  // Since this call is virtual, the exact rooting hazard static analysis is
  // not able to determine that it happens during finalization and should be
  // ignored. Moreover, the analysis cannot discover and validate the
  // potential targets of the virtual call to OnFinalize below because of the
  // indirection through nsCOMMPtr. Thus, we annotate the analysis here so
  // that it does not report OnFinalize as GCing with |obj| on stack.
  JS::AutoAssertNoGC nogc;

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(sgo, NS_ERROR_UNEXPECTED);

  sgo->OnFinalize(obj);

  return NS_OK;
}

NS_IMETHODIMP
nsWindowSH::OuterObject(nsIXPConnectWrappedNative *wrapper, JSContext * cx,
                        JSObject * obj, JSObject * *_retval)
{
  nsGlobalWindow *origWin = nsGlobalWindow::FromWrapper(wrapper);
  nsGlobalWindow *win = origWin->GetOuterWindowInternal();

  if (!win) {
    // If we no longer have an outer window. No code should ever be
    // running on a window w/o an outer, which means this hook should
    // never be called when we have no outer. But just in case, return
    // null to prevent leaking an inner window to code in a different
    // window.
    *_retval = nullptr;
    return NS_ERROR_UNEXPECTED;
  }

  JS::Rooted<JSObject*> winObj(cx, win->FastGetGlobalJSObject());
  MOZ_ASSERT(winObj);

  // Note that while |wrapper| is same-compartment with cx, the outer window
  // might not be. If we're running script in an inactive scope and evalute
  // |this|, the outer window is actually a cross-compartment wrapper. So we
  // need to wrap here.
  if (!JS_WrapObject(cx, &winObj)) {
    *_retval = nullptr;
    return NS_ERROR_UNEXPECTED;
  }

  *_retval = winObj;
  return NS_OK;
}

NS_IMETHODIMP
nsLocationSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                        JSObject *globalObj, JSObject **parentObj)
{
  // window.location can be held onto by both evil pages that want to track the
  // user's progress on the web and bookmarklets that want to use the location
  // object. Parent it to the outer window so that access checks do the Right
  // Thing.
  *parentObj = globalObj;

  nsCOMPtr<nsIDOMLocation> safeLoc(do_QueryInterface(nativeObj));
  if (!safeLoc) {
    // Oops, this wasn't really a location object. This can happen if someone
    // tries to use our scriptable helper as a real object and tries to wrap
    // it, see bug 319296
    return NS_OK;
  }

  nsLocation *loc = (nsLocation *)safeLoc.get();
  nsIDocShell *ds = loc->GetDocShell();
  if (!ds) {
    NS_WARNING("Refusing to create a location in the wrong scope");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_GetInterface(ds);
  if (!sgo) {
    NS_WARNING("Refusing to create a location in the wrong scope because the "
               "docshell is being destroyed");
    return NS_ERROR_UNEXPECTED;
  }

  *parentObj = sgo->GetGlobalJSObject();
  return *parentObj ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocationSH::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *obj, jsid aId, jsval *vp, bool *_retval)
{
  // Shadowing protection. This will go away when nsLocation moves to the new
  // bindings.
  JS::Rooted<jsid> id(cx, aId);
  if (wrapper->HasNativeMember(id)) {
    JS_ReportError(cx, "Permission denied to shadow native property");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// EventTarget helper

NS_IMETHODIMP
nsEventTargetSH::PreCreate(nsISupports *nativeObj, JSContext *cx,
                           JSObject *aGlobalObj, JSObject **parentObj)
{
  JS::Rooted<JSObject*> globalObj(cx, aGlobalObj);
  nsDOMEventTargetHelper *target =
    nsDOMEventTargetHelper::FromSupports(nativeObj);

  nsCOMPtr<nsIScriptGlobalObject> native_parent;
  target->GetParentObject(getter_AddRefs(native_parent));

  *parentObj = native_parent ? native_parent->GetGlobalJSObject() : globalObj;

  return *parentObj ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsEventTargetSH::AddProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *obj, jsid id, jsval *vp, bool *_retval)
{
  nsEventTargetSH::PreserveWrapper(GetNative(wrapper, obj));

  return NS_OK;
}

void
nsEventTargetSH::PreserveWrapper(nsISupports *aNative)
{
  nsDOMEventTargetHelper *target =
    nsDOMEventTargetHelper::FromSupports(aNative);
  target->PreserveWrapper(aNative);
}

// Generic array scriptable helper.

NS_IMETHODIMP
nsGenericArraySH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                             JSObject *aObj, jsid aId, uint32_t flags,
                             JSObject **objp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  if (id == sLength_id) {
    // Bail early; this isn't something we're interested in
    return NS_OK;
  }

  bool is_number = false;
  int32_t n = GetArrayIndexFromId(cx, id, &is_number);

  if (is_number && n >= 0) {
    // XXX The following is a cheap optimization to avoid hitting xpconnect to
    // get the length. We may want to consider asking our concrete
    // implementation for the length, and falling back onto the GetProperty if
    // it doesn't provide one.

    uint32_t length;
    nsresult rv = GetLength(wrapper, cx, obj, &length);
    NS_ENSURE_SUCCESS(rv, rv);

    uint32_t index = uint32_t(n);
    if (index < length) {
      *_retval = ::JS_DefineElement(cx, obj, index, JSVAL_VOID, nullptr, nullptr,
                                    JSPROP_ENUMERATE | JSPROP_SHARED);
      *objp = obj;
    }
  }

  return NS_OK;
}

nsresult
nsGenericArraySH::GetLength(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JS::Handle<JSObject*> obj, uint32_t *length)
{
  *length = 0;

  JS::Rooted<JS::Value> lenval(cx);
  if (!JS_GetProperty(cx, obj, "length", &lenval)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!JSVAL_IS_INT(lenval)) {
    // This can apparently happen with some sparse array impls falling back
    // onto this code.
    return NS_OK;
  }

  int32_t slen = JSVAL_TO_INT(lenval);
  if (slen < 0) {
    return NS_OK;
  }

  *length = (uint32_t)slen;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericArraySH::Enumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                            JSObject *aObj, bool *_retval)
{
  // Recursion protection in case someone tries to be smart and call
  // the enumerate hook from a user defined .length getter, or
  // somesuch.

  JS::Rooted<JSObject*> obj(cx, aObj);
  static bool sCurrentlyEnumerating;

  if (sCurrentlyEnumerating) {
    // Don't recurse to death.
    return NS_OK;
  }

  sCurrentlyEnumerating = true;

  JS::Rooted<JS::Value> len_val(cx);
  bool ok = ::JS_GetProperty(cx, obj, "length", &len_val);

  if (ok && JSVAL_IS_INT(len_val)) {
    int32_t length = JSVAL_TO_INT(len_val);

    for (int32_t i = 0; ok && i < length; ++i) {
      ok = ::JS_DefineElement(cx, obj, i, JSVAL_VOID, nullptr, nullptr,
                              JSPROP_ENUMERATE | JSPROP_SHARED);
    }
  }

  sCurrentlyEnumerating = false;

  return ok ? NS_OK : NS_ERROR_UNEXPECTED;
}

// Array scriptable helper

NS_IMETHODIMP
nsArraySH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                       JSObject *aObj, jsid aId, jsval *vp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  bool is_number = false;
  int32_t n = GetArrayIndexFromId(cx, id, &is_number);

  nsresult rv = NS_OK;

  if (is_number) {
    if (n < 0) {
      return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    // Make sure rv == NS_OK here, so GetItemAt implementations that never fail
    // don't have to set rv.
    rv = NS_OK;
    nsWrapperCache *cache = nullptr;
    nsISupports* array_item =
      GetItemAt(GetNative(wrapper, obj), n, &cache, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (array_item) {
      JS::Rooted<JS::Value> rval(cx);
      rv = WrapNative(cx, JS::CurrentGlobalOrNull(cx), array_item, cache,
                      true, &rval);
      NS_ENSURE_SUCCESS(rv, rv);
      *vp = rval;

      rv = NS_SUCCESS_I_DID_SOMETHING;
    }
  }

  return rv;
}


// StyleSheetList helper

nsISupports*
nsStyleSheetListSH::GetItemAt(nsISupports *aNative, uint32_t aIndex,
                              nsWrapperCache **aCache, nsresult *rv)
{
  nsIDOMStyleSheetList* list = static_cast<nsIDOMStyleSheetList*>(aNative);
  nsCOMPtr<nsIDOMStyleSheet> sheet;
  list->Item(aIndex, getter_AddRefs(sheet));
  return sheet;
}


// CSSRuleList scriptable helper

nsISupports*
nsCSSRuleListSH::GetItemAt(nsISupports *aNative, uint32_t aIndex,
                           nsWrapperCache **aCache, nsresult *aResult)
{
  nsICSSRuleList* list = static_cast<nsICSSRuleList*>(aNative);
#ifdef DEBUG
  {
    nsCOMPtr<nsICSSRuleList> list_qi = do_QueryInterface(aNative);

    // If this assertion fires the QI implementation for the object in
    // question doesn't use the nsICSSRuleList pointer as the nsISupports
    // pointer. That must be fixed, or we'll crash...
    NS_ABORT_IF_FALSE(list_qi == list, "Uh, fix QI!");
  }
#endif

  return list->Item(aIndex);
}


// Storage2SH

// One reason we need a newResolve hook is that in order for
// enumeration of storage object keys to work the keys we're
// enumerating need to exist on the storage object for the JS engine
// to find them.

NS_IMETHODIMP
nsStorage2SH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                         JSObject *obj, jsid aId, uint32_t flags,
                         JSObject **objp, bool *_retval)
{
  JS::Rooted<jsid> id(cx, aId);
  if (ObjectIsNativeWrapper(cx, obj)) {
    return NS_OK;
  }

  JS::Rooted<JSObject*> realObj(cx, wrapper->GetJSObject());

  JSAutoCompartment ac(cx, realObj);

  // First check to see if the property is defined on our prototype,
  // after converting id to a string if it's an integer.

  JS::Rooted<JSString*> jsstr(cx, IdToString(cx, id));
  if (!jsstr) {
    return NS_OK;
  }

  JS::Rooted<JSObject*> proto(cx);
  if (!::JS_GetPrototype(cx, realObj, &proto)) {
    return NS_ERROR_FAILURE;
  }
  bool hasProp;

  if (proto &&
      (::JS_HasPropertyById(cx, proto, id, &hasProp) &&
       hasProp)) {
    // We found the property we're resolving on the prototype,
    // nothing left to do here then.

    return NS_OK;
  }

  // We're resolving property that doesn't exist on the prototype,
  // check if the key exists in the storage object.

  nsCOMPtr<nsIDOMStorage> storage(do_QueryWrappedNative(wrapper));

  nsDependentJSString depStr;
  NS_ENSURE_TRUE(depStr.init(cx, jsstr), NS_ERROR_UNEXPECTED);

  // GetItem() will return null if the caller can't access the session
  // storage item.
  nsAutoString data;
  nsresult rv = storage->GetItem(depStr, data);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!DOMStringIsNull(data)) {
    if (!::JS_DefinePropertyById(cx, realObj, id, JSVAL_VOID, nullptr,
                                 nullptr, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    *objp = realObj;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsStorage2SH::GetProperty(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                          JSObject *aObj, jsid aId, jsval *vp, bool *_retval)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  JS::Rooted<jsid> id(cx, aId);
  nsCOMPtr<nsIDOMStorage> storage(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);

  JSString* key = IdToString(cx, id);
  NS_ENSURE_TRUE(key, NS_ERROR_UNEXPECTED);

  nsDependentJSString keyStr;
  NS_ENSURE_TRUE(keyStr.init(cx, key), NS_ERROR_UNEXPECTED);

  // For native wrappers, do not get random names on storage objects.
  if (ObjectIsNativeWrapper(cx, obj)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoString val;
  nsresult rv = storage->GetItem(keyStr, val);
  NS_ENSURE_SUCCESS(rv, rv);

  if (DOMStringIsNull(val)) {
    // No such key.
    *vp = JSVAL_VOID;
  } else {
    JSString* str =
      JS_NewUCStringCopyN(cx, static_cast<const jschar *>(val.get()),
                          val.Length());
    NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

    *vp = STRING_TO_JSVAL(str);
  }

  return NS_SUCCESS_I_DID_SOMETHING;
}

NS_IMETHODIMP
nsStorage2SH::SetProperty(nsIXPConnectWrappedNative *wrapper,
                          JSContext *cx, JSObject *obj, jsid aId,
                          jsval *vp, bool *_retval)
{
  JS::Rooted<jsid> id(cx, aId);
  nsCOMPtr<nsIDOMStorage> storage(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);

  JSString *key = IdToString(cx, id);
  NS_ENSURE_TRUE(key, NS_ERROR_UNEXPECTED);

  nsDependentJSString keyStr;
  NS_ENSURE_TRUE(keyStr.init(cx, key), NS_ERROR_UNEXPECTED);

  JS::Rooted<JS::Value> val(cx, *vp);
  JSString *value = JS::ToString(cx, val);
  NS_ENSURE_TRUE(value, NS_ERROR_UNEXPECTED);

  nsDependentJSString valueStr;
  NS_ENSURE_TRUE(valueStr.init(cx, value), NS_ERROR_UNEXPECTED);

  nsresult rv = storage->SetItem(keyStr, valueStr);
  if (NS_SUCCEEDED(rv)) {
    rv = NS_SUCCESS_I_DID_SOMETHING;
  }

  return rv;
}

NS_IMETHODIMP
nsStorage2SH::DelProperty(nsIXPConnectWrappedNative *wrapper,
                          JSContext *cx, JSObject *obj, jsid aId,
                          bool *_retval)
{
  JS::Rooted<jsid> id(cx, aId);
  nsCOMPtr<nsIDOMStorage> storage(do_QueryWrappedNative(wrapper));
  NS_ENSURE_TRUE(storage, NS_ERROR_UNEXPECTED);

  JSString *key = IdToString(cx, id);
  NS_ENSURE_TRUE(key, NS_ERROR_UNEXPECTED);

  nsDependentJSString keyStr;
  NS_ENSURE_TRUE(keyStr.init(cx, key), NS_ERROR_UNEXPECTED);

  nsresult rv = storage->RemoveItem(keyStr);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *_retval = true;
  return NS_SUCCESS_I_DID_SOMETHING;
}


NS_IMETHODIMP
nsStorage2SH::NewEnumerate(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                           JSObject *obj, uint32_t enum_op, jsval *statep,
                           jsid *idp, bool *_retval)
{
  if (enum_op == JSENUMERATE_INIT || enum_op == JSENUMERATE_INIT_ALL) {
    nsCOMPtr<nsPIDOMStorage> storage(do_QueryWrappedNative(wrapper));

    // XXXndeakin need to free the keys afterwards
    nsTArray<nsString> *keys = storage->GetKeys();
    NS_ENSURE_TRUE(keys, NS_ERROR_OUT_OF_MEMORY);

    *statep = PRIVATE_TO_JSVAL(keys);

    if (idp) {
      *idp = INT_TO_JSID(keys->Length());
    }
    return NS_OK;
  }

  nsTArray<nsString> *keys =
    (nsTArray<nsString> *)JSVAL_TO_PRIVATE(*statep);

  if (enum_op == JSENUMERATE_NEXT && keys->Length() != 0) {
    nsString& key = keys->ElementAt(0);
    JSString *str =
      JS_NewUCStringCopyN(cx, key.get(), key.Length());
    NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

    JS::Rooted<jsid> id(cx);
    JS_ValueToId(cx, JS::StringValue(str), &id);
    *idp = id;

    keys->RemoveElementAt(0);

    return NS_OK;
  }

  // destroy the keys array if we have no keys or if we're done
  NS_ABORT_IF_FALSE(enum_op == JSENUMERATE_DESTROY ||
                    (enum_op == JSENUMERATE_NEXT && keys->Length() == 0),
                    "Bad call from the JS engine");
  delete keys;

  *statep = JSVAL_NULL;

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
nsDOMConstructorSH::NewResolve(nsIXPConnectWrappedNative *wrapper, JSContext *cx,
                               JSObject *aObj, jsid aId, uint32_t flags,
                               JSObject **objp, bool *_retval)
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
  // NewResolve hooks.
  bool found;
  if (!JS_HasPropertyById(cx, nativePropsObj, id, &found)) {
    *_retval = false;
    return NS_OK;
  }

  if (found) {
    *objp = obj;
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

NS_IMETHODIMP
nsNonDOMObjectSH::GetFlags(uint32_t *aFlags)
{
  // This is NOT a DOM Object.  Use this helper class for cases when you need
  // to do something like implement nsISecurityCheckedComponent in a meaningful
  // way.
  *aFlags = nsIClassInfo::MAIN_THREAD_ONLY | nsIClassInfo::SINGLETON_CLASSINFO;
  return NS_OK;
}
