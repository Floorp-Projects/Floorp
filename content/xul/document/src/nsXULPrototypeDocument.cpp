/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Chris Waterson <waterson@netscape.com>
 *   L. David Baron <dbaron@fas.harvard.edu>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  A "prototype" document that stores shared document information
  for the XUL cache.

*/

#include "nsCOMPtr.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIPrincipal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsIURI.h"
#include "nsIXULDocument.h"
#include "nsIXULPrototypeDocument.h"
#include "jsapi.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsXULElement.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsDOMCID.h"


static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,
                     NS_DOM_SCRIPT_OBJECT_FACTORY_CID);


class nsXULPDGlobalObject : public nsIScriptGlobalObject,
                            public nsIScriptObjectPrincipal
{
public:
    nsXULPDGlobalObject();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIScriptGlobalObject methods
    NS_IMETHOD SetContext(nsIScriptContext *aContext);
    NS_IMETHOD GetContext(nsIScriptContext **aContext);
    NS_IMETHOD SetNewDocument(nsIDOMDocument *aDocument,
                              PRBool removeEventListeners);
    NS_IMETHOD SetDocShell(nsIDocShell *aDocShell);
    NS_IMETHOD GetDocShell(nsIDocShell **aDocShell);
    NS_IMETHOD SetOpenerWindow(nsIDOMWindowInternal *aOpener);
    NS_IMETHOD SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner);
    NS_IMETHOD GetGlobalObjectOwner(nsIScriptGlobalObjectOwner** aOwner);
    NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, 
                              nsEvent* aEvent, 
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus* aEventStatus);
    NS_IMETHOD_(JSObject *) GetGlobalJSObject();
    NS_IMETHOD OnFinalize(JSObject *aObject);

    // nsIScriptObjectPrincipal methods
    NS_IMETHOD GetPrincipal(nsIPrincipal** aPrincipal);
    
protected:
    virtual ~nsXULPDGlobalObject();

    nsCOMPtr<nsIScriptContext> mScriptContext;
    JSObject *mJSObject;    // XXX JS language rabies bigotry badness

    nsIScriptGlobalObjectOwner* mGlobalObjectOwner; // weak reference

    static JSClass gSharedGlobalClass;
};

class nsXULPrototypeDocument : public nsIXULPrototypeDocument,
                               public nsIScriptGlobalObjectOwner
{
public:
    static nsresult
    Create(nsIURI* aURI, nsXULPrototypeDocument** aResult);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISerializable interface
    NS_DECL_NSISERIALIZABLE

    // nsIXULPrototypeDocument interface
    NS_IMETHOD GetURI(nsIURI** aResult);
    NS_IMETHOD SetURI(nsIURI* aURI);

    NS_IMETHOD GetRootElement(nsXULPrototypeElement** aResult);
    NS_IMETHOD SetRootElement(nsXULPrototypeElement* aElement);

    NS_IMETHOD AddStyleSheetReference(nsIURI* aStyleSheet);
    NS_IMETHOD GetStyleSheetReferences(nsISupportsArray** aResult);

    NS_IMETHOD AddOverlayReference(nsIURI* aURI);
    NS_IMETHOD GetOverlayReferences(nsISupportsArray** aResult);

    NS_IMETHOD GetHeaderData(nsIAtom* aField, nsAWritableString& aData) const;
    NS_IMETHOD SetHeaderData(nsIAtom* aField, const nsAReadableString& aData);

    NS_IMETHOD GetDocumentPrincipal(nsIPrincipal** aResult);
    NS_IMETHOD SetDocumentPrincipal(nsIPrincipal* aPrincipal);

    NS_IMETHOD AwaitLoadDone(nsIXULDocument* aDocument, PRBool* aResult);
    NS_IMETHOD NotifyLoadDone();

    // nsIScriptGlobalObjectOwner methods
    NS_DECL_NSISCRIPTGLOBALOBJECTOWNER

    NS_DEFINE_STATIC_CID_ACCESSOR(NS_XULPROTOTYPEDOCUMENT_CID);

protected:
    nsCOMPtr<nsIURI> mURI;
    nsXULPrototypeElement* mRoot;
    nsCOMPtr<nsISupportsArray> mStyleSheetReferences;
    nsCOMPtr<nsISupportsArray> mOverlayReferences;
    nsCOMPtr<nsIPrincipal> mDocumentPrincipal;

    nsCOMPtr<nsIScriptGlobalObject> mGlobalObject;

    PRPackedBool mLoaded;
    nsCOMPtr<nsICollection> mPrototypeWaiters;

    nsXULPrototypeDocument();
    virtual ~nsXULPrototypeDocument();
    nsresult Init();

    friend NS_IMETHODIMP
    NS_NewXULPrototypeDocument(nsISupports* aOuter, REFNSIID aIID, void** aResult);
};



void PR_CALLBACK
nsXULPDGlobalObject_finalize(JSContext *cx, JSObject *obj)
{
    nsISupports *nativeThis = (nsISupports*)JS_GetPrivate(cx, obj);

    nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(nativeThis));

    if (sgo) {
        sgo->OnFinalize(obj);
    }

    // The addref was part of JSObject construction
    NS_RELEASE(nativeThis);
}


JSBool PR_CALLBACK
nsXULPDGlobalObject_resolve(JSContext *cx, JSObject *obj, jsval id)
{
    JSBool did_resolve = JS_FALSE;

    return JS_ResolveStandardClass(cx, obj, id, &did_resolve);
}


JSClass nsXULPDGlobalObject::gSharedGlobalClass = {
    "nsXULPrototypeScript compilation scope",
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, nsXULPDGlobalObject_resolve,  JS_ConvertStub,
    nsXULPDGlobalObject_finalize
};



//----------------------------------------------------------------------
//
// ctors, dtors, n' stuff
//

nsXULPrototypeDocument::nsXULPrototypeDocument()
    : mRoot(nsnull),
      mGlobalObject(nsnull),
      mLoaded(PR_FALSE)
{
    NS_INIT_REFCNT();
}


nsresult
nsXULPrototypeDocument::Init()
{
    nsresult rv;

    rv = NS_NewISupportsArray(getter_AddRefs(mStyleSheetReferences));
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewISupportsArray(getter_AddRefs(mOverlayReferences));
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsXULPrototypeDocument::~nsXULPrototypeDocument()
{
    if (mGlobalObject) {
        mGlobalObject->SetContext(nsnull); // remove circular reference
        mGlobalObject->SetGlobalObjectOwner(nsnull); // just in case
    }
    delete mRoot;
}

NS_IMPL_ISUPPORTS3(nsXULPrototypeDocument,
                   nsIXULPrototypeDocument,
                   nsIScriptGlobalObjectOwner,
                   nsISerializable)

NS_IMETHODIMP
NS_NewXULPrototypeDocument(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsXULPrototypeDocument* result = new nsXULPrototypeDocument();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = result->Init();
    if (NS_FAILED(rv)) {
        delete result;
        return rv;
    }

    NS_ADDREF(result);
    rv = result->QueryInterface(aIID, aResult);
    NS_RELEASE(result);

    return rv;
}


//----------------------------------------------------------------------
//
// nsISerializable methods
//

#define XUL_FAST_LOAD_VERSION   0

NS_IMETHODIMP
nsXULPrototypeDocument::Read(nsIObjectInputStream* aStream)
{
    nsresult rv;

    PRUint32 version;
    rv = aStream->Read32(&version);
    if (NS_FAILED(rv)) return rv;

    if (version != XUL_FAST_LOAD_VERSION)
        return NS_ERROR_FAILURE;

    rv = aStream->ReadObject(PR_TRUE, getter_AddRefs(mURI));
    if (NS_FAILED(rv)) return rv;

    // XXXbe more to come
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::Write(nsIObjectOutputStream* aStream)
{
    nsresult rv;

    rv = aStream->Write32(XUL_FAST_LOAD_VERSION);
    if (NS_FAILED(rv)) return rv;

    rv = aStream->WriteCompoundObject(mURI, NS_GET_IID(nsIURI), PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIScriptContext> scriptContext;
    rv = mGlobalObject->GetContext(getter_AddRefs(scriptContext));
    if (NS_FAILED(rv)) return rv;

#if 0
    rv = mRoot->Serialize(aStream, scriptContext);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISupportsArray> mStyleSheetReferences;
    nsCOMPtr<nsISupportsArray> mOverlayReferences;
    nsCOMPtr<nsIPrincipal> mDocumentPrincipal;

    nsCOMPtr<nsIScriptGlobalObject> mGlobalObject;
#endif
    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsIXULPrototypeDocument methods
//

NS_IMETHODIMP
nsXULPrototypeDocument::GetURI(nsIURI** aResult)
{
    *aResult = mURI;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::SetURI(nsIURI* aURI)
{
    mURI = dont_QueryInterface(aURI);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::GetRootElement(nsXULPrototypeElement** aResult)
{
    *aResult = mRoot;
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::SetRootElement(nsXULPrototypeElement* aElement)
{
    mRoot = aElement;
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::AddStyleSheetReference(nsIURI* aURI)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    mStyleSheetReferences->AppendElement(aURI);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::GetStyleSheetReferences(nsISupportsArray** aResult)
{
    *aResult = mStyleSheetReferences;
    NS_ADDREF(*aResult);
    return NS_OK;
}



NS_IMETHODIMP
nsXULPrototypeDocument::AddOverlayReference(nsIURI* aURI)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    mOverlayReferences->AppendElement(aURI);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::GetOverlayReferences(nsISupportsArray** aResult)
{
    *aResult = mOverlayReferences;
    NS_ADDREF(*aResult);
    return NS_OK;
}



NS_IMETHODIMP
nsXULPrototypeDocument::GetHeaderData(nsIAtom* aField, nsAWritableString& aData) const
{
    // XXX Not implemented
    aData.Truncate();
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::SetHeaderData(nsIAtom* aField, const nsAReadableString& aData)
{
    // XXX Not implemented
    return NS_OK;
}



NS_IMETHODIMP
nsXULPrototypeDocument::GetDocumentPrincipal(nsIPrincipal** aResult)
{
    if (!mDocumentPrincipal) {
        nsresult rv;
        nsCOMPtr<nsIScriptSecurityManager> securityManager = 
                 do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);

        if (NS_FAILED(rv))
            return NS_ERROR_FAILURE;

        rv = securityManager->GetCodebasePrincipal(mURI, getter_AddRefs(mDocumentPrincipal));

        if (NS_FAILED(rv))
            return NS_ERROR_FAILURE;
    }

    *aResult = mDocumentPrincipal;
    NS_ADDREF(*aResult);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::SetDocumentPrincipal(nsIPrincipal* aPrincipal)
{
    mDocumentPrincipal = aPrincipal;
    return NS_OK;
}



NS_IMETHODIMP
nsXULPrototypeDocument::AwaitLoadDone(nsIXULDocument* aDocument, PRBool* aResult)
{
    nsresult rv = NS_OK;

    *aResult = mLoaded;

    if (!mLoaded) {
        if (!mPrototypeWaiters) {
            nsCOMPtr<nsISupportsArray> supportsArray;
            rv = NS_NewISupportsArray(getter_AddRefs(supportsArray));
            if (NS_FAILED(rv)) return rv;

            mPrototypeWaiters = do_QueryInterface(supportsArray);
        }

        rv = mPrototypeWaiters->AppendElement(aDocument);
    }

    return rv;
}


NS_IMETHODIMP
nsXULPrototypeDocument::NotifyLoadDone()
{
    nsresult rv = NS_OK;

    mLoaded = PR_TRUE;

    if (mPrototypeWaiters) {
        PRUint32 n;
        rv = mPrototypeWaiters->Count(&n);
        if (NS_SUCCEEDED(rv)) {
            for (PRUint32 i = 0; i < n; i++) {
                nsCOMPtr<nsIXULDocument> doc;
                rv = mPrototypeWaiters->GetElementAt(i, getter_AddRefs(doc));
                if (NS_FAILED(rv)) break;

                rv = doc->OnPrototypeLoadDone();
                if (NS_FAILED(rv)) break;
            }
        }

        mPrototypeWaiters = nsnull;
    }

    return rv;
}

//----------------------------------------------------------------------
//
// nsIScriptGlobalObjectOwner methods
//

NS_IMETHODIMP
nsXULPrototypeDocument::GetScriptGlobalObject(nsIScriptGlobalObject** _result)
{
    if (!mGlobalObject) {
        mGlobalObject = new nsXULPDGlobalObject();
        if (!mGlobalObject) {
            *_result = nsnull;
            return NS_ERROR_OUT_OF_MEMORY;
            }
        mGlobalObject->SetGlobalObjectOwner(this); // does not refcount
    }
    *_result = mGlobalObject;
    NS_ADDREF(*_result);
    return NS_OK;
}

NS_IMETHODIMP
nsXULPrototypeDocument::ReportScriptError(nsIScriptError *errorObject)
{
   nsresult rv;

   if (errorObject == nsnull)
      return NS_ERROR_NULL_POINTER;

   // Get the console service, where we're going to register the error.
   nsCOMPtr<nsIConsoleService> consoleService
      (do_GetService("@mozilla.org/consoleservice;1"));

   if (consoleService != nsnull) {
       rv = consoleService->LogMessage(errorObject);
       if (NS_SUCCEEDED(rv)) {
           return NS_OK;
       } else {
           return rv;
       }
   } else {
       return NS_ERROR_NOT_AVAILABLE;
   }
}

//----------------------------------------------------------------------
//
// nsXULPDGlobalObject
//

nsXULPDGlobalObject::nsXULPDGlobalObject()
    : mJSObject(nsnull),
      mGlobalObjectOwner(nsnull)
{
    NS_INIT_REFCNT();
}


nsXULPDGlobalObject::~nsXULPDGlobalObject()
{
}

NS_IMPL_ADDREF(nsXULPDGlobalObject)
NS_IMPL_RELEASE(nsXULPDGlobalObject)

NS_INTERFACE_MAP_BEGIN(nsXULPDGlobalObject)
    NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
    NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptGlobalObject)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
//
// nsIScriptGlobalObject methods
//

NS_IMETHODIMP
nsXULPDGlobalObject::SetContext(nsIScriptContext *aContext)
{
    mScriptContext = aContext;
    return NS_OK;
}


NS_IMETHODIMP
nsXULPDGlobalObject::GetContext(nsIScriptContext **aContext)
{
    // This whole fragile mess is predicated on the fact that
    // GetContext() will be called before GetScriptObject() is.
    if (! mScriptContext) {
        nsCOMPtr<nsIDOMScriptObjectFactory> factory =
            do_GetService(kDOMScriptObjectFactoryCID);
        NS_ENSURE_TRUE(factory, NS_ERROR_FAILURE);

        nsresult rv =
            factory->NewScriptContext(nsnull, getter_AddRefs(mScriptContext));
        if (NS_FAILED(rv))
            return rv;

        JSContext *cx = (JSContext *)mScriptContext->GetNativeContext();

        mJSObject = ::JS_NewObject(cx, &gSharedGlobalClass, nsnull, nsnull);
        if (!mJSObject)
            return NS_ERROR_OUT_OF_MEMORY;

        ::JS_SetGlobalObject(cx, mJSObject);

        // Add an owning reference from JS back to us. This'll be
        // released when the JSObject is finalized.
        ::JS_SetPrivate(cx, mJSObject, this);
        NS_ADDREF(this);
    }

    *aContext = mScriptContext;
    NS_IF_ADDREF(*aContext);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPDGlobalObject::SetNewDocument(nsIDOMDocument *aDocument,
                                    PRBool removeEventListeners)
{
    NS_NOTREACHED("waaah!");
    return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsXULPDGlobalObject::SetDocShell(nsIDocShell *aDocShell)
{
    NS_NOTREACHED("waaah!");
    return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsXULPDGlobalObject::GetDocShell(nsIDocShell **aDocShell)
{
    NS_WARNING("waaah!");
    return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsXULPDGlobalObject::SetOpenerWindow(nsIDOMWindowInternal *aOpener)
{
    NS_NOTREACHED("waaah!");
    return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsXULPDGlobalObject::SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner)
{
    mGlobalObjectOwner = aOwner; // weak reference
    return NS_OK;
}


NS_IMETHODIMP
nsXULPDGlobalObject::GetGlobalObjectOwner(nsIScriptGlobalObjectOwner** aOwner)
{
    *aOwner = mGlobalObjectOwner;
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}


NS_IMETHODIMP
nsXULPDGlobalObject::HandleDOMEvent(nsIPresContext* aPresContext, 
                                       nsEvent* aEvent, 
                                       nsIDOMEvent** aDOMEvent,
                                       PRUint32 aFlags,
                                       nsEventStatus* aEventStatus)
{
    NS_NOTREACHED("waaah!");
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP_(JSObject *)
nsXULPDGlobalObject::GetGlobalJSObject()
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

NS_IMETHODIMP
nsXULPDGlobalObject::OnFinalize(JSObject *aObject)
{
    NS_ASSERTION(aObject == mJSObject, "Wrong object finalized!");

    mJSObject = nsnull;

    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIScriptObjectPrincipal methods
//

NS_IMETHODIMP
nsXULPDGlobalObject::GetPrincipal(nsIPrincipal** aPrincipal)
{
    if (!mGlobalObjectOwner) {
        *aPrincipal = nsnull;
        return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIXULPrototypeDocument> protoDoc
      = do_QueryInterface(mGlobalObjectOwner);
    return protoDoc->GetDocumentPrincipal(aPrincipal);
}

