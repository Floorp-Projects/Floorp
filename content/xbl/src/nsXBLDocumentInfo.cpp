/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIDOMScriptObjectFactory.h"
#include "jsapi.h"
#include "nsIURI.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIChromeRegistry.h"
#include "nsIPrincipal.h"

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

// An XBLDocumentInfo object has a special context associated with it which we can use to pre-compile 
// properties and methods of XBL bindings against.
class nsXBLDocGlobalObject : public nsIScriptGlobalObject,
                             public nsIScriptObjectPrincipal
{
public:
  nsXBLDocGlobalObject();

  // nsISupports interface
  NS_DECL_ISUPPORTS
  
  // nsIScriptGlobalObject methods
  virtual void SetContext(nsIScriptContext *aContext);
  virtual nsIScriptContext *GetContext();
  virtual nsresult SetNewDocument(nsIDOMDocument *aDocument,
                                  PRBool aRemoveEventListeners,
                                  PRBool aClearScope);
  virtual void SetDocShell(nsIDocShell *aDocShell);
  virtual nsIDocShell *GetDocShell();
  virtual void SetOpenerWindow(nsIDOMWindowInternal *aOpener);
  virtual void SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner);
  virtual nsIScriptGlobalObjectOwner *GetGlobalObjectOwner();
  virtual nsresult HandleDOMEvent(nsIPresContext* aPresContext, 
                                  nsEvent* aEvent, 
                                  nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);
  virtual JSObject *GetGlobalJSObject();
  virtual void OnFinalize(JSObject *aObject);
  virtual void SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts);

  // nsIScriptObjectPrincipal methods
  virtual nsIPrincipal* GetPrincipal();
    
protected:
  virtual ~nsXBLDocGlobalObject();

  nsCOMPtr<nsIScriptContext> mScriptContext;
  JSObject *mJSObject;    // XXX JS language rabies bigotry badness

  nsIScriptGlobalObjectOwner* mGlobalObjectOwner; // weak reference
  static JSClass gSharedGlobalClass;
};

void PR_CALLBACK nsXBLDocGlobalObject_finalize(JSContext *cx, JSObject *obj)
{
  nsISupports *nativeThis = (nsISupports*)JS_GetPrivate(cx, obj);

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(nativeThis));

  if (sgo)
      sgo->OnFinalize(obj);

  // The addref was part of JSObject construction
  NS_RELEASE(nativeThis);
}


JSBool PR_CALLBACK nsXBLDocGlobalObject_resolve(JSContext *cx, JSObject *obj, jsval id)
{
  JSBool did_resolve = JS_FALSE;
  return JS_ResolveStandardClass(cx, obj, id, &did_resolve);
}


JSClass nsXBLDocGlobalObject::gSharedGlobalClass = {
    "nsXBLPrototypeScript compilation scope",
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, nsXBLDocGlobalObject_resolve,  JS_ConvertStub,
    nsXBLDocGlobalObject_finalize
};

//----------------------------------------------------------------------
//
// nsXBLDocGlobalObject
//

nsXBLDocGlobalObject::nsXBLDocGlobalObject()
    : mJSObject(nsnull),
      mGlobalObjectOwner(nsnull)
{
}


nsXBLDocGlobalObject::~nsXBLDocGlobalObject()
{}


NS_IMPL_ISUPPORTS2(nsXBLDocGlobalObject, nsIScriptGlobalObject, nsIScriptObjectPrincipal)

void JS_DLL_CALLBACK
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
         (NS_REINTERPRET_CAST(const PRUnichar*, report->ucmessage),
          NS_ConvertUTF8toUCS2(report->filename).get(),
          NS_REINTERPRET_CAST(const PRUnichar*, report->uclinebuf),
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
nsXBLDocGlobalObject::SetContext(nsIScriptContext *aContext)
{
  mScriptContext = aContext;
  if (mScriptContext) {
    JSContext* cx = (JSContext *)mScriptContext->GetNativeContext();
    JS_SetErrorReporter(cx, XBL_ProtoErrorReporter);
  }
}


nsIScriptContext *
nsXBLDocGlobalObject::GetContext()
{
  // This whole fragile mess is predicated on the fact that
  // GetContext() will be called before GetScriptObject() is.
  if (! mScriptContext) {
    nsCOMPtr<nsIDOMScriptObjectFactory> factory = do_GetService(kDOMScriptObjectFactoryCID);
    NS_ENSURE_TRUE(factory, nsnull);

    nsresult rv =  factory->NewScriptContext(nsnull, getter_AddRefs(mScriptContext));
    if (NS_FAILED(rv))
        return nsnull;

    JSContext *cx = (JSContext *)mScriptContext->GetNativeContext();

    JS_SetErrorReporter(cx, XBL_ProtoErrorReporter);
    mJSObject = ::JS_NewObject(cx, &gSharedGlobalClass, nsnull, nsnull);
    if (!mJSObject)
        return nsnull;

    ::JS_SetGlobalObject(cx, mJSObject);

    // Add an owning reference from JS back to us. This'll be
    // released when the JSObject is finalized.
    ::JS_SetPrivate(cx, mJSObject, this);
    NS_ADDREF(this);
  }

  return mScriptContext;
}


nsresult
nsXBLDocGlobalObject::SetNewDocument(nsIDOMDocument *aDocument,
                                     PRBool aRemoveEventListeners,
                                     PRBool aClearScope)
{
  NS_NOTREACHED("waaah!");
  return NS_ERROR_UNEXPECTED;
}


void
nsXBLDocGlobalObject::SetDocShell(nsIDocShell *aDocShell)
{
  NS_NOTREACHED("waaah!");
}


nsIDocShell *
nsXBLDocGlobalObject::GetDocShell()
{
  NS_WARNING("waaah!");
  return nsnull;
}


void
nsXBLDocGlobalObject::SetOpenerWindow(nsIDOMWindowInternal *aOpener)
{
  NS_NOTREACHED("waaah!");
}


void
nsXBLDocGlobalObject::SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner)
{
  mGlobalObjectOwner = aOwner; // weak reference
}


nsIScriptGlobalObjectOwner *
nsXBLDocGlobalObject::GetGlobalObjectOwner()
{
  return mGlobalObjectOwner;
}


nsresult
nsXBLDocGlobalObject::HandleDOMEvent(nsIPresContext* aPresContext, 
                                       nsEvent* aEvent, 
                                       nsIDOMEvent** aDOMEvent,
                                       PRUint32 aFlags,
                                       nsEventStatus* aEventStatus)
{
  NS_NOTREACHED("waaah!");
  return NS_ERROR_UNEXPECTED;
}

JSObject *
nsXBLDocGlobalObject::GetGlobalJSObject()
{
  // The prototype document has its own special secret script object
  // that can be used to compile scripts and event handlers.

  if (!mScriptContext)
    return nsnull;

  JSContext* cx = NS_REINTERPRET_CAST(JSContext*,
                                      mScriptContext->GetNativeContext());
  if (!cx)
    return nsnull;

  return ::JS_GetGlobalObject(cx);
}

void
nsXBLDocGlobalObject::OnFinalize(JSObject *aObject)
{
  NS_ASSERTION(aObject == mJSObject, "Wrong object finalized!");

  mJSObject = nsnull;
}

void
nsXBLDocGlobalObject::SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts)
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
  nsresult rv = NS_OK;
  if (!mGlobalObjectOwner) {
    return nsnull;
  }

  nsCOMPtr<nsIXBLDocumentInfo> docInfo = do_QueryInterface(mGlobalObjectOwner, &rv);
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<nsIDocument> document;
  rv = docInfo->GetDocument(getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return document->GetPrincipal();
}

static PRBool IsChromeOrResourceURI(nsIURI* aURI)
{
  PRBool isChrome = PR_FALSE;
  PRBool isResource = PR_FALSE;
  if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && 
      NS_SUCCEEDED(aURI->SchemeIs("resource", &isResource)))
      return (isChrome || isResource);
  return PR_FALSE;
}

/* Implementation file */
NS_IMPL_ISUPPORTS3(nsXBLDocumentInfo, nsIXBLDocumentInfo, nsIScriptGlobalObjectOwner, nsISupportsWeakReference)

nsXBLDocumentInfo::nsXBLDocumentInfo(nsIDocument* aDocument)
  : mDocument(aDocument),
    mScriptAccess(PR_TRUE),
    mBindingTable(nsnull)
{
  nsIURI* uri = aDocument->GetDocumentURI();
  if (IsChromeOrResourceURI(uri)) {
    // Cache whether or not this chrome XBL can execute scripts.
    nsCOMPtr<nsIXULChromeRegistry> reg(do_GetService(NS_CHROMEREGISTRY_CONTRACTID));
    if (reg) {
      PRBool allow = PR_TRUE;
      reg->AllowScriptsForSkin(uri, &allow);
      SetScriptAccess(allow);
    }
  }
}

nsXBLDocumentInfo::~nsXBLDocumentInfo()
{
  /* destructor code */
  if (mGlobalObject) {
    mGlobalObject->SetContext(nsnull); // remove circular reference
    mGlobalObject->SetGlobalObjectOwner(nsnull); // just in case
  }
  delete mBindingTable;
}

NS_IMETHODIMP
nsXBLDocumentInfo::GetPrototypeBinding(const nsACString& aRef, nsXBLPrototypeBinding** aResult)
{
  *aResult = nsnull;
  if (!mBindingTable)
    return NS_OK;

  const nsPromiseFlatCString& flat = PromiseFlatCString(aRef);
  nsCStringKey key(flat.get());
  *aResult = NS_STATIC_CAST(nsXBLPrototypeBinding*, mBindingTable->Get(&key));

  return NS_OK;
}

static PRBool PR_CALLBACK
DeletePrototypeBinding(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsXBLPrototypeBinding* binding = NS_STATIC_CAST(nsXBLPrototypeBinding*, aData);
  delete binding;
  return PR_TRUE;
}

NS_IMETHODIMP
nsXBLDocumentInfo::SetPrototypeBinding(const nsACString& aRef, nsXBLPrototypeBinding* aBinding)
{
  if (!mBindingTable)
    mBindingTable = new nsObjectHashtable(nsnull, nsnull, DeletePrototypeBinding, nsnull);

  const nsPromiseFlatCString& flat = PromiseFlatCString(aRef);
  nsCStringKey key(flat.get());
  mBindingTable->Put(&key, aBinding);

  return NS_OK;
}

PRBool PR_CALLBACK FlushScopedSkinSheets(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsXBLPrototypeBinding* proto = (nsXBLPrototypeBinding*)aData;
  proto->FlushSkinSheets();
  return PR_TRUE;
}

NS_IMETHODIMP
nsXBLDocumentInfo::FlushSkinStylesheets()
{
  if (mBindingTable)
    mBindingTable->Enumerate(FlushScopedSkinSheets);
  return NS_OK;

}

//----------------------------------------------------------------------
//
// nsIScriptGlobalObjectOwner methods
//

nsIScriptGlobalObject*
nsXBLDocumentInfo::GetScriptGlobalObject()
{
  if (!mGlobalObject) {
    
    mGlobalObject = new nsXBLDocGlobalObject();
    
    if (!mGlobalObject)
      return nsnull;

    mGlobalObject->SetGlobalObjectOwner(this); // does not refcount
  }

  return mGlobalObject;
}

nsresult NS_NewXBLDocumentInfo(nsIDocument* aDocument, nsIXBLDocumentInfo** aResult)
{
  NS_PRECONDITION(aDocument, "Must have a document!");

  *aResult = new nsXBLDocumentInfo(aDocument);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);
  return NS_OK;
}
