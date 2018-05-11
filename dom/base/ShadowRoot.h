/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_shadowroot_h__
#define mozilla_dom_shadowroot_h__

#include "mozilla/dom/DocumentFragment.h"
#include "mozilla/dom/DocumentOrShadowRoot.h"
#include "mozilla/dom/NameSpaceConstants.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIdentifierMapEntry.h"
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

class ShadowRoot final : public DocumentFragment,
                         public DocumentOrShadowRoot,
                         public nsStubMutationObserver
{
public:
  NS_IMPL_FROMNODE_HELPER(ShadowRoot, IsShadowRoot());

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ShadowRoot,
                                           DocumentFragment)
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  ShadowRoot(Element* aElement, ShadowRootMode aMode,
             already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  // Shadow DOM v1
  Element* Host() const
  {
    MOZ_ASSERT(GetHost(), "ShadowRoot always has a host, how did we create "
                          "this ShadowRoot?");
    return GetHost();
  }

  ShadowRootMode Mode() const
  {
    return mMode;
  }
  bool IsClosed() const
  {
    return mMode == ShadowRootMode::Closed;
  }

  void InsertSheet(StyleSheet* aSheet, nsIContent* aLinkingContent);
  void RemoveSheet(StyleSheet* aSheet);
  void RuleAdded(StyleSheet&, css::Rule&);
  void RuleRemoved(StyleSheet&, css::Rule&);
  void RuleChanged(StyleSheet&, css::Rule*);
  void StyleSheetApplicableStateChanged(StyleSheet&, bool aApplicable);

  StyleSheetList* StyleSheets()
  {
    return &DocumentOrShadowRoot::EnsureDOMStyleSheets();
  }

  /**
   * Clones internal state, for example stylesheets, of aOther to 'this'.
   */
  void CloneInternalDataFrom(ShadowRoot* aOther);
private:
  void InsertSheetIntoAuthorData(size_t aIndex, StyleSheet&);

  void InsertSheetAt(size_t aIndex, StyleSheet&);
  void AppendStyleSheet(StyleSheet&);

  /**
   * Try to reassign an element to a slot and returns whether the assignment
   * changed.
   */
  void MaybeReassignElement(Element* aElement);

  /**
   * Represents the insertion point in a slot for a given node.
   */
  struct SlotAssignment
  {
    HTMLSlotElement* mSlot = nullptr;
    Maybe<uint32_t> mIndex;

    SlotAssignment() = default;
    SlotAssignment(HTMLSlotElement* aSlot, const Maybe<uint32_t>& aIndex)
      : mSlot(aSlot)
      , mIndex(aIndex)
    { }
  };

  /**
   * Return the assignment corresponding to the content node at this particular
   * point in time.
   *
   * It's the caller's responsibility to actually call InsertAssignedNode /
   * AppendAssignedNode in the slot as needed.
   */
  SlotAssignment SlotAssignmentFor(nsIContent* aContent);

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

  const RawServoAuthorStyles* ServoStyles() const
  {
    return mServoStyles.get();
  }

  RawServoAuthorStyles* ServoStyles()
  {
    return mServoStyles.get();
  }

  mozilla::ServoStyleRuleMap& ServoStyleRuleMap();

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void AddToIdTable(Element* aElement, nsAtom* aId);
  void RemoveFromIdTable(Element* aElement, nsAtom* aId);

  // WebIDL methods.
  using mozilla::dom::DocumentOrShadowRoot::GetElementById;

  Element* GetActiveElement();
  void GetInnerHTML(nsAString& aInnerHTML);
  void SetInnerHTML(const nsAString& aInnerHTML, ErrorResult& aError);

  bool IsComposedDocParticipant() const
  {
    return mIsComposedDocParticipant;
  }

  void SetIsComposedDocParticipant(bool aIsComposedDocParticipant);

  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

protected:
  // FIXME(emilio): This will need to become more fine-grained.
  void ApplicableRulesChanged();

  virtual ~ShadowRoot();

  const ShadowRootMode mMode;

  // The computed data from the style sheets.
  UniquePtr<RawServoAuthorStyles> mServoStyles;
  UniquePtr<mozilla::ServoStyleRuleMap> mStyleRuleMap;

  using SlotArray = AutoTArray<HTMLSlotElement*, 1>;
  // Map from name of slot to an array of all slots in the shadow DOM with with
  // the given name. The slots are stored as a weak pointer because the elements
  // are in the shadow tree and should be kept alive by its parent.
  nsClassHashtable<nsStringHashKey, SlotArray> mSlotMap;

  // Flag to indicate whether the descendants of this shadow root are part of the
  // composed document. Ideally, we would use a node flag on nodes to
  // mark whether it is in the composed document, but we have run out of flags
  // so instead we track it here.
  bool mIsComposedDocParticipant;

  nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult,
                 bool aPreallocateChildren) const override;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_shadowroot_h__
