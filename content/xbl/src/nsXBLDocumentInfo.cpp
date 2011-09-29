/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIScriptSecurityManager.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "mozilla/Services.h"
#include "xpcpublic.h"
 
static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

// An XBLDocumentInfo object has a special context associated with it which we can use to pre-compile 
// properties and methods of XBL bindings against.
class nsXBLDocGlobalObject : public nsIScriptGlobalObject,
                             public nsIScriptObjectPrincipal
{
public:
  nsXBLDocGlobalObject(nsIScriptGlobalObjectOwner *aGlobalObjectOwner);

  // nsISupports interface
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  
  // nsIScriptGlobalObject methods
  virtual nsresult EnsureScriptEnvironment(PRUint32 aLangID);
  virtual nsresult SetScriptContext(PRUint32 lang_id, nsIScriptContext *aContext);

  virtual nsIScriptContext *GetContext();
  virtual JSObject *GetGlobalJSObject();
  virtual void OnFinalize(JSObject* aObject);
  virtual void SetScriptsEnabled(bool aEnabled, bool aFireTimeouts);

  // nsIScriptObjectPrincipal methods
  virtual nsIPrincipal* GetPrincipal();

  static JSBool doCheckAccess(JSContext *cx, JSObject *obj, jsid id,
                              PRUint32 accessType);

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXBLDocGlobalObject,
                                           nsIScriptGlobalObject)

  void ClearGlobalObjectOwner();

protected:
  virtual ~nsXBLDocGlobalObject();

  void SetContext(nsIScriptContext *aContext);
  nsIScriptContext *GetScriptContext(PRUint32 language);
  void *GetScriptGlobal(PRUint32 language);

  nsCOMPtr<nsIScriptContext> mScriptContext;
  JSObject *mJSObject;    // XXX JS language rabies bigotry badness

  nsIScriptGlobalObjectOwner* mGlobalObjectOwner; // weak reference
  static JSClass gSharedGlobalClass;
};

JSBool
nsXBLDocGlobalObject::doCheckAccess(JSContext *cx, JSObject *obj, jsid id, PRUint32 accessType)
{
  nsIScriptSecurityManager *ssm = nsContentUtils::GetSecurityManager();
  if (!ssm) {
    ::JS_ReportError(cx, "Unable to verify access to a global object property.");
    return JS_FALSE;
  }

  // Make sure to actually operate on our object, and not some object further
  // down on the proto chain.
  while (JS_GET_CLASS(cx, obj) != &nsXBLDocGlobalObject::gSharedGlobalClass) {
    obj = ::JS_GetPrototype(cx, obj);
    if (!obj) {
      ::JS_ReportError(cx, "Invalid access to a global object property.");
      return JS_FALSE;
    }
  }

  nsresult rv = ssm->CheckPropertyAccess(cx, obj, JS_GET_CLASS(cx, obj)->name,
                                         id, accessType);
  return NS_SUCCEEDED(rv);
}

static JSBool
nsXBLDocGlobalObject_getProperty(JSContext *cx, JSObject *obj,
                                 jsid id, jsval *vp)
{
  return nsXBLDocGlobalObject::
    doCheckAccess(cx, obj, id, nsIXPCSecurityManager::ACCESS_GET_PROPERTY);
}

static JSBool
nsXBLDocGlobalObject_setProperty(JSContext *cx, JSObject *obj,
                                 jsid id, JSBool strict, jsval *vp)
{
  return nsXBLDocGlobalObject::
    doCheckAccess(cx, obj, id, nsIXPCSecurityManager::ACCESS_SET_PROPERTY);
}

static JSBool
nsXBLDocGlobalObject_checkAccess(JSContext *cx, JSObject *obj, jsid id,
                                 JSAccessMode mode, jsval *vp)
{
  PRUint32 translated;
  if (mode & JSACC_WRITE) {
    translated = nsIXPCSecurityManager::ACCESS_SET_PROPERTY;
  } else {
    translated = nsIXPCSecurityManager::ACCESS_GET_PROPERTY;
  }

  return nsXBLDocGlobalObject::
    doCheckAccess(cx, obj, id, translated);
}

static void
nsXBLDocGlobalObject_finalize(JSContext *cx, JSObject *obj)
{
  nsISupports *nativeThis = (nsISupports*)JS_GetPrivate(cx, obj);

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(nativeThis));

  if (sgo)
    sgo->OnFinalize(obj);

  // The addref was part of JSObject construction
  NS_RELEASE(nativeThis);
}

static JSBool
nsXBLDocGlobalObject_resolve(JSContext *cx, JSObject *obj, jsid id)
{
  JSBool did_resolve = JS_FALSE;
  return JS_ResolveStandardClass(cx, obj, id, &did_resolve);
}


JSClass nsXBLDocGlobalObject::gSharedGlobalClass = {
    "nsXBLPrototypeScript compilation scope",
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_PropertyStub,
    nsXBLDocGlobalObject_getProperty, nsXBLDocGlobalObject_setProperty,
    JS_EnumerateStub, nsXBLDocGlobalObject_resolve,
    JS_ConvertStub, nsXBLDocGlobalObject_finalize,
    NULL, nsXBLDocGlobalObject_checkAccess
};

//----------------------------------------------------------------------
//
// nsXBLDocGlobalObject
//

nsXBLDocGlobalObject::nsXBLDocGlobalObject(nsIScriptGlobalObjectOwner *aGlobalObjectOwner)
    : mJSObject(nsnull),
      mGlobalObjectOwner(aGlobalObjectOwner) // weak reference
{
}


nsXBLDocGlobalObject::~nsXBLDocGlobalObject()
{}


NS_IMPL_CYCLE_COLLECTION_1(nsXBLDocGlobalObject, mScriptContext)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXBLDocGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
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
    PRUint32 column = report->uctokenptr - report->uclinebuf;

    errorObject->Init
         (reinterpret_cast<const PRUnichar*>(report->ucmessage),
          NS_ConvertUTF8toUTF16(report->filename).get(),
          reinterpret_cast<const PRUnichar*>(report->uclinebuf),
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

void
nsXBLDocGlobalObject::SetContext(nsIScriptContext *aScriptContext)
{
  if (!aScriptContext) {
    mScriptContext = nsnull;
    return;
  }
  NS_ASSERTION(aScriptContext->GetScriptTypeID() ==
                                        nsIProgrammingLanguage::JAVASCRIPT,
               "xbl is not multi-language");
  aScriptContext->WillInitializeContext();
  // NOTE: We init this context with a NULL global, so we automatically
  // hook up to the existing nsIScriptGlobalObject global setup by
  // nsGlobalWindow.
  nsresult rv;
  rv = aScriptContext->InitContext();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Script Language's InitContext failed");
  aScriptContext->SetGCOnDestruction(PR_FALSE);
  aScriptContext->DidInitializeContext();
  // and we set up our global manually
  mScriptContext = aScriptContext;
}

nsresult
nsXBLDocGlobalObject::SetScriptContext(PRUint32 lang_id, nsIScriptContext *aContext)
{
  NS_ASSERTION(lang_id == nsIProgrammingLanguage::JAVASCRIPT, "Only JS allowed!");
  SetContext(aContext);
  return NS_OK;
}

nsIScriptContext *
nsXBLDocGlobalObject::GetScriptContext(PRUint32 language)
{
  // This impl still assumes JS
  NS_ENSURE_TRUE(language==nsIProgrammingLanguage::JAVASCRIPT, nsnull);
  return GetContext();
}

void *
nsXBLDocGlobalObject::GetScriptGlobal(PRUint32 language)
{
  // This impl still assumes JS
  NS_ENSURE_TRUE(language==nsIProgrammingLanguage::JAVASCRIPT, nsnull);
  return GetGlobalJSObject();
}

nsresult
nsXBLDocGlobalObject::EnsureScriptEnvironment(PRUint32 aLangID)
{
  if (aLangID != nsIProgrammingLanguage::JAVASCRIPT) {
    NS_WARNING("XBL still JS only");
    return NS_ERROR_INVALID_ARG;
  }
  if (mScriptContext)
    return NS_OK; // already initialized for this lang
  nsCOMPtr<nsIDOMScriptObjectFactory> factory = do_GetService(kDOMScriptObjectFactoryCID);
  NS_ENSURE_TRUE(factory, nsnull);

  nsresult rv;

  nsCOMPtr<nsIScriptRuntime> scriptRuntime;
  rv = NS_GetScriptRuntimeByID(aLangID, getter_AddRefs(scriptRuntime));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIScriptContext> newCtx;
  rv = scriptRuntime->CreateContext(getter_AddRefs(newCtx));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetScriptContext(aLangID, newCtx);

  JSContext *cx = mScriptContext->GetNativeContext();
  JSAutoRequest ar(cx);

  // nsJSEnvironment set the error reporter to NS_ScriptErrorReporter so
  // we must apparently override that with our own (although it isn't clear 
  // why - see bug 339647)
  JS_SetErrorReporter(cx, XBL_ProtoErrorReporter);

  nsIPrincipal *principal = GetPrincipal();
  JSCompartment *compartment;

  rv = xpc_CreateGlobalObject(cx, &gSharedGlobalClass, principal, nsnull,
                              false, &mJSObject, &compartment);
  NS_ENSURE_SUCCESS(rv, nsnull);

  ::JS_SetGlobalObject(cx, mJSObject);

  // Add an owning reference from JS back to us. This'll be
  // released when the JSObject is finalized.
  ::JS_SetPrivate(cx, mJSObject, this);
  NS_ADDREF(this);
  return NS_OK;
}

nsIScriptContext *
nsXBLDocGlobalObject::GetContext()
{
  // This whole fragile mess is predicated on the fact that
  // GetContext() will be called before GetScriptObject() is.
  if (! mScriptContext) {
    nsresult rv = EnsureScriptEnvironment(nsIProgrammingLanguage::JAVASCRIPT);
    // JS is builtin so we make noise if it fails to initialize.
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to setup JS!?");
    NS_ENSURE_SUCCESS(rv, nsnull);
    NS_ASSERTION(mScriptContext, "Failed to find a script context!?");
  }
  return mScriptContext;
}

void
nsXBLDocGlobalObject::ClearGlobalObjectOwner()
{
  mGlobalObjectOwner = nsnull;
}

JSObject *
nsXBLDocGlobalObject::GetGlobalJSObject()
{
  // The prototype document has its own special secret script object
  // that can be used to compile scripts and event handlers.

  if (!mScriptContext)
    return nsnull;

  JSContext* cx = mScriptContext->GetNativeContext();
  if (!cx)
    return nsnull;

  JSObject *ret = ::JS_GetGlobalObject(cx);
  NS_ASSERTION(mJSObject == ret, "How did this magic switch happen?");
  return ret;
}

void
nsXBLDocGlobalObject::OnFinalize(JSObject* aObject)
{
  NS_ASSERTION(aObject == mJSObject, "Wrong object finalized!");
  mJSObject = NULL;
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
    return nsnull;
  }

  nsRefPtr<nsXBLDocumentInfo> docInfo =
    static_cast<nsXBLDocumentInfo*>(mGlobalObjectOwner);

  nsCOMPtr<nsIDocument> document = docInfo->GetDocument();
  if (!document)
    return NULL;

  return document->NodePrincipal();
}

static bool IsChromeURI(nsIURI* aURI)
{
  bool isChrome = false;
  if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)))
      return isChrome;
  return PR_FALSE;
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

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXBLDocumentInfo)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXBLDocumentInfo)
  if (tmp->mBindingTable) {
    tmp->mBindingTable->Enumerate(UnlinkProtoJSObjects, nsnull);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mGlobalObject)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXBLDocumentInfo)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocument)
  if (tmp->mBindingTable) {
    tmp->mBindingTable->Enumerate(TraverseProtos, &cb);
  }
  cb.NoteXPCOMChild(static_cast<nsIScriptGlobalObject*>(tmp->mGlobalObject));
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsXBLDocumentInfo)
  if (tmp->mBindingTable) {
    ProtoTracer closure = { aCallback, aClosure };
    tmp->mBindingTable->Enumerate(TraceProtos, &closure);
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXBLDocumentInfo)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObjectOwner)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptGlobalObjectOwner)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXBLDocumentInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXBLDocumentInfo)

nsXBLDocumentInfo::nsXBLDocumentInfo(nsIDocument* aDocument)
  : mDocument(aDocument),
    mScriptAccess(PR_TRUE),
    mIsChrome(PR_FALSE),
    mBindingTable(nsnull),
    mFirstBinding(nsnull)
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
    mIsChrome = PR_TRUE;
  }
}

nsXBLDocumentInfo::~nsXBLDocumentInfo()
{
  /* destructor code */
  if (mGlobalObject) {
    // remove circular reference
    mGlobalObject->SetScriptContext(nsIProgrammingLanguage::JAVASCRIPT, nsnull);
    mGlobalObject->ClearGlobalObjectOwner(); // just in case
  }
  if (mBindingTable) {
    NS_DROP_JS_OBJECTS(this, nsXBLDocumentInfo);
    delete mBindingTable;
  }
}

nsXBLPrototypeBinding*
nsXBLDocumentInfo::GetPrototypeBinding(const nsACString& aRef)
{
  if (!mBindingTable)
    return NULL;

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
  return PR_TRUE;
}

nsresult
nsXBLDocumentInfo::SetPrototypeBinding(const nsACString& aRef, nsXBLPrototypeBinding* aBinding)
{
  if (!mBindingTable) {
    mBindingTable = new nsObjectHashtable(nsnull, nsnull, DeletePrototypeBinding, nsnull);

    NS_HOLD_JS_OBJECTS(this, nsXBLDocumentInfo);
  }

  const nsPromiseFlatCString& flat = PromiseFlatCString(aRef);
  nsCStringKey key(flat.get());
  NS_ENSURE_STATE(!mBindingTable->Get(&key));
  mBindingTable->Put(&key, aBinding);

  return NS_OK;
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
  return PR_TRUE;
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
      return nsnull;

    mGlobalObject = global;
  }

  return mGlobalObject;
}

nsXBLDocumentInfo* NS_NewXBLDocumentInfo(nsIDocument* aDocument)
{
  NS_PRECONDITION(aDocument, "Must have a document!");

  nsXBLDocumentInfo* result;

  result = new nsXBLDocumentInfo(aDocument);
  NS_ADDREF(result);
  return result;
}
