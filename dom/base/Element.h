/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all element classes; this provides an implementation
 * of DOM Core's nsIDOMElement, implements nsIContent, provides
 * utility methods for subclasses, and so forth.
 */

#ifndef mozilla_dom_Element_h__
#define mozilla_dom_Element_h__

#include "mozilla/dom/FragmentOrElement.h" // for base class
#include "nsChangeHint.h"                  // for enum
#include "mozilla/EventStates.h"           // for member
#include "mozilla/dom/DirectionalityUtils.h"
#include "nsIDOMElement.h"
#include "nsILinkHandler.h"
#include "nsINodeList.h"
#include "nsNodeUtils.h"
#include "nsAttrAndChildArray.h"
#include "mozFlushType.h"
#include "nsDOMAttributeMap.h"
#include "nsPresContext.h"
#include "mozilla/CORSMode.h"
#include "mozilla/Attributes.h"
#include "nsIScrollableFrame.h"
#include "mozilla/dom/Attr.h"
#include "nsISMILAttr.h"
#include "mozilla/dom/DOMRect.h"
#include "nsAttrValue.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/DOMTokenListSupportedTokens.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/Nullable.h"
#include "Units.h"
#include "DOMIntersectionObserver.h"

class nsIFrame;
class nsIDOMMozNamedAttrMap;
class nsIURI;
class nsIScrollableFrame;
class nsAttrValueOrString;
class nsContentList;
class nsDOMTokenList;
struct nsRect;
class nsFocusManager;
class nsGlobalWindow;
class nsICSSDeclaration;
class nsISMILAttr;
class nsDocument;
class nsDOMStringMap;

namespace mozilla {
class DeclarationBlock;
namespace dom {
  struct AnimationFilter;
  struct ScrollIntoViewOptions;
  struct ScrollToOptions;
  class DOMIntersectionObserver;
  class ElementOrCSSPseudoElement;
  class UnrestrictedDoubleOrKeyframeAnimationOptions;
} // namespace dom
} // namespace mozilla


already_AddRefed<nsContentList>
NS_GetContentList(nsINode* aRootNode,
                  int32_t  aMatchNameSpaceId,
                  const nsAString& aTagname);

#define ELEMENT_FLAG_BIT(n_) NODE_FLAG_BIT(NODE_TYPE_SPECIFIC_BITS_OFFSET + (n_))

// Element-specific flags
enum {
  // Set if the element has a pending style change.
  ELEMENT_HAS_PENDING_RESTYLE =                 NODE_SHARED_RESTYLE_BIT_1,

  // Set if the element is a potential restyle root (that is, has a style
  // change pending _and_ that style change will attempt to restyle
  // descendants).
  ELEMENT_IS_POTENTIAL_RESTYLE_ROOT =           NODE_SHARED_RESTYLE_BIT_2,

  // Set if the element has a pending animation-only style change as
  // part of an animation-only style update (where we update styles from
  // animation to the current refresh tick, but leave everything else as
  // it was).
  ELEMENT_HAS_PENDING_ANIMATION_ONLY_RESTYLE =  ELEMENT_FLAG_BIT(0),

  // Set if the element is a potential animation-only restyle root (that
  // is, has an animation-only style change pending _and_ that style
  // change will attempt to restyle descendants).
  ELEMENT_IS_POTENTIAL_ANIMATION_ONLY_RESTYLE_ROOT = ELEMENT_FLAG_BIT(1),

  // Set if this element has a pending restyle with an eRestyle_SomeDescendants
  // restyle hint.
  ELEMENT_IS_CONDITIONAL_RESTYLE_ANCESTOR = ELEMENT_FLAG_BIT(2),

  // Just the HAS_PENDING bits, for convenience
  ELEMENT_PENDING_RESTYLE_FLAGS =
    ELEMENT_HAS_PENDING_RESTYLE |
    ELEMENT_HAS_PENDING_ANIMATION_ONLY_RESTYLE,

  // Just the IS_POTENTIAL bits, for convenience
  ELEMENT_POTENTIAL_RESTYLE_ROOT_FLAGS =
    ELEMENT_IS_POTENTIAL_RESTYLE_ROOT |
    ELEMENT_IS_POTENTIAL_ANIMATION_ONLY_RESTYLE_ROOT,

  // All of the restyle bits together, for convenience.
  ELEMENT_ALL_RESTYLE_FLAGS = ELEMENT_PENDING_RESTYLE_FLAGS |
                              ELEMENT_POTENTIAL_RESTYLE_ROOT_FLAGS |
                              ELEMENT_IS_CONDITIONAL_RESTYLE_ANCESTOR,

  // Set if this element is marked as 'scrollgrab' (see bug 912666)
  ELEMENT_HAS_SCROLLGRAB = ELEMENT_FLAG_BIT(3),

  // Remaining bits are for subclasses
  ELEMENT_TYPE_SPECIFIC_BITS_OFFSET = NODE_TYPE_SPECIFIC_BITS_OFFSET + 4
};

#undef ELEMENT_FLAG_BIT

// Make sure we have space for our bits
ASSERT_NODE_FLAGS_SPACE(ELEMENT_TYPE_SPECIFIC_BITS_OFFSET);

namespace mozilla {
enum class CSSPseudoElementType : uint8_t;
class EventChainPostVisitor;
class EventChainPreVisitor;
class EventChainVisitor;
class EventListenerManager;
class EventStateManager;

namespace dom {

class Animation;
class CustomElementRegistry;
class Link;
class DOMRect;
class DOMRectList;
class DestinationInsertionPointList;
class Grid;

// IID for the dom::Element interface
#define NS_ELEMENT_IID \
{ 0xc67ed254, 0xfd3b, 0x4b10, \
  { 0x96, 0xa2, 0xc5, 0x8b, 0x7b, 0x64, 0x97, 0xd1 } }

class Element : public FragmentOrElement
{
public:
#ifdef MOZILLA_INTERNAL_API
  explicit Element(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo) :
    FragmentOrElement(aNodeInfo),
    mState(NS_EVENT_STATE_MOZ_READONLY)
  {
    MOZ_ASSERT(mNodeInfo->NodeType() == nsIDOMNode::ELEMENT_NODE,
               "Bad NodeType in aNodeInfo");
    SetIsElement();
  }
#endif // MOZILLA_INTERNAL_API

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ELEMENT_IID)

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) override;

  /**
   * Method to get the full state of this element.  See mozilla/EventStates.h
   * for the possible bits that could be set here.
   */
  EventStates State() const
  {
    // mState is maintained by having whoever might have changed it
    // call UpdateState() or one of the other mState mutators.
    return mState;
  }

  /**
   * Ask this element to update its state.  If aNotify is false, then
   * state change notifications will not be dispatched; in that
   * situation it is the caller's responsibility to dispatch them.
   *
   * In general, aNotify should only be false if we're guaranteed that
   * the element can't have a frame no matter what its style is
   * (e.g. if we're in the middle of adding it to the document or
   * removing it from the document).
   */
  void UpdateState(bool aNotify);

  /**
   * Method to update mState with link state information.  This does not notify.
   */
  void UpdateLinkState(EventStates aState);

  virtual int32_t TabIndexDefault()
  {
    return -1;
  }

  /**
   * Get tabIndex of this element. If not found, return TabIndexDefault.
   */
  int32_t TabIndex();

  /**
   * Set tabIndex value to this element.
   */
  void SetTabIndex(int32_t aTabIndex, mozilla::ErrorResult& aError);

  /**
   * Make focus on this element.
   */
  virtual void Focus(mozilla::ErrorResult& aError);

  /**
   * Show blur and clear focus.
   */
  virtual void Blur(mozilla::ErrorResult& aError);

  /**
   * The style state of this element. This is the real state of the element
   * with any style locks applied for pseudo-class inspecting.
   */
  EventStates StyleState() const
  {
    if (!HasLockedStyleStates()) {
      return mState;
    }
    return StyleStateFromLocks();
  }

  /**
   * The style state locks applied to this element.
   */
  EventStates LockedStyleStates() const;

  /**
   * Add a style state lock on this element.
   */
  void LockStyleStates(EventStates aStates);

  /**
   * Remove a style state lock on this element.
   */
  void UnlockStyleStates(EventStates aStates);

  /**
   * Clear all style state locks on this element.
   */
  void ClearStyleStateLocks();

  /**
   * Get the inline style declaration, if any, for this element.
   */
  virtual DeclarationBlock* GetInlineStyleDeclaration();

  /**
   * Set the inline style declaration for this element. This will send
   * an appropriate AttributeChanged notification if aNotify is true.
   */
  virtual nsresult SetInlineStyleDeclaration(DeclarationBlock* aDeclaration,
                                             const nsAString* aSerialized,
                                             bool aNotify);

  /**
   * Get the SMIL override style declaration for this element. If the
   * rule hasn't been created, this method simply returns null.
   */
  virtual DeclarationBlock* GetSMILOverrideStyleDeclaration();

  /**
   * Set the SMIL override style declaration for this element. If
   * aNotify is true, this method will notify the document's pres
   * context, so that the style changes will be noticed.
   */
  virtual nsresult SetSMILOverrideStyleDeclaration(
    DeclarationBlock* aDeclaration, bool aNotify);

  /**
   * Returns a new nsISMILAttr that allows the caller to animate the given
   * attribute on this element.
   *
   * The CALLER OWNS the result and is responsible for deleting it.
   */
  virtual nsISMILAttr* GetAnimatedAttr(int32_t aNamespaceID, nsIAtom* aName)
  {
    return nullptr;
  }

  /**
   * Get the SMIL override style for this element. This is a style declaration
   * that is applied *after* the inline style, and it can be used e.g. to store
   * animated style values.
   *
   * Note: This method is analogous to the 'GetStyle' method in
   * nsGenericHTMLElement and nsStyledElement.
   */
  virtual nsICSSDeclaration* GetSMILOverrideStyle();

  /**
   * Returns if the element is labelable as per HTML specification.
   */
  virtual bool IsLabelable() const;

  /**
   * Returns if the element is interactive content as per HTML specification.
   */
  virtual bool IsInteractiveHTMLContent(bool aIgnoreTabindex) const;

  /**
   * Is the attribute named stored in the mapped attributes?
   *
   * // XXXbz we use this method in HasAttributeDependentStyle, so svg
   *    returns true here even though it stores nothing in the mapped
   *    attributes.
   */
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  /**
   * Get a hint that tells the style system what to do when
   * an attribute on this node changes, if something needs to happen
   * in response to the change *other* than the result of what is
   * mapped into style data via any type of style rule.
   */
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              int32_t aModType) const;

  inline Directionality GetDirectionality() const {
    if (HasFlag(NODE_HAS_DIRECTION_RTL)) {
      return eDir_RTL;
    }

    if (HasFlag(NODE_HAS_DIRECTION_LTR)) {
      return eDir_LTR;
    }

    return eDir_NotSet;
  }

  inline void SetDirectionality(Directionality aDir, bool aNotify) {
    UnsetFlags(NODE_ALL_DIRECTION_FLAGS);
    if (!aNotify) {
      RemoveStatesSilently(DIRECTION_STATES);
    }

    switch (aDir) {
      case (eDir_RTL):
        SetFlags(NODE_HAS_DIRECTION_RTL);
        if (!aNotify) {
          AddStatesSilently(NS_EVENT_STATE_RTL);
        }
        break;

      case(eDir_LTR):
        SetFlags(NODE_HAS_DIRECTION_LTR);
        if (!aNotify) {
          AddStatesSilently(NS_EVENT_STATE_LTR);
        }
        break;

      default:
        break;
    }

    /*
     * Only call UpdateState if we need to notify, because we call
     * SetDirectionality for every element, and UpdateState is very very slow
     * for some elements.
     */
    if (aNotify) {
      UpdateState(true);
    }
  }

  bool GetBindingURL(nsIDocument *aDocument, css::URLValue **aResult);

  // The bdi element defaults to dir=auto if it has no dir attribute set.
  // Other elements will only have dir=auto if they have an explicit dir=auto,
  // which will mean that HasValidDir() returns true but HasFixedDir() returns
  // false
  inline bool HasDirAuto() const {
    return (!HasFixedDir() &&
            (HasValidDir() || IsHTMLElement(nsGkAtoms::bdi)));
  }

  Directionality GetComputedDirectionality() const;

protected:
  /**
   * Method to get the _intrinsic_ content state of this element.  This is the
   * state that is independent of the element's presentation.  To get the full
   * content state, use State().  See mozilla/EventStates.h for
   * the possible bits that could be set here.
   */
  virtual EventStates IntrinsicState() const;

  /**
   * Method to add state bits.  This should be called from subclass
   * constructors to set up our event state correctly at construction
   * time and other places where we don't want to notify a state
   * change.
   */
  void AddStatesSilently(EventStates aStates)
  {
    mState |= aStates;
  }

  /**
   * Method to remove state bits.  This should be called from subclass
   * constructors to set up our event state correctly at construction
   * time and other places where we don't want to notify a state
   * change.
   */
  void RemoveStatesSilently(EventStates aStates)
  {
    mState &= ~aStates;
  }

private:
  // Need to allow the ESM, nsGlobalWindow, and the focus manager to
  // set our state
  friend class mozilla::EventStateManager;
  friend class ::nsGlobalWindow;
  friend class ::nsFocusManager;

  // Allow CusomtElementRegistry to call AddStates.
  friend class CustomElementRegistry;

  // Also need to allow Link to call UpdateLinkState.
  friend class Link;

  void NotifyStateChange(EventStates aStates);

  void NotifyStyleStateChange(EventStates aStates);

  // Style state computed from element's state and style locks.
  EventStates StyleStateFromLocks() const;

protected:
  // Methods for the ESM to manage state bits.  These will handle
  // setting up script blockers when they notify, so no need to do it
  // in the callers unless desired.
  virtual void AddStates(EventStates aStates)
  {
    NS_PRECONDITION(!aStates.HasAtLeastOneOfStates(INTRINSIC_STATES),
                    "Should only be adding ESM-managed states here");
    AddStatesSilently(aStates);
    NotifyStateChange(aStates);
  }
  virtual void RemoveStates(EventStates aStates)
  {
    NS_PRECONDITION(!aStates.HasAtLeastOneOfStates(INTRINSIC_STATES),
                    "Should only be removing ESM-managed states here");
    RemoveStatesSilently(aStates);
    NotifyStateChange(aStates);
  }
public:
  virtual void UpdateEditableState(bool aNotify) override;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;

  /**
   * Normalizes an attribute name and returns it as a nodeinfo if an attribute
   * with that name exists. This method is intended for character case
   * conversion if the content object is case insensitive (e.g. HTML). Returns
   * the nodeinfo of the attribute with the specified name if one exists or
   * null otherwise.
   *
   * @param aStr the unparsed attribute string
   * @return the node info. May be nullptr.
   */
  already_AddRefed<mozilla::dom::NodeInfo>
  GetExistingAttrNameFromQName(const nsAString& aStr) const;

  nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
  }

  /**
   * Helper for SetAttr/SetParsedAttr. This method will return true if aNotify
   * is true or there are mutation listeners that must be triggered, the
   * attribute is currently set, and the new value that is about to be set is
   * different to the current value. As a perf optimization the new and old
   * values will not actually be compared if we aren't notifying and we don't
   * have mutation listeners (in which case it's cheap to just return false
   * and let the caller go ahead and set the value).
   * @param aOldValue Set to the old value of the attribute, but only if there
   *   are event listeners. If set, the type of aOldValue will be either
   *   nsAttrValue::eString or nsAttrValue::eAtom.
   * @param aModType Set to nsIDOMMutationEvent::MODIFICATION or to
   *   nsIDOMMutationEvent::ADDITION, but only if this helper returns true
   * @param aHasListeners Set to true if there are mutation event listeners
   *   listening for NS_EVENT_BITS_MUTATION_ATTRMODIFIED
   */
  bool MaybeCheckSameAttrVal(int32_t aNamespaceID, nsIAtom* aName,
                             nsIAtom* aPrefix,
                             const nsAttrValueOrString& aValue,
                             bool aNotify, nsAttrValue& aOldValue,
                             uint8_t* aModType, bool* aHasListeners);

  bool OnlyNotifySameValueSet(int32_t aNamespaceID, nsIAtom* aName,
                              nsIAtom* aPrefix,
                              const nsAttrValueOrString& aValue,
                              bool aNotify, nsAttrValue& aOldValue,
                              uint8_t* aModType, bool* aHasListeners);

  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom* aName, nsIAtom* aPrefix,
                           const nsAString& aValue, bool aNotify) override;
  // aParsedValue receives the old value of the attribute. That's useful if
  // either the input or output value of aParsedValue is StoresOwnData.
  nsresult SetParsedAttr(int32_t aNameSpaceID, nsIAtom* aName, nsIAtom* aPrefix,
                         nsAttrValue& aParsedValue, bool aNotify);
  // GetAttr is not inlined on purpose, to keep down codesize from all
  // the inlined nsAttrValue bits for C++ callers.
  bool GetAttr(int32_t aNameSpaceID, nsIAtom* aName,
               nsAString& aResult) const;
  inline bool HasAttr(int32_t aNameSpaceID, nsIAtom* aName) const;
  // aCaseSensitive == eIgnoreCaase means ASCII case-insensitive matching.
  inline bool AttrValueIs(int32_t aNameSpaceID, nsIAtom* aName,
                          const nsAString& aValue,
                          nsCaseTreatment aCaseSensitive) const;
  inline bool AttrValueIs(int32_t aNameSpaceID, nsIAtom* aName,
                          nsIAtom* aValue,
                          nsCaseTreatment aCaseSensitive) const;
  virtual int32_t FindAttrValueIn(int32_t aNameSpaceID,
                                  nsIAtom* aName,
                                  AttrValuesArray* aValues,
                                  nsCaseTreatment aCaseSensitive) const override;
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify) override;
  virtual const nsAttrName* GetAttrNameAt(uint32_t aIndex) const override;
  virtual BorrowedAttrInfo GetAttrInfoAt(uint32_t aIndex) const override;
  virtual uint32_t GetAttrCount() const override;
  virtual bool IsNodeOfType(uint32_t aFlags) const override;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override
  {
    List(out, aIndent, EmptyCString());
  }
  virtual void DumpContent(FILE* out, int32_t aIndent, bool aDumpAll) const override;
  void List(FILE* out, int32_t aIndent, const nsCString& aPrefix) const;
  void ListAttributes(FILE* out) const;
#endif

  void Describe(nsAString& aOutDescription) const override;

  /*
   * Attribute Mapping Helpers
   */
  struct MappedAttributeEntry {
    nsIAtom** attribute;
  };

  /**
   * A common method where you can just pass in a list of maps to check
   * for attribute dependence. Most implementations of
   * IsAttributeMapped should use this function as a default
   * handler.
   */
  template<size_t N>
  static bool
  FindAttributeDependence(const nsIAtom* aAttribute,
                          const MappedAttributeEntry* const (&aMaps)[N])
  {
    return FindAttributeDependence(aAttribute, aMaps, N);
  }

  static nsIAtom*** HTMLSVGPropertiesToTraverseAndUnlink();

private:
  void DescribeAttribute(uint32_t index, nsAString& aOutDescription) const;

  static bool
  FindAttributeDependence(const nsIAtom* aAttribute,
                          const MappedAttributeEntry* const aMaps[],
                          uint32_t aMapCount);

protected:
  inline bool GetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                      DOMString& aResult) const
  {
    NS_ASSERTION(nullptr != aName, "must have attribute name");
    NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown,
                 "must have a real namespace ID!");
    MOZ_ASSERT(aResult.HasStringBuffer() && aResult.StringBufferLength() == 0,
               "Should have empty string coming in");
    const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName, aNameSpaceID);
    if (val) {
      val->ToString(aResult);
      return true;
    }
    // else DOMString comes pre-emptied.
    return false;
  }

public:
  bool HasAttrs() const { return mAttrsAndChildren.HasAttrs(); }

  inline bool GetAttr(const nsAString& aName, DOMString& aResult) const
  {
    MOZ_ASSERT(aResult.HasStringBuffer() && aResult.StringBufferLength() == 0,
               "Should have empty string coming in");
    const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName);
    if (val) {
      val->ToString(aResult);
      return true;
    }
    // else DOMString comes pre-emptied.
    return false;
  }

  void GetTagName(nsAString& aTagName) const
  {
    aTagName = NodeName();
  }
  void GetId(nsAString& aId) const
  {
    GetAttr(kNameSpaceID_None, nsGkAtoms::id, aId);
  }
  void GetId(DOMString& aId) const
  {
    GetAttr(kNameSpaceID_None, nsGkAtoms::id, aId);
  }
  void SetId(const nsAString& aId)
  {
    SetAttr(kNameSpaceID_None, nsGkAtoms::id, aId, true);
  }
  void GetClassName(nsAString& aClassName)
  {
    GetAttr(kNameSpaceID_None, nsGkAtoms::_class, aClassName);
  }
  void GetClassName(DOMString& aClassName)
  {
    GetAttr(kNameSpaceID_None, nsGkAtoms::_class, aClassName);
  }
  void SetClassName(const nsAString& aClassName)
  {
    SetAttr(kNameSpaceID_None, nsGkAtoms::_class, aClassName, true);
  }

  nsDOMTokenList* ClassList();
  nsDOMAttributeMap* Attributes()
  {
    nsDOMSlots* slots = DOMSlots();
    if (!slots->mAttributeMap) {
      slots->mAttributeMap = new nsDOMAttributeMap(this);
    }

    return slots->mAttributeMap;
  }

  void GetAttributeNames(nsTArray<nsString>& aResult);

  void GetAttribute(const nsAString& aName, nsString& aReturn)
  {
    DOMString str;
    GetAttribute(aName, str);
    str.ToString(aReturn);
  }

  void GetAttribute(const nsAString& aName, DOMString& aReturn);
  void GetAttributeNS(const nsAString& aNamespaceURI,
                      const nsAString& aLocalName,
                      nsAString& aReturn);
  void SetAttribute(const nsAString& aName, const nsAString& aValue,
                    ErrorResult& aError);
  void SetAttributeNS(const nsAString& aNamespaceURI,
                      const nsAString& aLocalName,
                      const nsAString& aValue,
                      ErrorResult& aError);
  void RemoveAttribute(const nsAString& aName,
                       ErrorResult& aError);
  void RemoveAttributeNS(const nsAString& aNamespaceURI,
                         const nsAString& aLocalName,
                         ErrorResult& aError);
  bool HasAttribute(const nsAString& aName) const
  {
    return InternalGetAttrNameFromQName(aName) != nullptr;
  }
  bool HasAttributeNS(const nsAString& aNamespaceURI,
                      const nsAString& aLocalName) const;
  bool HasAttributes() const
  {
    return HasAttrs();
  }
  Element* Closest(const nsAString& aSelector,
                   ErrorResult& aResult);
  bool Matches(const nsAString& aSelector,
               ErrorResult& aError);
  already_AddRefed<nsIHTMLCollection>
    GetElementsByTagName(const nsAString& aQualifiedName);
  already_AddRefed<nsIHTMLCollection>
    GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                           const nsAString& aLocalName,
                           ErrorResult& aError);
  already_AddRefed<nsIHTMLCollection>
    GetElementsByClassName(const nsAString& aClassNames);

private:
  /**
   * Implement the algorithm specified at
   * https://dom.spec.whatwg.org/#insert-adjacent for both
   * |insertAdjacentElement()| and |insertAdjacentText()| APIs.
   */
  nsINode* InsertAdjacent(const nsAString& aWhere,
                          nsINode* aNode,
                          ErrorResult& aError);

public:
  Element* InsertAdjacentElement(const nsAString& aWhere,
                                 Element& aElement,
                                 ErrorResult& aError);

  void InsertAdjacentText(const nsAString& aWhere,
                          const nsAString& aData,
                          ErrorResult& aError);

  void SetPointerCapture(int32_t aPointerId, ErrorResult& aError)
  {
    bool activeState = false;
    if (!nsIPresShell::GetPointerInfo(aPointerId, activeState)) {
      aError.Throw(NS_ERROR_DOM_INVALID_POINTER_ERR);
      return;
    }
    if (!IsInUncomposedDoc()) {
      aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return;
    }
    if (!activeState) {
      return;
    }
    nsIPresShell::SetPointerCapturingContent(aPointerId, this);
  }
  void ReleasePointerCapture(int32_t aPointerId, ErrorResult& aError)
  {
    bool activeState = false;
    if (!nsIPresShell::GetPointerInfo(aPointerId, activeState)) {
      aError.Throw(NS_ERROR_DOM_INVALID_POINTER_ERR);
      return;
    }
    if (HasPointerCapture(aPointerId)) {
      nsIPresShell::ReleasePointerCapturingContent(aPointerId);
    }
  }
  bool HasPointerCapture(long aPointerId)
  {
    nsIPresShell::PointerCaptureInfo* pointerCaptureInfo =
      nsIPresShell::GetPointerCaptureInfo(aPointerId);
    if (pointerCaptureInfo && pointerCaptureInfo->mPendingContent == this) {
      return true;
    }
    return false;
  }
  void SetCapture(bool aRetargetToElement)
  {
    // If there is already an active capture, ignore this request. This would
    // occur if a splitter, frame resizer, etc had already captured and we don't
    // want to override those.
    if (!nsIPresShell::GetCapturingContent()) {
      nsIPresShell::SetCapturingContent(this, CAPTURE_PREVENTDRAG |
        (aRetargetToElement ? CAPTURE_RETARGETTOELEMENT : 0));
    }
  }

  void SetCaptureAlways(bool aRetargetToElement)
  {
    nsIPresShell::SetCapturingContent(this,
        CAPTURE_PREVENTDRAG | CAPTURE_IGNOREALLOWED |
        (aRetargetToElement ? CAPTURE_RETARGETTOELEMENT : 0));
  }

  void ReleaseCapture()
  {
    if (nsIPresShell::GetCapturingContent() == this) {
      nsIPresShell::SetCapturingContent(nullptr, 0);
    }
  }

  void RequestFullscreen(ErrorResult& aError);
  void RequestPointerLock();
  Attr* GetAttributeNode(const nsAString& aName);
  already_AddRefed<Attr> SetAttributeNode(Attr& aNewAttr,
                                          ErrorResult& aError);
  already_AddRefed<Attr> RemoveAttributeNode(Attr& aOldAttr,
                                             ErrorResult& aError);
  Attr* GetAttributeNodeNS(const nsAString& aNamespaceURI,
                           const nsAString& aLocalName);
  already_AddRefed<Attr> SetAttributeNodeNS(Attr& aNewAttr,
                                            ErrorResult& aError);

  already_AddRefed<DOMRectList> GetClientRects();
  already_AddRefed<DOMRect> GetBoundingClientRect();

  already_AddRefed<ShadowRoot> CreateShadowRoot(ErrorResult& aError);
  already_AddRefed<DestinationInsertionPointList> GetDestinationInsertionPoints();

  ShadowRoot *FastGetShadowRoot() const
  {
    nsDOMSlots* slots = GetExistingDOMSlots();
    return slots ? slots->mShadowRoot.get() : nullptr;
  }

  void ScrollIntoView();
  void ScrollIntoView(bool aTop);
  void ScrollIntoView(const ScrollIntoViewOptions &aOptions);
  void Scroll(double aXScroll, double aYScroll);
  void Scroll(const ScrollToOptions& aOptions);
  void ScrollTo(double aXScroll, double aYScroll);
  void ScrollTo(const ScrollToOptions& aOptions);
  void ScrollBy(double aXScrollDif, double aYScrollDif);
  void ScrollBy(const ScrollToOptions& aOptions);
  /* Scrolls without flushing the layout.
   * aDx is the x offset, aDy the y offset in CSS pixels.
   * Returns true if we actually scrolled.
   */
  bool ScrollByNoFlush(int32_t aDx, int32_t aDy);
  int32_t ScrollTop();
  void SetScrollTop(int32_t aScrollTop);
  int32_t ScrollLeft();
  void SetScrollLeft(int32_t aScrollLeft);
  int32_t ScrollWidth();
  int32_t ScrollHeight();
  void MozScrollSnap();
  int32_t ClientTop()
  {
    return nsPresContext::AppUnitsToIntCSSPixels(GetClientAreaRect().y);
  }
  int32_t ClientLeft()
  {
    return nsPresContext::AppUnitsToIntCSSPixels(GetClientAreaRect().x);
  }
  int32_t ClientWidth()
  {
    return nsPresContext::AppUnitsToIntCSSPixels(GetClientAreaRect().width);
  }
  int32_t ClientHeight()
  {
    return nsPresContext::AppUnitsToIntCSSPixels(GetClientAreaRect().height);
  }
  int32_t ScrollTopMin()
  {
    nsIScrollableFrame* sf = GetScrollFrame();
    return sf ?
           nsPresContext::AppUnitsToIntCSSPixels(sf->GetScrollRange().y) : 0;
  }
  int32_t ScrollTopMax()
  {
    nsIScrollableFrame* sf = GetScrollFrame();
    return sf ?
           nsPresContext::AppUnitsToIntCSSPixels(sf->GetScrollRange().YMost()) :
           0;
  }
  int32_t ScrollLeftMin()
  {
    nsIScrollableFrame* sf = GetScrollFrame();
    return sf ?
           nsPresContext::AppUnitsToIntCSSPixels(sf->GetScrollRange().x) : 0;
  }
  int32_t ScrollLeftMax()
  {
    nsIScrollableFrame* sf = GetScrollFrame();
    return sf ?
           nsPresContext::AppUnitsToIntCSSPixels(sf->GetScrollRange().XMost()) :
           0;
  }

  void GetGridFragments(nsTArray<RefPtr<Grid>>& aResult);

  already_AddRefed<Animation>
  Animate(JSContext* aContext,
          JS::Handle<JSObject*> aKeyframes,
          const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
          ErrorResult& aError);

  // A helper method that factors out the common functionality needed by
  // Element::Animate and CSSPseudoElement::Animate
  static already_AddRefed<Animation>
  Animate(const Nullable<ElementOrCSSPseudoElement>& aTarget,
          JSContext* aContext,
          JS::Handle<JSObject*> aKeyframes,
          const UnrestrictedDoubleOrKeyframeAnimationOptions& aOptions,
          ErrorResult& aError);

  // Note: GetAnimations will flush style while GetAnimationsUnsorted won't.
  // Callers must keep this element alive because flushing style may destroy
  // this element.
  void GetAnimations(const AnimationFilter& filter,
                     nsTArray<RefPtr<Animation>>& aAnimations);
  static void GetAnimationsUnsorted(Element* aElement,
                                    CSSPseudoElementType aPseudoType,
                                    nsTArray<RefPtr<Animation>>& aAnimations);

  NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML);
  virtual void SetInnerHTML(const nsAString& aInnerHTML, ErrorResult& aError);
  void GetOuterHTML(nsAString& aOuterHTML);
  void SetOuterHTML(const nsAString& aOuterHTML, ErrorResult& aError);
  void InsertAdjacentHTML(const nsAString& aPosition, const nsAString& aText,
                          ErrorResult& aError);

  //----------------------------------------

  /**
   * Add a script event listener with the given event handler name
   * (like onclick) and with the value as JS
   * @param aEventName the event listener name
   * @param aValue the JS to attach
   * @param aDefer indicates if deferred execution is allowed
   */
  nsresult SetEventHandler(nsIAtom* aEventName,
                           const nsAString& aValue,
                           bool aDefer = true);

  /**
   * Do whatever needs to be done when the mouse leaves a link
   */
  nsresult LeaveLink(nsPresContext* aPresContext);

  static bool ShouldBlur(nsIContent *aContent);

  /**
   * Method to create and dispatch a left-click event loosely based on
   * aSourceEvent. If aFullDispatch is true, the event will be dispatched
   * through the full dispatching of the presshell of the aPresContext; if it's
   * false the event will be dispatched only as a DOM event.
   * If aPresContext is nullptr, this does nothing.
   *
   * @param aFlags      Extra flags for the dispatching event.  The true flags
   *                    will be respected.
   */
  static nsresult DispatchClickEvent(nsPresContext* aPresContext,
                                     WidgetInputEvent* aSourceEvent,
                                     nsIContent* aTarget,
                                     bool aFullDispatch,
                                     const EventFlags* aFlags,
                                     nsEventStatus* aStatus);

  /**
   * Method to dispatch aEvent to aTarget. If aFullDispatch is true, the event
   * will be dispatched through the full dispatching of the presshell of the
   * aPresContext; if it's false the event will be dispatched only as a DOM
   * event.
   * If aPresContext is nullptr, this does nothing.
   */
  using nsIContent::DispatchEvent;
  static nsresult DispatchEvent(nsPresContext* aPresContext,
                                WidgetEvent* aEvent,
                                nsIContent* aTarget,
                                bool aFullDispatch,
                                nsEventStatus* aStatus);

  /**
   * Get the primary frame for this content with flushing
   *
   * @param aType the kind of flush to do, typically Flush_Frames or
   *              Flush_Layout
   * @return the primary frame
   */
  nsIFrame* GetPrimaryFrame(mozFlushType aType);
  // Work around silly C++ name hiding stuff
  nsIFrame* GetPrimaryFrame() const { return nsIContent::GetPrimaryFrame(); }

  const nsAttrValue* GetParsedAttr(nsIAtom* aAttr) const
  {
    return mAttrsAndChildren.GetAttr(aAttr);
  }

  const nsAttrValue* GetParsedAttr(nsIAtom* aAttr, int32_t aNameSpaceID) const
  {
    return mAttrsAndChildren.GetAttr(aAttr, aNameSpaceID);
  }

  /**
   * Returns the attribute map, if there is one.
   *
   * @return existing attribute map or nullptr.
   */
  nsDOMAttributeMap *GetAttributeMap()
  {
    nsDOMSlots *slots = GetExistingDOMSlots();

    return slots ? slots->mAttributeMap.get() : nullptr;
  }

  virtual void RecompileScriptEventListeners()
  {
  }

  /**
   * Get the attr info for the given namespace ID and attribute name.  The
   * namespace ID must not be kNameSpaceID_Unknown and the name must not be
   * null.  Note that this can only return info on attributes that actually
   * live on this element (and is only virtual to handle XUL prototypes).  That
   * is, this should only be called from methods that only care about attrs
   * that effectively live in mAttrsAndChildren.
   */
  virtual BorrowedAttrInfo GetAttrInfo(int32_t aNamespaceID, nsIAtom* aName) const;

  virtual void NodeInfoChanged()
  {
  }

  /**
   * Parse a string into an nsAttrValue for a CORS attribute.  This
   * never fails.  The resulting value is an enumerated value whose
   * GetEnumValue() returns one of the above constants.
   */
  static void ParseCORSValue(const nsAString& aValue, nsAttrValue& aResult);

  /**
   * Return the CORS mode for a given string
   */
  static CORSMode StringToCORSMode(const nsAString& aValue);

  /**
   * Return the CORS mode for a given nsAttrValue (which may be null,
   * but if not should have been parsed via ParseCORSValue).
   */
  static CORSMode AttrValueToCORSMode(const nsAttrValue* aValue);

  // These are just used to implement nsIDOMElement using
  // NS_FORWARD_NSIDOMELEMENT_TO_GENERIC and for quickstubs.
  void
    GetElementsByTagName(const nsAString& aQualifiedName,
                         nsIDOMHTMLCollection** aResult);
  nsresult
    GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                           const nsAString& aLocalName,
                           nsIDOMHTMLCollection** aResult);
  nsresult
    GetElementsByClassName(const nsAString& aClassNames,
                           nsIDOMHTMLCollection** aResult);
  void GetClassList(nsISupports** aClassList);

  virtual JSObject* WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) final override;

  nsINode* GetScopeChainParent() const override;

  /**
   * Locate an nsIEditor rooted at this content node, if there is one.
   */
  nsIEditor* GetEditorInternal();

  /**
   * Helper method for NS_IMPL_BOOL_ATTR macro.
   * Gets value of boolean attribute. Only works for attributes in null
   * namespace.
   *
   * @param aAttr    name of attribute.
   * @param aValue   Boolean value of attribute.
   */
  bool GetBoolAttr(nsIAtom* aAttr) const
  {
    return HasAttr(kNameSpaceID_None, aAttr);
  }

  /**
   * Helper method for NS_IMPL_BOOL_ATTR macro.
   * Sets value of boolean attribute by removing attribute or setting it to
   * the empty string. Only works for attributes in null namespace.
   *
   * @param aAttr    name of attribute.
   * @param aValue   Boolean value of attribute.
   */
  nsresult SetBoolAttr(nsIAtom* aAttr, bool aValue);

  /**
   * Helper method for NS_IMPL_ENUM_ATTR_DEFAULT_VALUE.
   * Gets the enum value string of an attribute and using a default value if
   * the attribute is missing or the string is an invalid enum value.
   *
   * @param aType     the name of the attribute.
   * @param aDefault  the default value if the attribute is missing or invalid.
   * @param aResult   string corresponding to the value [out].
   */
  void GetEnumAttr(nsIAtom* aAttr,
                   const char* aDefault,
                   nsAString& aResult) const;

  /**
   * Helper method for NS_IMPL_ENUM_ATTR_DEFAULT_MISSING_INVALID_VALUES.
   * Gets the enum value string of an attribute and using the default missing
   * value if the attribute is missing or the default invalid value if the
   * string is an invalid enum value.
   *
   * @param aType            the name of the attribute.
   * @param aDefaultMissing  the default value if the attribute is missing.  If
                             null and the attribute is missing, aResult will be
                             set to the null DOMString; this only matters for
                             cases in which we're reflecting a nullable string.
   * @param aDefaultInvalid  the default value if the attribute is invalid.
   * @param aResult          string corresponding to the value [out].
   */
  void GetEnumAttr(nsIAtom* aAttr,
                   const char* aDefaultMissing,
                   const char* aDefaultInvalid,
                   nsAString& aResult) const;

  /**
   * Unset an attribute.
   */
  void UnsetAttr(nsIAtom* aAttr, ErrorResult& aError)
  {
    aError = UnsetAttr(kNameSpaceID_None, aAttr, true);
  }

  /**
   * Set an attribute in the simplest way possible.
   */
  void SetAttr(nsIAtom* aAttr, const nsAString& aValue, ErrorResult& aError)
  {
    aError = SetAttr(kNameSpaceID_None, aAttr, aValue, true);
  }

  /**
   * Set a content attribute via a reflecting nullable string IDL
   * attribute (e.g. a CORS attribute).  If DOMStringIsNull(aValue),
   * this will actually remove the content attribute.
   */
  void SetOrRemoveNullableStringAttr(nsIAtom* aName, const nsAString& aValue,
                                     ErrorResult& aError);

  /**
   * Retrieve the ratio of font-size-inflated text font size to computed font
   * size for this element. This will query the element for its primary frame,
   * and then use this to get font size inflation information about the frame.
   *
   * @returns The font size inflation ratio (inflated font size to uninflated
   *          font size) for the primary frame of this element. Returns 1.0
   *          by default if font size inflation is not enabled. Returns -1
   *          if the element does not have a primary frame.
   *
   * @note The font size inflation ratio that is returned is actually the
   *       font size inflation data for the element's _primary frame_, not the
   *       element itself, but for most purposes, this should be sufficient.
   */
  float FontSizeInflation();

  net::ReferrerPolicy GetReferrerPolicyAsEnum();

  /*
   * Helpers for .dataset.  This is implemented on Element, though only some
   * sorts of elements expose it to JS as a .dataset property
   */
  // Getter, to be called from bindings.
  already_AddRefed<nsDOMStringMap> Dataset();
  // Callback for destructor of dataset to ensure to null out our weak pointer
  // to it.
  void ClearDataset();

  void RegisterIntersectionObserver(DOMIntersectionObserver* aObserver);
  void UnregisterIntersectionObserver(DOMIntersectionObserver* aObserver);
  bool UpdateIntersectionObservation(DOMIntersectionObserver* aObserver, int32_t threshold);

protected:
  /*
   * Named-bools for use with SetAttrAndNotify to make call sites easier to
   * read.
   */
  static const bool kFireMutationEvent           = true;
  static const bool kDontFireMutationEvent       = false;
  static const bool kNotifyDocumentObservers     = true;
  static const bool kDontNotifyDocumentObservers = false;
  static const bool kCallAfterSetAttr            = true;
  static const bool kDontCallAfterSetAttr        = false;

  /**
   * Set attribute and (if needed) notify documentobservers and fire off
   * mutation events.  This will send the AttributeChanged notification.
   * Callers of this method are responsible for calling AttributeWillChange,
   * since that needs to happen before the new attr value has been set, and
   * in particular before it has been parsed.
   *
   * For the boolean parameters, consider using the named bools above to aid
   * code readability.
   *
   * @param aNamespaceID  namespace of attribute
   * @param aAttribute    local-name of attribute
   * @param aPrefix       aPrefix of attribute
   * @param aOldValue     The old value of the attribute to use as a fallback
   *                      in the cases where the actual old value (i.e.
   *                      its current value) is !StoresOwnData() --- in which
   *                      case the current value is probably already useless.
   *                      If the current value is StoresOwnData() (or absent),
   *                      aOldValue will not be used.
   * @param aParsedValue  parsed new value of attribute. Replaced by the
   *                      old value of the attribute. This old value is only
   *                      useful if either it or the new value is StoresOwnData.
   * @param aModType      nsIDOMMutationEvent::MODIFICATION or ADDITION.  Only
   *                      needed if aFireMutation or aNotify is true.
   * @param aFireMutation should mutation-events be fired?
   * @param aNotify       should we notify document-observers?
   * @param aCallAfterSetAttr should we call AfterSetAttr?
   */
  nsresult SetAttrAndNotify(int32_t aNamespaceID,
                            nsIAtom* aName,
                            nsIAtom* aPrefix,
                            const nsAttrValue& aOldValue,
                            nsAttrValue& aParsedValue,
                            uint8_t aModType,
                            bool aFireMutation,
                            bool aNotify,
                            bool aCallAfterSetAttr);

  /**
   * Scroll to a new position using behavior evaluated from CSS and
   * a CSSOM-View DOM method ScrollOptions dictionary.  The scrolling may
   * be performed asynchronously or synchronously depending on the resolved
   * scroll-behavior.
   *
   * @param aScroll       Destination of scroll, in CSS pixels
   * @param aOptions      Dictionary of options to be evaluated
   */
  void Scroll(const CSSIntPoint& aScroll, const ScrollOptions& aOptions);

  /**
   * Convert an attribute string value to attribute type based on the type of
   * attribute.  Called by SetAttr().  Note that at the moment we only do this
   * for attributes in the null namespace (kNameSpaceID_None).
   *
   * @param aNamespaceID the namespace of the attribute to convert
   * @param aAttribute the attribute to convert
   * @param aValue the string value to convert
   * @param aResult the nsAttrValue [OUT]
   * @return true if the parsing was successful, false otherwise
   */
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  /**
   * Try to set the attribute as a mapped attribute, if applicable.  This will
   * only be called for attributes that are in the null namespace and only on
   * attributes that returned true when passed to IsAttributeMapped.  The
   * caller will not try to set the attr in any other way if this method
   * returns true (the value of aRetval does not matter for that purpose).
   *
   * @param aDocument the current document of this node (an optimization)
   * @param aName the name of the attribute
   * @param aValue the nsAttrValue to set
   * @param [out] aRetval the nsresult status of the operation, if any.
   * @return true if the setting was attempted, false otherwise.
   */
  virtual bool SetMappedAttribute(nsIDocument* aDocument,
                                    nsIAtom* aName,
                                    nsAttrValue& aValue,
                                    nsresult* aRetval);

  /**
   * Hook that is called by Element::SetAttr to allow subclasses to
   * deal with attribute sets.  This will only be called after we verify that
   * we're actually doing an attr set and will be called before
   * AttributeWillChange and before ParseAttribute and hence before we've set
   * the new value.
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aValue the value it's being set to represented as either a string or
   *        a parsed nsAttrValue. Alternatively, if the attr is being removed it
   *        will be null. BeforeSetAttr is allowed to modify aValue by parsing
   *        the string to an nsAttrValue (to avoid having to reparse it in
   *        ParseAttribute).
   * @param aNotify Whether we plan to notify document observers.
   */
  // Note that this is inlined so that when subclasses call it it gets
  // inlined.  Those calls don't go through a vtable.
  virtual nsresult BeforeSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                 nsAttrValueOrString* aValue,
                                 bool aNotify);

  /**
   * Hook that is called by Element::SetAttr to allow subclasses to
   * deal with attribute sets.  This will only be called after we have called
   * SetAndTakeAttr and AttributeChanged (that is, after we have actually set
   * the attr).  It will always be called under a scriptblocker.
   *
   * @param aNamespaceID the namespace of the attr being set
   * @param aName the localname of the attribute being set
   * @param aValue the value it's being set to.  If null, the attr is being
   *        removed.
   * @param aNotify Whether we plan to notify document observers.
   */
  // Note that this is inlined so that when subclasses call it it gets
  // inlined.  Those calls don't go through a vtable.
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify)
  {
    return NS_OK;
  }

  /**
   * Hook to allow subclasses to produce a different EventListenerManager if
   * needed for attachment of attribute-defined handlers
   */
  virtual EventListenerManager*
    GetEventListenerManagerForAttr(nsIAtom* aAttrName, bool* aDefer);

  /**
   * Internal hook for converting an attribute name-string to nsAttrName in
   * case there is such existing attribute. aNameToUse can be passed to get
   * name which was used for looking for the attribute (lowercase in HTML).
   */
  const nsAttrName*
  InternalGetAttrNameFromQName(const nsAString& aStr,
                               nsAutoString* aNameToUse = nullptr) const;

  nsIFrame* GetStyledFrame();

  virtual Element* GetNameSpaceElement() override
  {
    return this;
  }

  Attr* GetAttributeNodeNSInternal(const nsAString& aNamespaceURI,
                                   const nsAString& aLocalName);

  inline void RegisterActivityObserver();
  inline void UnregisterActivityObserver();

  /**
   * Add/remove this element to the documents id cache
   */
  void AddToIdTable(nsIAtom* aId);
  void RemoveFromIdTable();

  /**
   * Functions to carry out event default actions for links of all types
   * (HTML links, XLinks, SVG "XLinks", etc.)
   */

  /**
   * Check that we meet the conditions to handle a link event
   * and that we are actually on a link.
   *
   * @param aVisitor event visitor
   * @param aURI the uri of the link, set only if the return value is true [OUT]
   * @return true if we can handle the link event, false otherwise
   */
  bool CheckHandleEventForLinksPrecondition(EventChainVisitor& aVisitor,
                                            nsIURI** aURI) const;

  /**
   * Handle status bar updates before they can be cancelled.
   */
  nsresult GetEventTargetParentForLinks(EventChainPreVisitor& aVisitor);

  /**
   * Handle default actions for link event if the event isn't consumed yet.
   */
  nsresult PostHandleEventForLinks(EventChainPostVisitor& aVisitor);

  /**
   * Get the target of this link element. Consumers should established that
   * this element is a link (probably using IsLink) before calling this
   * function (or else why call it?)
   *
   * Note: for HTML this gets the value of the 'target' attribute; for XLink
   * this gets the value of the xlink:_moz_target attribute, or failing that,
   * the value of xlink:show, converted to a suitably equivalent named target
   * (e.g. _blank).
   */
  virtual void GetLinkTarget(nsAString& aTarget);

  nsDOMTokenList* GetTokenList(nsIAtom* aAtom,
                               const DOMTokenListSupportedTokenArray aSupportedTokens = nullptr);

  nsTArray<nsDOMSlots::IntersectionObserverRegistration>* RegisteredIntersectionObservers();

private:
  /**
   * Get this element's client area rect in app units.
   * @return the frame's client area
   */
  nsRect GetClientAreaRect();

  nsIScrollableFrame* GetScrollFrame(nsIFrame **aStyledFrame = nullptr,
                                     bool aFlushLayout = true);

  // Data members
  EventStates mState;
};

class RemoveFromBindingManagerRunnable : public mozilla::Runnable
{
public:
  RemoveFromBindingManagerRunnable(nsBindingManager* aManager,
                                   nsIContent* aContent,
                                   nsIDocument* aDoc);

  NS_IMETHOD Run() override;
private:
  virtual ~RemoveFromBindingManagerRunnable();
  RefPtr<nsBindingManager> mManager;
  RefPtr<nsIContent> mContent;
  nsCOMPtr<nsIDocument> mDoc;
};

class DestinationInsertionPointList : public nsINodeList
{
public:
  explicit DestinationInsertionPointList(Element* aElement);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DestinationInsertionPointList)

  // nsIDOMNodeList
  NS_DECL_NSIDOMNODELIST

  // nsINodeList
  virtual nsIContent* Item(uint32_t aIndex) override;
  virtual int32_t IndexOf(nsIContent* aContent) override;
  virtual nsINode* GetParentObject() override { return mParent; }
  virtual uint32_t Length() const;
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;
protected:
  virtual ~DestinationInsertionPointList();

  RefPtr<Element> mParent;
  nsCOMArray<nsIContent> mDestinationPoints;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Element, NS_ELEMENT_IID)

inline bool
Element::HasAttr(int32_t aNameSpaceID, nsIAtom* aName) const
{
  NS_ASSERTION(nullptr != aName, "must have attribute name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown,
               "must have a real namespace ID!");

  return mAttrsAndChildren.IndexOfAttr(aName, aNameSpaceID) >= 0;
}

inline bool
Element::AttrValueIs(int32_t aNameSpaceID,
                     nsIAtom* aName,
                     const nsAString& aValue,
                     nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");

  const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName, aNameSpaceID);
  return val && val->Equals(aValue, aCaseSensitive);
}

inline bool
Element::AttrValueIs(int32_t aNameSpaceID,
                     nsIAtom* aName,
                     nsIAtom* aValue,
                     nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");
  NS_ASSERTION(aValue, "Null value atom");

  const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName, aNameSpaceID);
  return val && val->Equals(aValue, aCaseSensitive);
}

} // namespace dom
} // namespace mozilla

inline mozilla::dom::Element* nsINode::AsElement()
{
  MOZ_ASSERT(IsElement());
  return static_cast<mozilla::dom::Element*>(this);
}

inline const mozilla::dom::Element* nsINode::AsElement() const
{
  MOZ_ASSERT(IsElement());
  return static_cast<const mozilla::dom::Element*>(this);
}

inline void nsINode::UnsetRestyleFlagsIfGecko()
{
  if (IsElement() && !IsStyledByServo()) {
    UnsetFlags(ELEMENT_ALL_RESTYLE_FLAGS);
  }
}

/**
 * Macros to implement Clone(). _elementName is the class for which to implement
 * Clone.
 */
#define NS_IMPL_ELEMENT_CLONE(_elementName)                                 \
nsresult                                                                    \
_elementName::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const \
{                                                                           \
  *aResult = nullptr;                                                       \
  already_AddRefed<mozilla::dom::NodeInfo> ni =                             \
    RefPtr<mozilla::dom::NodeInfo>(aNodeInfo).forget();                   \
  _elementName *it = new _elementName(ni);                                  \
  if (!it) {                                                                \
    return NS_ERROR_OUT_OF_MEMORY;                                          \
  }                                                                         \
                                                                            \
  nsCOMPtr<nsINode> kungFuDeathGrip = it;                                   \
  nsresult rv = const_cast<_elementName*>(this)->CopyInnerTo(it);           \
  if (NS_SUCCEEDED(rv)) {                                                   \
    kungFuDeathGrip.swap(*aResult);                                         \
  }                                                                         \
                                                                            \
  return rv;                                                                \
}

#define NS_IMPL_ELEMENT_CLONE_WITH_INIT(_elementName)                       \
nsresult                                                                    \
_elementName::Clone(mozilla::dom::NodeInfo *aNodeInfo, nsINode **aResult) const \
{                                                                           \
  *aResult = nullptr;                                                       \
  already_AddRefed<mozilla::dom::NodeInfo> ni =                             \
    RefPtr<mozilla::dom::NodeInfo>(aNodeInfo).forget();                   \
  _elementName *it = new _elementName(ni);                                  \
  if (!it) {                                                                \
    return NS_ERROR_OUT_OF_MEMORY;                                          \
  }                                                                         \
                                                                            \
  nsCOMPtr<nsINode> kungFuDeathGrip = it;                                   \
  nsresult rv = it->Init();                                                 \
  nsresult rv2 = const_cast<_elementName*>(this)->CopyInnerTo(it);          \
  if (NS_FAILED(rv2)) {                                                     \
    rv = rv2;                                                               \
  }                                                                         \
  if (NS_SUCCEEDED(rv)) {                                                   \
    kungFuDeathGrip.swap(*aResult);                                         \
  }                                                                         \
                                                                            \
  return rv;                                                                \
}

/**
 * A macro to implement the getter and setter for a given string
 * valued content property. The method uses the generic GetAttr and
 * SetAttr methods.  We use the 5-argument form of SetAttr, because
 * some consumers only implement that one, hiding superclass
 * 4-argument forms.
 */
#define NS_IMPL_STRING_ATTR(_class, _method, _atom)                     \
  NS_IMETHODIMP                                                         \
  _class::Get##_method(nsAString& aValue)                               \
  {                                                                     \
    GetAttr(kNameSpaceID_None, nsGkAtoms::_atom, aValue);               \
    return NS_OK;                                                       \
  }                                                                     \
  NS_IMETHODIMP                                                         \
  _class::Set##_method(const nsAString& aValue)                         \
  {                                                                     \
    return SetAttr(kNameSpaceID_None, nsGkAtoms::_atom, nullptr, aValue, true); \
  }

/**
 * A macro to implement the getter and setter for a given boolean
 * valued content property. The method uses the GetBoolAttr and
 * SetBoolAttr methods.
 */
#define NS_IMPL_BOOL_ATTR(_class, _method, _atom)                     \
  NS_IMETHODIMP                                                       \
  _class::Get##_method(bool* aValue)                                  \
  {                                                                   \
    *aValue = GetBoolAttr(nsGkAtoms::_atom);                          \
    return NS_OK;                                                     \
  }                                                                   \
  NS_IMETHODIMP                                                       \
  _class::Set##_method(bool aValue)                                   \
  {                                                                   \
    return SetBoolAttr(nsGkAtoms::_atom, aValue);                     \
  }

#define NS_FORWARD_NSIDOMELEMENT_TO_GENERIC                                   \
typedef mozilla::dom::Element Element;                                        \
NS_IMETHOD GetTagName(nsAString& aTagName) final override                     \
{                                                                             \
  Element::GetTagName(aTagName);                                              \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD GetId(nsAString& aId) final override                               \
{                                                                             \
  Element::GetId(aId);                                                        \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD SetId(const nsAString& aId) final override                         \
{                                                                             \
  Element::SetId(aId);                                                        \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD GetClassName(nsAString& aClassName) final override                 \
{                                                                             \
  Element::GetClassName(aClassName);                                          \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD SetClassName(const nsAString& aClassName) final override           \
{                                                                             \
  Element::SetClassName(aClassName);                                          \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD GetClassList(nsISupports** aClassList) final override              \
{                                                                             \
  Element::GetClassList(aClassList);                                          \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD GetAttributes(nsIDOMMozNamedAttrMap** aAttributes) final override  \
{                                                                             \
  NS_ADDREF(*aAttributes = Attributes());                                     \
  return NS_OK;                                                               \
}                                                                             \
using Element::GetAttribute;                                                  \
NS_IMETHOD GetAttribute(const nsAString& name, nsAString& _retval) final      \
  override                                                                    \
{                                                                             \
  nsString attr;                                                              \
  GetAttribute(name, attr);                                                   \
  _retval = attr;                                                             \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD GetAttributeNS(const nsAString& namespaceURI,                      \
                          const nsAString& localName,                         \
                          nsAString& _retval) final override                  \
{                                                                             \
  Element::GetAttributeNS(namespaceURI, localName, _retval);                  \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD SetAttribute(const nsAString& name,                                \
                        const nsAString& value) override                      \
{                                                                             \
  mozilla::ErrorResult rv;                                                    \
  Element::SetAttribute(name, value, rv);                                     \
  return rv.StealNSResult();                                                  \
}                                                                             \
NS_IMETHOD SetAttributeNS(const nsAString& namespaceURI,                      \
                          const nsAString& qualifiedName,                     \
                          const nsAString& value) final override              \
{                                                                             \
  mozilla::ErrorResult rv;                                                    \
  Element::SetAttributeNS(namespaceURI, qualifiedName, value, rv);            \
  return rv.StealNSResult();                                                      \
}                                                                             \
using Element::RemoveAttribute;                                               \
NS_IMETHOD RemoveAttribute(const nsAString& name) final override              \
{                                                                             \
  mozilla::ErrorResult rv;                                                    \
  RemoveAttribute(name, rv);                                                  \
  return rv.StealNSResult();                                                      \
}                                                                             \
NS_IMETHOD RemoveAttributeNS(const nsAString& namespaceURI,                   \
                             const nsAString& localName) final override       \
{                                                                             \
  mozilla::ErrorResult rv;                                                    \
  Element::RemoveAttributeNS(namespaceURI, localName, rv);                    \
  return rv.StealNSResult();                                                      \
}                                                                             \
using Element::HasAttribute;                                                  \
NS_IMETHOD HasAttribute(const nsAString& name,                                \
                           bool* _retval) final override                      \
{                                                                             \
  *_retval = HasAttribute(name);                                              \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD HasAttributeNS(const nsAString& namespaceURI,                      \
                          const nsAString& localName,                         \
                          bool* _retval) final override                       \
{                                                                             \
  *_retval = Element::HasAttributeNS(namespaceURI, localName);                \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD HasAttributes(bool* _retval) final override                        \
{                                                                             \
  *_retval = Element::HasAttributes();                                        \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD GetAttributeNode(const nsAString& name,                            \
                            nsIDOMAttr** _retval) final override              \
{                                                                             \
  NS_IF_ADDREF(*_retval = Element::GetAttributeNode(name));                   \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD SetAttributeNode(nsIDOMAttr* newAttr,                              \
                            nsIDOMAttr** _retval) final override              \
{                                                                             \
  if (!newAttr) {                                                             \
    return NS_ERROR_INVALID_POINTER;                                          \
  }                                                                           \
  mozilla::ErrorResult rv;                                                    \
  mozilla::dom::Attr* attr = static_cast<mozilla::dom::Attr*>(newAttr);       \
  *_retval = Element::SetAttributeNode(*attr, rv).take();                     \
  return rv.StealNSResult();                                                      \
}                                                                             \
NS_IMETHOD RemoveAttributeNode(nsIDOMAttr* oldAttr,                           \
                               nsIDOMAttr** _retval) final override           \
{                                                                             \
  if (!oldAttr) {                                                             \
    return NS_ERROR_INVALID_POINTER;                                          \
  }                                                                           \
  mozilla::ErrorResult rv;                                                    \
  mozilla::dom::Attr* attr = static_cast<mozilla::dom::Attr*>(oldAttr);       \
  *_retval = Element::RemoveAttributeNode(*attr, rv).take();                  \
  return rv.StealNSResult();                                                      \
}                                                                             \
NS_IMETHOD GetAttributeNodeNS(const nsAString& namespaceURI,                  \
                              const nsAString& localName,                     \
                              nsIDOMAttr** _retval) final override            \
{                                                                             \
  NS_IF_ADDREF(*_retval = Element::GetAttributeNodeNS(namespaceURI,           \
                                                      localName));            \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD SetAttributeNodeNS(nsIDOMAttr* newAttr,                            \
                              nsIDOMAttr** _retval) final override            \
{                                                                             \
  mozilla::ErrorResult rv;                                                    \
  mozilla::dom::Attr* attr = static_cast<mozilla::dom::Attr*>(newAttr);       \
  *_retval = Element::SetAttributeNodeNS(*attr, rv).take();                   \
  return rv.StealNSResult();                                                      \
}                                                                             \
NS_IMETHOD GetElementsByTagName(const nsAString& name,                        \
                                nsIDOMHTMLCollection** _retval) final         \
                                                                override      \
{                                                                             \
  Element::GetElementsByTagName(name, _retval);                               \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD GetElementsByTagNameNS(const nsAString& namespaceURI,              \
                                  const nsAString& localName,                 \
                                  nsIDOMHTMLCollection** _retval) final       \
                                                                  override    \
{                                                                             \
  return Element::GetElementsByTagNameNS(namespaceURI, localName,             \
                                         _retval);                            \
}                                                                             \
NS_IMETHOD GetElementsByClassName(const nsAString& classes,                   \
                                  nsIDOMHTMLCollection** _retval) final       \
                                                                  override    \
{                                                                             \
  return Element::GetElementsByClassName(classes, _retval);                   \
}                                                                             \
NS_IMETHOD GetChildElements(nsIDOMNodeList** aChildElements) final override   \
{                                                                             \
  nsIHTMLCollection* list = FragmentOrElement::Children();                    \
  return CallQueryInterface(list, aChildElements);                            \
}                                                                             \
NS_IMETHOD GetFirstElementChild(nsIDOMElement** aFirstElementChild) final     \
  override                                                                    \
{                                                                             \
  Element* element = Element::GetFirstElementChild();                         \
  if (!element) {                                                             \
    *aFirstElementChild = nullptr;                                            \
    return NS_OK;                                                             \
  }                                                                           \
  return CallQueryInterface(element, aFirstElementChild);                     \
}                                                                             \
NS_IMETHOD GetLastElementChild(nsIDOMElement** aLastElementChild) final       \
  override                                                                    \
{                                                                             \
  Element* element = Element::GetLastElementChild();                          \
  if (!element) {                                                             \
    *aLastElementChild = nullptr;                                             \
    return NS_OK;                                                             \
  }                                                                           \
  return CallQueryInterface(element, aLastElementChild);                      \
}                                                                             \
NS_IMETHOD GetPreviousElementSibling(nsIDOMElement** aPreviousElementSibling) \
  final override                                                              \
{                                                                             \
  Element* element = Element::GetPreviousElementSibling();                    \
  if (!element) {                                                             \
    *aPreviousElementSibling = nullptr;                                       \
    return NS_OK;                                                             \
  }                                                                           \
  return CallQueryInterface(element, aPreviousElementSibling);                \
}                                                                             \
NS_IMETHOD GetNextElementSibling(nsIDOMElement** aNextElementSibling)         \
  final override                                                              \
{                                                                             \
  Element* element = Element::GetNextElementSibling();                        \
  if (!element) {                                                             \
    *aNextElementSibling = nullptr;                                           \
    return NS_OK;                                                             \
  }                                                                           \
  return CallQueryInterface(element, aNextElementSibling);                    \
}                                                                             \
NS_IMETHOD GetChildElementCount(uint32_t* aChildElementCount) final override  \
{                                                                             \
  *aChildElementCount = Element::ChildElementCount();                         \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD MozRemove() final override                                         \
{                                                                             \
  nsINode::Remove();                                                          \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD GetClientRects(nsIDOMClientRectList** _retval) final override      \
{                                                                             \
  *_retval = Element::GetClientRects().take();                                \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD GetBoundingClientRect(nsIDOMClientRect** _retval) final override   \
{                                                                             \
  *_retval = Element::GetBoundingClientRect().take();                         \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD GetClientTop(int32_t* aClientTop) final override                   \
{                                                                             \
  *aClientTop = Element::ClientTop();                                         \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD GetClientLeft(int32_t* aClientLeft) final override                 \
{                                                                             \
  *aClientLeft = Element::ClientLeft();                                       \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD GetClientWidth(int32_t* aClientWidth) final override               \
{                                                                             \
  *aClientWidth = Element::ClientWidth();                                     \
  return NS_OK;                                                               \
}                                                                             \
NS_IMETHOD GetClientHeight(int32_t* aClientHeight) final override             \
{                                                                             \
  *aClientHeight = Element::ClientHeight();                                   \
  return NS_OK;                                                               \
}
#endif // mozilla_dom_Element_h__
