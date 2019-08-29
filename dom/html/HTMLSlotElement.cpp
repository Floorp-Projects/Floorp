/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/HTMLSlotElementBinding.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsGkAtoms.h"

nsGenericHTMLElement* NS_NewHTMLSlotElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    mozilla::dom::FromParser aFromParser) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo(std::move(aNodeInfo));
  return new mozilla::dom::HTMLSlotElement(nodeInfo.forget());
}

namespace mozilla {
namespace dom {

HTMLSlotElement::HTMLSlotElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {}

HTMLSlotElement::~HTMLSlotElement() = default;

NS_IMPL_ADDREF_INHERITED(HTMLSlotElement, nsGenericHTMLElement)
NS_IMPL_RELEASE_INHERITED(HTMLSlotElement, nsGenericHTMLElement)

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLSlotElement, nsGenericHTMLElement,
                                   mAssignedNodes)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HTMLSlotElement)
NS_INTERFACE_MAP_END_INHERITING(nsGenericHTMLElement)

NS_IMPL_ELEMENT_CLONE(HTMLSlotElement)

nsresult HTMLSlotElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  RefPtr<ShadowRoot> oldContainingShadow = GetContainingShadow();

  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  ShadowRoot* containingShadow = GetContainingShadow();
  if (containingShadow && !oldContainingShadow) {
    containingShadow->AddSlot(this);
  }

  return NS_OK;
}

void HTMLSlotElement::UnbindFromTree(bool aNullParent) {
  RefPtr<ShadowRoot> oldContainingShadow = GetContainingShadow();

  nsGenericHTMLElement::UnbindFromTree(aNullParent);

  if (oldContainingShadow && !GetContainingShadow()) {
    oldContainingShadow->RemoveSlot(this);
  }
}

nsresult HTMLSlotElement::BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                        const nsAttrValueOrString* aValue,
                                        bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::name) {
    if (ShadowRoot* containingShadow = GetContainingShadow()) {
      containingShadow->RemoveSlot(this);
    }
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName, aValue,
                                             aNotify);
}

nsresult HTMLSlotElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                       const nsAttrValue* aValue,
                                       const nsAttrValue* aOldValue,
                                       nsIPrincipal* aSubjectPrincipal,
                                       bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::name) {
    if (ShadowRoot* containingShadow = GetContainingShadow()) {
      containingShadow->AddSlot(this);
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

/**
 * Flatten assigned nodes given a slot, as in:
 * https://dom.spec.whatwg.org/#find-flattened-slotables
 */
static void FlattenAssignedNodes(HTMLSlotElement* aSlot,
                                 nsTArray<RefPtr<nsINode>>& aNodes) {
  if (!aSlot->GetContainingShadow()) {
    return;
  }

  const nsTArray<RefPtr<nsINode>>& assignedNodes = aSlot->AssignedNodes();

  // If assignedNodes is empty, use children of slot as fallback content.
  if (assignedNodes.IsEmpty()) {
    for (nsIContent* child = aSlot->GetFirstChild(); child;
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
    auto* slot = HTMLSlotElement::FromNode(assignedNode);
    if (slot && slot->GetContainingShadow()) {
      FlattenAssignedNodes(slot, aNodes);
    } else {
      aNodes.AppendElement(assignedNode);
    }
  }
}

void HTMLSlotElement::AssignedNodes(const AssignedNodesOptions& aOptions,
                                    nsTArray<RefPtr<nsINode>>& aNodes) {
  if (aOptions.mFlatten) {
    return FlattenAssignedNodes(this, aNodes);
  }

  aNodes = mAssignedNodes;
}

void HTMLSlotElement::AssignedElements(const AssignedNodesOptions& aOptions,
                                       nsTArray<RefPtr<Element>>& aElements) {
  AutoTArray<RefPtr<nsINode>, 128> assignedNodes;
  AssignedNodes(aOptions, assignedNodes);
  for (const RefPtr<nsINode>& assignedNode : assignedNodes) {
    if (assignedNode->IsElement()) {
      aElements.AppendElement(assignedNode->AsElement());
    }
  }
}

const nsTArray<RefPtr<nsINode>>& HTMLSlotElement::AssignedNodes() const {
  return mAssignedNodes;
}

void HTMLSlotElement::InsertAssignedNode(uint32_t aIndex, nsIContent& aNode) {
  MOZ_ASSERT(!aNode.GetAssignedSlot(), "Losing track of a slot");
  mAssignedNodes.InsertElementAt(aIndex, &aNode);
  aNode.SetAssignedSlot(this);
}

void HTMLSlotElement::AppendAssignedNode(nsIContent& aNode) {
  MOZ_ASSERT(!aNode.GetAssignedSlot(), "Losing track of a slot");
  mAssignedNodes.AppendElement(&aNode);
  aNode.SetAssignedSlot(this);
}

void HTMLSlotElement::RemoveAssignedNode(nsIContent& aNode) {
  // This one runs from unlinking, so we can't guarantee that the slot pointer
  // hasn't been cleared.
  MOZ_ASSERT(!aNode.GetAssignedSlot() || aNode.GetAssignedSlot() == this,
             "How exactly?");
  mAssignedNodes.RemoveElement(&aNode);
  aNode.SetAssignedSlot(nullptr);
}

void HTMLSlotElement::ClearAssignedNodes() {
  for (RefPtr<nsINode>& node : mAssignedNodes) {
    MOZ_ASSERT(!node->AsContent()->GetAssignedSlot() ||
                   node->AsContent()->GetAssignedSlot() == this,
               "How exactly?");
    node->AsContent()->SetAssignedSlot(nullptr);
  }

  mAssignedNodes.Clear();
}

void HTMLSlotElement::EnqueueSlotChangeEvent() {
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

void HTMLSlotElement::FireSlotChangeEvent() {
  nsContentUtils::DispatchTrustedEvent(
      OwnerDoc(), static_cast<nsIContent*>(this),
      NS_LITERAL_STRING("slotchange"), CanBubble::eYes, Cancelable::eNo);
}

JSObject* HTMLSlotElement::WrapNode(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return HTMLSlotElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
