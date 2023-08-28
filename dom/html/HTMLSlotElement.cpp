/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PresShell.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "mozilla/dom/HTMLUnknownElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/Text.h"
#include "mozilla/AppShutdown.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"

nsGenericHTMLElement* NS_NewHTMLSlotElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    mozilla::dom::FromParser aFromParser) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo(std::move(aNodeInfo));
  auto* nim = nodeInfo->NodeInfoManager();
  return new (nim) mozilla::dom::HTMLSlotElement(nodeInfo.forget());
}

namespace mozilla::dom {

HTMLSlotElement::HTMLSlotElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {}

HTMLSlotElement::~HTMLSlotElement() {
  for (const auto& node : mManuallyAssignedNodes) {
    MOZ_ASSERT(node->AsContent()->GetManualSlotAssignment() == this);
    node->AsContent()->SetManualSlotAssignment(nullptr);
  }
}

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
  mInManualShadowRoot =
      containingShadow &&
      containingShadow->SlotAssignment() == SlotAssignmentMode::Manual;
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

void HTMLSlotElement::BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                    const nsAttrValue* aValue, bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::name) {
    if (ShadowRoot* containingShadow = GetContainingShadow()) {
      containingShadow->RemoveSlot(this);
    }
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName, aValue,
                                             aNotify);
}

void HTMLSlotElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
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

  aNodes = mAssignedNodes.Clone();
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

const nsTArray<nsINode*>& HTMLSlotElement::ManuallyAssignedNodes() const {
  return mManuallyAssignedNodes;
}

void HTMLSlotElement::Assign(const Sequence<OwningElementOrText>& aNodes) {
  nsAutoScriptBlocker scriptBlocker;

  // no-op if the input nodes and the assigned nodes are identical
  // This also works if the two 'assign' calls are like
  //   > slot.assign(node1, node2);
  //   > slot.assign(node1, node2, node1, node2);
  if (!mAssignedNodes.IsEmpty() && aNodes.Length() >= mAssignedNodes.Length()) {
    nsTHashMap<nsPtrHashKey<nsIContent>, size_t> nodeIndexMap;
    for (size_t i = 0; i < aNodes.Length(); ++i) {
      nsIContent* content;
      if (aNodes[i].IsElement()) {
        content = aNodes[i].GetAsElement();
      } else {
        content = aNodes[i].GetAsText();
      }
      MOZ_ASSERT(content);
      // We only care about the first index this content appears
      // in the array
      nodeIndexMap.LookupOrInsert(content, i);
    }

    if (nodeIndexMap.Count() == mAssignedNodes.Length()) {
      bool isIdentical = true;
      for (size_t i = 0; i < mAssignedNodes.Length(); ++i) {
        size_t indexInInputNodes;
        if (!nodeIndexMap.Get(mAssignedNodes[i]->AsContent(),
                              &indexInInputNodes) ||
            indexInInputNodes != i) {
          isIdentical = false;
          break;
        }
      }
      if (isIdentical) {
        return;
      }
    }
  }

  // 1. For each node of this's manually assigned nodes, set node's manual slot
  // assignment to null.
  for (nsINode* node : mManuallyAssignedNodes) {
    MOZ_ASSERT(node->AsContent()->GetManualSlotAssignment() == this);
    node->AsContent()->SetManualSlotAssignment(nullptr);
  }

  // 2. Let nodesSet be a new ordered set.
  mManuallyAssignedNodes.Clear();

  nsIContent* host = nullptr;
  ShadowRoot* root = GetContainingShadow();

  // An optimization to keep track which slots need to enqueue
  // slotchange event, such that they can be enqueued later in
  // tree order.
  nsTHashSet<RefPtr<HTMLSlotElement>> changedSlots;

  // Clear out existing assigned nodes
  if (mInManualShadowRoot) {
    if (!mAssignedNodes.IsEmpty()) {
      changedSlots.EnsureInserted(this);
      if (root) {
        root->InvalidateStyleAndLayoutOnSubtree(this);
      }
      ClearAssignedNodes();
    }

    MOZ_ASSERT(mAssignedNodes.IsEmpty());
    host = GetContainingShadowHost();
  }

  for (const OwningElementOrText& elementOrText : aNodes) {
    nsIContent* content;
    if (elementOrText.IsElement()) {
      content = elementOrText.GetAsElement();
    } else {
      content = elementOrText.GetAsText();
    }

    MOZ_ASSERT(content);
    // XXXsmaug Should we have a helper for
    //         https://infra.spec.whatwg.org/#ordered-set?
    if (content->GetManualSlotAssignment() != this) {
      if (HTMLSlotElement* oldSlot = content->GetAssignedSlot()) {
        if (changedSlots.EnsureInserted(oldSlot)) {
          if (root) {
            MOZ_ASSERT(oldSlot->GetContainingShadow() == root);
            root->InvalidateStyleAndLayoutOnSubtree(oldSlot);
          }
        }
      }

      if (changedSlots.EnsureInserted(this)) {
        if (root) {
          root->InvalidateStyleAndLayoutOnSubtree(this);
        }
      }
      // 3.1 (HTML Spec) If content's manual slot assignment refers to a slot,
      // then remove node from that slot's manually assigned nodes. 3.2 (HTML
      // Spec) Set content's manual slot assignment to this.
      if (HTMLSlotElement* oldSlot = content->GetManualSlotAssignment()) {
        oldSlot->RemoveManuallyAssignedNode(*content);
      }
      content->SetManualSlotAssignment(this);
      mManuallyAssignedNodes.AppendElement(content);

      if (root && host && content->GetParent() == host) {
        // Equivalent to 4.2.2.4.3 (DOM Spec) `Set slot's assigned nodes to
        // slottables`
        root->MaybeReassignContent(*content);
      }
    }
  }

  // The `assign slottables` step is completed already at this point,
  // however we haven't fired the `slotchange` event yet because this
  // needs to be done in tree order.
  if (root) {
    for (nsIContent* child = root->GetFirstChild(); child;
         child = child->GetNextNode()) {
      if (HTMLSlotElement* slot = HTMLSlotElement::FromNode(child)) {
        if (changedSlots.EnsureRemoved(slot)) {
          slot->EnqueueSlotChangeEvent();
        }
      }
    }
    MOZ_ASSERT(changedSlots.IsEmpty());
  }
}

void HTMLSlotElement::InsertAssignedNode(uint32_t aIndex, nsIContent& aNode) {
  MOZ_ASSERT(!aNode.GetAssignedSlot(), "Losing track of a slot");
  mAssignedNodes.InsertElementAt(aIndex, &aNode);
  aNode.SetAssignedSlot(this);
  SlotAssignedNodeChanged(this, aNode);
}

void HTMLSlotElement::AppendAssignedNode(nsIContent& aNode) {
  MOZ_ASSERT(!aNode.GetAssignedSlot(), "Losing track of a slot");
  mAssignedNodes.AppendElement(&aNode);
  aNode.SetAssignedSlot(this);
  SlotAssignedNodeChanged(this, aNode);
}

void HTMLSlotElement::RemoveAssignedNode(nsIContent& aNode) {
  // This one runs from unlinking, so we can't guarantee that the slot pointer
  // hasn't been cleared.
  MOZ_ASSERT(!aNode.GetAssignedSlot() || aNode.GetAssignedSlot() == this,
             "How exactly?");
  mAssignedNodes.RemoveElement(&aNode);
  aNode.SetAssignedSlot(nullptr);
  SlotAssignedNodeChanged(this, aNode);
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
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
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
  nsContentUtils::DispatchTrustedEvent(OwnerDoc(), this, u"slotchange"_ns,
                                       CanBubble::eYes, Cancelable::eNo);
}

void HTMLSlotElement::RemoveManuallyAssignedNode(nsIContent& aNode) {
  mManuallyAssignedNodes.RemoveElement(&aNode);
  RemoveAssignedNode(aNode);
}

JSObject* HTMLSlotElement::WrapNode(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return HTMLSlotElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
