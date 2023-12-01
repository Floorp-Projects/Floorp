/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_shadowroot_h__
#define mozilla_dom_shadowroot_h__

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/DocumentBinding.h"
#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/DocumentOrShadowRoot.h"
#include "mozilla/dom/NameSpaceConstants.h"
#include "mozilla/dom/ShadowRootBinding.h"
#include "mozilla/ServoBindings.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsStubMutationObserver.h"
#include "nsTHashtable.h"

class nsAtom;
class nsIContent;

namespace mozilla {

class EventChainPreVisitor;
class ServoStyleRuleMap;

enum class StyleRuleChangeKind : uint32_t;

namespace css {
class Rule;
}

namespace dom {

class CSSImportRule;
class Element;
class HTMLInputElement;

class ShadowRoot final : public DocumentFragment, public DocumentOrShadowRoot {
  friend class DocumentOrShadowRoot;

 public:
  NS_IMPL_FROMNODE_HELPER(ShadowRoot, IsShadowRoot());

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ShadowRoot, DocumentFragment)
  NS_DECL_ISUPPORTS_INHERITED

  ShadowRoot(Element* aElement, ShadowRootMode aMode,
             Element::DelegatesFocus aDelegatesFocus,
             SlotAssignmentMode aSlotAssignment,
             already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  void AddSizeOfExcludingThis(nsWindowSizes&, size_t* aNodeSize) const final;

  // Try to reassign an element or text to a slot.
  void MaybeReassignContent(nsIContent& aElementOrText);
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
  bool DelegatesFocus() const {
    return mDelegatesFocus == Element::DelegatesFocus::Yes;
  }
  SlotAssignmentMode SlotAssignment() const { return mSlotAssignment; }
  bool IsClosed() const { return mMode == ShadowRootMode::Closed; }

  void RemoveSheetFromStyles(StyleSheet&);
  void RuleAdded(StyleSheet&, css::Rule&);
  void RuleRemoved(StyleSheet&, css::Rule&);
  void RuleChanged(StyleSheet&, css::Rule*, StyleRuleChangeKind);
  void ImportRuleLoaded(CSSImportRule&, StyleSheet&);
  void SheetCloned(StyleSheet&);
  void StyleSheetApplicableStateChanged(StyleSheet&);

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

 private:
  void InsertSheetIntoAuthorData(size_t aIndex, StyleSheet&,
                                 const nsTArray<RefPtr<StyleSheet>>&);

  void AppendStyleSheet(StyleSheet& aSheet) {
    InsertSheetAt(SheetCount(), aSheet);
  }

  /**
   * Represents the insertion point in a slot for a given node.
   */
  struct SlotInsertionPoint {
    HTMLSlotElement* mSlot = nullptr;
    Maybe<uint32_t> mIndex;

    SlotInsertionPoint() = default;
    SlotInsertionPoint(HTMLSlotElement* aSlot, const Maybe<uint32_t>& aIndex)
        : mSlot(aSlot), mIndex(aIndex) {}
  };

  /**
   * Return the assignment corresponding to the content node at this particular
   * point in time.
   *
   * It's the caller's responsibility to actually call InsertAssignedNode /
   * AppendAssignedNode in the slot as needed.
   */
  SlotInsertionPoint SlotInsertionPointFor(nsIContent&);

  /**
   * Returns the effective slot name for a given slottable. In most cases, this
   * is just the value of the slot attribute, if any, or the empty string, but
   * this also deals with the <details> shadow tree specially.
   */
  void GetSlotNameFor(const nsIContent&, nsAString&) const;

  /**
   * Re-assign the current main summary if it has changed.
   *
   * Must be called only if mIsDetailsShadowTree is true.
   */
  enum class SummaryChangeReason { Deletion, Insertion };
  void MaybeReassignMainSummary(SummaryChangeReason);

 public:
  void AddSlot(HTMLSlotElement* aSlot);
  void RemoveSlot(HTMLSlotElement* aSlot);
  bool HasSlots() const { return !mSlotMap.IsEmpty(); };
  HTMLSlotElement* GetDefaultSlot() const {
    SlotArray* list = mSlotMap.Get(u""_ns);
    return list ? (*list)->ElementAt(0) : nullptr;
  }

  void PartAdded(const Element&);
  void PartRemoved(const Element&);

  IMPL_EVENT_HANDLER(slotchange);

  const nsTArray<const Element*>& Parts() const { return mParts; }

  const StyleAuthorStyles* GetServoStyles() const { return mServoStyles.get(); }

  StyleAuthorStyles* GetServoStyles() { return mServoStyles.get(); }

  mozilla::ServoStyleRuleMap& ServoStyleRuleMap();

  JSObject* WrapNode(JSContext*, JS::Handle<JSObject*> aGivenProto) final;

  void NodeInfoChanged(Document* aOldDoc) override;

  void AddToIdTable(Element* aElement, nsAtom* aId);
  void RemoveFromIdTable(Element* aElement, nsAtom* aId);

  // WebIDL methods.
  using mozilla::dom::DocumentOrShadowRoot::GetElementById;

  Element* GetActiveElement();

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

  bool IsUAWidget() const { return HasBeenInUAWidget(); }

  void SetIsUAWidget() {
    MOZ_ASSERT(!HasChildren());
    SetIsNativeAnonymousRoot();
    SetFlags(NODE_HAS_BEEN_IN_UA_WIDGET);
  }

  bool IsAvailableToElementInternals() const {
    return mIsAvailableToElementInternals;
  }

  void SetAvailableToElementInternals() {
    mIsAvailableToElementInternals = true;
  }

  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

 protected:
  // FIXME(emilio): This will need to become more fine-grained.
  void ApplicableRulesChanged();

  virtual ~ShadowRoot();

  // Make sure that the first field is pointer-aligned so it doesn't get packed
  // in the base class' padding, since otherwise rust-bindgen can't generate
  // correct bindings for it, see
  // https://github.com/rust-lang/rust-bindgen/issues/380

  // The computed data from the style sheets.
  UniquePtr<StyleAuthorStyles> mServoStyles;
  UniquePtr<mozilla::ServoStyleRuleMap> mStyleRuleMap;

  using SlotArray = TreeOrderedArray<HTMLSlotElement>;
  // Map from name of slot to an array of all slots in the shadow DOM with with
  // the given name. The slots are stored as a weak pointer because the elements
  // are in the shadow tree and should be kept alive by its parent.
  nsClassHashtable<nsStringHashKey, SlotArray> mSlotMap;

  // Unordered array of all elements that have a part attribute in this shadow
  // tree.
  nsTArray<const Element*> mParts;

  const ShadowRootMode mMode;

  Element::DelegatesFocus mDelegatesFocus;

  const SlotAssignmentMode mSlotAssignment;

  // Whether this is the <details> internal shadow tree.
  bool mIsDetailsShadowTree : 1;

  // https://dom.spec.whatwg.org/#shadowroot-available-to-element-internals
  bool mIsAvailableToElementInternals : 1;

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_shadowroot_h__
