/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "nsXBLDocumentInfo.h"
#include "nsHashtable.h"
#include "nsIDocument.h"
#include "nsXBLPrototypeBinding.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIScriptRuntime.h"
#include "nsIDOMScriptObjectFactory.h"
#include "jsapi.h"
#include "nsIURI.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIChromeRegistry.h"
#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "mozilla/Services.h"
#include "xpcpublic.h"
#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/StartupCacheUtils.h"
#include "nsCCUncollectableMarker.h"
#include "mozilla/dom/BindingUtils.h"

using namespace mozilla::scache;
using namespace mozilla;

static const char kXBLCachePrefix[] = "xblcache";

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

// An XBLDocumentInfo object has a special context associated with it which we can use to pre-compile 
// properties and methods of XBL bindings against.
class nsXBLDocGlobalObject : public nsIScriptGlobalObject
{
public:
  nsXBLDocGlobalObject(nsXBLDocumentInfo *aGlobalObjectOwner);

  // nsISupports interface
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsIGlobalObject methods
  virtual JSObject *GetGlobalJSObject();
  
  // nsIScriptGlobalObject methods
  virtual nsresult EnsureScriptEnvironment();
  void ClearScriptContext()
  {
    mScriptContext = nullptr;
  }

  virtual nsIScriptContext *GetContext();
  virtual void OnFinalize(JSObject* aObject);
  virtual void SetScriptsEnabled(bool aEnabled, bool aFireTimeouts);

  // nsIScriptObjectPrincipal methods
  virtual nsIPrincipal* GetPrincipal();

  static JSBool doCheckAccess(JSContext *cx, JS::Handle<JSObject*> obj,
                              JS::Handle<jsid> id, uint32_t accessType);

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXBLDocGlobalObject,
                                           nsIScriptGlobalObject)

  void ClearGlobalObjectOwner();

  void UnmarkScriptContext();

protected:
  virtual ~nsXBLDocGlobalObject();

  nsIScriptContext *GetScriptContext();

  nsCOMPtr<nsIScriptContext> mScriptContext;
  JSObject *mJSObject;

  nsXBLDocumentInfo* mGlobalObjectOwner; // weak reference
  static JSClass gSharedGlobalClass;
};

JSBool
nsXBLDocGlobalObject::doCheckAccess(JSContext *cx, JS::Handle<JSObject*> obj,
                                    JS::Handle<jsid> id, uint32_t accessType)
{
  nsIScriptSecurityManager *ssm = nsContentUtils::GetSecurityManager();
  if (!ssm) {
    ::JS_ReportError(cx, "Unable to verify access to a global object property.");
    return JS_FALSE;
  }

  // Make sure to actually operate on our object, and not some object further
  // down on the proto chain.
  JS::Rooted<JSObject*> base(cx, obj);
  while (JS_GetClass(base) != &nsXBLDocGlobalObject::gSharedGlobalClass) {
    if (!::JS_GetPrototype(cx, base, base.address())) {
      return JS_FALSE;
    }
    if (!base) {
      ::JS_ReportError(cx, "Invalid access to a global object property.");
      return JS_FALSE;
    }
  }

  nsresult rv = ssm->CheckPropertyAccess(cx, base, JS_GetClass(base)->name,
                                         id, accessType);
  return NS_SUCCEEDED(rv);
}

static JSBool
nsXBLDocGlobalObject_getProperty(JSContext *cx, JSHandleObject obj,
                                 JSHandleId id, JSMutableHandleValue vp)
{
  return nsXBLDocGlobalObject::
    doCheckAccess(cx, obj, id, nsIXPCSecurityManager::ACCESS_GET_PROPERTY);
}

static JSBool
nsXBLDocGlobalObject_setProperty(JSContext *cx, JSHandleObject obj,
                                 JSHandleId id, JSBool strict, JSMutableHandleValue vp)
{
  return nsXBLDocGlobalObject::
    doCheckAccess(cx, obj, id, nsIXPCSecurityManager::ACCESS_SET_PROPERTY);
}

static JSBool
nsXBLDocGlobalObject_checkAccess(JSContext *cx, JSHandleObject obj, JSHandleId id,
                                 JSAccessMode mode, JSMutableHandleValue vp)
{
  uint32_t translated;
  if (mode & JSACC_WRITE) {
    translated = nsIXPCSecurityManager::ACCESS_SET_PROPERTY;
  } else {
    translated = nsIXPCSecurityManager::ACCESS_GET_PROPERTY;
  }

  return nsXBLDocGlobalObject::
    doCheckAccess(cx, obj, id, translated);
}

static void
nsXBLDocGlobalObject_finalize(JSFreeOp *fop, JSObject *obj)
{
  nsISupports *nativeThis = (nsISupports*)JS_GetPrivate(obj);

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(nativeThis));

  if (sgo)
    sgo->OnFinalize(obj);

  // The addref was part of JSObject construction
  NS_RELEASE(nativeThis);
}

static JSBool
nsXBLDocGlobalObject_resolve(JSContext *cx, JSHandleObject obj, JSHandleId id)
{
  JSBool did_resolve = JS_FALSE;
  return JS_ResolveStandardClass(cx, obj, id, &did_resolve);
}


JSClass nsXBLDocGlobalObject::gSharedGlobalClass = {
    "nsXBLPrototypeScript compilation scope",
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS |
    JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(0),
    JS_PropertyStub,  JS_PropertyStub,
    nsXBLDocGlobalObject_getProperty, nsXBLDocGlobalObject_setProperty,
    JS_EnumerateStub, nsXBLDocGlobalObject_resolve,
    JS_ConvertStub, nsXBLDocGlobalObject_finalize,
    nsXBLDocGlobalObject_checkAccess, nullptr, nullptr, nullptr,
    nullptr
};

//----------------------------------------------------------------------
//
// nsXBLDocGlobalObject
//

nsXBLDocGlobalObject::nsXBLDocGlobalObject(nsXBLDocumentInfo *aGlobalObjectOwner)
    : mJSObject(nullptr),
      mGlobalObjectOwner(aGlobalObjectOwner) // weak reference
{
}


nsXBLDocGlobalObject::~nsXBLDocGlobalObject()
{}


NS_IMPL_CYCLE_COLLECTION_1(nsXBLDocGlobalObject, mScriptContext)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXBLDocGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptGlobalObject)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXBLDocGlobalObject)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXBLDocGlobalObject)

void
XBL_ProtoErrorReporter(JSContext *cx,
                       const char *message,
                       JSErrorReport *report)
{
  // Make an nsIScriptError and populate it with information from
  // this error.
  nsCOMPtr<nsIScriptError>
    errorObject(do_CreateInstance("@mozilla.org/scripterror;1"));
  nsCOMPtr<nsIConsoleService>
    consoleService(do_GetService("@mozilla.org/consoleservice;1"));

  if (errorObject && consoleService) {
    uint32_t column = report->uctokenptr - report->uclinebuf;

    const PRUnichar* ucmessage =
      static_cast<const PRUnichar*>(report->ucmessage);
    const PRUnichar* uclinebuf =
      static_cast<const PRUnichar*>(report->uclinebuf);

    errorObject->Init
         (ucmessage ? nsDependentString(ucmessage) : EmptyString(),
          NS_ConvertUTF8toUTF16(report->filename),
          uclinebuf ? nsDependentString(uclinebuf) : EmptyString(),
          report->lineno, column, report->flags,
          "xbl javascript"
          );
    consoleService->LogMessage(errorObject);
  }
}

//----------------------------------------------------------------------
//
// nsIScriptGlobalObject methods
//

nsIScriptContext *
nsXBLDocGlobalObject::GetScriptContext()
{
  return GetContext();
}

nsresult
nsXBLDocGlobalObject::EnsureScriptEnvironment()
{
  if (mScriptContext) {
    // Already initialized.
    return NS_OK;
  }

  nsCOMPtr<nsIScriptRuntime> scriptRuntime;
  NS_GetJSRuntime(getter_AddRefs(scriptRuntime));
  NS_ENSURE_TRUE(scriptRuntime, NS_OK);

  nsCOMPtr<nsIScriptContext> newCtx = scriptRuntime->CreateContext(false, nullptr);
  MOZ_ASSERT(newCtx);

  newCtx->WillInitializeContext();
  // NOTE: We init this context with a nullptr global, so we automatically
  // hook up to the existing nsIScriptGlobalObject global setup by
  // nsGlobalWindow.
  DebugOnly<nsresult> rv = newCtx->InitContext();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Script Language's InitContext failed");
  newCtx->DidInitializeContext();

  mScriptContext = newCtx;

  AutoPushJSContext cx(mScriptContext->GetNativeContext());
  JSAutoRequest ar(cx);

  // nsJSEnvironment set the error reporter to NS_ScriptErrorReporter so
  // we must apparently override that with our own (although it isn't clear 
  // why - see bug 339647)
  JS_SetErrorReporter(cx, XBL_ProtoErrorReporter);

  mJSObject = JS_NewGlobalObject(cx, &gSharedGlobalClass,
                                 nsJSPrincipals::get(GetPrincipal()),
                                 JS::SystemZone);
  if (!mJSObject)
      return NS_OK;

  // Set the location information for the new global, so that tools like
  // about:memory may use that information
  nsIURI *ownerURI = mGlobalObjectOwner->DocumentURI();
  xpc::SetLocationForGlobal(mJSObject, ownerURI);

  ::JS_SetGlobalObject(cx, mJSObject);

  // Add an owning reference from JS back to us. This'll be
  // released when the JSObject is finalized.
  ::JS_SetPrivate(mJSObject, this);
  NS_ADDREF(this);
  return NS_OK;
}

nsIScriptContext *
nsXBLDocGlobalObject::GetContext()
{
  // This whole fragile mess is predicated on the fact that
  // GetContext() will be called before GetScriptObject() is.
  if (! mScriptContext) {
    nsresult rv = EnsureScriptEnvironment();
    // JS is builtin so we make noise if it fails to initialize.
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to setup JS!?");
    NS_ENSURE_SUCCESS(rv, nullptr);
    NS_ASSERTION(mScriptContext, "Failed to find a script context!?");
  }
  return mScriptContext;
}

void
nsXBLDocGlobalObject::ClearGlobalObjectOwner()
{
  mGlobalObjectOwner = nullptr;
}

void
nsXBLDocGlobalObject::UnmarkScriptContext()
{
  if (mScriptContext) {
    xpc_UnmarkGrayObject(mScriptContext->GetNativeGlobal());
  }
}

JSObject *
nsXBLDocGlobalObject::GetGlobalJSObject()
{
  // The prototype document has its own special secret script object
  // that can be used to compile scripts and event handlers.

  if (!mScriptContext)
    return nullptr;

  JSContext* cx = mScriptContext->GetNativeContext();
  if (!cx)
    return nullptr;

  JSObject *ret = ::JS_GetGlobalObject(cx);
  NS_ASSERTION(mJSObject == ret, "How did this magic switch happen?");
  return ret;
}

void
nsXBLDocGlobalObject::OnFinalize(JSObject* aObject)
{
  NS_ASSERTION(aObject == mJSObject, "Wrong object finalized!");
  mJSObject = nullptr;
}

void
nsXBLDocGlobalObject::SetScriptsEnabled(bool aEnabled, bool aFireTimeouts)
{
    // We don't care...
}

//----------------------------------------------------------------------
//
// nsIScriptObjectPrincipal methods
//

nsIPrincipal*
nsXBLDocGlobalObject::GetPrincipal()
{
  if (!mGlobalObjectOwner) {
    // XXXbz this should really save the principal when
    // ClearGlobalObjectOwner() happens.
    return nullptr;
  }

  nsRefPtr<nsXBLDocumentInfo> docInfo =
    static_cast<nsXBLDocumentInfo*>(mGlobalObjectOwner);

  nsCOMPtr<nsIDocument> document = docInfo->GetDocument();
  if (!document)
    return nullptr;

  return document->NodePrincipal();
}

static bool IsChromeURI(nsIURI* aURI)
{
  bool isChrome = false;
  if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)))
      return isChrome;
  return false;
}

/* Implementation file */

static bool
TraverseProtos(nsHashKey *aKey, void *aData, void* aClosure)
{
  nsCycleCollectionTraversalCallback *cb = 
    static_cast<nsCycleCollectionTraversalCallback*>(aClosure);
  nsXBLPrototypeBinding *proto = static_cast<nsXBLPrototypeBinding*>(aData);
  proto->Traverse(*cb);
  return kHashEnumerateNext;
}

static bool
UnlinkProtoJSObjects(nsHashKey *aKey, void *aData, void* aClosure)
{
  nsXBLPrototypeBinding *proto = static_cast<nsXBLPrototypeBinding*>(aData);
  proto->UnlinkJSObjects();
  return kHashEnumerateNext;
}

struct ProtoTracer
{
  TraceCallback mCallback;
  void *mClosure;
};

static bool
TraceProtos(nsHashKey *aKey, void *aData, void* aClosure)
{
  ProtoTracer* closure = static_cast<ProtoTracer*>(aClosure);
  nsXBLPrototypeBinding *proto = static_cast<nsXBLPrototypeBinding*>(aData);
  proto->Trace(closure->mCallback, closure->mClosure);
  return kHashEnumerateNext;
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXBLDocumentInfo)
  if (tmp->mBindingTable) {
    tmp->mBindingTable->Enumerate(UnlinkProtoJSObjects, nullptr);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobalObject)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXBLDocumentInfo)
  if (tmp->mDocument &&
      nsCCUncollectableMarker::InGeneration(cb, tmp->mDocument->GetMarkedCCGeneration())) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
    return NS_SUCCESS_INTERRUPTED_TRAVERSE;
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  if (tmp->mBindingTable) {
    tmp->mBindingTable->Enumerate(TraverseProtos, &cb);
  }
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mGlobalObject");
  cb.NoteXPCOMChild(static_cast<nsIScriptGlobalObject*>(tmp->mGlobalObject));
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsXBLDocumentInfo)
  if (tmp->mBindingTable) {
    ProtoTracer closure = { aCallback, aClosure };
    tmp->mBindingTable->Enumerate(TraceProtos, &closure);
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

static void
UnmarkXBLJSObject(void* aP, const char* aName, void* aClosure)
{
  xpc_UnmarkGrayObject(static_cast<JSObject*>(aP));
}

static bool
UnmarkProtos(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsXBLPrototypeBinding* proto = static_cast<nsXBLPrototypeBinding*>(aData);
  proto->Trace(UnmarkXBLJSObject, nullptr);
  return kHashEnumerateNext;
}

void
nsXBLDocumentInfo::MarkInCCGeneration(uint32_t aGeneration)
{
  if (mDocument) {
    mDocument->MarkUncollectableForCCGeneration(aGeneration);
  }
  // Unmark any JS we hold
  if (mBindingTable) {
    mBindingTable->Enumerate(UnmarkProtos, nullptr);
  }
  if (mGlobalObject) {
    mGlobalObject->UnmarkScriptContext();
  }
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXBLDocumentInfo)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObjectOwner)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptGlobalObjectOwner)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXBLDocumentInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXBLDocumentInfo)

nsXBLDocumentInfo::nsXBLDocumentInfo(nsIDocument* aDocument)
  : mDocument(aDocument),
    mScriptAccess(true),
    mIsChrome(false),
    mBindingTable(nullptr),
    mFirstBinding(nullptr)
{
  nsIURI* uri = aDocument->GetDocumentURI();
  if (IsChromeURI(uri)) {
    // Cache whether or not this chrome XBL can execute scripts.
    nsCOMPtr<nsIXULChromeRegistry> reg =
      mozilla::services::GetXULChromeRegistryService();
    if (reg) {
      bool allow = true;
      reg->AllowScriptsForPackage(uri, &allow);
      mScriptAccess = allow;
    }
    mIsChrome = true;
  }
}

nsXBLDocumentInfo::~nsXBLDocumentInfo()
{
  /* destructor code */
  if (mGlobalObject) {
    // remove circular reference
    mGlobalObject->ClearScriptContext();
    mGlobalObject->ClearGlobalObjectOwner(); // just in case
  }
  if (mBindingTable) {
    delete mBindingTable;
    mBindingTable = nullptr;
    NS_DROP_JS_OBJECTS(this, nsXBLDocumentInfo);
  }
}

nsXBLPrototypeBinding*
nsXBLDocumentInfo::GetPrototypeBinding(const nsACString& aRef)
{
  if (!mBindingTable)
    return nullptr;

  if (aRef.IsEmpty()) {
    // Return our first binding
    return mFirstBinding;
  }

  const nsPromiseFlatCString& flat = PromiseFlatCString(aRef);
  nsCStringKey key(flat.get());
  return static_cast<nsXBLPrototypeBinding*>(mBindingTable->Get(&key));
}

static bool
DeletePrototypeBinding(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsXBLPrototypeBinding* binding = static_cast<nsXBLPrototypeBinding*>(aData);
  delete binding;
  return true;
}

nsresult
nsXBLDocumentInfo::SetPrototypeBinding(const nsACString& aRef, nsXBLPrototypeBinding* aBinding)
{
  if (!mBindingTable) {
    mBindingTable = new nsObjectHashtable(nullptr, nullptr, DeletePrototypeBinding, nullptr);

    NS_HOLD_JS_OBJECTS(this, nsXBLDocumentInfo);
  }

  const nsPromiseFlatCString& flat = PromiseFlatCString(aRef);
  nsCStringKey key(flat.get());
  NS_ENSURE_STATE(!mBindingTable->Get(&key));
  mBindingTable->Put(&key, aBinding);

  return NS_OK;
}

void
nsXBLDocumentInfo::RemovePrototypeBinding(const nsACString& aRef)
{
  if (mBindingTable) {
    // Use a flat string to avoid making a copy.
    const nsPromiseFlatCString& flat = PromiseFlatCString(aRef);
    nsCStringKey key(flat);
    mBindingTable->Remove(&key);
  }
}

// Callback to enumerate over the bindings from this document and write them
// out to the cache.
bool
WriteBinding(nsHashKey *aKey, void *aData, void* aClosure)
{
  nsXBLPrototypeBinding* binding = static_cast<nsXBLPrototypeBinding *>(aData);
  binding->Write((nsIObjectOutputStream*)aClosure);

  return kHashEnumerateNext;
}

// static
nsresult
nsXBLDocumentInfo::ReadPrototypeBindings(nsIURI* aURI, nsXBLDocumentInfo** aDocInfo)
{
  *aDocInfo = nullptr;

  nsAutoCString spec(kXBLCachePrefix);
  nsresult rv = PathifyURI(aURI, spec);
  NS_ENSURE_SUCCESS(rv, rv);

  StartupCache* startupCache = StartupCache::GetSingleton();
  NS_ENSURE_TRUE(startupCache, NS_ERROR_FAILURE);

  nsAutoArrayPtr<char> buf;
  uint32_t len;
  rv = startupCache->GetBuffer(spec.get(), getter_Transfers(buf), &len);
  // GetBuffer will fail if the binding is not in the cache.
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIObjectInputStream> stream;
  rv = NewObjectInputStreamFromBuffer(buf, len, getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);
  buf.forget();

  // The file compatibility.ini stores the build id. This is checked in
  // nsAppRunner.cpp and will delete the cache if a different build is
  // present. However, we check that the version matches here to be safe. 
  uint32_t version;
  rv = stream->Read32(&version);
  NS_ENSURE_SUCCESS(rv, rv);
  if (version != XBLBinding_Serialize_Version) {
    // The version that exists is different than expected, likely created with a
    // different build, so invalidate the cache.
    startupCache->InvalidateCache();
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIPrincipal> principal;
  nsContentUtils::GetSecurityManager()->
    GetSystemPrincipal(getter_AddRefs(principal));

  nsCOMPtr<nsIDOMDocument> domdoc;
  rv = NS_NewXBLDocument(getter_AddRefs(domdoc), aURI, nullptr, principal);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
  NS_ASSERTION(doc, "Must have a document!");
  nsRefPtr<nsXBLDocumentInfo> docInfo = new nsXBLDocumentInfo(doc);

  while (1) {
    uint8_t flags;
    nsresult rv = stream->Read8(&flags);
    NS_ENSURE_SUCCESS(rv, rv);
    if (flags == XBLBinding_Serialize_NoMoreBindings)
      break;

    nsXBLPrototypeBinding* binding = new nsXBLPrototypeBinding();
    rv = binding->Read(stream, docInfo, doc, flags);
    if (NS_FAILED(rv)) {
      delete binding;
      return rv;
    }
  }

  docInfo.swap(*aDocInfo);
  return NS_OK;
}

nsresult
nsXBLDocumentInfo::WritePrototypeBindings()
{
  // Only write out bindings with the system principal
  if (!nsContentUtils::IsSystemPrincipal(mDocument->NodePrincipal()))
    return NS_OK;

  nsAutoCString spec(kXBLCachePrefix);
  nsresult rv = PathifyURI(DocumentURI(), spec);
  NS_ENSURE_SUCCESS(rv, rv);

  StartupCache* startupCache = StartupCache::GetSingleton();
  NS_ENSURE_TRUE(startupCache, rv);

  nsCOMPtr<nsIObjectOutputStream> stream;
  nsCOMPtr<nsIStorageStream> storageStream;
  rv = NewObjectOutputWrappedStorageStream(getter_AddRefs(stream),
                                           getter_AddRefs(storageStream),
                                           true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->Write32(XBLBinding_Serialize_Version);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mBindingTable)
    mBindingTable->Enumerate(WriteBinding, stream);

  // write a end marker at the end
  rv = stream->Write8(XBLBinding_Serialize_NoMoreBindings);
  NS_ENSURE_SUCCESS(rv, rv);

  stream->Close();
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t len;
  nsAutoArrayPtr<char> buf;
  rv = NewBufferFromStorageStream(storageStream, getter_Transfers(buf), &len);
  NS_ENSURE_SUCCESS(rv, rv);

  return startupCache->PutBuffer(spec.get(), buf, len);
}

void
nsXBLDocumentInfo::SetFirstPrototypeBinding(nsXBLPrototypeBinding* aBinding)
{
  mFirstBinding = aBinding;
}

bool FlushScopedSkinSheets(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsXBLPrototypeBinding* proto = (nsXBLPrototypeBinding*)aData;
  proto->FlushSkinSheets();
  return true;
}

void
nsXBLDocumentInfo::FlushSkinStylesheets()
{
  if (mBindingTable)
    mBindingTable->Enumerate(FlushScopedSkinSheets);
}

//----------------------------------------------------------------------
//
// nsIScriptGlobalObjectOwner methods
//

nsIScriptGlobalObject*
nsXBLDocumentInfo::GetScriptGlobalObject()
{
  if (!mGlobalObject) {
    nsXBLDocGlobalObject *global = new nsXBLDocGlobalObject(this);
    if (!global)
      return nullptr;

    mGlobalObject = global;
  }

  return mGlobalObject;
}
