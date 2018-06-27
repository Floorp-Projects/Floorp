/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/HTMLSlotElementBinding.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsGkAtoms.h"
#include "nsDocument.h"

nsGenericHTMLElement*
NS_NewHTMLSlotElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                      mozilla::dom::FromParser aFromParser)
{
  RefPtr<mozilla::dom::NodeInfo> nodeInfo(aNodeInfo);
  if (nsDocument::IsShadowDOMEnabled(nodeInfo->GetDocument())) {
    already_AddRefed<mozilla::dom::NodeInfo> nodeInfoArg(nodeInfo.forget());
    return new mozilla::dom::HTMLSlotElement(nodeInfoArg);
  }

  already_AddRefed<mozilla::dom::NodeInfo> nodeInfoArg(nodeInfo.forget());
  return new mozilla::dom::HTMLUnknownElement(nodeInfoArg);
}

namespace mozilla {
namespace dom {

HTMLSlotElement::HTMLSlotElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

HTMLSlotElement::~HTMLSlotElement() = default;

NS_IMPL_ADDREF_INHERITED(HTMLSlotElement, nsGenericHTMLElement)
NS_IMPL_RELEASE_INHERITED(HTMLSlotElement, nsGenericHTMLElement)

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLSlotElement,
                                   nsGenericHTMLElement,
                                   mAssignedNodes)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HTMLSlotElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLSlotElement)

nsresult
HTMLSlotElement::BindToTree(nsIDocument* aDocument,
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
    containingShadow->AddSlot(this);
  }

  return NS_OK;
}

void
HTMLSlotElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  RefPtr<ShadowRoot> oldContainingShadow = GetContainingShadow();

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);

  if (oldContainingShadow && !GetContainingShadow()) {
    oldContainingShadow->RemoveSlot(this);
  }
}

nsresult
HTMLSlotElement::BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                               const nsAttrValueOrString* aValue,
                               bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::name) {
    if (ShadowRoot* containingShadow = GetContainingShadow()) {
      containingShadow->RemoveSlot(this);
    }
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName, aValue,
                                             aNotify);
}

nsresult
HTMLSlotElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                              const nsAttrValue* aValue,
                              const nsAttrValue* aOldValue,
                              nsIPrincipal* aSubjectPrincipal,
                              bool aNotify)
{

  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::name) {
    if (ShadowRoot* containingShadow = GetContainingShadow()) {
      containingShadow->AddSlot(this);
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aOldValue, aSubjectPrincipal,
                                            aNotify);
}

/**
 * Flatten assigned nodes given a slot, as in:
 * https://dom.spec.whatwg.org/#find-flattened-slotables
 */
static void
FlattenAssignedNodes(HTMLSlotElement* aSlot, nsTArray<RefPtr<nsINode>>& aNodes)
{
  if (!aSlot->GetContainingShadow()) {
    return;
  }

  const nsTArray<RefPtr<nsINode>>& assignedNodes = aSlot->AssignedNodes();

  // If assignedNodes is empty, use children of slot as fallback content.
  if (assignedNodes.IsEmpty()) {
    for (nsIContent* child = aSlot->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      if (!child->IsSlotable()) {
        continue;
      }

      if (auto* slot = HTMLSlotElement::FromNode(child)) {
        FlattenAssignedNodes(slot, aNodes);
      } else {
        aNodes.AppendElement(child);
      }
    }
    return;
  }

  for (const RefPtr<nsINode>& assignedNode : assignedNodes) {
    if (auto* slot = HTMLSlotElement::FromNode(assignedNode)) {
      FlattenAssignedNodes(slot, aNodes);
    } else {
      aNodes.AppendElement(assignedNode);
    }
  }
}

void
HTMLSlotElement::AssignedNodes(const AssignedNodesOptions& aOptions,
                               nsTArray<RefPtr<nsINode>>& aNodes)
{
  if (aOptions.mFlatten) {
    return FlattenAssignedNodes(this, aNodes);
  }

  aNodes = mAssignedNodes;
}

const nsTArray<RefPtr<nsINode>>&
HTMLSlotElement::AssignedNodes() const
{
  return mAssignedNodes;
}

void
HTMLSlotElement::InsertAssignedNode(uint32_t aIndex, nsINode* aNode)
{
  MOZ_ASSERT(!aNode->AsContent()->GetAssignedSlot(), "Losing track of a slot");
  mAssignedNodes.InsertElementAt(aIndex, aNode);
  aNode->AsContent()->SetAssignedSlot(this);
}

void
HTMLSlotElement::AppendAssignedNode(nsINode* aNode)
{
  MOZ_ASSERT(!aNode->AsContent()->GetAssignedSlot(), "Losing track of a slot");
  mAssignedNodes.AppendElement(aNode);
  aNode->AsContent()->SetAssignedSlot(this);
}

void
HTMLSlotElement::RemoveAssignedNode(nsINode* aNode)
{
  // This one runs from unlinking, so we can't guarantee that the slot pointer
  // hasn't been cleared.
  MOZ_ASSERT(!aNode->AsContent()->GetAssignedSlot() ||
             aNode->AsContent()->GetAssignedSlot() == this, "How exactly?");
  mAssignedNodes.RemoveElement(aNode);
  aNode->AsContent()->SetAssignedSlot(nullptr);
}

void
HTMLSlotElement::ClearAssignedNodes()
{
  for (RefPtr<nsINode>& node : mAssignedNodes) {
    MOZ_ASSERT(!node->AsContent()->GetAssignedSlot() ||
               node->AsContent()->GetAssignedSlot() == this, "How exactly?");
    node->AsContent()->SetAssignedSlot(nullptr);
  }

  mAssignedNodes.Clear();
}

void
HTMLSlotElement::EnqueueSlotChangeEvent()
{
  if (mInSignalSlotList) {
    return;
  }

  // FIXME(bug 1459704): Need to figure out how to deal with microtasks posted
  // during shutdown.
  if (gXPCOMThreadsShutDown) {
    return;
  }

  DocGroup* docGroup = OwnerDoc()->GetDocGroup();
  if (!docGroup) {
    return;
  }

  mInSignalSlotList = true;
  docGroup->SignalSlotChange(*this);
}

void
HTMLSlotElement::FireSlotChangeEvent()
{
  nsContentUtils::DispatchTrustedEvent(OwnerDoc(),
                                       static_cast<nsIContent*>(this),
                                       NS_LITERAL_STRING("slotchange"),
                                       CanBubble::eYes,
                                       Cancelable::eNo);
}

JSObject*
HTMLSlotElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLSlotElement_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
