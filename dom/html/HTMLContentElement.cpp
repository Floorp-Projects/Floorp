/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLContentElement.h"
#include "mozilla/dom/HTMLContentElementBinding.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "mozilla/dom/NodeListBinding.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/css/StyleRule.h"
#include "mozilla/GenericSpecifiedValuesInlines.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsIAtom.h"
#include "nsCSSRuleProcessor.h"
#include "nsRuleProcessorData.h"
#include "nsRuleWalker.h"
#include "nsCSSParser.h"
#include "nsDocument.h"

// Expand NS_IMPL_NS_NEW_HTML_ELEMENT(Content) to add check for web components
// being enabled.
nsGenericHTMLElement*
NS_NewHTMLContentElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                         mozilla::dom::FromParser aFromParser)
{
  // When this check is removed, remove the nsDocument.h and
  // HTMLUnknownElement.h includes.  Also remove nsINode::IsHTMLContentElement.
  //
  // We have to jump through some hoops to be able to produce both NodeInfo* and
  // already_AddRefed<NodeInfo>& for our callees.
  RefPtr<mozilla::dom::NodeInfo> nodeInfo(aNodeInfo);
  if (!nsDocument::IsWebComponentsEnabled(nodeInfo)) {
    already_AddRefed<mozilla::dom::NodeInfo> nodeInfoArg(nodeInfo.forget());
    return new mozilla::dom::HTMLUnknownElement(nodeInfoArg);
  }

  already_AddRefed<mozilla::dom::NodeInfo> nodeInfoArg(nodeInfo.forget());
  return new mozilla::dom::HTMLContentElement(nodeInfoArg);
}

using namespace mozilla::dom;

HTMLContentElement::HTMLContentElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo), mValidSelector(true), mIsInsertionPoint(false)
{
}

HTMLContentElement::~HTMLContentElement()
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLContentElement,
                                   nsGenericHTMLElement,
                                   mMatchedNodes)

NS_IMPL_ADDREF_INHERITED(HTMLContentElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLContentElement, Element)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(HTMLContentElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLContentElement)

JSObject*
HTMLContentElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLContentElementBinding::Wrap(aCx, this, aGivenProto);
}

nsresult
HTMLContentElement::BindToTree(nsIDocument* aDocument,
                               nsIContent* aParent,
                               nsIContent* aBindingParent,
                               bool aCompileEventHandlers)
{
  RefPtr<ShadowRoot> oldContainingShadow = GetContainingShadow();

  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  ShadowRoot* containingShadow = GetContainingShadow();
  if (containingShadow && !oldContainingShadow) {
    nsINode* parentNode = nsINode::GetParentNode();
    while (parentNode && parentNode != containingShadow) {
      if (parentNode->IsHTMLContentElement()) {
        // Content element in fallback content is not an insertion point.
        return NS_OK;
      }
      parentNode = parentNode->GetParentNode();
    }

    // If the content element is being inserted into a ShadowRoot,
    // add this element to the list of insertion points.
    mIsInsertionPoint = true;
    containingShadow->AddInsertionPoint(this);
    containingShadow->SetInsertionPointChanged();
  }

  return NS_OK;
}

void
HTMLContentElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  RefPtr<ShadowRoot> oldContainingShadow = GetContainingShadow();

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

  if (oldContainingShadow && !GetContainingShadow() && mIsInsertionPoint) {
    oldContainingShadow->RemoveInsertionPoint(this);

    // Remove all the matched nodes now that the
    // insertion point is no longer an insertion point.
    ClearMatchedNodes();
    oldContainingShadow->SetInsertionPointChanged();

    mIsInsertionPoint = false;
  }
}

void
HTMLContentElement::AppendMatchedNode(nsIContent* aContent)
{
  mMatchedNodes.AppendElement(aContent);
  nsTArray<nsIContent*>& destInsertionPoint = aContent->DestInsertionPoints();
  destInsertionPoint.AppendElement(this);

  if (mMatchedNodes.Length() == 1) {
    // Fallback content gets dropped so we need to updated fallback
    // content distribution.
    UpdateFallbackDistribution();
  }
}

void
HTMLContentElement::UpdateFallbackDistribution()
{
  for (nsIContent* child = nsINode::GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    nsTArray<nsIContent*>& destInsertionPoint = child->DestInsertionPoints();
    destInsertionPoint.Clear();
    if (mMatchedNodes.IsEmpty()) {
      destInsertionPoint.AppendElement(this);
    }
  }
}

void
HTMLContentElement::RemoveMatchedNode(nsIContent* aContent)
{
  mMatchedNodes.RemoveElement(aContent);
  ShadowRoot::RemoveDestInsertionPoint(this, aContent->DestInsertionPoints());

  if (mMatchedNodes.IsEmpty()) {
    // Fallback content is activated so we need to update fallback
    // content distribution.
    UpdateFallbackDistribution();
  }
}

void
HTMLContentElement::InsertMatchedNode(uint32_t aIndex, nsIContent* aContent)
{
  mMatchedNodes.InsertElementAt(aIndex, aContent);
  nsTArray<nsIContent*>& destInsertionPoint = aContent->DestInsertionPoints();
  destInsertionPoint.AppendElement(this);

  if (mMatchedNodes.Length() == 1) {
    // Fallback content gets dropped so we need to updated fallback
    // content distribution.
    UpdateFallbackDistribution();
  }
}

void
HTMLContentElement::ClearMatchedNodes()
{
  for (uint32_t i = 0; i < mMatchedNodes.Length(); i++) {
    ShadowRoot::RemoveDestInsertionPoint(this, mMatchedNodes[i]->DestInsertionPoints());
  }

  mMatchedNodes.Clear();

  UpdateFallbackDistribution();
}

static bool
IsValidContentSelectors(nsCSSSelector* aSelector)
{
  nsCSSSelector* currentSelector = aSelector;
  while (currentSelector) {
    // Blacklist invalid selector fragments.
    if (currentSelector->IsPseudoElement() ||
        currentSelector->mPseudoClassList ||
        currentSelector->mNegations ||
        currentSelector->mOperator) {
      return false;
    }

    currentSelector = currentSelector->mNext;
  }

  return true;
}

nsresult
HTMLContentElement::AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                 const nsAttrValue* aValue,
                                 const nsAttrValue* aOldValue, bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None && aName == nsGkAtoms::select) {
    if (aValue) {
      // Select attribute was updated, the insertion point may match different
      // elements.
      nsIDocument* doc = OwnerDoc();
      nsCSSParser parser(doc->CSSLoader());

      mValidSelector = true;
      mSelectorList = nullptr;

      nsAutoString valueStr;
      aValue->ToString(valueStr);
      nsresult rv = parser.ParseSelectorString(valueStr,
                                               doc->GetDocumentURI(),
                                               // Bug 11240
                                               0, // XXX get the line number!
                                               getter_Transfers(mSelectorList));

      // We don't want to return an exception if parsing failed because
      // the spec does not define it as an exception case.
      if (NS_SUCCEEDED(rv)) {
        // Ensure that all the selectors are valid
        nsCSSSelectorList* selectors = mSelectorList;
        while (selectors) {
          if (!IsValidContentSelectors(selectors->mSelectors)) {
            // If we have an invalid selector, we can not match anything.
            mValidSelector = false;
            mSelectorList = nullptr;
            break;
          }
          selectors = selectors->mNext;
        }
      }

      ShadowRoot* containingShadow = GetContainingShadow();
      if (containingShadow) {
        containingShadow->DistributeAllNodes();
      }
    } else {
      // The select attribute was removed. This insertion point becomes
      // a universal selector.
      mValidSelector = true;
      mSelectorList = nullptr;

      ShadowRoot* containingShadow = GetContainingShadow();
      if (containingShadow) {
        containingShadow->DistributeAllNodes();
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNamespaceID, aName, aValue,
                                            aOldValue, aNotify);
}

bool
HTMLContentElement::Match(nsIContent* aContent)
{
  if (!mValidSelector) {
    return false;
  }

  if (mSelectorList) {
    nsIDocument* doc = OwnerDoc();
    ShadowRoot* containingShadow = GetContainingShadow();
    nsIContent* host = containingShadow->GetHost();

    TreeMatchContext matchingContext(false, nsRuleWalker::eRelevantLinkUnvisited,
                                     doc, TreeMatchContext::eNeverMatchVisited);
    matchingContext.SetHasSpecifiedScope();
    matchingContext.AddScopeElement(host->AsElement());

    if (!aContent->IsElement()) {
      return false;
    }

    return nsCSSRuleProcessor::SelectorListMatches(aContent->AsElement(),
                                                   matchingContext,
                                                   mSelectorList);
  }

  return true;
}

already_AddRefed<DistributedContentList>
HTMLContentElement::GetDistributedNodes()
{
  RefPtr<DistributedContentList> list = new DistributedContentList(this);
  return list.forget();
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DistributedContentList, mParent,
                                      mDistributedNodes)

NS_INTERFACE_TABLE_HEAD(DistributedContentList)
  NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
  NS_INTERFACE_TABLE(DistributedContentList, nsINodeList, nsIDOMNodeList)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(DistributedContentList)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DistributedContentList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DistributedContentList)

DistributedContentList::DistributedContentList(HTMLContentElement* aHostElement)
  : mParent(aHostElement)
{
  if (aHostElement->IsInsertionPoint()) {
    if (aHostElement->MatchedNodes().IsEmpty()) {
      // Fallback content.
      nsINode* contentNode = aHostElement;
      for (nsIContent* content = contentNode->GetFirstChild();
           content;
           content = content->GetNextSibling()) {
        mDistributedNodes.AppendElement(content);
      }
    } else {
      mDistributedNodes.AppendElements(aHostElement->MatchedNodes());
    }
  }
}

DistributedContentList::~DistributedContentList()
{
}

nsIContent*
DistributedContentList::Item(uint32_t aIndex)
{
  return mDistributedNodes.SafeElementAt(aIndex);
}

NS_IMETHODIMP
DistributedContentList::Item(uint32_t aIndex, nsIDOMNode** aReturn)
{
  nsIContent* item = Item(aIndex);
  if (!item) {
    return NS_ERROR_FAILURE;
  }

  return CallQueryInterface(item, aReturn);
}

NS_IMETHODIMP
DistributedContentList::GetLength(uint32_t* aLength)
{
  *aLength = mDistributedNodes.Length();
  return NS_OK;
}

int32_t
DistributedContentList::IndexOf(nsIContent* aContent)
{
  return mDistributedNodes.IndexOf(aContent);
}

JSObject*
DistributedContentList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return NodeListBinding::Wrap(aCx, this, aGivenProto);
}

