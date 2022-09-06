/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XPathResult.h"
#include "txExprResult.h"
#include "txNodeSet.h"
#include "nsError.h"
#include "mozilla/dom/Attr.h"
#include "mozilla/dom/Element.h"
#include "nsDOMString.h"
#include "txXPathTreeWalker.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/XPathResultBinding.h"

namespace mozilla::dom {

XPathResult::XPathResult(nsINode* aParent)
    : mParent(aParent),
      mDocument(nullptr),
      mCurrentPos(0),
      mResultType(ANY_TYPE),
      mInvalidIteratorState(true),
      mBooleanResult(false),
      mNumberResult(0) {}

XPathResult::XPathResult(const XPathResult& aResult)
    : mParent(aResult.mParent),
      mResult(aResult.mResult),
      mResultNodes(aResult.mResultNodes.Clone()),
      mDocument(aResult.mDocument),
      mContextNode(aResult.mContextNode),
      mCurrentPos(0),
      mResultType(aResult.mResultType),
      mInvalidIteratorState(aResult.mInvalidIteratorState),
      mBooleanResult(aResult.mBooleanResult),
      mNumberResult(aResult.mNumberResult),
      mStringResult(aResult.mStringResult) {
  if (mDocument) {
    mDocument->AddMutationObserver(this);
  }
}

XPathResult::~XPathResult() { RemoveObserver(); }

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(XPathResult)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(XPathResult)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent) { tmp->RemoveObserver(); }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(XPathResult)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResultNodes)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(XPathResult)
NS_IMPL_CYCLE_COLLECTING_RELEASE(XPathResult)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XPathResult)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* XPathResult::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return XPathResult_Binding::Wrap(aCx, this, aGivenProto);
}

void XPathResult::RemoveObserver() {
  if (mDocument) {
    mDocument->RemoveMutationObserver(this);
  }
}

nsINode* XPathResult::IterateNext(ErrorResult& aRv) {
  if (!isIterator()) {
    aRv.ThrowTypeError("Result is not an iterator");
    return nullptr;
  }

  if (mDocument) {
    mDocument->FlushPendingNotifications(FlushType::Content);
  }

  if (mInvalidIteratorState) {
    aRv.ThrowInvalidStateError(
        "The document has been mutated since the result was returned");
    return nullptr;
  }

  return mResultNodes.SafeElementAt(mCurrentPos++);
}

void XPathResult::NodeWillBeDestroyed(const nsINode* aNode) {
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  // Set to null to avoid unregistring unnecessarily
  mDocument = nullptr;
  Invalidate(aNode->IsContent() ? aNode->AsContent() : nullptr);
}

void XPathResult::CharacterDataChanged(nsIContent* aContent,
                                       const CharacterDataChangeInfo&) {
  Invalidate(aContent);
}

void XPathResult::AttributeChanged(Element* aElement, int32_t aNameSpaceID,
                                   nsAtom* aAttribute, int32_t aModType,
                                   const nsAttrValue* aOldValue) {
  Invalidate(aElement);
}

void XPathResult::ContentAppended(nsIContent* aFirstNewContent) {
  Invalidate(aFirstNewContent->GetParent());
}

void XPathResult::ContentInserted(nsIContent* aChild) {
  Invalidate(aChild->GetParent());
}

void XPathResult::ContentRemoved(nsIContent* aChild,
                                 nsIContent* aPreviousSibling) {
  Invalidate(aChild->GetParent());
}

void XPathResult::SetExprResult(txAExprResult* aExprResult,
                                uint16_t aResultType, nsINode* aContextNode,
                                ErrorResult& aRv) {
  MOZ_ASSERT(aExprResult);

  if ((isSnapshot(aResultType) || isIterator(aResultType) ||
       isNode(aResultType)) &&
      aExprResult->getResultType() != txAExprResult::NODESET) {
    // The DOM spec doesn't really say what should happen when reusing an
    // XPathResult and an error is thrown. Let's not touch the XPathResult
    // in that case.
    aRv.ThrowTypeError("Result type mismatch");
    return;
  }

  mResultType = aResultType;
  mContextNode = do_GetWeakReference(aContextNode);

  if (mDocument) {
    mDocument->RemoveMutationObserver(this);
    mDocument = nullptr;
  }

  mResultNodes.Clear();

  // XXX This will keep the recycler alive, should we clear it?
  mResult = aExprResult;
  switch (mResultType) {
    case BOOLEAN_TYPE: {
      mBooleanResult = mResult->booleanValue();
      break;
    }
    case NUMBER_TYPE: {
      mNumberResult = mResult->numberValue();
      break;
    }
    case STRING_TYPE: {
      mResult->stringValue(mStringResult);
      break;
    }
    default: {
      MOZ_ASSERT(isNode() || isIterator() || isSnapshot());
    }
  }

  if (aExprResult->getResultType() == txAExprResult::NODESET) {
    txNodeSet* nodeSet = static_cast<txNodeSet*>(aExprResult);
    int32_t i, count = nodeSet->size();
    for (i = 0; i < count; ++i) {
      nsINode* node = txXPathNativeNode::getNode(nodeSet->get(i));
      mResultNodes.AppendElement(node);
    }

    if (count > 0) {
      mResult = nullptr;
    }
  }

  if (!isIterator()) {
    return;
  }

  mCurrentPos = 0;
  mInvalidIteratorState = false;

  if (!mResultNodes.IsEmpty()) {
    // If we support the document() function in DOM-XPath we need to
    // observe all documents that we have resultnodes in.
    mDocument = mResultNodes[0]->OwnerDoc();
    NS_ASSERTION(mDocument, "We need a document!");
    if (mDocument) {
      mDocument->AddMutationObserver(this);
    }
  }
}

void XPathResult::Invalidate(const nsIContent* aChangeRoot) {
  nsCOMPtr<nsINode> contextNode = do_QueryReferent(mContextNode);
  // If the changes are happening in a different anonymous trees, no
  // invalidation should happen.
  if (contextNode && aChangeRoot &&
      !nsContentUtils::IsInSameAnonymousTree(contextNode, aChangeRoot)) {
    return;
  }

  mInvalidIteratorState = true;
  // Make sure nulling out mDocument is the last thing we do.
  if (mDocument) {
    mDocument->RemoveMutationObserver(this);
    mDocument = nullptr;
  }
}

nsresult XPathResult::GetExprResult(txAExprResult** aExprResult) {
  if (isIterator() && mInvalidIteratorState) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (mResult) {
    NS_ADDREF(*aExprResult = mResult);

    return NS_OK;
  }

  if (mResultNodes.IsEmpty()) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  RefPtr<txNodeSet> nodeSet = new txNodeSet(nullptr);
  uint32_t i, count = mResultNodes.Length();
  for (i = 0; i < count; ++i) {
    UniquePtr<txXPathNode> node(
        txXPathNativeNode::createXPathNode(mResultNodes[i]));
    if (!node) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nodeSet->append(*node);
  }

  NS_ADDREF(*aExprResult = nodeSet);

  return NS_OK;
}

already_AddRefed<XPathResult> XPathResult::Clone(ErrorResult& aError) {
  if (isIterator() && mInvalidIteratorState) {
    aError = NS_ERROR_DOM_INVALID_STATE_ERR;
    return nullptr;
  }

  return do_AddRef(new XPathResult(*this));
}

}  // namespace mozilla::dom
