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
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsDOMJSUtils.h"
#include "nsCxPusher.h"
#include "nsIDocument.h"
#include "nsIJSRuntimeService.h"
#include "nsIXPConnect.h"
#include "nsIDOMElement.h"
#include "prmem.h"
#include "nsIContent.h"
#include "nsPluginInstanceOwner.h"
#include "nsWrapperCacheInlines.h"
#include "js/HashTable.h"
#include "mozilla/HashFunctions.h"


#define NPRUNTIME_JSCLASS_NAME "NPObject JS wrapper class"

using namespace mozilla::plugins::parent;
using namespace mozilla;

#include "mozilla/plugins/PluginScriptableObjectParent.h"
using mozilla::plugins::PluginScriptableObjectParent;
using mozilla::plugins::ParentNPObject;

struct JSObjWrapperHasher : public js::DefaultHasher<nsJSObjWrapperKey>
{
  typedef nsJSObjWrapperKey Key;
  typedef Key Lookup;

  static uint32_t hash(const Lookup &l) {
    return HashGeneric(l.mJSObj, l.mNpp);
  }

  static void rekey(Key &k, const Key& newKey) {
    MOZ_ASSERT(k.mNpp == newKey.mNpp);
    k.mJSObj = newKey.mJSObj;
  }
};

// Hash of JSObject wrappers that wraps JSObjects as NPObjects. There
// will be one wrapper per JSObject per plugin instance, i.e. if two
// plugins access the JSObject x, two wrappers for x will be
// created. This is needed to be able to properly drop the wrappers
// when a plugin is torn down in case there's a leak in the plugin (we
// don't want to leak the world just because a plugin leaks an
// NPObject).
typedef js::HashMap<nsJSObjWrapperKey,
                    nsJSObjWrapper*,
                    JSObjWrapperHasher,
                    js::SystemAllocPolicy> JSObjWrapperTable;
static JSObjWrapperTable sJSObjWrappers;

// Hash of NPObject wrappers that wrap NPObjects as JSObjects.
static PLDHashTable sNPObjWrappers;

// Global wrapper count. This includes JSObject wrappers *and*
// NPObject wrappers. When this count goes to zero, there are no more
// wrappers and we can kill off hash tables etc.
static int32_t sWrapperCount;

// The JSRuntime. Used to unroot JSObjects when no JSContext is
// reachable.
static JSRuntime *sJSRuntime;

static nsTArray<NPObject*>* sDelayedReleases;

namespace {

inline bool
NPObjectIsOutOfProcessProxy(NPObject *obj)
{
  return obj->_class == PluginScriptableObjectParent::GetClass();
}

} // anonymous namespace

// Helper class that reports any JS exceptions that were thrown while
// the plugin executed JS.

class AutoJSExceptionReporter
{
public:
  AutoJSExceptionReporter(JSContext *cx)
    : mCx(cx)
  {
  }

  ~AutoJSExceptionReporter()
  {
    JS_ReportPendingException(mCx);
  }

protected:
  JSContext *mCx;
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
NPObjWrapper_AddProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp);

static bool
NPObjWrapper_DelProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, bool *succeeded);

static bool
NPObjWrapper_SetProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, bool strict,
                         JS::MutableHandle<JS::Value> vp);

static bool
NPObjWrapper_GetProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp);

static bool
NPObjWrapper_newEnumerate(JSContext *cx, JS::Handle<JSObject*> obj, JSIterateOp enum_op,
                          JS::Value *statep, jsid *idp);

static bool
NPObjWrapper_NewResolve(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, unsigned flags,
                        JS::MutableHandle<JSObject*> objp);

static bool
NPObjWrapper_Convert(JSContext *cx, JS::Handle<JSObject*> obj, JSType type, JS::MutableHandle<JS::Value> vp);

static void
NPObjWrapper_Finalize(JSFreeOp *fop, JSObject *obj);

static bool
NPObjWrapper_Call(JSContext *cx, unsigned argc, JS::Value *vp);

static bool
NPObjWrapper_Construct(JSContext *cx, unsigned argc, JS::Value *vp);

static bool
CreateNPObjectMember(NPP npp, JSContext *cx, JSObject *obj, NPObject *npobj,
                     JS::Handle<jsid> id, NPVariant* getPropertyResult, JS::Value *vp);

const JSClass sNPObjectJSWrapperClass =
  {
    NPRUNTIME_JSCLASS_NAME,
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_NEW_RESOLVE | JSCLASS_NEW_ENUMERATE,
    NPObjWrapper_AddProperty,
    NPObjWrapper_DelProperty,
    NPObjWrapper_GetProperty,
    NPObjWrapper_SetProperty,
    (JSEnumerateOp)NPObjWrapper_newEnumerate,
    (JSResolveOp)NPObjWrapper_NewResolve,
    NPObjWrapper_Convert,
    NPObjWrapper_Finalize,
    NPObjWrapper_Call,
    nullptr,                                                /* hasInstance */
    NPObjWrapper_Construct
  };

typedef struct NPObjectMemberPrivate {
    JS::Heap<JSObject *> npobjWrapper;
    JS::Heap<JS::Value> fieldValue;
    JS::Heap<jsid> methodName;
    NPP   npp;
} NPObjectMemberPrivate;

static bool
NPObjectMember_Convert(JSContext *cx, JS::Handle<JSObject*> obj, JSType type, JS::MutableHandle<JS::Value> vp);

static void
NPObjectMember_Finalize(JSFreeOp *fop, JSObject *obj);

static bool
NPObjectMember_Call(JSContext *cx, unsigned argc, JS::Value *vp);

static void
NPObjectMember_Trace(JSTracer *trc, JSObject *obj);

static const JSClass sNPObjectMemberClass =
  {
    "NPObject Ambiguous Member class", JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub, JS_DeletePropertyStub,
    JS_PropertyStub, JS_StrictPropertyStub, JS_EnumerateStub,
    JS_ResolveStub, NPObjectMember_Convert,
    NPObjectMember_Finalize, NPObjectMember_Call,
    nullptr, nullptr, NPObjectMember_Trace
  };

static void
OnWrapperDestroyed();

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

static void
OnWrapperCreated()
{
  if (sWrapperCount++ == 0) {
    static const char rtsvc_id[] = "@mozilla.org/js/xpc/RuntimeService;1";
    nsCOMPtr<nsIJSRuntimeService> rtsvc(do_GetService(rtsvc_id));
    if (!rtsvc)
      return;

    rtsvc->GetRuntime(&sJSRuntime);
    NS_ASSERTION(sJSRuntime != nullptr, "no JSRuntime?!");

    // Register our GC callback to perform delayed destruction of finalized
    // NPObjects. Leave this callback around and don't ever unregister it.
    rtsvc->RegisterGCCallback(DelayedReleaseGCCallback);
  }
}

static void
OnWrapperDestroyed()
{
  NS_ASSERTION(sWrapperCount, "Whaaa, unbalanced created/destroyed calls!");

  if (--sWrapperCount == 0) {
    if (sJSObjWrappers.initialized()) {
      MOZ_ASSERT(sJSObjWrappers.count() == 0);

      // No more wrappers, and our hash was initialized. Finish the
      // hash to prevent leaking it.
      sJSObjWrappers.finish();
    }

    if (sNPObjWrappers.ops) {
      MOZ_ASSERT(sNPObjWrappers.entryCount == 0);

      // No more wrappers, and our hash was initialized. Finish the
      // hash to prevent leaking it.
      PL_DHashTableFinish(&sNPObjWrappers);

      sNPObjWrappers.ops = nullptr;
    }

    // No more need for this.
    sJSRuntime = nullptr;
  }
}

namespace mozilla {
namespace plugins {
namespace parent {

JSContext *
GetJSContext(NPP npp)
{
  NS_ENSURE_TRUE(npp, nullptr);

  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance *)npp->ndata;
  NS_ENSURE_TRUE(inst, nullptr);

  nsRefPtr<nsPluginInstanceOwner> owner = inst->GetOwner();
  NS_ENSURE_TRUE(owner, nullptr);

  nsCOMPtr<nsIDocument> doc;
  owner->GetDocument(getter_AddRefs(doc));
  NS_ENSURE_TRUE(doc, nullptr);

  nsCOMPtr<nsISupports> documentContainer = doc->GetContainer();
  nsCOMPtr<nsIScriptGlobalObject> sgo(do_GetInterface(documentContainer));
  NS_ENSURE_TRUE(sgo, nullptr);

  nsIScriptContext *scx = sgo->GetContext();
  NS_ENSURE_TRUE(scx, nullptr);

  return scx->GetNativeContext();
}

}
}
}

static NPP
LookupNPP(NPObject *npobj);


static JS::Value
NPVariantToJSVal(NPP npp, JSContext *cx, const NPVariant *variant)
{
  switch (variant->type) {
  case NPVariantType_Void :
    return JSVAL_VOID;
  case NPVariantType_Null :
    return JSVAL_NULL;
  case NPVariantType_Bool :
    return BOOLEAN_TO_JSVAL(NPVARIANT_TO_BOOLEAN(*variant));
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
        return STRING_TO_JSVAL(str);
      }

      break;
    }
  case NPVariantType_Object:
    {
      if (npp) {
        JSObject *obj =
          nsNPObjWrapper::GetNewOrUsed(npp, cx, NPVARIANT_TO_OBJECT(*variant));

        if (obj) {
          return OBJECT_TO_JSVAL(obj);
        }
      }

      NS_ERROR("Error wrapping NPObject!");

      break;
    }
  default:
    NS_ERROR("Unknown NPVariant type!");
  }

  NS_ERROR("Unable to convert NPVariant to jsval!");

  return JSVAL_VOID;
}

bool
JSValToNPVariant(NPP npp, JSContext *cx, JS::Value val, NPVariant *variant)
{
  NS_ASSERTION(npp, "Must have an NPP to wrap a jsval!");

  if (JSVAL_IS_PRIMITIVE(val)) {
    if (val == JSVAL_VOID) {
      VOID_TO_NPVARIANT(*variant);
    } else if (JSVAL_IS_NULL(val)) {
      NULL_TO_NPVARIANT(*variant);
    } else if (JSVAL_IS_BOOLEAN(val)) {
      BOOLEAN_TO_NPVARIANT(JSVAL_TO_BOOLEAN(val), *variant);
    } else if (JSVAL_IS_INT(val)) {
      INT32_TO_NPVARIANT(JSVAL_TO_INT(val), *variant);
    } else if (JSVAL_IS_DOUBLE(val)) {
      double d = JSVAL_TO_DOUBLE(val);
      int i;
      if (JS_DoubleIsInt32(d, &i)) {
        INT32_TO_NPVARIANT(i, *variant);
      } else {
        DOUBLE_TO_NPVARIANT(d, *variant);
      }
    } else if (JSVAL_IS_STRING(val)) {
      JSString *jsstr = JSVAL_TO_STRING(val);
      size_t length;
      const jschar *chars = ::JS_GetStringCharsZAndLength(cx, jsstr, &length);
      if (!chars) {
          return false;
      }

      nsDependentString str(chars, length);

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
    obj = JSVAL_TO_OBJECT(val);
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
  : mNpp(npp)
{
  MOZ_COUNT_CTOR(nsJSObjWrapper);
  OnWrapperCreated();
}

nsJSObjWrapper::~nsJSObjWrapper()
{
  MOZ_COUNT_DTOR(nsJSObjWrapper);

  // Invalidate first, since it relies on sJSRuntime and sJSObjWrappers.
  NP_Invalidate(this);

  OnWrapperDestroyed();
}

void
nsJSObjWrapper::ClearJSObject() {
  // Unroot the object's JSObject
  JS_RemoveObjectRootRT(sJSRuntime, &mJSObj);

  // Forget our reference to the JSObject.
  mJSObj = nullptr;
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

    // Remove the wrapper from the hash
    MOZ_ASSERT(sJSObjWrappers.initialized());
    nsJSObjWrapperKey key(jsnpobj->mJSObj, jsnpobj->mNpp);
    sJSObjWrappers.remove(key);

    jsnpobj->ClearJSObject();
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
  JSContext *cx = GetJSContext(npp);

  if (!cx) {
    return false;
  }

  if (!npobj) {
    ThrowJSException(cx,
                     "Null npobj in nsJSObjWrapper::NP_HasMethod!");

    return false;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;

  nsCxPusher pusher;
  pusher.Push(cx);
  JSAutoCompartment ac(cx, npjsobj->mJSObj);

  AutoJSExceptionReporter reporter(cx);

  JS::Rooted<JS::Value> v(cx);
  bool ok = GetProperty(cx, npjsobj->mJSObj, id, &v);

  return ok && !JSVAL_IS_PRIMITIVE(v) &&
    ::JS_ObjectIsFunction(cx, JSVAL_TO_OBJECT(v));
}

static bool
doInvoke(NPObject *npobj, NPIdentifier method, const NPVariant *args,
         uint32_t argCount, bool ctorCall, NPVariant *result)
{
  NPP npp = NPPStack::Peek();
  JSContext *cx = GetJSContext(npp);

  if (!cx) {
    return false;
  }

  if (!npobj || !result) {
    ThrowJSException(cx, "Null npobj, or result in doInvoke!");

    return false;
  }

  // Initialize *result
  VOID_TO_NPVARIANT(*result);

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;

  nsCxPusher pusher;
  pusher.Push(cx);
  JSAutoCompartment ac(cx, npjsobj->mJSObj);
  JS::Rooted<JS::Value> fv(cx);

  AutoJSExceptionReporter reporter(cx);

  if (method != NPIdentifier_VOID) {
    if (!GetProperty(cx, npjsobj->mJSObj, method, &fv) ||
        ::JS_TypeOfValue(cx, fv) != JSTYPE_FUNCTION) {
      return false;
    }
  } else {
    fv = OBJECT_TO_JSVAL(npjsobj->mJSObj);
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
      ::JS_New(cx, npjsobj->mJSObj, jsargs.length(), jsargs.begin());

    if (newObj) {
      v.setObject(*newObj);
      ok = true;
    }
  } else {
    ok = ::JS_CallFunctionValue(cx, npjsobj->mJSObj, fv, jsargs.length(),
                                jsargs.begin(), v.address());
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
  JSContext *cx = GetJSContext(npp);

  if (!cx) {
    return false;
  }

  if (!npobj) {
    ThrowJSException(cx,
                     "Null npobj in nsJSObjWrapper::NP_HasProperty!");

    return false;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;
  bool found, ok = false;

  nsCxPusher pusher;
  pusher.Push(cx);
  AutoJSExceptionReporter reporter(cx);
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
  JSContext *cx = GetJSContext(npp);

  if (!cx) {
    return false;
  }

  if (!npobj) {
    ThrowJSException(cx,
                     "Null npobj in nsJSObjWrapper::NP_GetProperty!");

    return false;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;

  nsCxPusher pusher;
  pusher.Push(cx);
  AutoJSExceptionReporter reporter(cx);
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
  JSContext *cx = GetJSContext(npp);

  if (!cx) {
    return false;
  }

  if (!npobj) {
    ThrowJSException(cx,
                     "Null npobj in nsJSObjWrapper::NP_SetProperty!");

    return false;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;
  bool ok = false;

  nsCxPusher pusher;
  pusher.Push(cx);
  AutoJSExceptionReporter reporter(cx);
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
  JSContext *cx = GetJSContext(npp);

  if (!cx) {
    return false;
  }

  if (!npobj) {
    ThrowJSException(cx,
                     "Null npobj in nsJSObjWrapper::NP_RemoveProperty!");

    return false;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;
  bool ok = false;

  nsCxPusher pusher;
  pusher.Push(cx);
  AutoJSExceptionReporter reporter(cx);
  bool deleted = false;
  JS::Rooted<JSObject*> obj(cx, npjsobj->mJSObj);
  JSAutoCompartment ac(cx, obj);

  NS_ASSERTION(NPIdentifierIsInt(npid) || NPIdentifierIsString(npid),
               "id must be either string or int!\n");
  JS::Rooted<jsid> id(cx, NPIdentifierToJSId(npid));
  ok = ::JS_DeletePropertyById2(cx, obj, id, &deleted);
  if (ok && deleted) {
    // FIXME: See bug 425823, we shouldn't need to do this, and once
    // that bug is fixed we can remove this code.

    bool hasProp;
    ok = ::JS_HasPropertyById(cx, obj, id, &hasProp);

    if (ok && hasProp) {
      // The property might have been deleted, but it got
      // re-resolved, so no, it's not really deleted.

      deleted = false;
    }
  }

  return ok && deleted;
}

//static
bool
nsJSObjWrapper::NP_Enumerate(NPObject *npobj, NPIdentifier **idarray,
                             uint32_t *count)
{
  NPP npp = NPPStack::Peek();
  JSContext *cx = GetJSContext(npp);

  *idarray = 0;
  *count = 0;

  if (!cx) {
    return false;
  }

  if (!npobj) {
    ThrowJSException(cx,
                     "Null npobj in nsJSObjWrapper::NP_Enumerate!");

    return false;
  }

  nsJSObjWrapper *npjsobj = (nsJSObjWrapper *)npobj;

  nsCxPusher pusher;
  pusher.Push(cx);
  AutoJSExceptionReporter reporter(cx);
  JSAutoCompartment ac(cx, npjsobj->mJSObj);

  JS::AutoIdArray ida(cx, JS_Enumerate(cx, npjsobj->mJSObj));
  if (!ida) {
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
      str = JS_InternJSString(cx, str);
      if (!str) {
        PR_Free(*idarray);
        return false;
      }
      id = StringToNPIdentifier(cx, str);
    } else {
      NS_ASSERTION(JSVAL_IS_INT(v),
                   "The element in ida must be either string or int!\n");
      id = IntToNPIdentifier(JSVAL_TO_INT(v));
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



/*
 * This function is called during minor GCs for each key in the sJSObjWrappers
 * table that has been moved.
 *
 * Note that the wrapper may be dead at this point, and even the table may have
 * been finalized if all wrappers have died.
 */
static void
JSObjWrapperKeyMarkCallback(JSTracer *trc, JSObject *obj, void *data) {
  NPP npp = static_cast<NPP>(data);
  if (!sJSObjWrappers.initialized())
    return;

  JSObject *prior = obj;
  nsJSObjWrapperKey oldKey(prior, npp);
  JSObjWrapperTable::Ptr p = sJSObjWrappers.lookup(oldKey);
  if (!p)
    return;

  JS_CallObjectTracer(trc, &obj, "sJSObjWrappers key object");
  nsJSObjWrapperKey newKey(obj, npp);
  sJSObjWrappers.rekeyIfMoved(oldKey, newKey);
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

  const JSClass *clazz = JS_GetClass(obj);

  if (clazz == &sNPObjectJSWrapperClass) {
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
    if (!sJSObjWrappers.init(16)) {
      NS_ERROR("Error initializing PLDHashTable!");

      return nullptr;
    }
  }

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

  nsJSObjWrapperKey key(obj, npp);
  if (!sJSObjWrappers.putNew(key, wrapper)) {
    // Out of memory, free the wrapper we created.
    _releaseobject(wrapper);
    return nullptr;
  }

  NS_ASSERTION(wrapper->mNpp == npp, "nsJSObjWrapper::mNpp not initialized!");

  // Root the JSObject, its lifetime is now tied to that of the
  // NPObject.
  if (!::JS_AddNamedObjectRoot(cx, &wrapper->mJSObj, "nsJSObjWrapper::mJSObject")) {
    NS_ERROR("Failed to root JSObject!");

    sJSObjWrappers.remove(key);
    _releaseobject(wrapper);

    return nullptr;
  }

  // Add postbarrier for the hashtable key
  JS_StoreObjectPostBarrierCallback(cx, JSObjWrapperKeyMarkCallback, obj, wrapper->mNpp);

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
    if (JS_GetClass(obj) == &sNPObjectJSWrapperClass) {
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
NPObjWrapper_AddProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp)
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
NPObjWrapper_DelProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, bool *succeeded)
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

    if (!hasProperty) {
      *succeeded = true;
      return true;
    }
  }

  if (!npobj->_class->removeProperty(npobj, identifier))
    *succeeded = false;

  return ReportExceptionIfPending(cx);
}

static bool
NPObjWrapper_SetProperty(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, bool strict,
                         JS::MutableHandle<JS::Value> vp)
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

  return true;
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
    PluginScriptableObjectParent* actor =
      static_cast<ParentNPObject*>(npobj)->parent;

    // actor may be null if the plugin crashed.
    if (!actor)
      return false;

    bool success = actor->GetPropertyHelper(identifier, &hasProperty,
                                            &hasMethod, &npv);
    if (!ReportExceptionIfPending(cx)) {
      if (success)
        _releasevariantvalue(&npv);
      return false;
    }

    if (success) {
      // We return NPObject Member class here to support ambiguous members.
      if (hasProperty && hasMethod)
        return CreateNPObjectMember(npp, cx, obj, npobj, id, &npv, vp.address());

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
    return CreateNPObjectMember(npp, cx, obj, npobj, id, nullptr, vp.address());

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

  JSObject *funobj = JSVAL_TO_OBJECT(argv[-2]);
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
      JSString *name = ::JS_InternJSString(cx, funId);
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
  JS::Rooted<JSObject*> obj(cx, JS_THIS_OBJECT(cx, vp));
  if (!obj)
      return false;

  return CallNPMethodInternal(cx, obj, argc, JS_ARGV(cx, vp), vp, false);
}

struct NPObjectEnumerateState {
  uint32_t     index;
  uint32_t     length;
  NPIdentifier *value;
};

static bool
NPObjWrapper_newEnumerate(JSContext *cx, JS::Handle<JSObject*> obj, JSIterateOp enum_op,
                          JS::Value *statep, jsid *idp)
{
  NPObject *npobj = GetNPObject(cx, obj);
  NPIdentifier *enum_value;
  uint32_t length;
  NPObjectEnumerateState *state;

  if (!npobj || !npobj->_class) {
    ThrowJSException(cx, "Bad NPObject as private data!");
    return false;
  }

  PluginDestructionGuard pdg(LookupNPP(npobj));

  NS_ASSERTION(statep, "Must have a statep to enumerate!");

  switch(enum_op) {
  case JSENUMERATE_INIT:
  case JSENUMERATE_INIT_ALL:
    state = new NPObjectEnumerateState();
    if (!state) {
      ThrowJSException(cx, "Memory allocation failed for "
                       "NPObjectEnumerateState!");

      return false;
    }

    if (!NP_CLASS_STRUCT_VERSION_HAS_ENUM(npobj->_class) ||
        !npobj->_class->enumerate) {
      enum_value = 0;
      length = 0;
    } else if (!npobj->_class->enumerate(npobj, &enum_value, &length)) {
      delete state;

      if (ReportExceptionIfPending(cx)) {
        // ReportExceptionIfPending returns a return value, which is true
        // if no exception was thrown. In that case, throw our own.
        ThrowJSException(cx, "Error enumerating properties on scriptable "
                             "plugin object");
      }

      return false;
    }

    state->value = enum_value;
    state->length = length;
    state->index = 0;
    *statep = PRIVATE_TO_JSVAL(state);
    if (idp) {
      *idp = INT_TO_JSID(length);
    }

    break;

  case JSENUMERATE_NEXT:
    state = (NPObjectEnumerateState *)JSVAL_TO_PRIVATE(*statep);
    enum_value = state->value;
    length = state->length;
    if (state->index != length) {
      *idp = NPIdentifierToJSId(enum_value[state->index++]);
      return true;
    }

    // FALL THROUGH

  case JSENUMERATE_DESTROY:
    state = (NPObjectEnumerateState *)JSVAL_TO_PRIVATE(*statep);
    if (state->value)
      PR_Free(state->value);
    delete state;
    *statep = JSVAL_NULL;

    break;
  }

  return true;
}

static bool
NPObjWrapper_NewResolve(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, unsigned flags,
                        JS::MutableHandle<JSObject*> objp)
{
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
    if (!::JS_DefinePropertyById(cx, obj, id, JSVAL_VOID, nullptr,
                                 nullptr, JSPROP_ENUMERATE | JSPROP_SHARED)) {
        return false;
    }

    objp.set(obj);

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

    objp.set(obj);

    return fnc != nullptr;
  }

  // no property or method
  return true;
}

static bool
NPObjWrapper_Convert(JSContext *cx, JS::Handle<JSObject*> obj, JSType hint, JS::MutableHandle<JS::Value> vp)
{
  JS_ASSERT(hint == JSTYPE_NUMBER || hint == JSTYPE_STRING || hint == JSTYPE_VOID);

  // Plugins do not simply use JS_ConvertStub, and the default [[DefaultValue]]
  // behavior, because that behavior involves calling toString or valueOf on
  // objects which weren't designed to accommodate this.  Usually this wouldn't
  // be a problem, because the absence of either property, or the presence of
  // either property with a value that isn't callable, will cause that property
  // to simply be ignored.  But there is a problem in one specific case: Java,
  // specifically java.lang.Integer.  The Integer class has static valueOf
  // methods, none of which are nullary, so the JS-reflected method will behave
  // poorly when called with no arguments.  We work around this problem by
  // giving plugins a [[DefaultValue]] which uses only toString and not valueOf.

  JS::Rooted<JS::Value> v(cx, JSVAL_VOID);
  if (!JS_GetProperty(cx, obj, "toString", &v))
    return false;
  if (!JSVAL_IS_PRIMITIVE(v) && JS_ObjectIsCallable(cx, JSVAL_TO_OBJECT(v))) {
    if (!JS_CallFunctionValue(cx, obj, v, 0, nullptr, vp.address()))
      return false;
    if (JSVAL_IS_PRIMITIVE(vp))
      return true;
  }

  JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                       JS_GetClass(obj)->name,
                       hint == JSTYPE_VOID
                       ? "primitive type"
                       : hint == JSTYPE_NUMBER
                       ? "number"
                       : "string");
  return false;
}

static void
NPObjWrapper_Finalize(JSFreeOp *fop, JSObject *obj)
{
  NPObject *npobj = (NPObject *)::JS_GetPrivate(obj);
  if (npobj) {
    if (sNPObjWrappers.ops) {
      PL_DHashTableOperate(&sNPObjWrappers, npobj, PL_DHASH_REMOVE);
    }
  }

  if (!sDelayedReleases)
    sDelayedReleases = new nsTArray<NPObject*>;
  sDelayedReleases->AppendElement(npobj);
}

static bool
NPObjWrapper_Call(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::Rooted<JSObject*> obj(cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));
  return CallNPMethodInternal(cx, obj, argc, JS_ARGV(cx, vp), vp, false);
}

static bool
NPObjWrapper_Construct(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::Rooted<JSObject*> obj(cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));
  return CallNPMethodInternal(cx, obj, argc, JS_ARGV(cx, vp), vp, true);
}

class NPObjWrapperHashEntry : public PLDHashEntryHdr
{
public:
  NPObject *mNPObj; // Must be the first member for the PLDHash stubs to work
  JSObject *mJSObj;
  NPP mNpp;
};


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

  if (!sNPObjWrappers.ops) {
    // No hash yet (or any more), no used wrappers available.

    return;
  }

  NPObjWrapperHashEntry *entry = static_cast<NPObjWrapperHashEntry *>
    (PL_DHashTableOperate(&sNPObjWrappers, npobj, PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_BUSY(entry) && entry->mJSObj) {
    // Found a live NPObject wrapper, null out its JSObjects' private
    // data.

    ::JS_SetPrivate(entry->mJSObj, nullptr);

    // Remove the npobj from the hash now that it went away.
    PL_DHashTableRawRemove(&sNPObjWrappers, entry);

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

  if (!sNPObjWrappers.ops) {
    // No hash yet (or any more), initialize it.

    if (!PL_DHashTableInit(&sNPObjWrappers, PL_DHashGetStubOps(), nullptr,
                           sizeof(NPObjWrapperHashEntry), 16)) {
      NS_ERROR("Error initializing PLDHashTable!");

      return nullptr;
    }
  }

  NPObjWrapperHashEntry *entry = static_cast<NPObjWrapperHashEntry *>
    (PL_DHashTableOperate(&sNPObjWrappers, npobj, PL_DHASH_ADD));

  if (!entry) {
    // Out of memory
    JS_ReportOutOfMemory(cx);

    return nullptr;
  }

  if (PL_DHASH_ENTRY_IS_BUSY(entry) && entry->mJSObj) {
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

  uint32_t generation = sNPObjWrappers.generation;

  // No existing JSObject, create one.

  JS::Rooted<JSObject*> obj(cx, ::JS_NewObject(cx, &sNPObjectJSWrapperClass, JS::NullPtr(),
                                               JS::NullPtr()));

  if (generation != sNPObjWrappers.generation) {
      // Reload entry if the JS_NewObject call caused a GC and reallocated
      // the table (see bug 445229). This is guaranteed to succeed.

      entry = static_cast<NPObjWrapperHashEntry *>
        (PL_DHashTableOperate(&sNPObjWrappers, npobj, PL_DHASH_LOOKUP));
      NS_ASSERTION(entry && PL_DHASH_ENTRY_IS_BUSY(entry),
                   "Hashtable didn't find what we just added?");
  }

  if (!obj) {
    // OOM? Remove the stale entry from the hash.

    PL_DHashTableRawRemove(&sNPObjWrappers, entry);

    return nullptr;
  }

  OnWrapperCreated();

  entry->mJSObj = obj;

  ::JS_SetPrivate(obj, npobj);

  // The new JSObject now holds on to npobj
  _retainobject(npobj);

  return obj;
}


// Struct for passing an NPP and a JSContext to
// NPObjWrapperPluginDestroyedCallback
struct NppAndCx
{
  NPP npp;
  JSContext *cx;
};

static PLDHashOperator
NPObjWrapperPluginDestroyedCallback(PLDHashTable *table, PLDHashEntryHdr *hdr,
                                    uint32_t number, void *arg)
{
  NPObjWrapperHashEntry *entry = (NPObjWrapperHashEntry *)hdr;
  NppAndCx *nppcx = reinterpret_cast<NppAndCx *>(arg);

  if (entry->mNpp == nppcx->npp) {
    // Prevent invalidate() and deallocate() from touching the hash
    // we're enumerating.
    const PLDHashTableOps *ops = table->ops;
    table->ops = nullptr;

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

    table->ops = ops;

    if (sDelayedReleases && sDelayedReleases->RemoveElement(npobj)) {
      OnWrapperDestroyed();
    }

    return PL_DHASH_REMOVE;
  }

  return PL_DHASH_NEXT;
}

// static
void
nsJSNPRuntime::OnPluginDestroy(NPP npp)
{
  if (sJSObjWrappers.initialized()) {
    for (JSObjWrapperTable::Enum e(sJSObjWrappers); !e.empty(); e.popFront()) {
      nsJSObjWrapper *npobj = e.front().value();
      MOZ_ASSERT(npobj->_class == &nsJSObjWrapper::sJSObjWrapperNPClass);
      if (npobj->mNpp == npp) {
        npobj->ClearJSObject();
        _releaseobject(npobj);
        e.removeFront();
      }
    }
  }

  // Use the safe JSContext here as we're not always able to find the
  // JSContext associated with the NPP any more.
  AutoSafeJSContext cx;
  if (sNPObjWrappers.ops) {
    NppAndCx nppcx = { npp, cx };
    PL_DHashTableEnumerate(&sNPObjWrappers,
                           NPObjWrapperPluginDestroyedCallback, &nppcx);
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

  NPObjWrapperHashEntry *entry = static_cast<NPObjWrapperHashEntry *>
    (PL_DHashTableOperate(&sNPObjWrappers, npobj, PL_DHASH_ADD));

  if (PL_DHASH_ENTRY_IS_FREE(entry)) {
    return nullptr;
  }

  NS_ASSERTION(entry->mNpp, "Live NPObject entry w/o an NPP!");

  return entry->mNpp;
}

bool
CreateNPObjectMember(NPP npp, JSContext *cx, JSObject *obj, NPObject* npobj,
                     JS::Handle<jsid> id,  NPVariant* getPropertyResult, JS::Value *vp)
{
  NS_ENSURE_TRUE(vp, false);

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

  JSObject *memobj = ::JS_NewObject(cx, &sNPObjectMemberClass, JS::NullPtr(), JS::NullPtr());
  if (!memobj) {
    PR_Free(memberPrivate);
    return false;
  }

  *vp = OBJECT_TO_JSVAL(memobj);
  ::JS_AddValueRoot(cx, vp);

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
    if (!ReportExceptionIfPending(cx)) {
      ::JS_RemoveValueRoot(cx, vp);
      return false;
    }

    if (!hasProperty) {
      ::JS_RemoveValueRoot(cx, vp);
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

  ::JS_RemoveValueRoot(cx, vp);

  return true;
}

static bool
NPObjectMember_Convert(JSContext *cx, JS::Handle<JSObject*> obj, JSType type, JS::MutableHandle<JS::Value> vp)
{
  NPObjectMemberPrivate *memberPrivate =
    (NPObjectMemberPrivate *)::JS_GetInstancePrivate(cx, obj,
                                                     &sNPObjectMemberClass,
                                                     nullptr);
  if (!memberPrivate) {
    NS_ERROR("no Ambiguous Member Private data!");
    return false;
  }

  switch (type) {
  case JSTYPE_VOID:
  case JSTYPE_STRING:
  case JSTYPE_NUMBER:
    vp.set(memberPrivate->fieldValue);
    if (vp.isObject()) {
      JS::Rooted<JSObject*> objVal(cx, &vp.toObject());
      return JS_DefaultValue(cx, objVal, type, vp);
    }
    return true;
  case JSTYPE_BOOLEAN:
  case JSTYPE_OBJECT:
    vp.set(memberPrivate->fieldValue);
    return true;
  case JSTYPE_FUNCTION:
    // Leave this to NPObjectMember_Call.
    return true;
  default:
    NS_ERROR("illegal operation on JSObject prototype object");
    return false;
  }
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
  JS::Rooted<JSObject*> memobj(cx, JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));
  NS_ENSURE_TRUE(memobj, false);

  NPObjectMemberPrivate *memberPrivate =
    (NPObjectMemberPrivate *)::JS_GetInstancePrivate(cx, memobj,
                                                     &sNPObjectMemberClass,
                                                     JS_ARGV(cx, vp));
  if (!memberPrivate || !memberPrivate->npobjWrapper)
    return false;

  NPObject *npobj = GetNPObject(cx, memberPrivate->npobjWrapper);
  if (!npobj) {
    ThrowJSException(cx, "Call on invalid member object");

    return false;
  }

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
  JS::Value *argv = JS_ARGV(cx, vp);
  for (i = 0; i < argc; ++i) {
    if (!JSValToNPVariant(memberPrivate->npp, cx, argv[i], npargs + i)) {
      ThrowJSException(cx, "Error converting jsvals to NPVariants!");

      if (npargs != npargs_buf) {
        PR_Free(npargs);
      }

      return false;
    }
  }


  NPVariant npv;
  bool ok;
  ok = npobj->_class->invoke(npobj, JSIdToNPIdentifier(memberPrivate->methodName),
                             npargs, argc, &npv);

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
      ThrowJSException(cx, "Error calling method on NPObject!");

    return false;
  }

  JS_SET_RVAL(cx, vp, NPVariantToJSVal(memberPrivate->npp, cx, &npv));

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

  // Our NPIdentifier is not always interned, so we must root it explicitly.
  JS_CallHeapIdTracer(trc, &memberPrivate->methodName, "NPObjectMemberPrivate.methodName");

  if (!JSVAL_IS_PRIMITIVE(memberPrivate->fieldValue)) {
    JS_CallHeapValueTracer(trc, &memberPrivate->fieldValue,
                           "NPObject Member => fieldValue");
  }

  // There's no strong reference from our private data to the
  // NPObject, so make sure to mark the NPObject wrapper to keep the
  // NPObject alive as long as this NPObjectMember is alive.
  if (memberPrivate->npobjWrapper) {
    JS_CallHeapObjectTracer(trc, &memberPrivate->npobjWrapper,
                            "NPObject Member => npobjWrapper");
  }
}
