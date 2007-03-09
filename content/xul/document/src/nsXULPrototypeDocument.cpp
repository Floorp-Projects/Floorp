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


#include "nsXULPrototypeDocument.h"
#include "nsXULDocument.h"

#include "nsAString.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIPrincipal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptRuntime.h"
#include "nsIServiceManager.h"
#include "nsIArray.h"
#include "nsIURI.h"
#include "jsapi.h"
#include "nsString.h"
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
    nsXULPDGlobalObject(nsXULPrototypeDocument* owner);

    // nsISupports interface
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS

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

    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXULPDGlobalObject,
                                             nsIScriptGlobalObject)

protected:
    virtual ~nsXULPDGlobalObject();

    nsXULPrototypeDocument* mGlobalObjectOwner; // weak reference

    nsCOMPtr<nsIScriptContext>  mScriptContexts[NS_STID_ARRAY_UBOUND];
    void *                      mScriptGlobals[NS_STID_ARRAY_UBOUND];

    static JSClass gSharedGlobalClass;
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
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_GLOBAL_FLAGS,
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

    PRUint32 count = mProcessingInstructions.Length();
    for (PRUint32 i = 0; i < count; i++)
    {
        mProcessingInstructions[i]->Release();
    }

    if (mRoot)
        mRoot->ReleaseSubtree();

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gSystemPrincipal);
        NS_IF_RELEASE(gSystemGlobal);
    }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULPrototypeDocument)
NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsXULPrototypeDocument)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULPrototypeDocument)
  // XXX Can't traverse tmp->mRoot, non-XPCOM refcounted object
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mGlobalObject)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN(nsXULPrototypeDocument)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObjectOwner)
  NS_INTERFACE_MAP_ENTRY(nsISerializable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptGlobalObjectOwner)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsXULPrototypeDocument)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsXULPrototypeDocument,
                                          nsIScriptGlobalObjectOwner)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsXULPrototypeDocument,
                                           nsIScriptGlobalObjectOwner)

NS_IMETHODIMP
NS_NewXULPrototypeDocument(nsXULPrototypeDocument** aResult)
{
    *aResult = new nsXULPrototypeDocument();
    if (! *aResult)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = (*aResult)->Init();
    if (NS_FAILED(rv)) {
        delete *aResult;
        *aResult = nsnull;
        return rv;
    }

    NS_ADDREF(*aResult);
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
    // Now compare DocumentPrincipal() to gSystemPrincipal, in order to create
    // gSystemGlobal if the two pointers are equal.  Thus, gSystemGlobal
    // implies gSystemPrincipal.
    nsCOMPtr<nsIScriptGlobalObject> global;
    if (DocumentPrincipal() == gSystemPrincipal) {
        if (!gSystemGlobal) {
            gSystemGlobal = new nsXULPDGlobalObject(nsnull);
            if (! gSystemGlobal)
                return NS_ERROR_OUT_OF_MEMORY;
            NS_ADDREF(gSystemGlobal);
        }
        global = gSystemGlobal;
    } else {
        global = new nsXULPDGlobalObject(this); // does not refcount
        if (! global)
            return NS_ERROR_OUT_OF_MEMORY;
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

    rv = aStream->ReadObject(PR_TRUE, getter_AddRefs(mURI));

    PRUint32 count, i;
    nsCOMPtr<nsIURI> styleOverlayURI;

    rv |= aStream->Read32(&count);
    if (NS_FAILED(rv)) return rv;

    for (i = 0; i < count; ++i) {
        rv |= aStream->ReadObject(PR_TRUE, getter_AddRefs(styleOverlayURI));
        mStyleSheetReferences.AppendObject(styleOverlayURI);
    }


    // nsIPrincipal mNodeInfoManager->mPrincipal
    nsCOMPtr<nsIPrincipal> principal;
    rv |= aStream->ReadObject(PR_TRUE, getter_AddRefs(principal));
    // Better safe than sorry....
    mNodeInfoManager->SetDocumentPrincipal(principal);

    // nsIScriptGlobalObject mGlobalObject
    NewXULPDGlobalObject(getter_AddRefs(mGlobalObject));
    if (! mGlobalObject)
        return NS_ERROR_OUT_OF_MEMORY;

    mRoot = new nsXULPrototypeElement();
    if (! mRoot)
       return NS_ERROR_OUT_OF_MEMORY;

    // nsINodeInfo table
    nsCOMArray<nsINodeInfo> nodeInfos;

    rv |= aStream->Read32(&count);
    nsAutoString namespaceURI, qualifiedName;
    for (i = 0; i < count; ++i) {
        rv |= aStream->ReadString(namespaceURI);
        rv |= aStream->ReadString(qualifiedName);

        nsCOMPtr<nsINodeInfo> nodeInfo;
        rv |= mNodeInfoManager->GetNodeInfo(qualifiedName, namespaceURI, getter_AddRefs(nodeInfo));
        if (!nodeInfos.AppendObject(nodeInfo))
            rv |= NS_ERROR_OUT_OF_MEMORY;
    }

    // Document contents
    PRUint32 type;
    while (NS_SUCCEEDED(rv)) {
        rv |= aStream->Read32(&type);

        if ((nsXULPrototypeNode::Type)type == nsXULPrototypeNode::eType_PI) {
            nsXULPrototypePI* pi = new nsXULPrototypePI();
            if (! pi) {
               rv |= NS_ERROR_OUT_OF_MEMORY;
               break;
            }

            rv |= pi->Deserialize(aStream, mGlobalObject, mURI, &nodeInfos);
            rv |= AddProcessingInstruction(pi);
        } else if ((nsXULPrototypeNode::Type)type == nsXULPrototypeNode::eType_Element) {
            rv |= mRoot->Deserialize(aStream, mGlobalObject, mURI, &nodeInfos);
            break;
        } else {
            NS_NOTREACHED("Unexpected prototype node type");
            rv |= NS_ERROR_FAILURE;
            break;
        }
    }
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

    rv = aStream->WriteCompoundObject(mURI, NS_GET_IID(nsIURI), PR_TRUE);
    
    PRUint32 count;

    count = mStyleSheetReferences.Count();
    rv |= aStream->Write32(count);

    PRUint32 i;
    for (i = 0; i < count; ++i) {
        rv |= aStream->WriteCompoundObject(mStyleSheetReferences[i],
                                           NS_GET_IID(nsIURI), PR_TRUE);
    }

    // nsIPrincipal mNodeInfoManager->mPrincipal
    rv |= aStream->WriteObject(mNodeInfoManager->DocumentPrincipal(),
                               PR_TRUE);
    
    // nsINodeInfo table
    nsCOMArray<nsINodeInfo> nodeInfos;
    if (mRoot)
        rv |= GetNodeInfos(mRoot, nodeInfos);

    PRInt32 nodeInfoCount = nodeInfos.Count();
    rv |= aStream->Write32(nodeInfoCount);
    for (i = 0; i < nodeInfoCount; ++i) {
        nsINodeInfo *nodeInfo = nodeInfos[i];
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

    count = mProcessingInstructions.Length();
    for (i = 0; i < count; ++i) {
        nsXULPrototypePI* pi = mProcessingInstructions[i];
        rv |= pi->Serialize(aStream, globalObject, &nodeInfos);
    }

    if (mRoot)
        rv |= mRoot->Serialize(aStream, globalObject, &nodeInfos);
 
    return rv;
}


//----------------------------------------------------------------------
//

nsresult
nsXULPrototypeDocument::InitPrincipal(nsIURI* aURI, nsIPrincipal* aPrincipal)
{
    NS_ENSURE_ARG_POINTER(aURI);

    mURI = aURI;
    mNodeInfoManager->SetDocumentPrincipal(aPrincipal);
    return NS_OK;
}
    

nsIURI*
nsXULPrototypeDocument::GetURI()
{
    NS_ASSERTION(mURI, "null URI");
    return mURI;
}


nsXULPrototypeElement*
nsXULPrototypeDocument::GetRootElement()
{
    return mRoot;
}


void
nsXULPrototypeDocument::SetRootElement(nsXULPrototypeElement* aElement)
{
    mRoot = aElement;
}

nsresult
nsXULPrototypeDocument::AddProcessingInstruction(nsXULPrototypePI* aPI)
{
    NS_PRECONDITION(aPI, "null ptr");
    if (!mProcessingInstructions.AppendElement(aPI)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

const nsTArray<nsXULPrototypePI*>&
nsXULPrototypeDocument::GetProcessingInstructions() const
{
    return mProcessingInstructions;
}

void
nsXULPrototypeDocument::AddStyleSheetReference(nsIURI* aURI)
{
    NS_PRECONDITION(aURI, "null ptr");
    if (!mStyleSheetReferences.AppendObject(aURI)) {
        NS_WARNING("mStyleSheetReferences->AppendElement() failed."
                   "Stylesheet overlay dropped.");
    }
}

const nsCOMArray<nsIURI>&
nsXULPrototypeDocument::GetStyleSheetReferences() const
{
    return mStyleSheetReferences;
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
nsXULPrototypeDocument::DocumentPrincipal()
{
    NS_PRECONDITION(mNodeInfoManager, "missing nodeInfoManager");
    return mNodeInfoManager->DocumentPrincipal();
}


nsNodeInfoManager*
nsXULPrototypeDocument::GetNodeInfoManager()
{
    return mNodeInfoManager;
}


nsresult
nsXULPrototypeDocument::AwaitLoadDone(nsXULDocument* aDocument, PRBool* aResult)
{
    nsresult rv = NS_OK;

    *aResult = mLoaded;

    if (!mLoaded) {
        rv = mPrototypeWaiters.AppendElement(aDocument)
              ? NS_OK : NS_ERROR_OUT_OF_MEMORY; // addrefs
    }

    return rv;
}


nsresult
nsXULPrototypeDocument::NotifyLoadDone()
{
    // Call back to each XUL document that raced to start the same
    // prototype document load, lost the race, but hit the XUL
    // prototype cache because the winner filled the cache with
    // the not-yet-loaded prototype object.

    nsresult rv = NS_OK;

    mLoaded = PR_TRUE;

    for (PRUint32 i = mPrototypeWaiters.Length(); i > 0; ) {
        --i;
        // PR_TRUE means that OnPrototypeLoadDone will also
        // call ResumeWalk().
        rv = mPrototypeWaiters[i]->OnPrototypeLoadDone(PR_TRUE);
        if (NS_FAILED(rv)) break;
    }
    mPrototypeWaiters.Clear();

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

nsXULPDGlobalObject::nsXULPDGlobalObject(nsXULPrototypeDocument* owner)
    :  mGlobalObjectOwner(owner)
{
  memset(mScriptGlobals, 0, sizeof(mScriptGlobals));
}


nsXULPDGlobalObject::~nsXULPDGlobalObject()
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULPDGlobalObject)
NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsXULPDGlobalObject)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULPDGlobalObject)
  {
    PRUint32 lang_index;
    NS_STID_FOR_INDEX(lang_index) {
      cb.NoteXPCOMChild(tmp->mScriptContexts[lang_index]);
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN(nsXULPDGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptGlobalObject)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsXULPDGlobalObject)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsXULPDGlobalObject,
                                          nsIScriptGlobalObject)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsXULPDGlobalObject,
                                           nsIScriptGlobalObject)

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

  NS_ASSERTION(!aScriptContext || !mScriptContexts[lang_ndx],
               "Bad call to SetContext()!");

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
  if (!aOwner) {
    PRUint32 lang_ndx;
    NS_STID_FOR_INDEX(lang_ndx) {
      if (mScriptContexts[lang_ndx]) {
          mScriptContexts[lang_ndx]->FinalizeContext();
          mScriptContexts[lang_ndx] = nsnull;
      }
    }
  } else {
    NS_NOTREACHED("You can only set an owner when constructing the object.");
  }
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

    return mGlobalObjectOwner->DocumentPrincipal();
}

