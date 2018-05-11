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
#include "nsIStyleSheetLinkingElement.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLSlotElement.h"
#include "nsXBLPrototypeBinding.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/ServoStyleRuleMap.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(ShadowRoot)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ShadowRoot, DocumentFragment)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStyleSheets)
  for (StyleSheet* sheet : tmp->mStyleSheets) {
    // mServoStyles keeps another reference to it if applicable.
    if (sheet->IsApplicable()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mServoStyles->sheets[i]");
      cb.NoteXPCOMChild(sheet);
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDOMStyleSheets)
  for (auto iter = tmp->mIdentifierMap.ConstIter(); !iter.Done();
       iter.Next()) {
    iter.Get()->Traverse(&cb);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ShadowRoot)
  if (tmp->GetHost()) {
    tmp->GetHost()->RemoveMutationObserver(tmp);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDOMStyleSheets)
  tmp->mIdentifierMap.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(DocumentFragment)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ShadowRoot)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
NS_INTERFACE_MAP_END_INHERITING(DocumentFragment)

NS_IMPL_ADDREF_INHERITED(ShadowRoot, DocumentFragment)
NS_IMPL_RELEASE_INHERITED(ShadowRoot, DocumentFragment)

ShadowRoot::ShadowRoot(Element* aElement, ShadowRootMode aMode,
                       already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
  : DocumentFragment(aNodeInfo)
  , DocumentOrShadowRoot(*this)
  , mMode(aMode)
  , mServoStyles(Servo_AuthorStyles_Create())
  , mIsComposedDocParticipant(false)
{
  SetHost(aElement);

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
    // mHost may have been unlinked.
    host->RemoveMutationObserver(this);
  }

  if (IsComposedDocParticipant()) {
    OwnerDoc()->RemoveComposedDocShadowRoot(*this);
  }

  MOZ_DIAGNOSTIC_ASSERT(!OwnerDoc()->IsComposedDocShadowRoot(*this));

  UnsetFlags(NODE_IS_IN_SHADOW_TREE);

  // nsINode destructor expects mSubtreeRoot == this.
  SetSubtreeRootPointer(this);
}

void
ShadowRoot::SetIsComposedDocParticipant(bool aIsComposedDocParticipant)
{
  bool changed = mIsComposedDocParticipant != aIsComposedDocParticipant;
  mIsComposedDocParticipant = aIsComposedDocParticipant;
  if (!changed) {
    return;
  }

  nsIDocument* doc = OwnerDoc();
  if (IsComposedDocParticipant()) {
    doc->AddComposedDocShadowRoot(*this);
  } else {
    doc->RemoveComposedDocShadowRoot(*this);
  }
}

JSObject*
ShadowRoot::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::ShadowRootBinding::Wrap(aCx, this, aGivenProto);
}

void
ShadowRoot::CloneInternalDataFrom(ShadowRoot* aOther)
{
  size_t sheetCount = aOther->SheetCount();
  for (size_t i = 0; i < sheetCount; ++i) {
    StyleSheet* sheet = aOther->SheetAt(i);
    if (sheet->IsApplicable()) {
      RefPtr<StyleSheet> clonedSheet =
        sheet->Clone(nullptr, nullptr, this, nullptr);
      if (clonedSheet) {
        AppendStyleSheet(*clonedSheet.get());
      }
    }
  }
}

void
ShadowRoot::InvalidateStyleAndLayoutOnSubtree(Element* aElement)
{
  MOZ_ASSERT(aElement);

  if (!IsComposedDocParticipant()) {
    return;
  }

  MOZ_ASSERT(GetComposedDoc() == OwnerDoc());

  nsIPresShell* shell = OwnerDoc()->GetShell();
  if (!shell) {
    return;
  }

  shell->DestroyFramesForAndRestyle(aElement);
}

void
ShadowRoot::AddSlot(HTMLSlotElement* aSlot)
{
  MOZ_ASSERT(aSlot);

  // Note that if name attribute missing, the slot is a default slot.
  nsAutoString name;
  aSlot->GetName(name);

  nsTArray<HTMLSlotElement*>* currentSlots = mSlotMap.LookupOrAdd(name);
  MOZ_ASSERT(currentSlots);

  HTMLSlotElement* oldSlot = currentSlots->SafeElementAt(0);

  TreeOrderComparator comparator;
  currentSlots->InsertElementSorted(aSlot, comparator);

  HTMLSlotElement* currentSlot = currentSlots->ElementAt(0);
  if (currentSlot != aSlot) {
    return;
  }

  if (oldSlot && oldSlot != currentSlot) {
    // Move assigned nodes from old slot to new slot.
    InvalidateStyleAndLayoutOnSubtree(oldSlot);
    const nsTArray<RefPtr<nsINode>>& assignedNodes = oldSlot->AssignedNodes();
    bool doEnqueueSlotChange = false;
    while (assignedNodes.Length() > 0) {
      nsINode* assignedNode = assignedNodes[0];

      oldSlot->RemoveAssignedNode(assignedNode);
      currentSlot->AppendAssignedNode(assignedNode);
      doEnqueueSlotChange = true;
    }

    if (doEnqueueSlotChange) {
      oldSlot->EnqueueSlotChangeEvent();
      currentSlot->EnqueueSlotChangeEvent();
    }
  } else {
    bool doEnqueueSlotChange = false;
    // Otherwise add appropriate nodes to this slot from the host.
    for (nsIContent* child = GetHost()->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      nsAutoString slotName;
      if (child->IsElement()) {
        child->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::slot, slotName);
      }
      if (!child->IsSlotable() || !slotName.Equals(name)) {
        continue;
      }
      doEnqueueSlotChange = true;
      currentSlot->AppendAssignedNode(child);
    }

    if (doEnqueueSlotChange) {
      currentSlot->EnqueueSlotChangeEvent();
    }
  }
}

void
ShadowRoot::RemoveSlot(HTMLSlotElement* aSlot)
{
  MOZ_ASSERT(aSlot);

  nsAutoString name;
  aSlot->GetName(name);

  SlotArray* currentSlots = mSlotMap.Get(name);
  MOZ_DIAGNOSTIC_ASSERT(currentSlots && currentSlots->Contains(aSlot),
                        "Slot to deregister wasn't found?");
  if (currentSlots->Length() == 1) {
    MOZ_ASSERT(currentSlots->ElementAt(0) == aSlot);
    mSlotMap.Remove(name);

    if (!aSlot->AssignedNodes().IsEmpty()) {
      aSlot->ClearAssignedNodes();
      aSlot->EnqueueSlotChangeEvent();
    }

    return;
  }

  const bool wasFirstSlot = currentSlots->ElementAt(0) == aSlot;
  currentSlots->RemoveElement(aSlot);

  // Move assigned nodes from removed slot to the next slot in
  // tree order with the same name.
  if (!wasFirstSlot) {
    return;
  }

  HTMLSlotElement* replacementSlot = currentSlots->ElementAt(0);
  const nsTArray<RefPtr<nsINode>>& assignedNodes = aSlot->AssignedNodes();
  bool slottedNodesChanged = !assignedNodes.IsEmpty();
  while (!assignedNodes.IsEmpty()) {
    nsINode* assignedNode = assignedNodes[0];

    aSlot->RemoveAssignedNode(assignedNode);
    replacementSlot->AppendAssignedNode(assignedNode);
  }

  if (slottedNodesChanged) {
    aSlot->EnqueueSlotChangeEvent();
    replacementSlot->EnqueueSlotChangeEvent();
  }
}

// FIXME(emilio): There's a bit of code duplication between this and the
// equivalent ServoStyleSet methods, it'd be nice to not duplicate it...
void
ShadowRoot::RuleAdded(StyleSheet& aSheet, css::Rule& aRule)
{
  if (mStyleRuleMap) {
    mStyleRuleMap->RuleAdded(aSheet, aRule);
  }

  Servo_AuthorStyles_ForceDirty(mServoStyles.get());
  ApplicableRulesChanged();
}

void
ShadowRoot::RuleRemoved(StyleSheet& aSheet, css::Rule& aRule)
{
  if (mStyleRuleMap) {
    mStyleRuleMap->RuleRemoved(aSheet, aRule);
  }

  Servo_AuthorStyles_ForceDirty(mServoStyles.get());
  ApplicableRulesChanged();
}

void
ShadowRoot::RuleChanged(StyleSheet&, css::Rule*) {
  Servo_AuthorStyles_ForceDirty(mServoStyles.get());
  ApplicableRulesChanged();
}

void
ShadowRoot::ApplicableRulesChanged()
{
  if (!IsComposedDocParticipant()) {
    return;
  }

  nsIDocument* doc = OwnerDoc();
  if (nsIPresShell* shell = doc->GetShell()) {
    shell->RecordShadowStyleChange(*this);
  }
}

void
ShadowRoot::InsertSheetAt(size_t aIndex, StyleSheet& aSheet)
{
  DocumentOrShadowRoot::InsertSheetAt(aIndex, aSheet);
  if (aSheet.IsApplicable()) {
    InsertSheetIntoAuthorData(aIndex, aSheet);
  }
}

void
ShadowRoot::AppendStyleSheet(StyleSheet& aSheet)
{
  DocumentOrShadowRoot::AppendSheet(aSheet);
  if (aSheet.IsApplicable()) {
    Servo_AuthorStyles_AppendStyleSheet(mServoStyles.get(), &aSheet);
    if (mStyleRuleMap) {
      mStyleRuleMap->SheetAdded(aSheet);
    }
    ApplicableRulesChanged();
  }
}

void
ShadowRoot::InsertSheet(StyleSheet* aSheet, nsIContent* aLinkingContent)
{
  nsCOMPtr<nsIStyleSheetLinkingElement>
    linkingElement = do_QueryInterface(aLinkingContent);

  // FIXME(emilio, bug 1410578): <link> should probably also be allowed here.
  MOZ_ASSERT(linkingElement, "The only styles in a ShadowRoot should come "
                             "from <style>.");

  linkingElement->SetStyleSheet(aSheet); // This sets the ownerNode on the sheet

  // Find the correct position to insert into the style sheet list (must
  // be in tree order).
  for (size_t i = 0; i <= SheetCount(); i++) {
    if (i == SheetCount()) {
      AppendStyleSheet(*aSheet);
      return;
    }

    StyleSheet* sheet = SheetAt(i);
    nsINode* sheetOwningNode = sheet->GetOwnerNode();
    if (nsContentUtils::PositionIsBefore(aLinkingContent, sheetOwningNode)) {
      InsertSheetAt(i, *aSheet);
      return;
    }
  }
}

void
ShadowRoot::InsertSheetIntoAuthorData(size_t aIndex, StyleSheet& aSheet)
{
  MOZ_ASSERT(SheetAt(aIndex) == &aSheet);
  MOZ_ASSERT(aSheet.IsApplicable());

  if (mStyleRuleMap) {
    mStyleRuleMap->SheetAdded(aSheet);
  }

  for (size_t i = aIndex + 1; i < SheetCount(); ++i) {
    StyleSheet* beforeSheet = SheetAt(i);
    if (!beforeSheet->IsApplicable()) {
      continue;
    }

    Servo_AuthorStyles_InsertStyleSheetBefore(
      mServoStyles.get(), &aSheet, beforeSheet);
    ApplicableRulesChanged();
    return;
  }

  Servo_AuthorStyles_AppendStyleSheet(mServoStyles.get(), &aSheet);
  ApplicableRulesChanged();
}

// FIXME(emilio): This needs to notify document observers and such,
// presumably.
void
ShadowRoot::StyleSheetApplicableStateChanged(StyleSheet& aSheet, bool aApplicable)
{
  MOZ_ASSERT(mStyleSheets.Contains(&aSheet));
  if (aApplicable) {
    int32_t index = IndexOfSheet(aSheet);
    MOZ_RELEASE_ASSERT(index >= 0);
    InsertSheetIntoAuthorData(size_t(index), aSheet);
  } else {
    if (mStyleRuleMap) {
      mStyleRuleMap->SheetRemoved(aSheet);
    }
    Servo_AuthorStyles_RemoveStyleSheet(mServoStyles.get(), &aSheet);
    ApplicableRulesChanged();
  }
}

void
ShadowRoot::RemoveSheet(StyleSheet* aSheet)
{
  MOZ_ASSERT(aSheet);
  RefPtr<StyleSheet> sheet = DocumentOrShadowRoot::RemoveSheet(*aSheet);
  MOZ_ASSERT(sheet);
  if (sheet->IsApplicable()) {
    if (mStyleRuleMap) {
      mStyleRuleMap->SheetRemoved(*sheet);
    }
    Servo_AuthorStyles_RemoveStyleSheet(mServoStyles.get(), sheet);
    ApplicableRulesChanged();
  }
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

void
ShadowRoot::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
  aVisitor.mRootOfClosedTree = IsClosed();
  // Inform that we're about to exit the current scope.
  aVisitor.mRelatedTargetRetargetedInCurrentScope = false;

  // https://dom.spec.whatwg.org/#ref-for-get-the-parent%E2%91%A6
  if (!aVisitor.mEvent->mFlags.mComposed) {
    nsCOMPtr<nsIContent> originalTarget =
      do_QueryInterface(aVisitor.mEvent->mOriginalTarget);
    if (originalTarget->GetContainingShadow() == this) {
      // If we do stop propagation, we still want to propagate
      // the event to chrome (nsPIDOMWindow::GetParentTarget()).
      // The load event is special in that we don't ever propagate it
      // to chrome.
      nsCOMPtr<nsPIDOMWindowOuter> win = OwnerDoc()->GetWindow();
      EventTarget* parentTarget = win && aVisitor.mEvent->mMessage != eLoad
        ? win->GetParentTarget() : nullptr;

      aVisitor.SetParentTarget(parentTarget, true);
      return;
    }
  }

  nsIContent* shadowHost = GetHost();
  aVisitor.SetParentTarget(shadowHost, false);

  nsCOMPtr<nsIContent> content(do_QueryInterface(aVisitor.mEvent->mTarget));
  if (content && content->GetBindingParent() == shadowHost) {
    aVisitor.mEventTargetAtParent = shadowHost;
  }
}

ShadowRoot::SlotAssignment
ShadowRoot::SlotAssignmentFor(nsIContent* aContent)
{
  nsAutoString slotName;
  // Note that if slot attribute is missing, assign it to the first default
  // slot, if exists.
  if (aContent->IsElement()) {
    aContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::slot, slotName);
  }

  nsTArray<HTMLSlotElement*>* slots = mSlotMap.Get(slotName);
  if (!slots) {
    return { };
  }

  HTMLSlotElement* slot = slots->ElementAt(0);
  MOZ_ASSERT(slot);

  // Find the appropriate position in the assigned node list for the
  // newly assigned content.
  const nsTArray<RefPtr<nsINode>>& assignedNodes = slot->AssignedNodes();
  nsIContent* currentContent = GetHost()->GetFirstChild();
  Maybe<uint32_t> insertionIndex;
  for (uint32_t i = 0; i < assignedNodes.Length(); i++) {
    // Seek through the host's explicit children until the
    // assigned content is found.
    while (currentContent && currentContent != assignedNodes[i]) {
      if (currentContent == aContent) {
        insertionIndex.emplace(i);
        break;
      }

      currentContent = currentContent->GetNextSibling();
    }

    if (insertionIndex) {
      break;
    }
  }

  return { slot, insertionIndex };
}

void
ShadowRoot::MaybeReassignElement(Element* aElement)
{
  MOZ_ASSERT(aElement->GetParent() == GetHost());
  HTMLSlotElement* oldSlot = aElement->GetAssignedSlot();
  SlotAssignment assignment = SlotAssignmentFor(aElement);

  if (assignment.mSlot == oldSlot) {
    // Nothing to do here.
    return;
  }

  if (IsComposedDocParticipant()) {
    if (nsIPresShell* shell = OwnerDoc()->GetShell()) {
      shell->SlotAssignmentWillChange(*aElement, oldSlot, assignment.mSlot);
    }
  }

  if (oldSlot) {
    oldSlot->RemoveAssignedNode(aElement);
    oldSlot->EnqueueSlotChangeEvent();
  }

  if (assignment.mSlot) {
    if (assignment.mIndex) {
      assignment.mSlot->InsertAssignedNode(*assignment.mIndex, aElement);
    } else {
      assignment.mSlot->AppendAssignedNode(aElement);
    }
    assignment.mSlot->EnqueueSlotChangeEvent();
  }
}

Element*
ShadowRoot::GetActiveElement()
{
  return GetRetargetedFocusedElement();
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

void
ShadowRoot::AttributeChanged(Element* aElement,
                             int32_t aNameSpaceID,
                             nsAtom* aAttribute,
                             int32_t aModType,
                             const nsAttrValue* aOldValue)
{
  if (aNameSpaceID != kNameSpaceID_None || aAttribute != nsGkAtoms::slot) {
    return;
  }

  if (aElement->GetParent() != GetHost()) {
    return;
  }

  MaybeReassignElement(aElement);
}

void
ShadowRoot::ContentAppended(nsIContent* aFirstNewContent)
{
  for (nsIContent* content = aFirstNewContent;
       content;
       content = content->GetNextSibling()) {
    ContentInserted(content);
  }
}

void
ShadowRoot::ContentInserted(nsIContent* aChild)
{
  // Check to ensure that the child not an anonymous subtree root because
  // even though its parent could be the host it may not be in the host's child
  // list.
  if (aChild->IsRootOfAnonymousSubtree()) {
    return;
  }

  if (!aChild->IsSlotable()) {
    return;
  }

  if (aChild->GetParent() == GetHost()) {
    SlotAssignment assignment = SlotAssignmentFor(aChild);
    if (!assignment.mSlot) {
      return;
    }

    // Fallback content will go away, let layout know.
    if (assignment.mSlot->AssignedNodes().IsEmpty()) {
      InvalidateStyleAndLayoutOnSubtree(assignment.mSlot);
    }

    if (assignment.mIndex) {
      assignment.mSlot->InsertAssignedNode(*assignment.mIndex, aChild);
    } else {
      assignment.mSlot->AppendAssignedNode(aChild);
    }
    assignment.mSlot->EnqueueSlotChangeEvent();
    return;
  }

  // If parent's root is a shadow root, and parent is a slot whose assigned
  // nodes is the empty list, then run signal a slot change for parent.
  HTMLSlotElement* slot = HTMLSlotElement::FromNodeOrNull(aChild->GetParent());
  if (slot && slot->GetContainingShadow() == this &&
      slot->AssignedNodes().IsEmpty()) {
    slot->EnqueueSlotChangeEvent();
  }
}

void
ShadowRoot::ContentRemoved(nsIContent* aChild, nsIContent* aPreviousSibling)
{
  // Check to ensure that the child not an anonymous subtree root because
  // even though its parent could be the host it may not be in the host's child
  // list.
 if (aChild->IsRootOfAnonymousSubtree()) {
    return;
  }

  if (!aChild->IsSlotable()) {
    return;
  }

  if (aChild->GetParent() == GetHost()) {
    if (HTMLSlotElement* slot = aChild->GetAssignedSlot()) {
      // If the slot is going to start showing fallback content, we need to tell
      // layout about it.
      if (slot->AssignedNodes().Length() == 1) {
        InvalidateStyleAndLayoutOnSubtree(slot);
      }
      slot->RemoveAssignedNode(aChild);
      slot->EnqueueSlotChangeEvent();
    }
    return;
  }

  // If parent's root is a shadow root, and parent is a slot whose assigned
  // nodes is the empty list, then run signal a slot change for parent.
  HTMLSlotElement* slot = HTMLSlotElement::FromNodeOrNull(aChild->GetParent());
  if (slot && slot->GetContainingShadow() == this &&
      slot->AssignedNodes().IsEmpty()) {
    slot->EnqueueSlotChangeEvent();
  }
}

ServoStyleRuleMap&
ShadowRoot::ServoStyleRuleMap()
{
  if (!mStyleRuleMap) {
    mStyleRuleMap = MakeUnique<mozilla::ServoStyleRuleMap>();
  }
  mStyleRuleMap->EnsureTable(*this);
  return *mStyleRuleMap;
}

nsresult
ShadowRoot::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult,
                  bool aPreallocateChildren) const
{
  *aResult = nullptr;
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}
