/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_shadowroot_h__
#define mozilla_dom_shadowroot_h__

#include "mozilla/dom/DocumentBinding.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/DocumentOrShadowRoot.h"
#include "mozilla/dom/NameSpaceConstants.h"
#include "mozilla/dom/ShadowRootBinding.h"
#include "mozilla/ServoBindings.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIRadioGroupContainer.h"
#include "nsStubMutationObserver.h"
#include "nsTHashtable.h"

class nsAtom;
class nsIContent;
class nsXBLPrototypeBinding;

namespace mozilla {

class EventChainPreVisitor;
class ServoStyleRuleMap;

namespace css {
class Rule;
}

namespace dom {

class Element;
class HTMLInputElement;

class ShadowRoot final : public DocumentFragment,
                         public DocumentOrShadowRoot,
                         public nsIRadioGroupContainer {
 public:
  NS_IMPL_FROMNODE_HELPER(ShadowRoot, IsShadowRoot());

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ShadowRoot, DocumentFragment)
  NS_DECL_ISUPPORTS_INHERITED

  ShadowRoot(Element* aElement, ShadowRootMode aMode,
             already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  void AddSizeOfExcludingThis(nsWindowSizes&, size_t* aNodeSize) const final;

  // Try to reassign an element to a slot.
  void MaybeReassignElement(Element&);
  // Called when an element is inserted as a direct child of our host. Tries to
  // slot the child in one of our slots.
  void MaybeSlotHostChild(nsIContent&);
  // Called when a direct child of our host is removed. Tries to un-slot the
  // child from the currently-assigned slot, if any.
  void MaybeUnslotHostChild(nsIContent&);

  // Shadow DOM v1
  Element* Host() const {
    MOZ_ASSERT(GetHost(),
               "ShadowRoot always has a host, how did we create "
               "this ShadowRoot?");
    return GetHost();
  }

  ShadowRootMode Mode() const { return mMode; }
  bool IsClosed() const { return mMode == ShadowRootMode::Closed; }

  void RemoveSheet(StyleSheet* aSheet);
  void RuleAdded(StyleSheet&, css::Rule&);
  void RuleRemoved(StyleSheet&, css::Rule&);
  void RuleChanged(StyleSheet&, css::Rule*);
  void StyleSheetCloned(StyleSheet&);
  void StyleSheetApplicableStateChanged(StyleSheet&, bool aApplicable);

  StyleSheetList* StyleSheets() {
    return &DocumentOrShadowRoot::EnsureDOMStyleSheets();
  }

  /**
   * Clones internal state, for example stylesheets, of aOther to 'this'.
   */
  void CloneInternalDataFrom(ShadowRoot* aOther);
  void InsertSheetAt(size_t aIndex, StyleSheet&);

  // Calls UnbindFromTree for each of our kids, and also flags us as no longer
  // being connected.
  void Unbind();

  // Only intended for UA widgets / special shadow roots, or for handling
  // failure cases when adopting (see BlastSubtreeToPieces).
  //
  // Forgets our shadow host and unbinds all our kids.
  void Unattach();

  // Calls BindToTree on each of our kids, and also maybe flags us as being
  // connected.
  nsresult Bind();

 private:
  void InsertSheetIntoAuthorData(size_t aIndex, StyleSheet&);

  void AppendStyleSheet(StyleSheet& aSheet) {
    InsertSheetAt(SheetCount(), aSheet);
  }

  /**
   * Represents the insertion point in a slot for a given node.
   */
  struct SlotAssignment {
    HTMLSlotElement* mSlot = nullptr;
    Maybe<uint32_t> mIndex;

    SlotAssignment() = default;
    SlotAssignment(HTMLSlotElement* aSlot, const Maybe<uint32_t>& aIndex)
        : mSlot(aSlot), mIndex(aIndex) {}
  };

  /**
   * Return the assignment corresponding to the content node at this particular
   * point in time.
   *
   * It's the caller's responsibility to actually call InsertAssignedNode /
   * AppendAssignedNode in the slot as needed.
   */
  SlotAssignment SlotAssignmentFor(nsIContent&);

  /**
   * Explicitly invalidates the style and layout of the flattened-tree subtree
   * rooted at the element.
   *
   * You need to use this whenever the flat tree is going to be shuffled in a
   * way that layout doesn't understand via the usual ContentInserted /
   * ContentAppended / ContentRemoved notifications. For example, if removing an
   * element will cause a change in the flat tree such that other element will
   * start showing up (like fallback content), this method needs to be called on
   * an ancestor of that element.
   *
   * It is important that this runs _before_ actually shuffling the flat tree
   * around, so that layout knows the actual tree that it needs to invalidate.
   */
  void InvalidateStyleAndLayoutOnSubtree(Element*);

 public:
  void AddSlot(HTMLSlotElement* aSlot);
  void RemoveSlot(HTMLSlotElement* aSlot);
  bool HasSlots() const { return !mSlotMap.IsEmpty(); };
  HTMLSlotElement* GetDefaultSlot() const {
    SlotArray* list = mSlotMap.Get(NS_LITERAL_STRING(""));
    return list ? (*list)->ElementAt(0) : nullptr;
  }

  void PartAdded(const Element&);
  void PartRemoved(const Element&);

  const nsTArray<const Element*>& Parts() const { return mParts; }

  const RawServoAuthorStyles* GetServoStyles() const {
    return mServoStyles.get();
  }

  RawServoAuthorStyles* GetServoStyles() { return mServoStyles.get(); }

  mozilla::ServoStyleRuleMap& ServoStyleRuleMap();

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  void AddToIdTable(Element* aElement, nsAtom* aId);
  void RemoveFromIdTable(Element* aElement, nsAtom* aId);

  // WebIDL methods.
  using mozilla::dom::DocumentOrShadowRoot::GetElementById;

  Element* GetActiveElement();
  void GetInnerHTML(nsAString& aInnerHTML);
  void SetInnerHTML(const nsAString& aInnerHTML, ErrorResult& aError);

  /**
   * These methods allow UA Widget to insert DOM elements into the Shadow ROM
   * without putting their DOM reflectors to content scope first.
   * The inserted DOM will have their reflectors in the UA Widget scope.
   */
  nsINode* ImportNodeAndAppendChildAt(nsINode& aParentNode, nsINode& aNode,
                                      bool aDeep, mozilla::ErrorResult& rv);

  nsINode* CreateElementAndAppendChildAt(nsINode& aParentNode,
                                         const nsAString& aTagName,
                                         mozilla::ErrorResult& rv);

  bool IsUAWidget() const { return mIsUAWidget; }

  void SetIsUAWidget() {
    MOZ_ASSERT(!HasChildren());
    SetFlags(NODE_HAS_BEEN_IN_UA_WIDGET);
    mIsUAWidget = true;
  }

  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  // nsIRadioGroupContainer
  NS_IMETHOD WalkRadioGroup(const nsAString& aName, nsIRadioVisitor* aVisitor,
                            bool aFlushContent) override {
    return DocumentOrShadowRoot::WalkRadioGroup(aName, aVisitor, aFlushContent);
  }
  virtual void SetCurrentRadioButton(const nsAString& aName,
                                     HTMLInputElement* aRadio) override {
    DocumentOrShadowRoot::SetCurrentRadioButton(aName, aRadio);
  }
  virtual HTMLInputElement* GetCurrentRadioButton(
      const nsAString& aName) override {
    return DocumentOrShadowRoot::GetCurrentRadioButton(aName);
  }
  NS_IMETHOD
  GetNextRadioButton(const nsAString& aName, const bool aPrevious,
                     HTMLInputElement* aFocusedRadio,
                     HTMLInputElement** aRadioOut) override {
    return DocumentOrShadowRoot::GetNextRadioButton(aName, aPrevious,
                                                    aFocusedRadio, aRadioOut);
  }
  virtual void AddToRadioGroup(const nsAString& aName,
                               HTMLInputElement* aRadio) override {
    DocumentOrShadowRoot::AddToRadioGroup(aName, aRadio);
  }
  virtual void RemoveFromRadioGroup(const nsAString& aName,
                                    HTMLInputElement* aRadio) override {
    DocumentOrShadowRoot::RemoveFromRadioGroup(aName, aRadio);
  }
  virtual uint32_t GetRequiredRadioCount(
      const nsAString& aName) const override {
    return DocumentOrShadowRoot::GetRequiredRadioCount(aName);
  }
  virtual void RadioRequiredWillChange(const nsAString& aName,
                                       bool aRequiredAdded) override {
    DocumentOrShadowRoot::RadioRequiredWillChange(aName, aRequiredAdded);
  }
  virtual bool GetValueMissingState(const nsAString& aName) const override {
    return DocumentOrShadowRoot::GetValueMissingState(aName);
  }
  virtual void SetValueMissingState(const nsAString& aName,
                                    bool aValue) override {
    return DocumentOrShadowRoot::SetValueMissingState(aName, aValue);
  }

 protected:
  // FIXME(emilio): This will need to become more fine-grained.
  void ApplicableRulesChanged();

  virtual ~ShadowRoot();

  const ShadowRootMode mMode;

  // The computed data from the style sheets.
  UniquePtr<RawServoAuthorStyles> mServoStyles;
  UniquePtr<mozilla::ServoStyleRuleMap> mStyleRuleMap;

  using SlotArray = TreeOrderedArray<HTMLSlotElement>;
  // Map from name of slot to an array of all slots in the shadow DOM with with
  // the given name. The slots are stored as a weak pointer because the elements
  // are in the shadow tree and should be kept alive by its parent.
  nsClassHashtable<nsStringHashKey, SlotArray> mSlotMap;

  // Unordered array of all elements that have a part attribute in this shadow
  // tree.
  nsTArray<const Element*> mParts;

  bool mIsUAWidget;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_shadowroot_h__
