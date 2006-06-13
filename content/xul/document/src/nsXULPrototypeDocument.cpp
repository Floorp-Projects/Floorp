/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Chris Waterson <waterson@netscape.com>
 *   L. David Baron <dbaron@dbaron.org>
 *   Ben Goodger <ben@netscape.com>
 *   Mark Hammond <mhammond@skippinet.com.au>
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

/*

  A "prototype" document that stores shared document information
  for the XUL cache.

*/

#include "nsCOMPtr.h"
#include "nsAString.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIPrincipal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptRuntime.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h" // deprecated array interface.
#include "nsIArray.h"         // blessed array interface.
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
#include "nsNodeInfoManager.h"
#include "nsContentUtils.h"

#include "nsDOMJSUtils.h" // for GetScriptContextFromJSContext

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
    virtual void SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner);
    virtual nsIScriptGlobalObjectOwner *GetGlobalObjectOwner();
    virtual void OnFinalize(PRUint32 aLangID, void *aGlobal);
    virtual void SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts);
    virtual nsresult SetNewArguments(nsIArray *aArguments);

    virtual void *GetScriptGlobal(PRUint32 lang);
    virtual nsresult EnsureScriptEnvironment(PRUint32 aLangID);

    virtual nsIScriptContext *GetScriptContext(PRUint32 lang);
    virtual nsresult SetScriptContext(PRUint32 language, nsIScriptContext *ctx);

    // nsIScriptObjectPrincipal methods
    virtual nsIPrincipal* GetPrincipal();
    
protected:
    virtual ~nsXULPDGlobalObject();

    nsIScriptGlobalObjectOwner* mGlobalObjectOwner; // weak reference

    nsCOMPtr<nsIScriptContext>  mScriptContexts[NS_STID_ARRAY_UBOUND];
    void *                      mScriptGlobals[NS_STID_ARRAY_UBOUND];

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

    NS_IMETHOD GetHeaderData(nsIAtom* aField, nsAString& aData) const;
    NS_IMETHOD SetHeaderData(nsIAtom* aField, const nsAString& aData);

    virtual nsIPrincipal *GetDocumentPrincipal();
    void SetDocumentPrincipal(nsIPrincipal *aPrincipal);

    NS_IMETHOD AwaitLoadDone(nsIXULDocument* aDocument, PRBool* aResult);
    NS_IMETHOD NotifyLoadDone();
    
    virtual nsNodeInfoManager *GetNodeInfoManager();

    // nsIScriptGlobalObjectOwner methods
    virtual nsIScriptGlobalObject* GetScriptGlobalObject();

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

    nsRefPtr<nsNodeInfoManager> mNodeInfoManager;

    nsXULPrototypeDocument();
    virtual ~nsXULPrototypeDocument();
    nsresult Init();

    friend NS_IMETHODIMP
    NS_NewXULPrototypeDocument(nsISupports* aOuter, REFNSIID aIID,
                               void** aResult);

    nsresult NewXULPDGlobalObject(nsIScriptGlobalObject** aResult);

    static nsIPrincipal* gSystemPrincipal;
    static nsIScriptGlobalObject* gSystemGlobal;
    static PRUint32 gRefCnt;

    friend class nsXULPDGlobalObject;
};

nsIPrincipal* nsXULPrototypeDocument::gSystemPrincipal;
nsIScriptGlobalObject* nsXULPrototypeDocument::gSystemGlobal;
PRUint32 nsXULPrototypeDocument::gRefCnt;


void PR_CALLBACK
nsXULPDGlobalObject_finalize(JSContext *cx, JSObject *obj)
{
    nsISupports *nativeThis = (nsISupports*)JS_GetPrivate(cx, obj);

    nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(nativeThis));

    if (sgo) {
        sgo->OnFinalize(nsIProgrammingLanguage::JAVASCRIPT, obj);
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
    ++gRefCnt;
}


nsresult
nsXULPrototypeDocument::Init()
{
    nsresult rv;

    rv = NS_NewISupportsArray(getter_AddRefs(mStyleSheetReferences));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_NewISupportsArray(getter_AddRefs(mOverlayReferences));
    NS_ENSURE_SUCCESS(rv, rv);

    mNodeInfoManager = new nsNodeInfoManager();
    NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_OUT_OF_MEMORY);

    return mNodeInfoManager->Init(nsnull);
}

nsXULPrototypeDocument::~nsXULPrototypeDocument()
{
    if (mGlobalObject) {
        // cleaup cycles etc.
        mGlobalObject->SetGlobalObjectOwner(nsnull);
    }
    
    if (mRoot)
        mRoot->ReleaseSubtree();

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gSystemPrincipal);
        NS_IF_RELEASE(gSystemGlobal);
    }
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

// Helper method that shares a system global among all prototype documents
// that have the system principal as their security principal.   Called by
// nsXULPrototypeDocument::Read and nsXULPDGlobalObject::GetGlobalObject.
// This method greatly reduces the number of nsXULPDGlobalObjects and their
// nsIScriptContexts in apps that load many XUL documents via chrome: URLs.

nsresult
nsXULPrototypeDocument::NewXULPDGlobalObject(nsIScriptGlobalObject** aResult)
{
    nsIPrincipal *principal = GetDocumentPrincipal();
    if (!principal)
        return NS_ERROR_FAILURE;

    // Now that GetDocumentPrincipal has succeeded, we can safely compare its
    // result to gSystemPrincipal, in order to create gSystemGlobal if the two
    // pointers are equal.  Thus, gSystemGlobal implies gSystemPrincipal.
    nsCOMPtr<nsIScriptGlobalObject> global;
    if (principal == gSystemPrincipal) {
        if (!gSystemGlobal) {
            gSystemGlobal = new nsXULPDGlobalObject();
            if (! gSystemGlobal)
                return NS_ERROR_OUT_OF_MEMORY;
            NS_ADDREF(gSystemGlobal);
        }
        global = gSystemGlobal;
    } else {
        global = new nsXULPDGlobalObject();
        if (! global)
            return NS_ERROR_OUT_OF_MEMORY;
        global->SetGlobalObjectOwner(this); // does not refcount
    }
    *aResult = global;
    NS_ADDREF(*aResult);
    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsISerializable methods
//

NS_IMETHODIMP
nsXULPrototypeDocument::Read(nsIObjectInputStream* aStream)
{
    nsresult rv;

    PRUint32 version;
    rv = aStream->Read32(&version);
    if (version != XUL_FASTLOAD_FILE_VERSION)
        return NS_ERROR_FAILURE;

    rv |= aStream->ReadObject(PR_TRUE, getter_AddRefs(mURI));

    PRUint32 referenceCount;
    nsCOMPtr<nsIURI> referenceURI;

    PRUint32 i;
    // nsISupportsArray mStyleSheetReferences
    rv |= aStream->Read32(&referenceCount);
    if (NS_FAILED(rv)) return rv;
    for (i = 0; i < referenceCount; ++i) {
        rv |= aStream->ReadObject(PR_TRUE, getter_AddRefs(referenceURI));
        
        mStyleSheetReferences->AppendElement(referenceURI);
    }

    // nsISupportsArray mOverlayReferences
    rv |= aStream->Read32(&referenceCount);
    for (i = 0; i < referenceCount; ++i) {
        rv |= aStream->ReadObject(PR_TRUE, getter_AddRefs(referenceURI));
        
        mOverlayReferences->AppendElement(referenceURI);
    }

    // nsIPrincipal mDocumentPrincipal
    nsCOMPtr<nsIPrincipal> principal;
    rv |= NS_ReadOptionalObject(aStream, PR_TRUE, getter_AddRefs(principal));
    if (! principal) {
        principal = GetDocumentPrincipal();
        if (!principal)
            rv |= NS_ERROR_FAILURE;
    } else {
        mNodeInfoManager->SetDocumentPrincipal(principal);
        mDocumentPrincipal = principal;
    }

    // nsIScriptGlobalObject mGlobalObject
    NewXULPDGlobalObject(getter_AddRefs(mGlobalObject));
    if (! mGlobalObject)
        return NS_ERROR_OUT_OF_MEMORY;

    mRoot = new nsXULPrototypeElement();
    if (! mRoot)
       return NS_ERROR_OUT_OF_MEMORY;

    // nsINodeInfo table
    nsCOMArray<nsINodeInfo> nodeInfos;

    rv |= aStream->Read32(&referenceCount);
    nsAutoString namespaceURI, qualifiedName;
    for (i = 0; i < referenceCount; ++i) {
        rv |= aStream->ReadString(namespaceURI);
        rv |= aStream->ReadString(qualifiedName);

        nsCOMPtr<nsINodeInfo> nodeInfo;
        rv |= mNodeInfoManager->GetNodeInfo(qualifiedName, namespaceURI, getter_AddRefs(nodeInfo));
        if (!nodeInfos.AppendObject(nodeInfo))
            rv |= NS_ERROR_OUT_OF_MEMORY;
    }

    // Document contents
    PRUint32 type;
    rv |= aStream->Read32(&type);

    if ((nsXULPrototypeNode::Type)type != nsXULPrototypeNode::eType_Element)
        return NS_ERROR_FAILURE;

    rv |= mRoot->Deserialize(aStream, mGlobalObject, mURI, &nodeInfos);
    rv |= NotifyLoadDone();

    return rv;
}

static nsresult
GetNodeInfos(nsXULPrototypeElement* aPrototype,
             nsCOMArray<nsINodeInfo>& aArray)
{
    nsresult rv;
    if (aArray.IndexOf(aPrototype->mNodeInfo) < 0) {
        if (!aArray.AppendObject(aPrototype->mNodeInfo)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    // Search attributes
    PRUint32 i;
    for (i = 0; i < aPrototype->mNumAttributes; ++i) {
        nsCOMPtr<nsINodeInfo> ni;
        nsAttrName* name = &aPrototype->mAttributes[i].mName;
        if (name->IsAtom()) {
            rv = aPrototype->mNodeInfo->NodeInfoManager()->
                GetNodeInfo(name->Atom(), nsnull, kNameSpaceID_None,
                            getter_AddRefs(ni));
            NS_ENSURE_SUCCESS(rv, rv);
        }
        else {
            ni = name->NodeInfo();
        }

        if (aArray.IndexOf(ni) < 0) {
            if (!aArray.AppendObject(ni)) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }
    }

    // Search children
    for (i = 0; i < aPrototype->mNumChildren; ++i) {
        nsXULPrototypeNode* child = aPrototype->mChildren[i];
        if (child->mType == nsXULPrototypeNode::eType_Element) {
            rv = GetNodeInfos(NS_STATIC_CAST(nsXULPrototypeElement*, child),
                              aArray);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULPrototypeDocument::Write(nsIObjectOutputStream* aStream)
{
    nsresult rv;

    rv = aStream->Write32(XUL_FASTLOAD_FILE_VERSION);
    
    rv |= aStream->WriteCompoundObject(mURI, NS_GET_IID(nsIURI), PR_TRUE);
    
    PRUint32 referenceCount;
    nsCOMPtr<nsIURI> referenceURI;

    PRUint32 i;

    // nsISupportsArray mStyleSheetReferences
    mStyleSheetReferences->Count(&referenceCount);
    rv |= aStream->Write32(referenceCount);
    
    for (i = 0; i < referenceCount; ++i) {
        mStyleSheetReferences->QueryElementAt(i, NS_GET_IID(nsIURI), getter_AddRefs(referenceURI));
        
        rv |= aStream->WriteCompoundObject(referenceURI, NS_GET_IID(nsIURI), PR_TRUE);
    }

    // nsISupportsArray mOverlayReferences
    mOverlayReferences->Count(&referenceCount);
    rv |= aStream->Write32(referenceCount);
    
    for (i = 0; i < referenceCount; ++i) {
        mOverlayReferences->QueryElementAt(i, NS_GET_IID(nsIURI), getter_AddRefs(referenceURI));
        
        rv |= aStream->WriteCompoundObject(referenceURI, NS_GET_IID(nsIURI), PR_TRUE);
    }

    // nsIPrincipal mDocumentPrincipal
    rv |= NS_WriteOptionalObject(aStream, mDocumentPrincipal, PR_TRUE);
    
    // nsINodeInfo table
    nsCOMArray<nsINodeInfo> nodeInfos;
    if (mRoot)
        rv |= GetNodeInfos(mRoot, nodeInfos);

    PRInt32 nodeInfoCount = nodeInfos.Count();
    rv |= aStream->Write32(nodeInfoCount);
    for (PRInt32 j = 0; j < nodeInfoCount; ++j) {
        nsINodeInfo *nodeInfo = nodeInfos[j];
        NS_ENSURE_TRUE(nodeInfo, NS_ERROR_FAILURE);

        nsAutoString namespaceURI;
        rv |= nodeInfo->GetNamespaceURI(namespaceURI);
        rv |= aStream->WriteWStringZ(namespaceURI.get());

        nsAutoString qualifiedName;
        nodeInfo->GetQualifiedName(qualifiedName);
        rv |= aStream->WriteWStringZ(qualifiedName.get());
    }

    // Now serialize the document contents
    nsIScriptGlobalObject* globalObject = GetScriptGlobalObject();
    NS_ENSURE_TRUE(globalObject, NS_ERROR_UNEXPECTED);

    if (mRoot)
        rv |= mRoot->Serialize(aStream, globalObject, &nodeInfos);
 
    return rv;
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
    NS_ASSERTION(!mURI, "Can't change the uri of a xul prototype document");
    if (mURI)
        return NS_ERROR_ALREADY_INITIALIZED;

    mURI = aURI;
    if (!mDocumentPrincipal) {
        // If the document doesn't have a principal yet we'll force the creation of one
        // so that mNodeInfoManager properly gets one.
        GetDocumentPrincipal();
    }
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
nsXULPrototypeDocument::GetHeaderData(nsIAtom* aField, nsAString& aData) const
{
    // XXX Not implemented
    aData.Truncate();
    return NS_OK;
}


NS_IMETHODIMP
nsXULPrototypeDocument::SetHeaderData(nsIAtom* aField, const nsAString& aData)
{
    // XXX Not implemented
    return NS_OK;
}



nsIPrincipal*
nsXULPrototypeDocument::GetDocumentPrincipal()
{
    NS_PRECONDITION(mNodeInfoManager, "missing nodeInfoManager");
    if (!mDocumentPrincipal) {
        nsIScriptSecurityManager *securityManager =
            nsContentUtils::GetSecurityManager();
        nsresult rv = NS_OK;

        // XXX This should be handled by the security manager, see bug 160042
        PRBool isChrome = PR_FALSE;
        if (NS_SUCCEEDED(mURI->SchemeIs("chrome", &isChrome)) && isChrome) {
            if (gSystemPrincipal) {
                mDocumentPrincipal = gSystemPrincipal;
            } else {
                rv = securityManager->
                     GetSystemPrincipal(getter_AddRefs(mDocumentPrincipal));
                NS_IF_ADDREF(gSystemPrincipal = mDocumentPrincipal);
            }
        } else {
            rv = securityManager->
                 GetCodebasePrincipal(mURI, getter_AddRefs(mDocumentPrincipal));
        }

        if (NS_FAILED(rv))
            return nsnull;

        mNodeInfoManager->SetDocumentPrincipal(mDocumentPrincipal);
    }

    return mDocumentPrincipal;
}


void
nsXULPrototypeDocument::SetDocumentPrincipal(nsIPrincipal *aPrincipal)
{
    NS_PRECONDITION(mNodeInfoManager, "missing nodeInfoManager");
    mDocumentPrincipal = aPrincipal;
    mNodeInfoManager->SetDocumentPrincipal(aPrincipal);
}

nsNodeInfoManager*
nsXULPrototypeDocument::GetNodeInfoManager()
{
    return mNodeInfoManager;
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

nsIScriptGlobalObject*
nsXULPrototypeDocument::GetScriptGlobalObject()
{
    if (!mGlobalObject)
        NewXULPDGlobalObject(getter_AddRefs(mGlobalObject));

    return mGlobalObject;
}

//----------------------------------------------------------------------
//
// nsXULPDGlobalObject
//

nsXULPDGlobalObject::nsXULPDGlobalObject()
    :  mGlobalObjectOwner(nsnull)
{
  memset(mScriptGlobals, 0, sizeof(mScriptGlobals));
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

nsresult
nsXULPDGlobalObject::SetScriptContext(PRUint32 lang_id, nsIScriptContext *aScriptContext)
{
  // almost a clone of nsGlobalWindow
  nsresult rv;

  PRBool ok = NS_STID_VALID(lang_id);
  NS_ASSERTION(ok, "Invalid programming language ID requested");
  NS_ENSURE_TRUE(ok, NS_ERROR_INVALID_ARG);
  PRUint32 lang_ndx = NS_STID_INDEX(lang_id);

  if (!aScriptContext)
    NS_WARNING("Possibly early removal of script object, see bug #41608");
  else {
    // should probably assert the context is clean???
    aScriptContext->WillInitializeContext();
    // NOTE: We init this context with a NULL global - this is subtly
    // different than nsGlobalWindow which passes 'this'
    rv = aScriptContext->InitContext(nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsIScriptContext *existing;
  existing = mScriptContexts[lang_ndx];
  NS_ASSERTION(!aScriptContext || !existing, "Bad call to SetContext()!");

  if (existing)
    existing->SetOwner(nsnull);
  void *script_glob = nsnull;

  if (aScriptContext) {
    aScriptContext->DidInitializeContext();
    script_glob = aScriptContext->GetNativeGlobal();
    NS_ASSERTION(script_glob, "GetNativeGlobal returned NULL!");
  }
  mScriptContexts[lang_ndx] = aScriptContext;
  mScriptGlobals[lang_ndx] = script_glob;
  return NS_OK;
}

nsresult
nsXULPDGlobalObject::EnsureScriptEnvironment(PRUint32 lang_id)
{
  PRBool ok = NS_STID_VALID(lang_id);
  NS_ASSERTION(ok, "Invalid programming language ID requested");
  NS_ENSURE_TRUE(ok, NS_ERROR_INVALID_ARG);
  PRUint32 lang_ndx = NS_STID_INDEX(lang_id);

  if (mScriptContexts[lang_ndx] == nsnull) {
    nsresult rv;
    NS_ASSERTION(mScriptGlobals[lang_ndx] == nsnull, "Have global without context?");

    nsCOMPtr<nsIScriptRuntime> languageRuntime;
    rv = NS_GetScriptRuntimeByID(lang_id, getter_AddRefs(languageRuntime));
    NS_ENSURE_SUCCESS(rv, nsnull);

    nsCOMPtr<nsIScriptContext> ctxNew;
    rv = languageRuntime->CreateContext(getter_AddRefs(ctxNew));
    // For JS, we have to setup a special global object.  We do this then
    // attach it as the global for this context.  Then, ::SetScriptContext
    // will re-fetch the global and set it up in our language globals array.
    if (lang_id == nsIProgrammingLanguage::JAVASCRIPT) {
      // some special JS specific code we should abstract
      JSContext *cx = (JSContext *)ctxNew->GetNativeContext();
      JSAutoRequest ar(cx);
      JSObject *newGlob = ::JS_NewObject(cx, &gSharedGlobalClass, nsnull, nsnull);
      if (!newGlob)
        return nsnull;

      ::JS_SetGlobalObject(cx, newGlob);

      // Add an owning reference from JS back to us. This'll be
      // released when the JSObject is finalized.
      ::JS_SetPrivate(cx, newGlob, this);
      NS_ADDREF(this);
    }

    NS_ENSURE_SUCCESS(rv, nsnull);
    rv = SetScriptContext(lang_id, ctxNew);
    NS_ENSURE_SUCCESS(rv, nsnull);
  }
  return NS_OK;
}

nsIScriptContext *
nsXULPDGlobalObject::GetScriptContext(PRUint32 lang_id)
{
  // This global object creates a context on demand - do that now.
  nsresult rv = EnsureScriptEnvironment(lang_id);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to setup script language");
    return nsnull;
  }
  // Note that EnsureScriptEnvironment has validated lang_id
  return mScriptContexts[NS_STID_INDEX(lang_id)];
}

void *
nsXULPDGlobalObject::GetScriptGlobal(PRUint32 lang_id)
{
  PRBool ok = NS_STID_VALID(lang_id);
  NS_ASSERTION(ok, "Invalid programming language ID requested");
  NS_ENSURE_TRUE(ok, nsnull);
  PRUint32 lang_ndx = NS_STID_INDEX(lang_id);

  NS_ASSERTION(mScriptContexts[lang_ndx] != nsnull, "Querying for global before setting up context?");
  return mScriptGlobals[lang_ndx];
}


void
nsXULPDGlobalObject::SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner)
{
    if (aOwner == nsnull) {
        // this means we should cleanup
        PRUint32 lang_ndx;
        NS_STID_FOR_INDEX(lang_ndx) {
            if (mScriptContexts[lang_ndx]) {
                mScriptContexts[lang_ndx]->FinalizeContext();
                mScriptContexts[lang_ndx] = nsnull;
            }
        }
    }
    mGlobalObjectOwner = aOwner; // weak reference
}


nsIScriptGlobalObjectOwner *
nsXULPDGlobalObject::GetGlobalObjectOwner()
{
    return mGlobalObjectOwner;
}


void
nsXULPDGlobalObject::OnFinalize(PRUint32 aLangID, void *aObject)
{
    NS_ASSERTION(NS_STID_VALID(aLangID), "Invalid language ID");
    NS_ASSERTION(aObject == mScriptGlobals[NS_STID_INDEX(aLangID)],
                 "Wrong object finalized!");
    mScriptGlobals[NS_STID_INDEX(aLangID)] = nsnull;
}

void
nsXULPDGlobalObject::SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts)
{
    // We don't care...
}

nsresult
nsXULPDGlobalObject::SetNewArguments(nsIArray *aArguments)
{
    NS_NOTREACHED("waaah!");
    return NS_ERROR_UNEXPECTED;
}

//----------------------------------------------------------------------
//
// nsIScriptObjectPrincipal methods
//

nsIPrincipal*
nsXULPDGlobalObject::GetPrincipal()
{
    if (!mGlobalObjectOwner) {
        // See nsXULPrototypeDocument::NewXULPDGlobalObject, the comment
        // about gSystemGlobal implying gSystemPrincipal.
        if (this == nsXULPrototypeDocument::gSystemGlobal) {
            return nsXULPrototypeDocument::gSystemPrincipal;
        }
        return nsnull;
    }
    nsCOMPtr<nsIXULPrototypeDocument> protoDoc
      = do_QueryInterface(mGlobalObjectOwner);

    return protoDoc->GetDocumentPrincipal();
}

