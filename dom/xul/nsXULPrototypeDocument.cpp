/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsXULPrototypeDocument.h"
#include "XULDocument.h"

#include "nsAString.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIServiceManager.h"
#include "nsIArray.h"
#include "nsIURI.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsString.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsDOMCID.h"
#include "nsNodeInfoManager.h"
#include "nsContentUtils.h"
#include "nsCCUncollectableMarker.h"
#include "xpcpublic.h"
#include "mozilla/dom/BindingUtils.h"

using mozilla::dom::DestroyProtoAndIfaceCache;
using mozilla::dom::XULDocument;

uint32_t nsXULPrototypeDocument::gRefCnt;

//----------------------------------------------------------------------
//
// ctors, dtors, n' stuff
//

nsXULPrototypeDocument::nsXULPrototypeDocument()
    : mRoot(nullptr),
      mLoaded(false),
      mCCGeneration(0),
      mGCNumber(0)
{
    ++gRefCnt;
}


nsresult
nsXULPrototypeDocument::Init()
{
    mNodeInfoManager = new nsNodeInfoManager();
    return mNodeInfoManager->Init(nullptr);
}

nsXULPrototypeDocument::~nsXULPrototypeDocument()
{
    if (mRoot)
        mRoot->ReleaseSubtree();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULPrototypeDocument)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXULPrototypeDocument)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrototypeWaiters)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULPrototypeDocument)
    if (nsCCUncollectableMarker::InGeneration(cb, tmp->mCCGeneration)) {
        return NS_SUCCESS_INTERRUPTED_TRAVERSE;
    }
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRoot)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNodeInfoManager)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrototypeWaiters)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULPrototypeDocument)
    NS_INTERFACE_MAP_ENTRY(nsISerializable)
    NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULPrototypeDocument)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULPrototypeDocument)

NS_IMETHODIMP
NS_NewXULPrototypeDocument(nsXULPrototypeDocument** aResult)
{
    *aResult = nullptr;
    RefPtr<nsXULPrototypeDocument> doc =
      new nsXULPrototypeDocument();

    nsresult rv = doc->Init();
    if (NS_FAILED(rv)) {
        return rv;
    }

    doc.forget(aResult);
    return rv;
}

//----------------------------------------------------------------------
//
// nsISerializable methods
//

NS_IMETHODIMP
nsXULPrototypeDocument::Read(nsIObjectInputStream* aStream)
{
    nsresult rv;

    nsCOMPtr<nsISupports> supports;
    rv = aStream->ReadObject(true, getter_AddRefs(supports));
    mURI = do_QueryInterface(supports);

    uint32_t count, i;
    nsCOMPtr<nsIURI> styleOverlayURI;

    nsresult tmp = aStream->Read32(&count);
    if (NS_FAILED(tmp)) {
      return tmp;
    }
    if (NS_FAILED(rv)) {
      return rv;
    }

    for (i = 0; i < count; ++i) {
        tmp = aStream->ReadObject(true, getter_AddRefs(supports));
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }
        styleOverlayURI = do_QueryInterface(supports);
        mStyleSheetReferences.AppendObject(styleOverlayURI);
    }


    // nsIPrincipal mNodeInfoManager->mPrincipal
    nsCOMPtr<nsIPrincipal> principal;
    tmp = aStream->ReadObject(true, getter_AddRefs(supports));
    principal = do_QueryInterface(supports);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    // Better safe than sorry....
    mNodeInfoManager->SetDocumentPrincipal(principal);

    mRoot = new nsXULPrototypeElement();

    // mozilla::dom::NodeInfo table
    nsTArray<RefPtr<mozilla::dom::NodeInfo>> nodeInfos;

    tmp = aStream->Read32(&count);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    nsAutoString namespaceURI, prefixStr, localName;
    bool prefixIsNull;
    nsCOMPtr<nsIAtom> prefix;
    for (i = 0; i < count; ++i) {
        tmp = aStream->ReadString(namespaceURI);
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }
        tmp = aStream->ReadBoolean(&prefixIsNull);
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }
        if (prefixIsNull) {
            prefix = nullptr;
        } else {
            tmp = aStream->ReadString(prefixStr);
            if (NS_FAILED(tmp)) {
              rv = tmp;
            }
            prefix = NS_Atomize(prefixStr);
        }
        tmp = aStream->ReadString(localName);
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }

        RefPtr<mozilla::dom::NodeInfo> nodeInfo;
        // Using UINT16_MAX here as we don't know which nodeinfos will be
        // used for attributes and which for elements. And that doesn't really
        // matter.
        tmp = mNodeInfoManager->GetNodeInfo(localName, prefix, namespaceURI,
                                            UINT16_MAX,
                                            getter_AddRefs(nodeInfo));
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }
        nodeInfos.AppendElement(nodeInfo);
    }

    // Document contents
    uint32_t type;
    while (NS_SUCCEEDED(rv)) {
        tmp = aStream->Read32(&type);
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }

        if ((nsXULPrototypeNode::Type)type == nsXULPrototypeNode::eType_PI) {
            RefPtr<nsXULPrototypePI> pi = new nsXULPrototypePI();

            tmp = pi->Deserialize(aStream, this, mURI, &nodeInfos);
            if (NS_FAILED(tmp)) {
              rv = tmp;
            }
            tmp = AddProcessingInstruction(pi);
            if (NS_FAILED(tmp)) {
              rv = tmp;
            }
        } else if ((nsXULPrototypeNode::Type)type == nsXULPrototypeNode::eType_Element) {
            tmp = mRoot->Deserialize(aStream, this, mURI, &nodeInfos);
            if (NS_FAILED(tmp)) {
              rv = tmp;
            }
            break;
        } else {
            NS_NOTREACHED("Unexpected prototype node type");
            rv = NS_ERROR_FAILURE;
            break;
        }
    }
    tmp = NotifyLoadDone();
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }

    return rv;
}

static nsresult
GetNodeInfos(nsXULPrototypeElement* aPrototype,
             nsTArray<RefPtr<mozilla::dom::NodeInfo>>& aArray)
{
    if (aArray.IndexOf(aPrototype->mNodeInfo) == aArray.NoIndex) {
        aArray.AppendElement(aPrototype->mNodeInfo);
    }

    // Search attributes
    uint32_t i;
    for (i = 0; i < aPrototype->mNumAttributes; ++i) {
        RefPtr<mozilla::dom::NodeInfo> ni;
        nsAttrName* name = &aPrototype->mAttributes[i].mName;
        if (name->IsAtom()) {
            ni = aPrototype->mNodeInfo->NodeInfoManager()->
                GetNodeInfo(name->Atom(), nullptr, kNameSpaceID_None,
                            nsIDOMNode::ATTRIBUTE_NODE);
        }
        else {
            ni = name->NodeInfo();
        }

        if (aArray.IndexOf(ni) == aArray.NoIndex) {
            aArray.AppendElement(ni);
        }
    }

    // Search children
    for (i = 0; i < aPrototype->mChildren.Length(); ++i) {
        nsXULPrototypeNode* child = aPrototype->mChildren[i];
        if (child->mType == nsXULPrototypeNode::eType_Element) {
            nsresult rv =
              GetNodeInfos(static_cast<nsXULPrototypeElement*>(child), aArray);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULPrototypeDocument::Write(nsIObjectOutputStream* aStream)
{
    nsresult rv;

    rv = aStream->WriteCompoundObject(mURI, NS_GET_IID(nsIURI), true);
    
    uint32_t count;

    count = mStyleSheetReferences.Count();
    nsresult tmp = aStream->Write32(count);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }

    uint32_t i;
    for (i = 0; i < count; ++i) {
        tmp = aStream->WriteCompoundObject(mStyleSheetReferences[i],
                                           NS_GET_IID(nsIURI), true);
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }
    }

    // nsIPrincipal mNodeInfoManager->mPrincipal
    tmp = aStream->WriteObject(mNodeInfoManager->DocumentPrincipal(),
                               true);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    
#ifdef DEBUG
    // XXX Worrisome if we're caching things without system principal.
    if (!nsContentUtils::IsSystemPrincipal(mNodeInfoManager->DocumentPrincipal())) {
        NS_WARNING("Serializing document without system principal");
    }
#endif

    // mozilla::dom::NodeInfo table
    nsTArray<RefPtr<mozilla::dom::NodeInfo>> nodeInfos;
    if (mRoot) {
      tmp = GetNodeInfos(mRoot, nodeInfos);
      if (NS_FAILED(tmp)) {
        rv = tmp;
      }
    }

    uint32_t nodeInfoCount = nodeInfos.Length();
    tmp = aStream->Write32(nodeInfoCount);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
    for (i = 0; i < nodeInfoCount; ++i) {
        mozilla::dom::NodeInfo *nodeInfo = nodeInfos[i];
        NS_ENSURE_TRUE(nodeInfo, NS_ERROR_FAILURE);

        nsAutoString namespaceURI;
        nodeInfo->GetNamespaceURI(namespaceURI);
        tmp = aStream->WriteWStringZ(namespaceURI.get());
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }

        nsAutoString prefix;
        nodeInfo->GetPrefix(prefix);
        bool nullPrefix = DOMStringIsNull(prefix);
        tmp = aStream->WriteBoolean(nullPrefix);
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }
        if (!nullPrefix) {
            tmp = aStream->WriteWStringZ(prefix.get());
            if (NS_FAILED(tmp)) {
              rv = tmp;
            }
        }

        nsAutoString localName;
        nodeInfo->GetName(localName);
        tmp = aStream->WriteWStringZ(localName.get());
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }
    }

    // Now serialize the document contents
    count = mProcessingInstructions.Length();
    for (i = 0; i < count; ++i) {
        nsXULPrototypePI* pi = mProcessingInstructions[i];
        tmp = pi->Serialize(aStream, this, &nodeInfos);
        if (NS_FAILED(tmp)) {
          rv = tmp;
        }
    }

    if (mRoot) {
      tmp = mRoot->Serialize(aStream, this, &nodeInfos);
      if (NS_FAILED(tmp)) {
        rv = tmp;
      }
    }
 
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

const nsTArray<RefPtr<nsXULPrototypePI> >&
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

void
nsXULPrototypeDocument::SetDocumentPrincipal(nsIPrincipal* aPrincipal)
{
    mNodeInfoManager->SetDocumentPrincipal(aPrincipal);
}

void
nsXULPrototypeDocument::MarkInCCGeneration(uint32_t aCCGeneration)
{
    mCCGeneration = aCCGeneration;
}

nsNodeInfoManager*
nsXULPrototypeDocument::GetNodeInfoManager()
{
    return mNodeInfoManager;
}


nsresult
nsXULPrototypeDocument::AwaitLoadDone(XULDocument* aDocument, bool* aResult)
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

    mLoaded = true;

    for (uint32_t i = mPrototypeWaiters.Length(); i > 0; ) {
        --i;
        // true means that OnPrototypeLoadDone will also
        // call ResumeWalk().
        rv = mPrototypeWaiters[i]->OnPrototypeLoadDone(true);
        if (NS_FAILED(rv)) break;
    }
    mPrototypeWaiters.Clear();

    return rv;
}

void
nsXULPrototypeDocument::TraceProtos(JSTracer* aTrc, uint32_t aGCNumber)
{
  // Only trace the protos once per GC.
  if (mGCNumber == aGCNumber) {
    return;
  }

  mGCNumber = aGCNumber;
  if (mRoot) {
    mRoot->TraceAllScripts(aTrc);
  }
}
