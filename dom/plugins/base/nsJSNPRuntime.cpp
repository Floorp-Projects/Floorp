/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "jsfriendapi.h"
#include "jswrapper.h"

#include "nsIInterfaceRequestorUtils.h"
#include "nsJSNPRuntime.h"
#include "nsNPAPIPlugin.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIGlobalObject.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsDOMJSUtils.h"
#include "nsJSUtils.h"
#include "nsIDocument.h"
#include "nsIXPConnect.h"
#include "xpcpublic.h"
#include "nsIDOMElement.h"
#include "prmem.h"
#include "nsIContent.h"
#include "nsPluginInstanceOwner.h"
#include "nsWrapperCacheInlines.h"
#include "js/GCHashTable.h"
#include "js/TracingAPI.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/plugins/PluginAsyncSurrogate.h"

using mozilla::plugins::AsyncNPObject;
using mozilla::plugins::PluginAsyncSurrogate;

#define NPRUNTIME_JSCLASS_NAME "NPObject JS wrapper class"

using namespace mozilla::plugins::parent;
using namespace mozilla;

#include "mozilla/plugins/PluginScriptableObjectParent.h"
using mozilla::plugins::PluginScriptableObjectParent;
using mozilla::plugins::ParentNPObject;

struct JSObjWrapperHasher
{
  typedef nsJSObjWrapperKey Key;
  typedef Key Lookup;

  static uint32_t hash(const Lookup &l) {
    return js::MovableCellHasher<JS::Heap<JSObject*>>::hash(l.mJSObj) ^
           HashGeneric(l.mNpp);
  }

  static bool match(const Key& k, const Lookup &l) {
      return js::MovableCellHasher<JS::Heap<JSObject*>>::match(k.mJSObj, l.mJSObj) &&
             k.mNpp == l.mNpp;
  }
};

namespace js {
template <>
struct GCPolicy<nsJSObjWrapper*> {
    static void trace(JSTracer* trc, nsJSObjWrapper** wrapper, const char* name) {
        MOZ_ASSERT(wrapper);
        MOZ_ASSERT(*wrapper);
        (*wrapper)->trace(trc);
    }
};
} // namespace js

class NPObjWrapperHashEntry : public PLDHashEntryHdr
{
public:
  NPObject *mNPObj; // Must be the first member for the PLDHash stubs to work
  JS::TenuredHeap<JSObject*> mJSObj;
  NPP mNpp;
};

// Hash of JSObject wrappers that wraps JSObjects as NPObjects. There
// will be one wrapper per JSObject per plugin instance, i.e. if two
// plugins access the JSObject x, two wrappers for x will be
// created. This is needed to be able to properly drop the wrappers
// when a plugin is torn down in case there's a leak in the plugin (we
// don't want to leak the world just because a plugin leaks an
// NPObject).
typedef js::GCHashMap<nsJSObjWrapperKey,
                      nsJSObjWrapper*,
                      JSObjWrapperHasher,
                      js::SystemAllocPolicy> JSObjWrapperTable;
static JSObjWrapperTable sJSObjWrappers;

// Whether it's safe to iterate sJSObjWrappers.  Set to true when sJSObjWrappers
// has been initialized and is not currently being enumerated.
static bool sJSObjWrappersAccessible = false;

// Hash of NPObject wrappers that wrap NPObjects as JSObjects.
static PLDHashTable* sNPObjWrappers;

// Global wrapper count. This includes JSObject wrappers *and*
// NPObject wrappers. When this count goes to zero, there are no more
// wrappers and we can kill off hash tables etc.
static int32_t sWrapperCount;

static bool sCallbackIsRegistered = false;

static nsTArray<NPObject*>* sDelayedReleases;

namespace {

inline void
CastNPObject(NPObject *aObj, PluginScriptableObjectParent*& aActor,
             PluginAsyncSurrogate*& aSurrogate)
{
  aActor = nullptr;
  aSurrogate = nullptr;
  if (aObj->_class == PluginScriptableObjectParent::GetClass()) {
    aActor = static_cast<ParentNPObject*>(aObj)->parent;
  } else if (aObj->_class == PluginAsyncSurrogate::GetClass()) {
    aSurrogate = static_cast<AsyncNPObject*>(aObj)->mSurrogate;
  }
}

inline bool
NPObjectIsOutOfProcessProxy(NPObject *obj)
{
  return obj->_class == PluginScriptableObjectParent::GetClass() ||
         obj->_class == PluginAsyncSurrogate::GetClass();
}

} // namespace

// Helper class that reports any JS exceptions that were thrown while
// the plugin executed JS.

class MOZ_STACK_CLASS AutoJSExceptionReporter
{
public:
  AutoJSExceptionReporter(dom::AutoJSAPI& jsapi, nsJSObjWrapper* aWrapper)
    : mJsapi(jsapi)
    , mIsDestroyPending(aWrapper->mDestroyPending)
  {
    jsapi.TakeOwnershipOfErrorReporting();
  }

  ~AutoJSExceptionReporter()
  {
    if (mIsDestroyPending) {
      mJsapi.ClearException();
    }
  }

protected:
  dom::AutoJSAPI& mJsapi;
  bool mIsDestroyPending;
};


NPClass nsJSObjWrapper::sJSObjWrapperNPClass =
  {
    NP_CLASS_STRUCT_VERSION,
    nsJSObjWrapper::NP_Allocate,
    nsJSObjWrapper::NP_Deallocate,
    nsJSObjWrapper::NP_Invalidate,
    nsJSObjWrapper::NP_HasMethod,
    nsJSObjWrapper::NP_Invoke,
    nsJSObjWrapper::NP_InvokeDefault,
    nsJSObjWrapper::NP_HasProperty,
    nsJSObjWrapper::NP_GetProperty,
    nsJSObjWrapper::NP_SetProperty,
    nsJSObjWrapper::NP_RemoveProperty,
    nsJSObjWrapper::NP_Enumerate,
    nsJSObjWrapper::NP_Construct
  };

static bool
NPObjWrapper_AddProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, JS::Handle<JS::Value> v);

static bool
NPObjWrapper_DelProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                         JS::ObjectOpResult &result);

static bool
NPObjWrapper_SetProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                         JS::MutableHandle<JS::Value> vp, JS::ObjectOpResult &result);

static bool
NPObjWrapper_GetProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp);

static bool
NPObjWrapper_Enumerate(JSContext *cx, JS::Handle<JSObject*> obj, JS::AutoIdVector &properties,
                       bool enumerableOnly);

static bool
NPObjWrapper_Resolve(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                     bool *resolvedp);

static void
NPObjWrapper_Finalize(js::FreeOp *fop, JSObject *obj);

static void
NPObjWrapper_ObjectMoved(JSObject *obj, const JSObject *old);

static bool
NPObjWrapper_Call(JSContext *cx, unsigned argc, JS::Value *vp);

static bool
NPObjWrapper_Construct(JSContext *cx, unsigned argc, JS::Value *vp);

static bool
NPObjWrapper_toPrimitive(JSContext *cx, unsigned argc, JS::Value *vp);

static bool
CreateNPObjectMember(NPP npp, JSContext *cx, JSObject *obj, NPObject* npobj,
                     JS::Handle<jsid> id,  NPVariant* getPropertyResult,
                     JS::MutableHandle<JS::Value> vp);

const static js::Class sNPObjectJSWrapperClass =
  {
    NPRUNTIME_JSCLASS_NAME,
    JSCLASS_HAS_PRIVATE,
    NPObjWrapper_AddProperty,
    NPObjWrapper_DelProperty,
    NPObjWrapper_GetProperty,
    NPObjWrapper_SetProperty,
    nullptr,
    NPObjWrapper_Resolve,
    nullptr,                                                /* mayResolve */
    NPObjWrapper_Finalize,
    NPObjWrapper_Call,
    nullptr,                                                /* hasInstance */
    NPObjWrapper_Construct,
    nullptr,                                                /* trace */
    JS_NULL_CLASS_SPEC,
    {
      false,                                                /* isWrappedNative */
      nullptr,                                              /* weakmapKeyDelegateOp */
      NPObjWrapper_ObjectMoved
    },
    {
        nullptr, // lookupProperty
        nullptr, // defineProperty
        nullptr, // hasProperty
        nullptr, // getProperty
        nullptr, // setProperty
        nullptr, // getOwnPropertyDescriptor
        nullptr, // deleteProperty
        nullptr, nullptr, // watch/unwatch
        nullptr, // getElements
        NPObjWrapper_Enumerate,
        nullptr,
    }
  };

typedef struct NPObjectMemberPrivate {
    JS::Heap<JSObject *> npobjWrapper;
    JS::Heap<JS::Value> fieldValue;
    JS::Heap<jsid> methodName;
    NPP   npp;
} NPObjectMemberPrivate;

static bool
NPObjectMember_GetProperty(JSContext *cx, JS::HandleObject obj, JS::HandleId id,
                           JS::MutableHandleValue vp);

static void
NPObjectMember_Finalize(JSFreeOp *fop, JSObject *obj);

static bool
NPObjectMember_Call(JSContext *cx, unsigned argc, JS::Value *vp);

static void
NPObjectMember_Trace(JSTracer *trc, JSObject *obj);

static bool
NPObjectMember_toPrimitive(JSContext *cx, unsigned argc, JS::Value *vp);

static const JSClass sNPObjectMemberClass =
  {
    "NPObject Ambiguous Member class", JSCLASS_HAS_PRIVATE,
    nullptr, nullptr, NPObjectMember_GetProperty, nullptr,
    nullptr, nullptr, nullptr,
    NPObjectMember_Finalize, NPObjectMember_Call,
    nullptr, nullptr, NPObjectMember_Trace
  };

static void
OnWrapperDestroyed();

static void
TraceJSObjWrappers(JSTracer *trc, void *data)
{
  if (sJSObjWrappers.initialized()) {
    sJSObjWrappers.trace(trc);
  }
}

static void
DelayedReleaseGCCallback(JSGCStatus status)
{
  if (JSGC_END == status) {
    // Take ownership of sDelayedReleases and null it out now. The
    // _releaseobject call below can reenter GC and double-free these objects.
    nsAutoPtr<nsTArray<NPObject*> > delayedReleases(sDelayedReleases);
    sDelayedReleases = nullptr;

    if (delayedReleases) {
      for (uint32_t i = 0; i < delayedReleases->Length(); ++i) {
        NPObject* obj = (*delayedReleases)[i];
        if (obj)
          _releaseobject(obj);
        OnWrapperDestroyed();
      }
    }
  }
}

static bool
RegisterGCCallbacks()
{
  if (sCallbackIsRegistered) {
    return true;
  }

  JSRuntime *jsRuntime = xpc::GetJSRuntime();
  MOZ_ASSERT(jsRuntime != nullptr);

  // Register a callback to trace wrapped JSObjects.
  if (!JS_AddExtraGCRootsTracer(jsRuntime, TraceJSObjWrappers, nullptr)) {
    return false;
  }

  // Register our GC callback to perform delayed destruction of finalized
  // NPObjects.
  xpc::AddGCCallback(DelayedReleaseGCCallback);

  sCallbackIsRegistered = true;

  return true;
}

static void
UnregisterGCCallbacks()
{
  MOZ_ASSERT(sCallbackIsRegistered);

  JSRuntime *jsRuntime = xpc::GetJSRuntime();
  MOZ_ASSERT(jsRuntime != nullptr);

  // Remove tracing callback.
  JS_RemoveExtraGCRootsTracer(jsRuntime, TraceJSObjWrappers, nullptr);

  // Remove delayed destruction callback.
  if (sCallbackIsRegistered) {
    xpc::RemoveGCCallback(DelayedReleaseGCCallback);
    sCallbackIsRegistered = false;
  }
}

static bool
CreateJSObjWrapperTable()
{
  MOZ_ASSERT(!sJSObjWrappersAccessible);
  MOZ_ASSERT(!sJSObjWrappers.initialized());

  if (!RegisterGCCallbacks()) {
    return false;
  }

  if (!sJSObjWrappers.init(16)) {
    NS_ERROR("Error initializing PLDHashTable sJSObjWrappers!");
    return false;
  }

  sJSObjWrappersAccessible = true;
  return true;
}

static void
DestroyJSObjWrapperTable()
{
  MOZ_ASSERT(sJSObjWrappersAccessible);
  MOZ_ASSERT(sJSObjWrappers.initialized());
  MOZ_ASSERT(sJSObjWrappers.count() == 0);

  // No more wrappers, and our hash was initialized. Finish the
  // hash to prevent leaking it.
  sJSObjWrappers.finish();
  sJSObjWrappersAccessible = false;
}

static bool
CreateNPObjWrapperTable()
{
  MOZ_ASSERT(!sNPObjWrappers);

  if (!RegisterGCCallbacks()) {
    return false;
  }

  sNPObjWrappers =
    new PLDHashTable(PLDHashTable::StubOps(), sizeof(NPObjWrapperHashEntry));
  return true;
}

static void
DestroyNPObjWrapperTable()
{
  MOZ_ASSERT(sNPObjWrappers->EntryCount() == 0);

  delete sNPObjWrappers;
  sNPObjWrappers = nullptr;
}

static void
OnWrapperCreated()
{
  ++sWrapperCount;
}

static void
OnWrapperDestroyed()
{
  NS_ASSERTION(sWrapperCount, "Whaaa, unbalanced created/destroyed calls!");

  if (--sWrapperCount == 0) {
    if (sJSObjWrappersAccessible) {
      DestroyJSObjWrapperTable();
    }

    if (sNPObjWrappers) {
      // No more wrappers, and our hash was initialized. Finish the
      // hash to prevent leaking it.
      DestroyNPObjWrapperTable();
    }

    UnregisterGCCallbacks();
  }
}

namespace mozilla {
namespace plugins {
namespace parent {

static nsIGlobalObject*
GetGlobalObject(NPP npp)
{
  NS_ENSURE_TRUE(npp, nullptr);

  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *)npp->ndata;
  NS_ENSURE_TRUE(inst, nullptr);

  RefPtr<nsPluginInstanceOwner> owner = inst->GetOwner();
  NS_ENSURE_TRUE(owner, nullptr);

  nsCOMPtr<nsIDocument> doc;
  owner->GetDocument(getter_AddRefs(doc));
  NS_ENSURE_TRUE(doc, nullptr);

  return doc->GetScopeObject();
}

JSContext *
GetJSContext(NPP npp)
{
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(GetGlobalObject(npp));
  NS_ENSURE_TRUE(sgo, nullptr);

  nsIScriptContext *scx = sgo->GetContext();
  NS_ENSURE_TRUE(scx, nullptr);

  return scx->GetNativeContext();
}

} // namespace parent
} // namespace plugins
} // namespace mozilla

static NPP
LookupNPP(NPObject *npobj);


static JS::Value
NPVariantToJSVal(NPP npp, JSContext *cx, const NPVariant *variant)
{
  switch (variant->type) {
  case NPVariantType_Void :
    return JS::UndefinedValue();
  case NPVariantType_Null :
    return JS::NullValue();
  case NPVariantType_Bool :
    return JS::BooleanValue(NPVARIANT_TO_BOOLEAN(*variant));
  case NPVariantType_Int32 :
    {
      // Don't use INT_TO_JSVAL directly to prevent bugs when dealing
      // with ints larger than what fits in a integer JS::Value.
      return ::JS_NumberValue(NPVARIANT_TO_INT32(*variant));
    }
  case NPVariantType_Double :
    {
      return ::JS_NumberValue(NPVARIANT_TO_DOUBLE(*variant));
    }
  case NPVariantType_String :
    {
      const NPString *s = &NPVARIANT_TO_STRING(*variant);
      NS_ConvertUTF8toUTF16 utf16String(s->UTF8Characters, s->UTF8Length);

      JSString *str =
        ::JS_NewUCStringCopyN(cx, utf16String.get(), utf16String.Length());

      if (str) {
        return JS::StringValue(str);
      }

      break;
    }
  case NPVariantType_Object:
    {
      if (npp) {
        JSObject *obj =
          nsNPObjWrapper::GetNewOrUsed(npp, cx, NPVARIANT_TO_OBJECT(*variant));

        if (obj) {
          return JS::ObjectValue(*obj);
        }
      }

      NS_ERROR("Error wrapping NPObject!");

      break;
    }
  default:
    NS_ERROR("Unknown NPVariant type!");
  }

  NS_ERROR("Unable to convert NPVariant to jsval!");

  return JS::UndefinedValue();
}

bool
JSValToNPVariant(NPP npp, JSContext *cx, JS::Value val, NPVariant *variant)
{
  NS_ASSERTION(npp, "Must have an NPP to wrap a jsval!");

  if (val.isPrimitive()) {
    if (val.isUndefined()) {
      VOID_TO_NPVARIANT(*variant);
    } else if (val.isNull()) {
      NULL_TO_NPVARIANT(*variant);
    } else if (val.isBoolean()) {
      BOOLEAN_TO_NPVARIANT(val.toBoolean(), *variant);
    } else if (val.isInt32()) {
      INT32_TO_NPVARIANT(val.toInt32(), *variant);
    } else if (val.isDouble()) {
      double d = val.toDouble();
      int i;
      if (JS_DoubleIsInt32(d, &i)) {
        INT32_TO_NPVARIANT(i, *variant);
      } else {
        DOUBLE_TO_NPVARIANT(d, *variant);
      }
    } else if (val.isString()) {
      JSString *jsstr = val.toString();

      nsAutoJSString str;
      if (!str.init(cx, jsstr)) {
        return false;
      }

      uint32_t len;
      char *p = ToNewUTF8String(str, &len);

      if (!p) {
        return false;
      }

      STRINGN_TO_NPVARIANT(p, len, *variant);
    } else {
      NS_ERROR("Unknown primitive type!");

      return false;
    }

    return true;
  }

  // The reflected plugin object may be in another compartment if the plugin
  // element has since been adopted into a new document. We don't bother
  // transplanting the plugin objects, and just do a unwrap with security
  // checks if we encounter one of them as an argument. If the unwrap fails,
  // we run with the original wrapped object, since sometimes there are
  // legitimate cases where a security wrapper ends up here (for example,
  // Location objects, which are _always_ behind security wrappers).
  JS::Rooted<JSObject*> obj(cx, val.toObjectOrNull());
  obj = js::CheckedUnwrap(obj);
  if (!obj) {
    obj = val.toObjectOrNull();
  }

  NPObject *npobj = nsJSObjWrapper::GetNewOrUsed(npp, cx, obj);
  if (!npobj) {
    return false;
  }

  // Pass over ownership of npobj to *variant
  OBJECT_TO_NPVARIANT(npobj, *variant);

  return true;
}

static void
ThrowJSException(JSContext *cx, const char *message)
{
  const char *ex = PeekException();

  if (ex) {
    nsAutoString ucex;

    if (message) {
      AppendASCIItoUTF16(message, ucex);

      AppendASCIItoUTF16(" [plugin exception: ", ucex);
    }

    AppendUTF8toUTF16(ex, ucex);

    if (message) {
      AppendASCIItoUTF16("].", ucex);
    }

    JSString *str = ::JS_NewUCStringCopyN(cx, ucex.get(), ucex.Length());

    if (str) {
      JS::Rooted<JS::Value> exn(cx, JS::StringValue(str));
      ::JS_SetPendingException(cx, exn);
    }

    PopException();
  } else {
    ::JS_ReportError(cx, message);
  }
}

static bool
ReportExceptionIfPending(JSContext *cx)
{
  const char *ex = PeekException();

  if (!ex) {
    return true;
  }

  ThrowJSException(cx, nullptr);

  return false;
}

nsJSObjWrapper::nsJSObjWrapper(NPP npp)
  : mJSObj(nullptr), mNpp(npp), mDestroyPending(false)
{
  MOZ_COUNT_CTOR(nsJSObjWrapper);
  OnWrapperCreated();
}

nsJSObjWrapper::~nsJSObjWrapper()
{
  MOZ_COUNT_DTOR(nsJSObjWrapper);

  // Invalidate first, since it relies on sJSObjWrappers.
  NP_Invalidate(this);

  OnWrapperDestroyed();
}

// static
NPObject *
nsJSObjWrapper::NP_Allocate(NPP npp, NPClass *aClass)
{
  NS_ASSERTION(aClass == &sJSObjWrapperNPClass,
               "Huh, wrong class passed to NP_Allocate()!!!");

  return new nsJSObjWrapper(npp);
}

// static
void
nsJSObjWrapper::NP_Deallocate(NPObject *npobj)
{
  // nsJSObjWrapper::~nsJSObjWrapper() will call NP_Invalidate().
  delete (nsJSObjWrapper *)npobj;
}

// static
void
nsJSObjWrapper::NP_Invalidate(NPObject *npobj)
{
  nsJSObjWrapper *jsnpobj = (nsJSObjWrapper *)npobj;

  if (jsnpobj && jsnpobj->mJSObj) {

    if (sJSObjWrappersAccessible) {
      // Remove the wrapper from the hash
      nsJSObjWrapperKey key(jsnpobj->mJSObj, jsnpobj->mNpp);
      JSObjWrapperTable::Ptr ptr = sJSObjWrappers.lookup(key);
      MOZ_ASSERT(ptr.found());
      sJSObjWrappers.remove(ptr);
    }

    // Forget our reference to the JSObject.
    jsnpobj->mJSObj = nullptr;
  }
}

static bool
GetProperty(JSContext *cx, JSObject *objArg, NPIdentifier npid, JS::MutableHandle<JS::Value> rval)
{
  NS_ASSERTION(NPIdentifierIsInt(npid) || NPIdentifierIsString(npid),
               "id must be either string or int!\n");
  JS::Rooted<JSObject *> obj(cx, objArg);
  JS::Rooted<jsid> id(cx, NPIdentifierToJSId(npid));
  return ::JS_GetPropertyById(cx, obj, id, rval);
}

// static
bool
nsJSObjWrapper::NP_HasMethod(NPObject *npobj, NPIdentifier id)
{
  NPP npp = NPPStack::Peek();
  dom::AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.InitWithLegacyErrorReporting(GetGlobalObject(npp)))) {
    return false;
  }
  JSContext *cx = jsapi.cx();

  if (!npobj) {
    ThrowJSException(cx,
                     "Null npobj in nsJSObjWrapper::NP_HasMethod!");

    return false;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;

  JSAutoCompartment ac(cx, npjsobj->mJSObj);

  AutoJSExceptionReporter reporter(jsapi, npjsobj);

  JS::Rooted<JS::Value> v(cx);
  bool ok = GetProperty(cx, npjsobj->mJSObj, id, &v);

  return ok && !v.isPrimitive() &&
    ::JS_ObjectIsFunction(cx, v.toObjectOrNull());
}

static bool
doInvoke(NPObject *npobj, NPIdentifier method, const NPVariant *args,
         uint32_t argCount, bool ctorCall, NPVariant *result)
{
  NPP npp = NPPStack::Peek();

  nsCOMPtr<nsIGlobalObject> globalObject = GetGlobalObject(npp);
  if (NS_WARN_IF(!globalObject)) {
    return false;
  }

  // We're about to run script via JS_CallFunctionValue, so we need an
  // AutoEntryScript. NPAPI plugins are Gecko-specific and not in any spec.
  dom::AutoEntryScript aes(globalObject, "NPAPI doInvoke");
  JSContext *cx = aes.cx();

  if (!npobj || !result) {
    ThrowJSException(cx, "Null npobj, or result in doInvoke!");

    return false;
  }

  // Initialize *result
  VOID_TO_NPVARIANT(*result);

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;

  JS::Rooted<JSObject*> jsobj(cx, npjsobj->mJSObj);
  JSAutoCompartment ac(cx, jsobj);
  JS::Rooted<JS::Value> fv(cx);

  AutoJSExceptionReporter reporter(aes, npjsobj);

  if (method != NPIdentifier_VOID) {
    if (!GetProperty(cx, jsobj, method, &fv) ||
        ::JS_TypeOfValue(cx, fv) != JSTYPE_FUNCTION) {
      return false;
    }
  } else {
    fv.setObject(*jsobj);
  }

  // Convert args
  JS::AutoValueVector jsargs(cx);
  if (!jsargs.reserve(argCount)) {
      ::JS_ReportOutOfMemory(cx);
      return false;
  }
  for (uint32_t i = 0; i < argCount; ++i) {
    jsargs.infallibleAppend(NPVariantToJSVal(npp, cx, args + i));
  }

  JS::Rooted<JS::Value> v(cx);
  bool ok = false;

  if (ctorCall) {
    JSObject *newObj =
      ::JS_New(cx, jsobj, jsargs);

    if (newObj) {
      v.setObject(*newObj);
      ok = true;
    }
  } else {
    ok = ::JS_CallFunctionValue(cx, jsobj, fv, jsargs, &v);
  }

  if (ok)
    ok = JSValToNPVariant(npp, cx, v, result);

  return ok;
}

// static
bool
nsJSObjWrapper::NP_Invoke(NPObject *npobj, NPIdentifier method,
                          const NPVariant *args, uint32_t argCount,
                          NPVariant *result)
{
  if (method == NPIdentifier_VOID) {
    return false;
  }

  return doInvoke(npobj, method, args, argCount, false, result);
}

// static
bool
nsJSObjWrapper::NP_InvokeDefault(NPObject *npobj, const NPVariant *args,
                                 uint32_t argCount, NPVariant *result)
{
  return doInvoke(npobj, NPIdentifier_VOID, args, argCount, false,
                  result);
}

// static
bool
nsJSObjWrapper::NP_HasProperty(NPObject *npobj, NPIdentifier npid)
{
  NPP npp = NPPStack::Peek();
  dom::AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.InitWithLegacyErrorReporting(GetGlobalObject(npp)))) {
    return false;
  }
  JSContext *cx = jsapi.cx();

  if (!npobj) {
    ThrowJSException(cx,
                     "Null npobj in nsJSObjWrapper::NP_HasProperty!");

    return false;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;
  bool found, ok = false;

  AutoJSExceptionReporter reporter(jsapi, npjsobj);
  JS::Rooted<JSObject*> jsobj(cx, npjsobj->mJSObj);
  JSAutoCompartment ac(cx, jsobj);

  NS_ASSERTION(NPIdentifierIsInt(npid) || NPIdentifierIsString(npid),
               "id must be either string or int!\n");
  JS::Rooted<jsid> id(cx, NPIdentifierToJSId(npid));
  ok = ::JS_HasPropertyById(cx, jsobj, id, &found);
  return ok && found;
}

// static
bool
nsJSObjWrapper::NP_GetProperty(NPObject *npobj, NPIdentifier id,
                               NPVariant *result)
{
  NPP npp = NPPStack::Peek();

  nsCOMPtr<nsIGlobalObject> globalObject = GetGlobalObject(npp);
  if (NS_WARN_IF(!globalObject)) {
    return false;
  }

  // We're about to run script via JS_CallFunctionValue, so we need an
  // AutoEntryScript. NPAPI plugins are Gecko-specific and not in any spec.
  dom::AutoEntryScript aes(globalObject, "NPAPI get");
  JSContext *cx = aes.cx();

  if (!npobj) {
    ThrowJSException(cx,
                     "Null npobj in nsJSObjWrapper::NP_GetProperty!");

    return false;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;

  AutoJSExceptionReporter reporter(aes, npjsobj);
  JSAutoCompartment ac(cx, npjsobj->mJSObj);

  JS::Rooted<JS::Value> v(cx);
  return (GetProperty(cx, npjsobj->mJSObj, id, &v) &&
          JSValToNPVariant(npp, cx, v, result));
}

// static
bool
nsJSObjWrapper::NP_SetProperty(NPObject *npobj, NPIdentifier npid,
                               const NPVariant *value)
{
  NPP npp = NPPStack::Peek();

  nsCOMPtr<nsIGlobalObject> globalObject = GetGlobalObject(npp);
  if (NS_WARN_IF(!globalObject)) {
    return false;
  }

  // We're about to run script via JS_CallFunctionValue, so we need an
  // AutoEntryScript. NPAPI plugins are Gecko-specific and not in any spec.
  dom::AutoEntryScript aes(globalObject, "NPAPI set");
  JSContext *cx = aes.cx();

  if (!npobj) {
    ThrowJSException(cx,
                     "Null npobj in nsJSObjWrapper::NP_SetProperty!");

    return false;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;
  bool ok = false;

  AutoJSExceptionReporter reporter(aes, npjsobj);
  JS::Rooted<JSObject*> jsObj(cx, npjsobj->mJSObj);
  JSAutoCompartment ac(cx, jsObj);

  JS::Rooted<JS::Value> v(cx, NPVariantToJSVal(npp, cx, value));

  NS_ASSERTION(NPIdentifierIsInt(npid) || NPIdentifierIsString(npid),
               "id must be either string or int!\n");
  JS::Rooted<jsid> id(cx, NPIdentifierToJSId(npid));
  ok = ::JS_SetPropertyById(cx, jsObj, id, v);

  return ok;
}

// static
bool
nsJSObjWrapper::NP_RemoveProperty(NPObject *npobj, NPIdentifier npid)
{
  NPP npp = NPPStack::Peek();
  dom::AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.InitWithLegacyErrorReporting(GetGlobalObject(npp)))) {
    return false;
  }
  JSContext *cx = jsapi.cx();

  if (!npobj) {
    ThrowJSException(cx,
                     "Null npobj in nsJSObjWrapper::NP_RemoveProperty!");

    return false;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;

  AutoJSExceptionReporter reporter(jsapi, npjsobj);
  JS::ObjectOpResult result;
  JS::Rooted<JSObject*> obj(cx, npjsobj->mJSObj);
  JSAutoCompartment ac(cx, obj);

  NS_ASSERTION(NPIdentifierIsInt(npid) || NPIdentifierIsString(npid),
               "id must be either string or int!\n");
  JS::Rooted<jsid> id(cx, NPIdentifierToJSId(npid));
  if (!::JS_DeletePropertyById(cx, obj, id, result))
    return false;

  if (result) {
    // FIXME: See bug 425823, we shouldn't need to do this, and once
    // that bug is fixed we can remove this code.
    bool hasProp;
    if (!::JS_HasPropertyById(cx, obj, id, &hasProp))
      return false;
    if (!hasProp)
      return true;

    // The property might have been deleted, but it got
    // re-resolved, so no, it's not really deleted.
    result.failCantDelete();
  }

  return result.reportError(cx, obj, id);
}

//static
bool
nsJSObjWrapper::NP_Enumerate(NPObject *npobj, NPIdentifier **idarray,
                             uint32_t *count)
{
  NPP npp = NPPStack::Peek();
  dom::AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.InitWithLegacyErrorReporting(GetGlobalObject(npp)))) {
    return false;
  }
  JSContext *cx = jsapi.cx();

  *idarray = 0;
  *count = 0;

  if (!npobj) {
    ThrowJSException(cx,
                     "Null npobj in nsJSObjWrapper::NP_Enumerate!");

    return false;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;

  AutoJSExceptionReporter reporter(jsapi, npjsobj);
  JS::Rooted<JSObject*> jsobj(cx, npjsobj->mJSObj);
  JSAutoCompartment ac(cx, jsobj);

  JS::Rooted<JS::IdVector> ida(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, jsobj, &ida)) {
    return false;
  }

  *count = ida.length();
  *idarray = (NPIdentifier *)PR_Malloc(*count * sizeof(NPIdentifier));
  if (!*idarray) {
    ThrowJSException(cx, "Memory allocation failed for NPIdentifier!");
    return false;
  }

  for (uint32_t i = 0; i < *count; i++) {
    JS::Rooted<JS::Value> v(cx);
    if (!JS_IdToValue(cx, ida[i], &v)) {
      PR_Free(*idarray);
      return false;
    }

    NPIdentifier id;
    if (v.isString()) {
      JS::Rooted<JSString*> str(cx, v.toString());
      str = JS_AtomizeAndPinJSString(cx, str);
      if (!str) {
        PR_Free(*idarray);
        return false;
      }
      id = StringToNPIdentifier(cx, str);
    } else {
      NS_ASSERTION(v.isInt32(),
                   "The element in ida must be either string or int!\n");
      id = IntToNPIdentifier(v.toInt32());
    }

    (*idarray)[i] = id;
  }

  return true;
}

//static
bool
nsJSObjWrapper::NP_Construct(NPObject *npobj, const NPVariant *args,
                             uint32_t argCount, NPVariant *result)
{
  return doInvoke(npobj, NPIdentifier_VOID, args, argCount, true, result);
}

// Look up or create an NPObject that wraps the JSObject obj.

// static
NPObject *
nsJSObjWrapper::GetNewOrUsed(NPP npp, JSContext *cx, JS::Handle<JSObject*> obj)
{
  if (!npp) {
    NS_ERROR("Null NPP passed to nsJSObjWrapper::GetNewOrUsed()!");

    return nullptr;
  }

  // If we're running out-of-process and initializing asynchronously, and if
  // the plugin has been asked to destroy itself during initialization,
  // don't return any new NPObjects.
  nsNPAPIPluginInstance* inst = static_cast<nsNPAPIPluginInstance*>(npp->ndata);
  if (inst->GetPlugin()->GetLibrary()->IsOOP()) {
    PluginAsyncSurrogate* surrogate = PluginAsyncSurrogate::Cast(npp);
    if (surrogate && surrogate->IsDestroyPending()) {
      return nullptr;
    }
  }

  if (!cx) {
    cx = GetJSContext(npp);

    if (!cx) {
      NS_ERROR("Unable to find a JSContext in nsJSObjWrapper::GetNewOrUsed()!");

      return nullptr;
    }
  }

  // No need to enter the right compartment here as we only get the
  // class and private from the JSObject, neither of which cares about
  // compartments.

  if (nsNPObjWrapper::IsWrapper(obj)) {
    // obj is one of our own, its private data is the NPObject we're
    // looking for.

    NPObject *npobj = (NPObject *)::JS_GetPrivate(obj);

    // If the private is null, that means that the object has already been torn
    // down, possible because the owning plugin was destroyed (there can be
    // multiple plugins, so the fact that it was destroyed does not prevent one
    // of its dead JS objects from being passed to another plugin). There's not
    // much use in wrapping such a dead object, so we just return null, causing
    // us to throw.
    if (!npobj)
      return nullptr;

    if (LookupNPP(npobj) == npp)
      return _retainobject(npobj);
  }

  if (!sJSObjWrappers.initialized()) {
    // No hash yet (or any more), initialize it.
    if (!CreateJSObjWrapperTable())
      return nullptr;
  }
  MOZ_ASSERT(sJSObjWrappersAccessible);

  JSObjWrapperTable::Ptr p = sJSObjWrappers.lookupForAdd(nsJSObjWrapperKey(obj, npp));
  if (p) {
    MOZ_ASSERT(p->value());
    // Found a live nsJSObjWrapper, return it.

    return _retainobject(p->value());
  }

  // No existing nsJSObjWrapper, create one.

  nsJSObjWrapper *wrapper =
    (nsJSObjWrapper *)_createobject(npp, &sJSObjWrapperNPClass);

  if (!wrapper) {
    // Out of memory, entry not yet added to table.
    return nullptr;
  }

  wrapper->mJSObj = obj;

  // Insert the new wrapper into the hashtable, rooting the JSObject. Its
  // lifetime is now tied to that of the NPObject.
  if (!sJSObjWrappers.putNew(nsJSObjWrapperKey(obj, npp), wrapper)) {
    // Out of memory, free the wrapper we created.
    _releaseobject(wrapper);
    return nullptr;
  }

  return wrapper;
}

// Climb the prototype chain, unwrapping as necessary until we find an NP object
// wrapper.
//
// Because this function unwraps, its return value must be wrapped for the cx
// compartment for callers that plan to hold onto the result or do anything
// substantial with it.
static JSObject *
GetNPObjectWrapper(JSContext *cx, JSObject *aObj, bool wrapResult = true)
{
  JS::Rooted<JSObject*> obj(cx, aObj);
  while (obj && (obj = js::CheckedUnwrap(obj))) {
    if (nsNPObjWrapper::IsWrapper(obj)) {
      if (wrapResult && !JS_WrapObject(cx, &obj)) {
        return nullptr;
      }
      return obj;
    }
    if (!::JS_GetPrototype(cx, obj, &obj)) {
      return nullptr;
    }
  }
  return nullptr;
}

static NPObject *
GetNPObject(JSContext *cx, JSObject *obj)
{
  obj = GetNPObjectWrapper(cx, obj, /* wrapResult = */ false);
  if (!obj) {
    return nullptr;
  }

  return (NPObject *)::JS_GetPrivate(obj);
}


// Does not actually add a property because this is always followed by a
// SetProperty call.
static bool
NPObjWrapper_AddProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, JS::Handle<JS::Value> v)
{
  NPObject *npobj = GetNPObject(cx, obj);

  if (!npobj || !npobj->_class || !npobj->_class->hasProperty ||
      !npobj->_class->hasMethod) {
    ThrowJSException(cx, "Bad NPObject as private data!");

    return false;
  }

  if (NPObjectIsOutOfProcessProxy(npobj)) {
    return true;
  }

  PluginDestructionGuard pdg(LookupNPP(npobj));

  NPIdentifier identifier = JSIdToNPIdentifier(id);
  bool hasProperty = npobj->_class->hasProperty(npobj, identifier);
  if (!ReportExceptionIfPending(cx))
    return false;

  if (hasProperty)
    return true;

  // We must permit methods here since JS_DefineUCFunction() will add
  // the function as a property
  bool hasMethod = npobj->_class->hasMethod(npobj, identifier);
  if (!ReportExceptionIfPending(cx))
    return false;

  if (!hasMethod) {
    ThrowJSException(cx, "Trying to add unsupported property on NPObject!");

    return false;
  }

  return true;
}

static bool
NPObjWrapper_DelProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                         JS::ObjectOpResult &result)
{
  NPObject *npobj = GetNPObject(cx, obj);

  if (!npobj || !npobj->_class || !npobj->_class->hasProperty ||
      !npobj->_class->removeProperty) {
    ThrowJSException(cx, "Bad NPObject as private data!");

    return false;
  }

  PluginDestructionGuard pdg(LookupNPP(npobj));

  NPIdentifier identifier = JSIdToNPIdentifier(id);

  if (!NPObjectIsOutOfProcessProxy(npobj)) {
    bool hasProperty = npobj->_class->hasProperty(npobj, identifier);
    if (!ReportExceptionIfPending(cx))
      return false;

    if (!hasProperty)
      return result.succeed();
  }

  // This removeProperty hook may throw an exception and return false; or just
  // return false without an exception pending, which behaves like `delete
  // obj.prop` returning false: in strict mode it becomes a TypeError. Legacy
  // code---nothing else that uses the JSAPI works this way anymore.
  bool succeeded = npobj->_class->removeProperty(npobj, identifier);
  if (!ReportExceptionIfPending(cx))
    return false;
  return succeeded ? result.succeed() : result.failCantDelete();
}

static bool
NPObjWrapper_SetProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                         JS::MutableHandle<JS::Value> vp, JS::ObjectOpResult &result)
{
  NPObject *npobj = GetNPObject(cx, obj);

  if (!npobj || !npobj->_class || !npobj->_class->hasProperty ||
      !npobj->_class->setProperty) {
    ThrowJSException(cx, "Bad NPObject as private data!");

    return false;
  }

  // Find out what plugin (NPP) is the owner of the object we're
  // manipulating, and make it own any JSObject wrappers created here.
  NPP npp = LookupNPP(npobj);

  if (!npp) {
    ThrowJSException(cx, "No NPP found for NPObject!");

    return false;
  }

  PluginDestructionGuard pdg(npp);

  NPIdentifier identifier = JSIdToNPIdentifier(id);

  if (!NPObjectIsOutOfProcessProxy(npobj)) {
    bool hasProperty = npobj->_class->hasProperty(npobj, identifier);
    if (!ReportExceptionIfPending(cx))
      return false;

    if (!hasProperty) {
      ThrowJSException(cx, "Trying to set unsupported property on NPObject!");

      return false;
    }
  }

  NPVariant npv;
  if (!JSValToNPVariant(npp, cx, vp, &npv)) {
    ThrowJSException(cx, "Error converting jsval to NPVariant!");

    return false;
  }

  bool ok = npobj->_class->setProperty(npobj, identifier, &npv);
  _releasevariantvalue(&npv); // Release the variant
  if (!ReportExceptionIfPending(cx))
    return false;

  if (!ok) {
    ThrowJSException(cx, "Error setting property on NPObject!");

    return false;
  }

  return result.succeed();
}

static bool
NPObjWrapper_GetProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp)
{
  NPObject *npobj = GetNPObject(cx, obj);

  if (!npobj || !npobj->_class || !npobj->_class->hasProperty ||
      !npobj->_class->hasMethod || !npobj->_class->getProperty) {
    ThrowJSException(cx, "Bad NPObject as private data!");

    return false;
  }

  if (JSID_IS_SYMBOL(id)) {
    JS::RootedSymbol sym(cx, JSID_TO_SYMBOL(id));
    if (JS::GetSymbolCode(sym) == JS::SymbolCode::toPrimitive) {
      JS::RootedObject obj(cx, JS_GetFunctionObject(
                                 JS_NewFunction(
                                   cx, NPObjWrapper_toPrimitive, 1, 0,
                                   "Symbol.toPrimitive")));
      if (!obj)
        return false;
      vp.setObject(*obj);
      return true;
    }
  }

  // Find out what plugin (NPP) is the owner of the object we're
  // manipulating, and make it own any JSObject wrappers created here.
  NPP npp = LookupNPP(npobj);
  if (!npp) {
    ThrowJSException(cx, "No NPP found for NPObject!");

    return false;
  }

  PluginDestructionGuard pdg(npp);

  bool hasProperty, hasMethod;

  NPVariant npv;
  VOID_TO_NPVARIANT(npv);

  NPIdentifier identifier = JSIdToNPIdentifier(id);

  if (NPObjectIsOutOfProcessProxy(npobj)) {
    PluginScriptableObjectParent* actor = nullptr;
    PluginAsyncSurrogate* surrogate = nullptr;
    CastNPObject(npobj, actor, surrogate);

    // actor and surrogate may be null if the plugin crashed.
    if (!actor && !surrogate)
      return false;

    bool success = false;
    if (surrogate) {
      success = surrogate->GetPropertyHelper(npobj, identifier, &hasProperty,
                                             &hasMethod, &npv);
    } else if (actor) {
      success = actor->GetPropertyHelper(identifier, &hasProperty, &hasMethod,
                                         &npv);
    }

    if (!ReportExceptionIfPending(cx)) {
      if (success)
        _releasevariantvalue(&npv);
      return false;
    }

    if (success) {
      // We return NPObject Member class here to support ambiguous members.
      if (hasProperty && hasMethod)
        return CreateNPObjectMember(npp, cx, obj, npobj, id, &npv, vp);

      if (hasProperty) {
        vp.set(NPVariantToJSVal(npp, cx, &npv));
        _releasevariantvalue(&npv);

        if (!ReportExceptionIfPending(cx))
          return false;
      }
    }
    return true;
  }

  hasProperty = npobj->_class->hasProperty(npobj, identifier);
  if (!ReportExceptionIfPending(cx))
    return false;

  hasMethod = npobj->_class->hasMethod(npobj, identifier);
  if (!ReportExceptionIfPending(cx))
    return false;

  // We return NPObject Member class here to support ambiguous members.
  if (hasProperty && hasMethod)
    return CreateNPObjectMember(npp, cx, obj, npobj, id, nullptr, vp);

  if (hasProperty) {
    if (npobj->_class->getProperty(npobj, identifier, &npv))
      vp.set(NPVariantToJSVal(npp, cx, &npv));

    _releasevariantvalue(&npv);

    if (!ReportExceptionIfPending(cx))
      return false;
  }

  return true;
}

static bool
CallNPMethodInternal(JSContext *cx, JS::Handle<JSObject*> obj, unsigned argc,
                     JS::Value *argv, JS::Value *rval, bool ctorCall)
{
  NPObject *npobj = GetNPObject(cx, obj);

  if (!npobj || !npobj->_class) {
    ThrowJSException(cx, "Bad NPObject as private data!");

    return false;
  }

  // Find out what plugin (NPP) is the owner of the object we're
  // manipulating, and make it own any JSObject wrappers created here.
  NPP npp = LookupNPP(npobj);

  if (!npp) {
    ThrowJSException(cx, "Error finding NPP for NPObject!");

    return false;
  }

  PluginDestructionGuard pdg(npp);

  NPVariant npargs_buf[8];
  NPVariant *npargs = npargs_buf;

  if (argc > (sizeof(npargs_buf) / sizeof(NPVariant))) {
    // Our stack buffer isn't large enough to hold all arguments,
    // malloc a buffer.
    npargs = (NPVariant *)PR_Malloc(argc * sizeof(NPVariant));

    if (!npargs) {
      ThrowJSException(cx, "Out of memory!");

      return false;
    }
  }

  // Convert arguments
  uint32_t i;
  for (i = 0; i < argc; ++i) {
    if (!JSValToNPVariant(npp, cx, argv[i], npargs + i)) {
      ThrowJSException(cx, "Error converting jsvals to NPVariants!");

      if (npargs != npargs_buf) {
        PR_Free(npargs);
      }

      return false;
    }
  }

  NPVariant v;
  VOID_TO_NPVARIANT(v);

  JSObject *funobj = argv[-2].toObjectOrNull();
  bool ok;
  const char *msg = "Error calling method on NPObject!";

  if (ctorCall) {
    // construct a new NPObject based on the NPClass in npobj. Fail if
    // no construct method is available.

    if (NP_CLASS_STRUCT_VERSION_HAS_CTOR(npobj->_class) &&
        npobj->_class->construct) {
      ok = npobj->_class->construct(npobj, npargs, argc, &v);
    } else {
      ok = false;

      msg = "Attempt to construct object from class with no constructor.";
    }
  } else if (funobj != obj) {
    // A obj.function() style call is made, get the method name from
    // the function object.

    if (npobj->_class->invoke) {
      JSFunction *fun = ::JS_GetObjectFunction(funobj);
      JS::Rooted<JSString*> funId(cx, ::JS_GetFunctionId(fun));
      JSString *name = ::JS_AtomizeAndPinJSString(cx, funId);
      NPIdentifier id = StringToNPIdentifier(cx, name);

      ok = npobj->_class->invoke(npobj, id, npargs, argc, &v);
    } else {
      ok = false;

      msg = "Attempt to call a method on object with no invoke method.";
    }
  } else {
    if (npobj->_class->invokeDefault) {
      // obj is a callable object that is being called, no method name
      // available then. Invoke the default method.

      ok = npobj->_class->invokeDefault(npobj, npargs, argc, &v);
    } else {
      ok = false;

      msg = "Attempt to call a default method on object with no "
        "invokeDefault method.";
    }
  }

  // Release arguments.
  for (i = 0; i < argc; ++i) {
    _releasevariantvalue(npargs + i);
  }

  if (npargs != npargs_buf) {
    PR_Free(npargs);
  }

  if (!ok) {
    // ReportExceptionIfPending returns a return value, which is true
    // if no exception was thrown. In that case, throw our own.
    if (ReportExceptionIfPending(cx))
      ThrowJSException(cx, msg);

    return false;
  }

  *rval = NPVariantToJSVal(npp, cx, &v);

  // *rval now owns the value, release our reference.
  _releasevariantvalue(&v);

  return ReportExceptionIfPending(cx);
}

static bool
CallNPMethod(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::Rooted<JSObject*> obj(cx, JS_THIS_OBJECT(cx, vp));
  if (!obj)
      return false;

  return CallNPMethodInternal(cx, obj, args.length(), args.array(), vp, false);
}

static bool
NPObjWrapper_Enumerate(JSContext *cx, JS::Handle<JSObject*> obj,
                       JS::AutoIdVector &properties, bool enumerableOnly)
{
  NPObject *npobj = GetNPObject(cx, obj);
  if (!npobj || !npobj->_class) {
    ThrowJSException(cx, "Bad NPObject as private data!");
    return false;
  }

  PluginDestructionGuard pdg(LookupNPP(npobj));

  if (!NP_CLASS_STRUCT_VERSION_HAS_ENUM(npobj->_class) ||
      !npobj->_class->enumerate) {
    return true;
  }

  NPIdentifier *identifiers;
  uint32_t length;
  if (!npobj->_class->enumerate(npobj, &identifiers, &length)) {
    if (ReportExceptionIfPending(cx)) {
      // ReportExceptionIfPending returns a return value, which is true
      // if no exception was thrown. In that case, throw our own.
      ThrowJSException(cx, "Error enumerating properties on scriptable "
                           "plugin object");
    }
    return false;
  }

  if (!properties.reserve(length))
    return false;

  JS::Rooted<jsid> id(cx);
  for (uint32_t i = 0; i < length; i++) {
    id = NPIdentifierToJSId(identifiers[i]);
    properties.infallibleAppend(id);
  }

  PR_Free(identifiers);
  return true;
}

static bool
NPObjWrapper_Resolve(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                     bool *resolvedp)
{
  if (JSID_IS_SYMBOL(id))
    return true;

  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::JS);

  NPObject *npobj = GetNPObject(cx, obj);

  if (!npobj || !npobj->_class || !npobj->_class->hasProperty ||
      !npobj->_class->hasMethod) {
    ThrowJSException(cx, "Bad NPObject as private data!");

    return false;
  }

  PluginDestructionGuard pdg(LookupNPP(npobj));

  NPIdentifier identifier = JSIdToNPIdentifier(id);

  bool hasProperty = npobj->_class->hasProperty(npobj, identifier);
  if (!ReportExceptionIfPending(cx))
    return false;

  if (hasProperty) {
    NS_ASSERTION(JSID_IS_STRING(id) || JSID_IS_INT(id),
                 "id must be either string or int!\n");
    if (!::JS_DefinePropertyById(cx, obj, id, JS::UndefinedHandleValue,
                                 JSPROP_ENUMERATE | JSPROP_SHARED)) {
        return false;
    }

    *resolvedp = true;

    return true;
  }

  bool hasMethod = npobj->_class->hasMethod(npobj, identifier);
  if (!ReportExceptionIfPending(cx))
    return false;

  if (hasMethod) {
    NS_ASSERTION(JSID_IS_STRING(id) || JSID_IS_INT(id),
                 "id must be either string or int!\n");

    JSFunction *fnc = ::JS_DefineFunctionById(cx, obj, id, CallNPMethod, 0,
                                              JSPROP_ENUMERATE);

    *resolvedp = true;

    return fnc != nullptr;
  }

  // no property or method
  return true;
}

static void
NPObjWrapper_Finalize(js::FreeOp *fop, JSObject *obj)
{
  NPObject *npobj = (NPObject *)::JS_GetPrivate(obj);
  if (npobj) {
    if (sNPObjWrappers) {
      sNPObjWrappers->Remove(npobj);
    }
  }

  if (!sDelayedReleases)
    sDelayedReleases = new nsTArray<NPObject*>;
  sDelayedReleases->AppendElement(npobj);
}

static void
NPObjWrapper_ObjectMoved(JSObject *obj, const JSObject *old)
{
  // The wrapper JSObject has been moved, so we need to update the entry in the
  // sNPObjWrappers hash table, if present.

  if (!sNPObjWrappers) {
    return;
  }

  NPObject *npobj = (NPObject *)::JS_GetPrivate(obj);
  if (!npobj) {
    return;
  }

  // Calling PLDHashTable::Search() will not result in GC.
  JS::AutoSuppressGCAnalysis nogc;

  auto entry =
    static_cast<NPObjWrapperHashEntry*>(sNPObjWrappers->Search(npobj));
  MOZ_ASSERT(entry && entry->mJSObj);
  MOZ_ASSERT(entry->mJSObj == old);
  entry->mJSObj = obj;
}

static bool
NPObjWrapper_Call(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::Rooted<JSObject*> obj(cx, &args.callee());
  return CallNPMethodInternal(cx, obj, args.length(), args.array(), vp, false);
}

static bool
NPObjWrapper_Construct(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::Rooted<JSObject*> obj(cx, &args.callee());
  return CallNPMethodInternal(cx, obj, args.length(), args.array(), vp, true);
}

static bool
NPObjWrapper_toPrimitive(JSContext *cx, unsigned argc, JS::Value *vp)
{
  // Plugins do not simply use the default OrdinaryToPrimitive behavior,
  // because that behavior involves calling toString or valueOf on objects
  // which weren't designed to accommodate this.  Usually this wouldn't be a
  // problem, because the absence of either property, or the presence of either
  // property with a value that isn't callable, will cause that property to
  // simply be ignored.  But there is a problem in one specific case: Java,
  // specifically java.lang.Integer.  The Integer class has static valueOf
  // methods, none of which are nullary, so the JS-reflected method will behave
  // poorly when called with no arguments.  We work around this problem by
  // giving plugins a [Symbol.toPrimitive]() method which uses only toString
  // and not valueOf.

  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::RootedValue thisv(cx, args.thisv());
  if (thisv.isPrimitive())
    return true;

  JS::RootedObject obj(cx, &thisv.toObject());
  JS::RootedValue v(cx);
  if (!JS_GetProperty(cx, obj, "toString", &v))
    return false;
  if (v.isObject() && JS::IsCallable(&v.toObject())) {
    if (!JS_CallFunctionValue(cx, obj, v, JS::HandleValueArray::empty(), args.rval()))
      return false;
    if (args.rval().isPrimitive())
      return true;
  }

  JS_ReportErrorNumber(cx, js::GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                       JS_GetClass(obj)->name,
                       "primitive type");
  return false;
}

bool
nsNPObjWrapper::IsWrapper(JSObject *obj)
{
  return js::GetObjectClass(obj) == &sNPObjectJSWrapperClass;
}

// An NPObject is going away, make sure we null out the JS object's
// private data in case this is an NPObject that came from a plugin
// and it's destroyed prematurely.

// static
void
nsNPObjWrapper::OnDestroy(NPObject *npobj)
{
  if (!npobj) {
    return;
  }

  if (npobj->_class == &nsJSObjWrapper::sJSObjWrapperNPClass) {
    // npobj is one of our own, no private data to clean up here.

    return;
  }

  if (!sNPObjWrappers) {
    // No hash yet (or any more), no used wrappers available.

    return;
  }

  auto entry =
    static_cast<NPObjWrapperHashEntry*>(sNPObjWrappers->Search(npobj));

  if (entry && entry->mJSObj) {
    // Found a live NPObject wrapper, null out its JSObjects' private
    // data.

    ::JS_SetPrivate(entry->mJSObj, nullptr);

    // Remove the npobj from the hash now that it went away.
    sNPObjWrappers->RawRemove(entry);

    // The finalize hook will call OnWrapperDestroyed().
  }
}

// Look up or create a JSObject that wraps the NPObject npobj.

// static
JSObject *
nsNPObjWrapper::GetNewOrUsed(NPP npp, JSContext *cx, NPObject *npobj)
{
  if (!npobj) {
    NS_ERROR("Null NPObject passed to nsNPObjWrapper::GetNewOrUsed()!");

    return nullptr;
  }

  if (npobj->_class == &nsJSObjWrapper::sJSObjWrapperNPClass) {
    // npobj is one of our own, return its existing JSObject.

    JS::Rooted<JSObject*> obj(cx, ((nsJSObjWrapper *)npobj)->mJSObj);
    if (!JS_WrapObject(cx, &obj)) {
      return nullptr;
    }
    return obj;
  }

  if (!npp) {
    NS_ERROR("No npp passed to nsNPObjWrapper::GetNewOrUsed()!");

    return nullptr;
  }

  if (!sNPObjWrappers) {
    // No hash yet (or any more), initialize it.
    if (!CreateNPObjWrapperTable()) {
      return nullptr;
    }
  }

  auto entry =
    static_cast<NPObjWrapperHashEntry*>(sNPObjWrappers->Add(npobj, fallible));

  if (!entry) {
    // Out of memory
    JS_ReportOutOfMemory(cx);

    return nullptr;
  }

  if (entry->mJSObj) {
    // Found a live NPObject wrapper. It may not be in the same compartment
    // as cx, so we need to wrap it before returning it.
    JS::Rooted<JSObject*> obj(cx, entry->mJSObj);
    if (!JS_WrapObject(cx, &obj)) {
      return nullptr;
    }
    return obj;
  }

  entry->mNPObj = npobj;
  entry->mNpp = npp;

  uint32_t generation = sNPObjWrappers->Generation();

  // No existing JSObject, create one.

  JS::Rooted<JSObject*> obj(cx, ::JS_NewObject(cx, js::Jsvalify(&sNPObjectJSWrapperClass)));

  if (generation != sNPObjWrappers->Generation()) {
      // Reload entry if the JS_NewObject call caused a GC and reallocated
      // the table (see bug 445229). This is guaranteed to succeed.

      NS_ASSERTION(sNPObjWrappers->Search(npobj),
                   "Hashtable didn't find what we just added?");
  }

  if (!obj) {
    // OOM? Remove the stale entry from the hash.

    sNPObjWrappers->RawRemove(entry);

    return nullptr;
  }

  OnWrapperCreated();

  entry->mJSObj = obj;

  ::JS_SetPrivate(obj, npobj);

  // The new JSObject now holds on to npobj
  _retainobject(npobj);

  return obj;
}

// static
void
nsJSNPRuntime::OnPluginDestroy(NPP npp)
{
  if (sJSObjWrappersAccessible) {

    // Prevent modification of sJSObjWrappers table if we go reentrant.
    sJSObjWrappersAccessible = false;

    for (JSObjWrapperTable::Enum e(sJSObjWrappers); !e.empty(); e.popFront()) {
      nsJSObjWrapper *npobj = e.front().value();
      MOZ_ASSERT(npobj->_class == &nsJSObjWrapper::sJSObjWrapperNPClass);
      if (npobj->mNpp == npp) {
        if (npobj->_class && npobj->_class->invalidate) {
          npobj->_class->invalidate(npobj);
        }

        _releaseobject(npobj);

        e.removeFront();
      }
    }

    sJSObjWrappersAccessible = true;
  }

  if (sNPObjWrappers) {
    for (auto i = sNPObjWrappers->Iter(); !i.Done(); i.Next()) {
      auto entry = static_cast<NPObjWrapperHashEntry*>(i.Get());

      if (entry->mNpp == npp) {
        // HACK: temporarily hide the table we're enumerating so that
        // invalidate() and deallocate() don't touch it.
        PLDHashTable *tmp = sNPObjWrappers;
        sNPObjWrappers = nullptr;

        NPObject *npobj = entry->mNPObj;

        if (npobj->_class && npobj->_class->invalidate) {
          npobj->_class->invalidate(npobj);
        }

#ifdef NS_BUILD_REFCNT_LOGGING
        {
          int32_t refCnt = npobj->referenceCount;
          while (refCnt) {
            --refCnt;
            NS_LOG_RELEASE(npobj, refCnt, "BrowserNPObject");
          }
        }
#endif

        // Force deallocation of plugin objects since the plugin they came
        // from is being torn down.
        if (npobj->_class && npobj->_class->deallocate) {
          npobj->_class->deallocate(npobj);
        } else {
          PR_Free(npobj);
        }

        ::JS_SetPrivate(entry->mJSObj, nullptr);

        sNPObjWrappers = tmp;

        if (sDelayedReleases && sDelayedReleases->RemoveElement(npobj)) {
          OnWrapperDestroyed();
        }

        i.Remove();
      }
    }
  }
}

// static
void
nsJSNPRuntime::OnPluginDestroyPending(NPP npp)
{
  if (sJSObjWrappersAccessible) {
    // Prevent modification of sJSObjWrappers table if we go reentrant.
    sJSObjWrappersAccessible = false;
    for (JSObjWrapperTable::Enum e(sJSObjWrappers); !e.empty(); e.popFront()) {
      nsJSObjWrapper *npobj = e.front().value();
      MOZ_ASSERT(npobj->_class == &nsJSObjWrapper::sJSObjWrapperNPClass);
      if (npobj->mNpp == npp) {
        npobj->mDestroyPending = true;
      }
    }
    sJSObjWrappersAccessible = true;
  }
}

// Find the NPP for a NPObject.
static NPP
LookupNPP(NPObject *npobj)
{
  if (npobj->_class == &nsJSObjWrapper::sJSObjWrapperNPClass) {
    nsJSObjWrapper* o = static_cast<nsJSObjWrapper*>(npobj);
    return o->mNpp;
  }

  auto entry =
    static_cast<NPObjWrapperHashEntry*>(sNPObjWrappers->Add(npobj, fallible));

  if (!entry) {
    return nullptr;
  }

  NS_ASSERTION(entry->mNpp, "Live NPObject entry w/o an NPP!");

  return entry->mNpp;
}

static bool
CreateNPObjectMember(NPP npp, JSContext *cx, JSObject *obj, NPObject* npobj,
                     JS::Handle<jsid> id,  NPVariant* getPropertyResult,
                     JS::MutableHandle<JS::Value> vp)
{
  if (!npobj || !npobj->_class || !npobj->_class->getProperty ||
      !npobj->_class->invoke) {
    ThrowJSException(cx, "Bad NPObject");

    return false;
  }

  NPObjectMemberPrivate *memberPrivate =
    (NPObjectMemberPrivate *)PR_Malloc(sizeof(NPObjectMemberPrivate));
  if (!memberPrivate)
    return false;

  // Make sure to clear all members in case something fails here
  // during initialization.
  memset(memberPrivate, 0, sizeof(NPObjectMemberPrivate));

  JSObject *memobj = ::JS_NewObject(cx, &sNPObjectMemberClass);
  if (!memobj) {
    PR_Free(memberPrivate);
    return false;
  }

  vp.setObject(*memobj);

  ::JS_SetPrivate(memobj, (void *)memberPrivate);

  NPIdentifier identifier = JSIdToNPIdentifier(id);

  JS::Rooted<JS::Value> fieldValue(cx);
  NPVariant npv;

  if (getPropertyResult) {
    // Plugin has already handed us the value we want here.
    npv = *getPropertyResult;
  }
  else {
    VOID_TO_NPVARIANT(npv);

    NPBool hasProperty = npobj->_class->getProperty(npobj, identifier,
                                                    &npv);
    if (!ReportExceptionIfPending(cx) || !hasProperty) {
      return false;
    }
  }

  fieldValue = NPVariantToJSVal(npp, cx, &npv);

  // npobjWrapper is the JSObject through which we make sure we don't
  // outlive the underlying NPObject, so make sure it points to the
  // real JSObject wrapper for the NPObject.
  obj = GetNPObjectWrapper(cx, obj);

  memberPrivate->npobjWrapper = obj;

  memberPrivate->fieldValue = fieldValue;
  memberPrivate->methodName = id;
  memberPrivate->npp = npp;

  return true;
}

static bool
NPObjectMember_GetProperty(JSContext *cx, JS::HandleObject obj, JS::HandleId id,
                           JS::MutableHandleValue vp)
{
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::OTHER);

  if (JSID_IS_SYMBOL(id)) {
    JS::RootedSymbol sym(cx, JSID_TO_SYMBOL(id));
    if (JS::GetSymbolCode(sym) == JS::SymbolCode::toPrimitive) {
      JS::RootedObject obj(cx, JS_GetFunctionObject(
                                 JS_NewFunction(
                                   cx, NPObjectMember_toPrimitive, 1, 0,
                                   "Symbol.toPrimitive")));
      if (!obj)
        return false;
      vp.setObject(*obj);
      return true;
    }
  }

  return true;
}

static void
NPObjectMember_Finalize(JSFreeOp *fop, JSObject *obj)
{
  NPObjectMemberPrivate *memberPrivate;

  memberPrivate = (NPObjectMemberPrivate *)::JS_GetPrivate(obj);
  if (!memberPrivate)
    return;

  PR_Free(memberPrivate);
}

static bool
NPObjectMember_Call(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::Rooted<JSObject*> memobj(cx, &args.callee());
  NS_ENSURE_TRUE(memobj, false);

  NPObjectMemberPrivate *memberPrivate =
    (NPObjectMemberPrivate *)::JS_GetInstancePrivate(cx, memobj,
                                                     &sNPObjectMemberClass,
                                                     &args);
  if (!memberPrivate || !memberPrivate->npobjWrapper)
    return false;

  NPObject *npobj = GetNPObject(cx, memberPrivate->npobjWrapper);
  if (!npobj) {
    ThrowJSException(cx, "Call on invalid member object");

    return false;
  }

  NPVariant npargs_buf[8];
  NPVariant *npargs = npargs_buf;

  if (args.length() > (sizeof(npargs_buf) / sizeof(NPVariant))) {
    // Our stack buffer isn't large enough to hold all arguments,
    // malloc a buffer.
    npargs = (NPVariant *)PR_Malloc(args.length() * sizeof(NPVariant));

    if (!npargs) {
      ThrowJSException(cx, "Out of memory!");

      return false;
    }
  }

  // Convert arguments
  for (uint32_t i = 0; i < args.length(); ++i) {
    if (!JSValToNPVariant(memberPrivate->npp, cx, args[i], npargs + i)) {
      ThrowJSException(cx, "Error converting jsvals to NPVariants!");

      if (npargs != npargs_buf) {
        PR_Free(npargs);
      }

      return false;
    }
  }


  NPVariant npv;
  bool ok = npobj->_class->invoke(npobj,
                                  JSIdToNPIdentifier(memberPrivate->methodName),
                                  npargs, args.length(), &npv);

  // Release arguments.
  for (uint32_t i = 0; i < args.length(); ++i) {
    _releasevariantvalue(npargs + i);
  }

  if (npargs != npargs_buf) {
    PR_Free(npargs);
  }

  if (!ok) {
    // ReportExceptionIfPending returns a return value, which is true
    // if no exception was thrown. In that case, throw our own.
    if (ReportExceptionIfPending(cx))
      ThrowJSException(cx, "Error calling method on NPObject!");

    return false;
  }

  args.rval().set(NPVariantToJSVal(memberPrivate->npp, cx, &npv));

  // *vp now owns the value, release our reference.
  _releasevariantvalue(&npv);

  return ReportExceptionIfPending(cx);
}

static void
NPObjectMember_Trace(JSTracer *trc, JSObject *obj)
{
  NPObjectMemberPrivate *memberPrivate =
    (NPObjectMemberPrivate *)::JS_GetPrivate(obj);
  if (!memberPrivate)
    return;

  // Our NPIdentifier is not always interned, so we must trace it.
  JS::TraceEdge(trc, &memberPrivate->methodName, "NPObjectMemberPrivate.methodName");

  JS::TraceEdge(trc, &memberPrivate->fieldValue, "NPObject Member => fieldValue");

  // There's no strong reference from our private data to the
  // NPObject, so make sure to mark the NPObject wrapper to keep the
  // NPObject alive as long as this NPObjectMember is alive.
  JS::TraceEdge(trc, &memberPrivate->npobjWrapper,
                "NPObject Member => npobjWrapper");
}

static bool
NPObjectMember_toPrimitive(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::RootedValue thisv(cx, args.thisv());
  if (thisv.isPrimitive()) {
    args.rval().set(thisv);
    return true;
  }

  JS::RootedObject obj(cx, &thisv.toObject());
  NPObjectMemberPrivate *memberPrivate =
    (NPObjectMemberPrivate *)::JS_GetInstancePrivate(cx, obj,
                                                     &sNPObjectMemberClass,
                                                     &args);
  if (!memberPrivate)
    return false;

  JSType hint;
  if (!JS::GetFirstArgumentAsTypeHint(cx, args, &hint))
    return false;

  args.rval().set(memberPrivate->fieldValue);
  if (args.rval().isObject()) {
    JS::Rooted<JSObject*> objVal(cx, &args.rval().toObject());
    return JS::ToPrimitive(cx, objVal, hint, args.rval());
  }
  return true;
}

// static
bool
nsJSObjWrapper::HasOwnProperty(NPObject *npobj, NPIdentifier npid)
{
  NPP npp = NPPStack::Peek();
  dom::AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.InitWithLegacyErrorReporting(GetGlobalObject(npp)))) {
    return false;
  }
  JSContext *cx = jsapi.cx();

  if (!npobj) {
    ThrowJSException(cx,
                     "Null npobj in nsJSObjWrapper::NP_HasOwnProperty!");

    return false;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;
  bool found, ok = false;

  AutoJSExceptionReporter reporter(jsapi, npjsobj);
  JS::Rooted<JSObject*> jsobj(cx, npjsobj->mJSObj);
  JSAutoCompartment ac(cx, jsobj);

  NS_ASSERTION(NPIdentifierIsInt(npid) || NPIdentifierIsString(npid),
               "id must be either string or int!\n");
  JS::Rooted<jsid> id(cx, NPIdentifierToJSId(npid));
  ok = ::JS_AlreadyHasOwnPropertyById(cx, jsobj, id, &found);
  return ok && found;
}
