/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XPathResult.h"
#include "txExprResult.h"
#include "txNodeSet.h"
#include "nsError.h"
#include "mozilla/dom/Attr.h"
#include "mozilla/dom/Element.h"
#include "nsDOMClassInfoID.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsDOMString.h"
#include "txXPathTreeWalker.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/XPathResultBinding.h"

namespace mozilla {
namespace dom {

XPathResult::XPathResult(nsINode* aParent)
    : mParent(aParent),
      mDocument(nullptr),
      mCurrentPos(0),
      mResultType(ANY_TYPE),
      mInvalidIteratorState(true),
      mBooleanResult(false),
      mNumberResult(0)
{
}

XPathResult::XPathResult(const XPathResult &aResult)
    : mParent(aResult.mParent),
      mResult(aResult.mResult),
      mResultNodes(aResult.mResultNodes),
      mDocument(aResult.mDocument),
      mContextNode(aResult.mContextNode),
      mCurrentPos(0),
      mResultType(aResult.mResultType),
      mInvalidIteratorState(aResult.mInvalidIteratorState)
{
    if (mDocument) {
        mDocument->AddMutationObserver(this);
    }
}

XPathResult::~XPathResult()
{
    RemoveObserver();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(XPathResult)

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(XPathResult)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(XPathResult)
    NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
    {
        tmp->RemoveObserver();
    }
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(XPathResult)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResultNodes)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(XPathResult)
NS_IMPL_CYCLE_COLLECTING_RELEASE(XPathResult)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XPathResult)
    NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
    NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
    NS_INTERFACE_MAP_ENTRY(nsIXPathResult)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPathResult)
NS_INTERFACE_MAP_END

JSObject*
XPathResult::WrapObject(JSContext* aCx)
{
    return XPathResultBinding::Wrap(aCx, this);
}

void
XPathResult::RemoveObserver()
{
    if (mDocument) {
        mDocument->RemoveMutationObserver(this);
    }
}

nsINode*
XPathResult::IterateNext(ErrorResult& aRv)
{
    if (!isIterator()) {
        aRv.Throw(NS_ERROR_DOM_TYPE_ERR);
        return nullptr;
    }

    if (mDocument) {
        mDocument->FlushPendingNotifications(Flush_Content);
    }

    if (mInvalidIteratorState) {
        aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
        return nullptr;
    }

    return mResultNodes.SafeObjectAt(mCurrentPos++);
}

void
XPathResult::NodeWillBeDestroyed(const nsINode* aNode)
{
    nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
    // Set to null to avoid unregistring unnecessarily
    mDocument = nullptr;
    Invalidate(aNode->IsNodeOfType(nsINode::eCONTENT) ?
               static_cast<const nsIContent*>(aNode) : nullptr);
}

void
XPathResult::CharacterDataChanged(nsIDocument* aDocument,
                                  nsIContent *aContent,
                                  CharacterDataChangeInfo* aInfo)
{
    Invalidate(aContent);
}

void
XPathResult::AttributeChanged(nsIDocument* aDocument,
                              Element* aElement,
                              int32_t aNameSpaceID,
                              nsIAtom* aAttribute,
                              int32_t aModType)
{
    Invalidate(aElement);
}

void
XPathResult::ContentAppended(nsIDocument* aDocument,
                             nsIContent* aContainer,
                             nsIContent* aFirstNewContent,
                             int32_t aNewIndexInContainer)
{
    Invalidate(aContainer);
}

void
XPathResult::ContentInserted(nsIDocument* aDocument,
                             nsIContent* aContainer,
                             nsIContent* aChild,
                             int32_t aIndexInContainer)
{
    Invalidate(aContainer);
}

void
XPathResult::ContentRemoved(nsIDocument* aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            int32_t aIndexInContainer,
                            nsIContent* aPreviousSibling)
{
    Invalidate(aContainer);
}

nsresult
XPathResult::SetExprResult(txAExprResult* aExprResult, uint16_t aResultType,
                           nsINode* aContextNode)
{
    MOZ_ASSERT(aExprResult);

    if ((isSnapshot(aResultType) || isIterator(aResultType) ||
         isNode(aResultType)) &&
        aExprResult->getResultType() != txAExprResult::NODESET) {
        // The DOM spec doesn't really say what should happen when reusing an
        // XPathResult and an error is thrown. Let's not touch the XPathResult
        // in that case.
        return NS_ERROR_DOM_TYPE_ERR;
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
        case BOOLEAN_TYPE:
        {
            mBooleanResult = mResult->booleanValue();
            break;
        }
        case NUMBER_TYPE:
        {
            mNumberResult = mResult->numberValue();
            break;
        }
        case STRING_TYPE:
        {
            mResult->stringValue(mStringResult);
            break;
        }
        default:
        {
            MOZ_ASSERT(isNode() || isIterator() || isSnapshot());
        }
    }

    if (aExprResult->getResultType() == txAExprResult::NODESET) {
        txNodeSet *nodeSet = static_cast<txNodeSet*>(aExprResult);
        int32_t i, count = nodeSet->size();
        for (i = 0; i < count; ++i) {
            nsINode* node = txXPathNativeNode::getNode(nodeSet->get(i));
            mResultNodes.AppendObject(node);
        }

        if (count > 0) {
            mResult = nullptr;
        }
    }

    if (!isIterator()) {
        return NS_OK;
    }

    mInvalidIteratorState = false;

    if (mResultNodes.Count() > 0) {
        // If we support the document() function in DOM-XPath we need to
        // observe all documents that we have resultnodes in.
        mDocument = mResultNodes[0]->OwnerDoc();
        NS_ASSERTION(mDocument, "We need a document!");
        if (mDocument) {
            mDocument->AddMutationObserver(this);
        }
    }

    return NS_OK;
}

void
XPathResult::Invalidate(const nsIContent* aChangeRoot)
{
    nsCOMPtr<nsINode> contextNode = do_QueryReferent(mContextNode);
    if (contextNode && aChangeRoot && aChangeRoot->GetBindingParent()) {
        // If context node is in anonymous content, changes to
        // non-anonymous content need to invalidate the XPathResult. If
        // the changes are happening in a different anonymous trees, no
        // invalidation should happen.
        nsIContent* ctxBindingParent = nullptr;
        if (contextNode->IsNodeOfType(nsINode::eCONTENT)) {
            ctxBindingParent =
                static_cast<nsIContent*>(contextNode.get())
                    ->GetBindingParent();
        } else if (contextNode->IsNodeOfType(nsINode::eATTRIBUTE)) {
            Element* parent =
              static_cast<Attr*>(contextNode.get())->GetElement();
            if (parent) {
                ctxBindingParent = parent->GetBindingParent();
            }
        }
        if (ctxBindingParent != aChangeRoot->GetBindingParent()) {
          return;
        }
    }

    mInvalidIteratorState = true;
    // Make sure nulling out mDocument is the last thing we do.
    if (mDocument) {
        mDocument->RemoveMutationObserver(this);
        mDocument = nullptr;
    }
}

nsresult
XPathResult::GetExprResult(txAExprResult** aExprResult)
{
    if (isIterator() && mInvalidIteratorState) {
        return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    if (mResult) {
        NS_ADDREF(*aExprResult = mResult);

        return NS_OK;
    }

    if (mResultNodes.Count() == 0) {
        return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    nsRefPtr<txNodeSet> nodeSet = new txNodeSet(nullptr);
    if (!nodeSet) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    uint32_t i, count = mResultNodes.Count();
    for (i = 0; i < count; ++i) {
        nsAutoPtr<txXPathNode> node(txXPathNativeNode::createXPathNode(mResultNodes[i]));
        if (!node) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        nodeSet->append(*node);
    }

    NS_ADDREF(*aExprResult = nodeSet);

    return NS_OK;
}

nsresult
XPathResult::Clone(nsIXPathResult **aResult)
{
    *aResult = nullptr;

    if (isIterator() && mInvalidIteratorState) {
        return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    NS_ADDREF(*aResult = new XPathResult(*this));

    return NS_OK;
}

} // namespace dom
} // namespace mozilla
