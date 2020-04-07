/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULPrototypeDocument.h"

#include "nsXULElement.h"
#include "nsAString.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "nsString.h"
#include "nsDOMCID.h"
#include "nsNodeInfoManager.h"
#include "nsContentUtils.h"
#include "nsCCUncollectableMarker.h"
#include "xpcpublic.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsXULPrototypeCache.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Text.h"

using namespace mozilla;
using namespace mozilla::dom;
using mozilla::dom::DestroyProtoAndIfaceCache;

uint32_t nsXULPrototypeDocument::gRefCnt;

//----------------------------------------------------------------------
//
// ctors, dtors, n' stuff
//

nsXULPrototypeDocument::nsXULPrototypeDocument()
    : mRoot(nullptr),
      mLoaded(false),
      mCCGeneration(0),
      mGCNumber(0),
      mWasL10nCached(false) {
  ++gRefCnt;
}

nsresult nsXULPrototypeDocument::Init() {
  mNodeInfoManager = new nsNodeInfoManager();
  return mNodeInfoManager->Init(nullptr);
}

nsXULPrototypeDocument::~nsXULPrototypeDocument() {
  if (mRoot) mRoot->ReleaseSubtree();
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
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULPrototypeDocument)
  NS_INTERFACE_MAP_ENTRY(nsISerializable)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULPrototypeDocument)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULPrototypeDocument)

NS_IMETHODIMP
NS_NewXULPrototypeDocument(nsXULPrototypeDocument** aResult) {
  *aResult = nullptr;
  RefPtr<nsXULPrototypeDocument> doc = new nsXULPrototypeDocument();

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
nsXULPrototypeDocument::Read(nsIObjectInputStream* aStream) {
  nsCOMPtr<nsISupports> supports;
  nsresult rv = aStream->ReadObject(true, getter_AddRefs(supports));
  if (NS_FAILED(rv)) {
    return rv;
  }
  mURI = do_QueryInterface(supports);

  // nsIPrincipal mNodeInfoManager->mPrincipal
  nsAutoCString JSON;
  rv = aStream->ReadCString(JSON);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIPrincipal> principal = mozilla::BasePrincipal::FromJSON(JSON);

  // Better safe than sorry....
  mNodeInfoManager->SetDocumentPrincipal(principal);

  rv = aStream->ReadBoolean(&mWasL10nCached);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mRoot = new nsXULPrototypeElement();

  // mozilla::dom::NodeInfo table
  nsTArray<RefPtr<mozilla::dom::NodeInfo>> nodeInfos;

  uint32_t count, i;
  rv = aStream->Read32(&count);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoString namespaceURI, prefixStr, localName;
  bool prefixIsNull;
  RefPtr<nsAtom> prefix;
  for (i = 0; i < count; ++i) {
    rv = aStream->ReadString(namespaceURI);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = aStream->ReadBoolean(&prefixIsNull);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (prefixIsNull) {
      prefix = nullptr;
    } else {
      rv = aStream->ReadString(prefixStr);
      if (NS_FAILED(rv)) {
        return rv;
      }
      prefix = NS_Atomize(prefixStr);
    }
    rv = aStream->ReadString(localName);
    if (NS_FAILED(rv)) {
      return rv;
    }

    RefPtr<mozilla::dom::NodeInfo> nodeInfo;
    // Using UINT16_MAX here as we don't know which nodeinfos will be
    // used for attributes and which for elements. And that doesn't really
    // matter.
    rv = mNodeInfoManager->GetNodeInfo(localName, prefix, namespaceURI,
                                       UINT16_MAX, getter_AddRefs(nodeInfo));
    if (NS_FAILED(rv)) {
      return rv;
    }
    nodeInfos.AppendElement(nodeInfo);
  }

  // Document contents
  uint32_t type;
  while (NS_SUCCEEDED(rv)) {
    rv = aStream->Read32(&type);
    if (NS_FAILED(rv)) {
      return rv;
      break;
    }

    if ((nsXULPrototypeNode::Type)type == nsXULPrototypeNode::eType_PI) {
      RefPtr<nsXULPrototypePI> pi = new nsXULPrototypePI();

      rv = pi->Deserialize(aStream, this, mURI, &nodeInfos);
      if (NS_FAILED(rv)) {
        return rv;
      }
      rv = AddProcessingInstruction(pi);
      if (NS_FAILED(rv)) {
        return rv;
      }
    } else if ((nsXULPrototypeNode::Type)type ==
               nsXULPrototypeNode::eType_Element) {
      rv = mRoot->Deserialize(aStream, this, mURI, &nodeInfos);
      if (NS_FAILED(rv)) {
        return rv;
      }
      break;
    } else {
      MOZ_ASSERT_UNREACHABLE("Unexpected prototype node type");
      return NS_ERROR_FAILURE;
    }
  }

  return NotifyLoadDone();
}

static nsresult GetNodeInfos(nsXULPrototypeElement* aPrototype,
                             nsTArray<RefPtr<mozilla::dom::NodeInfo>>& aArray) {
  if (aArray.IndexOf(aPrototype->mNodeInfo) == aArray.NoIndex) {
    aArray.AppendElement(aPrototype->mNodeInfo);
  }

  // Search attributes
  size_t i;
  for (i = 0; i < aPrototype->mAttributes.Length(); ++i) {
    RefPtr<mozilla::dom::NodeInfo> ni;
    nsAttrName* name = &aPrototype->mAttributes[i].mName;
    if (name->IsAtom()) {
      ni = aPrototype->mNodeInfo->NodeInfoManager()->GetNodeInfo(
          name->Atom(), nullptr, kNameSpaceID_None, nsINode::ATTRIBUTE_NODE);
    } else {
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
nsXULPrototypeDocument::Write(nsIObjectOutputStream* aStream) {
  nsresult rv;

  rv = aStream->WriteCompoundObject(mURI, NS_GET_IID(nsIURI), true);

  // nsIPrincipal mNodeInfoManager->mPrincipal
  nsAutoCString JSON;
  mozilla::BasePrincipal::Cast(mNodeInfoManager->DocumentPrincipal())
      ->ToJSON(JSON);
  nsresult tmp = aStream->WriteStringZ(JSON.get());
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }

#ifdef DEBUG
  // XXX Worrisome if we're caching things without system principal.
  if (!mNodeInfoManager->DocumentPrincipal()->IsSystemPrincipal()) {
    NS_WARNING("Serializing document without system principal");
  }
#endif

  tmp = aStream->WriteBoolean(mWasL10nCached);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }

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
  uint32_t i;
  for (i = 0; i < nodeInfoCount; ++i) {
    mozilla::dom::NodeInfo* nodeInfo = nodeInfos[i];
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
  uint32_t count = mProcessingInstructions.Length();
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

nsresult nsXULPrototypeDocument::InitPrincipal(nsIURI* aURI,
                                               nsIPrincipal* aPrincipal) {
  NS_ENSURE_ARG_POINTER(aURI);

  mURI = aURI;
  mNodeInfoManager->SetDocumentPrincipal(aPrincipal);
  return NS_OK;
}

nsIURI* nsXULPrototypeDocument::GetURI() {
  NS_ASSERTION(mURI, "null URI");
  return mURI;
}

nsXULPrototypeElement* nsXULPrototypeDocument::GetRootElement() {
  return mRoot;
}

void nsXULPrototypeDocument::SetRootElement(nsXULPrototypeElement* aElement) {
  mRoot = aElement;
}

nsresult nsXULPrototypeDocument::AddProcessingInstruction(
    nsXULPrototypePI* aPI) {
  MOZ_ASSERT(aPI, "null ptr");
  if (!mProcessingInstructions.AppendElement(aPI)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

const nsTArray<RefPtr<nsXULPrototypePI>>&
nsXULPrototypeDocument::GetProcessingInstructions() const {
  return mProcessingInstructions;
}

nsIPrincipal* nsXULPrototypeDocument::DocumentPrincipal() {
  MOZ_ASSERT(mNodeInfoManager, "missing nodeInfoManager");
  return mNodeInfoManager->DocumentPrincipal();
}

void nsXULPrototypeDocument::SetDocumentPrincipal(nsIPrincipal* aPrincipal) {
  mNodeInfoManager->SetDocumentPrincipal(aPrincipal);
}

void nsXULPrototypeDocument::MarkInCCGeneration(uint32_t aCCGeneration) {
  mCCGeneration = aCCGeneration;
}

nsNodeInfoManager* nsXULPrototypeDocument::GetNodeInfoManager() {
  return mNodeInfoManager;
}

nsresult nsXULPrototypeDocument::AwaitLoadDone(Callback&& aCallback,
                                               bool* aResult) {
  nsresult rv = NS_OK;

  *aResult = mLoaded;

  if (!mLoaded) {
    rv = mPrototypeWaiters.AppendElement(std::move(aCallback))
             ? NS_OK
             : NS_ERROR_OUT_OF_MEMORY;  // addrefs
  }

  return rv;
}

nsresult nsXULPrototypeDocument::NotifyLoadDone() {
  // Call back to each XUL document that raced to start the same
  // prototype document load, lost the race, but hit the XUL
  // prototype cache because the winner filled the cache with
  // the not-yet-loaded prototype object.

  mLoaded = true;

  for (uint32_t i = mPrototypeWaiters.Length(); i > 0;) {
    --i;
    mPrototypeWaiters[i]();
  }
  mPrototypeWaiters.Clear();

  return NS_OK;
}

void nsXULPrototypeDocument::TraceProtos(JSTracer* aTrc) {
  // Only trace the protos once per GC if we are marking.
  if (aTrc->isMarkingTracer()) {
    uint32_t currentGCNumber = aTrc->gcNumberForMarking();
    if (mGCNumber == currentGCNumber) {
      return;
    }
    mGCNumber = currentGCNumber;
  }

  if (mRoot) {
    mRoot->TraceAllScripts(aTrc);
  }
}

void nsXULPrototypeDocument::SetIsL10nCached() { mWasL10nCached = true; }

void nsXULPrototypeDocument::RebuildPrototypeFromElement(
    nsXULPrototypeElement* aPrototype, Element* aElement, bool aDeep) {
  aPrototype->mHasIdAttribute = aElement->HasID();
  aPrototype->mHasClassAttribute = aElement->MayHaveClass();
  aPrototype->mHasStyleAttribute = aElement->MayHaveStyle();
  NodeInfo* oldNodeInfo = aElement->NodeInfo();
  RefPtr<NodeInfo> newNodeInfo = mNodeInfoManager->GetNodeInfo(
      oldNodeInfo->NameAtom(), oldNodeInfo->GetPrefixAtom(),
      oldNodeInfo->NamespaceID(), nsINode::ELEMENT_NODE);
  aPrototype->mNodeInfo = newNodeInfo;

  // First replace the prototype attributes with the new ones from this element.
  aPrototype->mAttributes.Clear();

  uint32_t count = aElement->GetAttrCount();
  nsXULPrototypeAttribute* protoAttr =
      aPrototype->mAttributes.AppendElements(count);
  for (uint32_t index = 0; index < count; index++) {
    BorrowedAttrInfo attr = aElement->GetAttrInfoAt(index);

    if (attr.mName->IsAtom()) {
      protoAttr->mName.SetTo(attr.mName->Atom());
    } else {
      NodeInfo* oldNodeInfo = attr.mName->NodeInfo();
      RefPtr<NodeInfo> newNodeInfo = mNodeInfoManager->GetNodeInfo(
          oldNodeInfo->NameAtom(), oldNodeInfo->GetPrefixAtom(),
          oldNodeInfo->NamespaceID(), nsINode::ATTRIBUTE_NODE);
      protoAttr->mName.SetTo(newNodeInfo);
    }
    protoAttr->mValue.SetTo(*attr.mValue);

    protoAttr++;
  }

  if (aDeep) {
    // We have to rebuild the prototype children from this element.
    // First release the tree under this element.
    aPrototype->ReleaseSubtree();

    RefPtr<nsXULPrototypeNode>* children =
        aPrototype->mChildren.AppendElements(aElement->GetChildCount());
    for (nsIContent* child = aElement->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      if (child->IsElement()) {
        Element* element = child->AsElement();
        RefPtr<nsXULPrototypeElement> elemProto = new nsXULPrototypeElement;
        RebuildPrototypeFromElement(elemProto, element, true);
        *children = elemProto;
      } else if (child->IsText()) {
        Text* text = child->AsText();
        RefPtr<nsXULPrototypeText> textProto = new nsXULPrototypeText();
        text->AppendTextTo(textProto->mValue);
        *children = textProto;
      } else {
        MOZ_ASSERT(false, "We handle only elements and text nodes here.");
      }

      children++;
    }
  }
}

void nsXULPrototypeDocument::RebuildL10nPrototype(Element* aElement,
                                                  bool aDeep) {
  if (mWasL10nCached) {
    return;
  }

  Document* doc = aElement->OwnerDoc();

  nsAutoString id;
  MOZ_RELEASE_ASSERT(aElement->GetAttr(nsGkAtoms::datal10nid, id));

  if (!doc) {
    return;
  }

  RefPtr<nsXULPrototypeElement> proto = doc->mL10nProtoElements.Get(aElement);
  MOZ_RELEASE_ASSERT(proto);
  RebuildPrototypeFromElement(proto, aElement, aDeep);
}
