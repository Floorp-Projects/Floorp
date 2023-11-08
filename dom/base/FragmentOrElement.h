/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all element classes as well as nsDocumentFragment.  This
 * provides an implementation of nsINode, implements nsIContent, provides
 * utility methods for subclasses, and so forth.
 */

#ifndef FragmentOrElement_h___
#define FragmentOrElement_h___

#include "mozilla/Attributes.h"
#include "mozilla/EnumSet.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/RadioGroupContainer.h"
#include "nsCycleCollectionParticipant.h"  // NS_DECL_CYCLE_*
#include "nsIContent.h"                    // base class
#include "nsAtomHashKeys.h"
#include "nsIHTMLCollection.h"
#include "nsIWeakReferenceUtils.h"

class ContentUnbinder;
class nsContentList;
class nsLabelsNodeList;
class nsDOMAttributeMap;
class nsDOMTokenList;
class nsIControllers;
class nsICSSDeclaration;
class nsDOMCSSAttributeDeclaration;
class nsDOMStringMap;
class nsIURI;

namespace mozilla {
class DeclarationBlock;
enum class ContentRelevancyReason;
using ContentRelevancy = EnumSet<ContentRelevancyReason, uint8_t>;
class ElementAnimationData;
namespace dom {
struct CustomElementData;
class Element;
class PopoverData;
}  // namespace dom
}  // namespace mozilla

/**
 * Tearoff to use for nodes to implement nsISupportsWeakReference
 */
class nsNodeSupportsWeakRefTearoff final : public nsISupportsWeakReference {
 public:
  explicit nsNodeSupportsWeakRefTearoff(nsINode* aNode) : mNode(aNode) {}

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsISupportsWeakReference
  NS_DECL_NSISUPPORTSWEAKREFERENCE

  NS_DECL_CYCLE_COLLECTION_CLASS(nsNodeSupportsWeakRefTearoff)

 private:
  ~nsNodeSupportsWeakRefTearoff() = default;

  nsCOMPtr<nsINode> mNode;
};

/**
 * A generic base class for DOM elements and document fragments,
 * implementing many nsIContent, nsINode and Element methods.
 */
namespace mozilla::dom {

class ShadowRoot;

class FragmentOrElement : public nsIContent {
 public:
  explicit FragmentOrElement(
      already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  explicit FragmentOrElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  // We want to avoid the overhead of extra function calls for
  // refcounting when we're not doing refcount logging, so we can't
  // NS_DECL_ISUPPORTS_INHERITED.
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;
  NS_INLINE_DECL_REFCOUNTING_INHERITED(FragmentOrElement, nsIContent);

  NS_DECL_ADDSIZEOFEXCLUDINGTHIS

  // nsINode interface methods
  virtual void GetTextContentInternal(nsAString& aTextContent,
                                      mozilla::OOMReporter& aError) override;
  virtual void SetTextContentInternal(const nsAString& aTextContent,
                                      nsIPrincipal* aSubjectPrincipal,
                                      mozilla::ErrorResult& aError) override;

  // nsIContent interface methods
  const nsTextFragment* GetText() override;
  uint32_t TextLength() const override;
  bool TextIsOnlyWhitespace() override;
  bool ThreadSafeTextIsOnlyWhitespace() const override;

  void DestroyContent() override;
  void SaveSubtreeState() override;

  nsIHTMLCollection* Children();
  uint32_t ChildElementCount() {
    if (!HasChildren()) {
      return 0;
    }
    return Children()->Length();
  }

  RadioGroupContainer& OwnedRadioGroupContainer() {
    auto* slots = ExtendedDOMSlots();
    if (!slots->mRadioGroupContainer) {
      slots->mRadioGroupContainer = MakeUnique<RadioGroupContainer>();
    }
    return *slots->mRadioGroupContainer;
  }

 public:
  /**
   * If there are listeners for DOMNodeInserted event, fires the event on all
   * aNodes
   */
  static void FireNodeInserted(Document* aDoc, nsINode* aParent,
                               const nsTArray<nsCOMPtr<nsIContent>>& aNodes);

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_WRAPPERCACHE_CLASS_INHERITED(
      FragmentOrElement, nsIContent)

  static void ClearContentUnbinder();
  static bool CanSkip(nsINode* aNode, bool aRemovingAllowed);
  static bool CanSkipInCC(nsINode* aNode);
  static bool CanSkipThis(nsINode* aNode);
  static void RemoveBlackMarkedNode(nsINode* aNode);
  static void MarkNodeChildren(nsINode* aNode);
  static void InitCCCallbacks();

  /**
   * Is the HTML local name a void element?
   */
  static bool IsHTMLVoid(nsAtom* aLocalName);

 protected:
  virtual ~FragmentOrElement();

  /**
   * Dummy CopyInnerTo so that we can use the same macros for
   * Elements and DocumentFragments.
   */
  nsresult CopyInnerTo(FragmentOrElement* aDest) { return NS_OK; }

 public:
  /**
   * There are a set of DOM- and scripting-specific instance variables
   * that may only be instantiated when a content object is accessed
   * through the DOM. Rather than burn actual slots in the content
   * objects for each of these instance variables, we put them off
   * in a side structure that's only allocated when the content is
   * accessed through the DOM.
   */

  class nsExtendedDOMSlots : public nsIContent::nsExtendedContentSlots {
   public:
    nsExtendedDOMSlots();
    ~nsExtendedDOMSlots();

    void TraverseExtendedSlots(nsCycleCollectionTraversalCallback&) final;
    void UnlinkExtendedSlots(nsIContent&) final;

    size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const final;

    /**
     * SMIL Overridde style rules (for SMIL animation of CSS properties)
     * @see Element::GetSMILOverrideStyle
     */
    RefPtr<nsDOMCSSAttributeDeclaration> mSMILOverrideStyle;

    /**
     * Holds any SMIL override style declaration for this element.
     */
    RefPtr<DeclarationBlock> mSMILOverrideStyleDeclaration;

    /**
     * The controllers of the XUL Element.
     */
    nsCOMPtr<nsIControllers> mControllers;

    /**
     * An object implementing the .labels property for this element.
     */
    RefPtr<nsLabelsNodeList> mLabelsList;

    /**
     * ShadowRoot bound to the element.
     */
    RefPtr<ShadowRoot> mShadowRoot;

    /**
     * Web components custom element data.
     */
    UniquePtr<CustomElementData> mCustomElementData;

    /**
     * Web animations data.
     */
    UniquePtr<ElementAnimationData> mAnimations;

    /**
     * PopoverData for the element.
     */
    UniquePtr<PopoverData> mPopoverData;

    /**
     * CustomStates for the element.
     */
    nsTArray<RefPtr<nsAtom>> mCustomStates;

    /**
     * RadioGroupContainer for radio buttons grouped under this disconnected
     * element.
     */
    UniquePtr<RadioGroupContainer> mRadioGroupContainer;

    /**
     * Last remembered size (in CSS pixels) for the element.
     * @see {@link https://drafts.csswg.org/css-sizing-4/#last-remembered}
     */
    Maybe<float> mLastRememberedBSize;
    Maybe<float> mLastRememberedISize;

    /**
     * Whether the content of this element is relevant for the purposes
     * of `content-visibility: auto.
     */
    Maybe<ContentRelevancy> mContentRelevancy;

    /**
     * Whether the content of this element is considered visible for
     * the purposes of `content-visibility: auto.
     */
    Maybe<bool> mVisibleForContentVisibility;

    /**
     * Whether content-visibility: auto is temporarily visible for
     * the purposes of the descendant of scrollIntoView.
     */
    bool mTemporarilyVisibleForScrolledIntoViewDescendant = false;

    /**
     * Explicitly set attr-elements, see
     * https://html.spec.whatwg.org/multipage/common-dom-interfaces.html#explicitly-set-attr-element
     */
    nsTHashMap<RefPtr<nsAtom>, nsWeakPtr> mExplicitlySetAttrElements;
  };

  class nsDOMSlots : public nsIContent::nsContentSlots {
   public:
    nsDOMSlots();
    ~nsDOMSlots();

    void Traverse(nsCycleCollectionTraversalCallback&) final;
    void Unlink(nsINode&) final;

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

    /**
     * The .style attribute (an interface that forwards to the actual
     * style rules)
     * @see nsGenericHTMLElement::GetStyle
     */
    nsCOMPtr<nsICSSDeclaration> mStyle;

    /**
     * The .dataset attribute.
     * @see nsGenericHTMLElement::GetDataset
     */
    nsDOMStringMap* mDataset;  // [Weak]

    /**
     * @see Element::Attributes
     */
    RefPtr<nsDOMAttributeMap> mAttributeMap;

    /**
     * An object implementing the .children property for this element.
     */
    RefPtr<nsContentList> mChildrenList;

    /**
     * An object implementing the .classList property for this element.
     */
    RefPtr<nsDOMTokenList> mClassList;

    /**
     * An object implementing the .part property for this element.
     */
    RefPtr<nsDOMTokenList> mPart;
  };

  /**
   * In case ExtendedDOMSlots is needed before normal DOMSlots, an instance of
   * FatSlots class, which combines those two slot types, is created.
   * This way we can avoid extra allocation for ExtendedDOMSlots.
   * FatSlots is useful for example when creating Custom Elements.
   */
  class FatSlots final : public nsDOMSlots, public nsExtendedDOMSlots {
   public:
    FatSlots() : nsDOMSlots(), nsExtendedDOMSlots() {
      MOZ_COUNT_CTOR(FatSlots);
      SetExtendedContentSlots(this, false);
    }

    ~FatSlots() final { MOZ_COUNT_DTOR(FatSlots); }
  };

 protected:
  void GetMarkup(bool aIncludeSelf, nsAString& aMarkup);
  void SetInnerHTMLInternal(const nsAString& aInnerHTML, ErrorResult& aError);

  // Override from nsINode
  nsIContent::nsContentSlots* CreateSlots() override {
    return new nsDOMSlots();
  }

  nsIContent::nsExtendedContentSlots* CreateExtendedSlots() final {
    return new nsExtendedDOMSlots();
  }

  nsDOMSlots* DOMSlots() { return static_cast<nsDOMSlots*>(Slots()); }

  nsDOMSlots* GetExistingDOMSlots() const {
    return static_cast<nsDOMSlots*>(GetExistingSlots());
  }

  nsExtendedDOMSlots* ExtendedDOMSlots() {
    nsContentSlots* slots = GetExistingContentSlots();
    if (!slots) {
      FatSlots* fatSlots = new FatSlots();
      mSlots = fatSlots;
      return fatSlots;
    }

    if (!slots->GetExtendedContentSlots()) {
      slots->SetExtendedContentSlots(CreateExtendedSlots(), true);
    }

    return static_cast<nsExtendedDOMSlots*>(slots->GetExtendedContentSlots());
  }

  const nsExtendedDOMSlots* GetExistingExtendedDOMSlots() const {
    return static_cast<const nsExtendedDOMSlots*>(
        GetExistingExtendedContentSlots());
  }

  nsExtendedDOMSlots* GetExistingExtendedDOMSlots() {
    return static_cast<nsExtendedDOMSlots*>(GetExistingExtendedContentSlots());
  }

  friend class ::ContentUnbinder;
};

}  // namespace mozilla::dom

#define NS_ELEMENT_INTERFACE_TABLE_TO_MAP_SEGUE               \
  if (NS_SUCCEEDED(rv)) return rv;                            \
                                                              \
  rv = FragmentOrElement::QueryInterface(aIID, aInstancePtr); \
  NS_INTERFACE_TABLE_TO_MAP_SEGUE

#endif /* FragmentOrElement_h___ */
