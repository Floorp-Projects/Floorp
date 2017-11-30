/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Preferences.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/ShadowRootBinding.h"
#include "mozilla/dom/DocumentFragment.h"
#include "ChildIterator.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsIDOMHTMLElement.h"
#include "nsIStyleSheetLinkingElement.h"
#include "mozilla/dom/Element.h"
#include "nsXBLPrototypeBinding.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(ShadowRoot)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ShadowRoot,
                                                  DocumentFragment)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStyleSheetList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAssociatedBinding)
  for (auto iter = tmp->mIdentifierMap.ConstIter(); !iter.Done();
       iter.Next()) {
    iter.Get()->Traverse(&cb);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ShadowRoot)
  if (tmp->GetHost()) {
    tmp->GetHost()->RemoveMutationObserver(tmp);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStyleSheetList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAssociatedBinding)
  tmp->mIdentifierMap.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(DocumentFragment)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ShadowRoot)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
NS_INTERFACE_MAP_END_INHERITING(DocumentFragment)

NS_IMPL_ADDREF_INHERITED(ShadowRoot, DocumentFragment)
NS_IMPL_RELEASE_INHERITED(ShadowRoot, DocumentFragment)

ShadowRoot::ShadowRoot(Element* aElement, bool aClosed,
                       already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                       nsXBLPrototypeBinding* aProtoBinding)
  : DocumentFragment(aNodeInfo)
  , mProtoBinding(aProtoBinding)
  , mInsertionPointChanged(false)
  , mIsComposedDocParticipant(false)
{
  SetHost(aElement);
  mMode = aClosed ? ShadowRootMode::Closed : ShadowRootMode::Open;

  // Nodes in a shadow tree should never store a value
  // in the subtree root pointer, nodes in the shadow tree
  // track the subtree root using GetContainingShadow().
  ClearSubtreeRootPointer();

  SetFlags(NODE_IS_IN_SHADOW_TREE);

  ExtendedDOMSlots()->mBindingParent = aElement;
  ExtendedDOMSlots()->mContainingShadow = this;

  // Add the ShadowRoot as a mutation observer on the host to watch
  // for mutations because the insertion points in this ShadowRoot
  // may need to be updated when the host children are modified.
  GetHost()->AddMutationObserver(this);
}

ShadowRoot::~ShadowRoot()
{
  if (auto* host = GetHost()) {
    // mHost may have been unlinked or a new ShadowRoot may have been
    // created, making this one obsolete.
    host->RemoveMutationObserver(this);
  }

  UnsetFlags(NODE_IS_IN_SHADOW_TREE);

  // nsINode destructor expects mSubtreeRoot == this.
  SetSubtreeRootPointer(this);
}

JSObject*
ShadowRoot::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::ShadowRootBinding::Wrap(aCx, this, aGivenProto);
}

ShadowRoot*
ShadowRoot::FromNode(nsINode* aNode)
{
  if (aNode->IsInShadowTree() && !aNode->GetParentNode()) {
    MOZ_ASSERT(aNode->NodeType() == nsIDOMNode::DOCUMENT_FRAGMENT_NODE,
               "ShadowRoot is a document fragment.");
    return static_cast<ShadowRoot*>(aNode);
  }

  return nullptr;
}

void
ShadowRoot::StyleSheetChanged()
{
  mProtoBinding->FlushSkinSheets();

  if (nsIPresShell* shell = OwnerDoc()->GetShell()) {
    OwnerDoc()->BeginUpdate(UPDATE_STYLE);
    shell->RecordShadowStyleChange(this);
    OwnerDoc()->EndUpdate(UPDATE_STYLE);
  }
}

void
ShadowRoot::InsertSheet(StyleSheet* aSheet,
                        nsIContent* aLinkingContent)
{
  nsCOMPtr<nsIStyleSheetLinkingElement>
    linkingElement = do_QueryInterface(aLinkingContent);
  MOZ_ASSERT(linkingElement, "The only styles in a ShadowRoot should come "
                             "from <style>.");

  linkingElement->SetStyleSheet(aSheet); // This sets the ownerNode on the sheet

  // Find the correct position to insert into the style sheet list (must
  // be in tree order).
  for (size_t i = 0; i <= mProtoBinding->SheetCount(); i++) {
    if (i == mProtoBinding->SheetCount()) {
      mProtoBinding->AppendStyleSheet(aSheet);
      break;
    }

    nsINode* sheetOwningNode = mProtoBinding->StyleSheetAt(i)->GetOwnerNode();
    if (nsContentUtils::PositionIsBefore(aLinkingContent, sheetOwningNode)) {
      mProtoBinding->InsertStyleSheetAt(i, aSheet);
      break;
    }
  }

  if (aSheet->IsApplicable()) {
    StyleSheetChanged();
  }
}

void
ShadowRoot::RemoveSheet(StyleSheet* aSheet)
{
  mProtoBinding->RemoveStyleSheet(aSheet);

  if (aSheet->IsApplicable()) {
    StyleSheetChanged();
  }
}

Element*
ShadowRoot::GetElementById(const nsAString& aElementId)
{
  nsIdentifierMapEntry *entry = mIdentifierMap.GetEntry(aElementId);
  return entry ? entry->GetIdElement() : nullptr;
}

already_AddRefed<nsContentList>
ShadowRoot::GetElementsByTagName(const nsAString& aTagName)
{
  return NS_GetContentList(this, kNameSpaceID_Unknown, aTagName);
}

already_AddRefed<nsContentList>
ShadowRoot::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                   const nsAString& aLocalName)
{
  int32_t nameSpaceId = kNameSpaceID_Wildcard;

  if (!aNamespaceURI.EqualsLiteral("*")) {
    nsresult rv =
      nsContentUtils::NameSpaceManager()->RegisterNameSpace(aNamespaceURI,
                                                            nameSpaceId);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  NS_ASSERTION(nameSpaceId != kNameSpaceID_Unknown, "Unexpected namespace ID!");

  return NS_GetContentList(this, nameSpaceId, aLocalName);
}

void
ShadowRoot::AddToIdTable(Element* aElement, nsAtom* aId)
{
  nsIdentifierMapEntry* entry = mIdentifierMap.PutEntry(aId);
  if (entry) {
    entry->AddIdElement(aElement);
  }
}

void
ShadowRoot::RemoveFromIdTable(Element* aElement, nsAtom* aId)
{
  nsIdentifierMapEntry* entry = mIdentifierMap.GetEntry(aId);
  if (entry) {
    entry->RemoveIdElement(aElement);
    if (entry->IsEmpty()) {
      mIdentifierMap.RemoveEntry(entry);
    }
  }
}

already_AddRefed<nsContentList>
ShadowRoot::GetElementsByClassName(const nsAString& aClasses)
{
  return nsContentUtils::GetElementsByClassName(this, aClasses);
}

void
ShadowRoot::DistributionChanged()
{
  // FIXME(emilio): We could be more granular in a bunch of cases.
  auto* host = GetHost();
  if (!host || !host->IsInComposedDoc()) {
    return;
  }

  auto* shell = OwnerDoc()->GetShell();
  if (!shell) {
    return;
  }

  shell->DestroyFramesForAndRestyle(host);
}

void
ShadowRoot::DistributeAllNodes()
{

  //XXX Handle <slot>.

  DistributionChanged();
}

void
ShadowRoot::GetInnerHTML(nsAString& aInnerHTML)
{
  GetMarkup(false, aInnerHTML);
}

void
ShadowRoot::SetInnerHTML(const nsAString& aInnerHTML, ErrorResult& aError)
{
  SetInnerHTMLInternal(aInnerHTML, aError);
}

Element*
ShadowRoot::Host()
{
  nsIContent* host = GetHost();
  MOZ_ASSERT(host && host->IsElement(),
             "ShadowRoot host should always be an element, "
             "how else did we create this ShadowRoot?");
  return host->AsElement();
}

bool
ShadowRoot::ApplyAuthorStyles()
{
  return mProtoBinding->InheritsStyle();
}

void
ShadowRoot::SetApplyAuthorStyles(bool aApplyAuthorStyles)
{
  mProtoBinding->SetInheritsStyle(aApplyAuthorStyles);

  nsIPresShell* shell = OwnerDoc()->GetShell();
  if (shell) {
    OwnerDoc()->BeginUpdate(UPDATE_STYLE);
    shell->RecordShadowStyleChange(this);
    OwnerDoc()->EndUpdate(UPDATE_STYLE);
  }
}

StyleSheetList*
ShadowRoot::StyleSheets()
{
  if (!mStyleSheetList) {
    mStyleSheetList = new ShadowRootStyleSheetList(this);
  }

  return mStyleSheetList;
}

/**
 * Returns whether the web components pool population algorithm
 * on the host would contain |aContent|. This function ignores
 * insertion points in the pool, thus should only be used to
 * test nodes that have not yet been distributed.
 */
bool
ShadowRoot::IsPooledNode(nsIContent* aContent) const
{
  if (nsContentUtils::IsContentInsertionPoint(aContent)) {
    // Insertion points never end up in the pool.
    return false;
  }

  auto* host = GetHost();
  auto* container = aContent->GetParent();
  if (container == host && !aContent->IsRootOfAnonymousSubtree()) {
    // Children of the host will end up in the pool. We check to ensure
    // that the content is in the same anonymous tree as the container
    // because anonymous content may report its container as the host
    // but it may not be in the host's child list.
    return true;
  }

  return false;
}

void
ShadowRoot::AttributeChanged(nsIDocument* aDocument,
                             Element* aElement,
                             int32_t aNameSpaceID,
                             nsAtom* aAttribute,
                             int32_t aModType,
                             const nsAttrValue* aOldValue)
{
  if (!IsPooledNode(aElement)) {
    return;
  }

  if (!aElement->IsInComposedDoc()) {
    return;
  }

  auto* shell = OwnerDoc()->GetShell();
  if (!shell) {
    return;
  }

  //XXX optimize this!
  shell->DestroyFramesForAndRestyle(aElement);
}

void
ShadowRoot::ContentAppended(nsIDocument* aDocument,
                            nsIContent* aContainer,
                            nsIContent* aFirstNewContent)
{
  for (nsIContent* content = aFirstNewContent;
       content;
       content = content->GetNextSibling()) {
    ContentInserted(aDocument, aContainer, aFirstNewContent);
  }
}

void
ShadowRoot::ContentInserted(nsIDocument* aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild)
{
  if (mInsertionPointChanged) {
    DistributeAllNodes();
    mInsertionPointChanged = false;
    return;
  }
}

void
ShadowRoot::ContentRemoved(nsIDocument* aDocument,
                           nsIContent* aContainer,
                           nsIContent* aChild,
                           nsIContent* aPreviousSibling)
{
  if (mInsertionPointChanged) {
    DistributeAllNodes();
    mInsertionPointChanged = false;
    return;
  }
}

nsresult
ShadowRoot::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                  bool aPreallocateChildren) const
{
  *aResult = nullptr;
  return NS_ERROR_DOM_DATA_CLONE_ERR;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(ShadowRootStyleSheetList, StyleSheetList,
                                   mShadowRoot)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ShadowRootStyleSheetList)
NS_INTERFACE_MAP_END_INHERITING(StyleSheetList)

NS_IMPL_ADDREF_INHERITED(ShadowRootStyleSheetList, StyleSheetList)
NS_IMPL_RELEASE_INHERITED(ShadowRootStyleSheetList, StyleSheetList)

ShadowRootStyleSheetList::ShadowRootStyleSheetList(ShadowRoot* aShadowRoot)
  : mShadowRoot(aShadowRoot)
{
}

ShadowRootStyleSheetList::~ShadowRootStyleSheetList()
{
}

StyleSheet*
ShadowRootStyleSheetList::IndexedGetter(uint32_t aIndex, bool& aFound)
{
  aFound = aIndex < mShadowRoot->mProtoBinding->SheetCount();
  if (!aFound) {
    return nullptr;
  }
  return mShadowRoot->mProtoBinding->StyleSheetAt(aIndex);
}

uint32_t
ShadowRootStyleSheetList::Length()
{
  return mShadowRoot->mProtoBinding->SheetCount();
}

