/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Element_h__
#define mozilla_dom_Element_h__

#include "mozilla/dom/FragmentOrElement.h" // for base class
#include "nsChangeHint.h"                  // for enum
#include "nsEventStates.h"                 // for member

class nsEventStateManager;
class nsFocusManager;
class nsGlobalWindow;
class nsICSSDeclaration;
class nsISMILAttr;

// Element-specific flags
enum {
  // Set if the element has a pending style change.
  ELEMENT_HAS_PENDING_RESTYLE     = (1 << NODE_TYPE_SPECIFIC_BITS_OFFSET),

  // Set if the element is a potential restyle root (that is, has a style
  // change pending _and_ that style change will attempt to restyle
  // descendants).
  ELEMENT_IS_POTENTIAL_RESTYLE_ROOT =
    (1 << (NODE_TYPE_SPECIFIC_BITS_OFFSET + 1)),

  // Set if the element has a pending animation style change.
  ELEMENT_HAS_PENDING_ANIMATION_RESTYLE =
    (1 << (NODE_TYPE_SPECIFIC_BITS_OFFSET + 2)),

  // Set if the element is a potential animation restyle root (that is,
  // has an animation style change pending _and_ that style change
  // will attempt to restyle descendants).
  ELEMENT_IS_POTENTIAL_ANIMATION_RESTYLE_ROOT =
    (1 << (NODE_TYPE_SPECIFIC_BITS_OFFSET + 3)),

  // All of those bits together, for convenience.
  ELEMENT_ALL_RESTYLE_FLAGS = ELEMENT_HAS_PENDING_RESTYLE |
                              ELEMENT_IS_POTENTIAL_RESTYLE_ROOT |
                              ELEMENT_HAS_PENDING_ANIMATION_RESTYLE |
                              ELEMENT_IS_POTENTIAL_ANIMATION_RESTYLE_ROOT,

  // Just the HAS_PENDING bits, for convenience
  ELEMENT_PENDING_RESTYLE_FLAGS = ELEMENT_HAS_PENDING_RESTYLE |
                                  ELEMENT_HAS_PENDING_ANIMATION_RESTYLE,

  // Remaining bits are for subclasses
  ELEMENT_TYPE_SPECIFIC_BITS_OFFSET = NODE_TYPE_SPECIFIC_BITS_OFFSET + 4
};

namespace mozilla {
namespace dom {

class Link;

// IID for the dom::Element interface
#define NS_ELEMENT_IID \
{ 0xc6c049a1, 0x96e8, 0x4580, \
  { 0xa6, 0x93, 0xb9, 0x5f, 0x53, 0xbe, 0xe8, 0x1c } }

class Element : public FragmentOrElement
{
public:
#ifdef MOZILLA_INTERNAL_API
  Element(already_AddRefed<nsINodeInfo> aNodeInfo) :
    FragmentOrElement(aNodeInfo),
    mState(NS_EVENT_STATE_MOZ_READONLY)
  {}
#endif // MOZILLA_INTERNAL_API

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ELEMENT_IID)

  /**
   * Method to get the full state of this element.  See nsEventStates.h for
   * the possible bits that could be set here.
   */
  nsEventStates State() const {
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
  void UpdateLinkState(nsEventStates aState);

  /**
   * Returns true if this element is either a full-screen element or an
   * ancestor of the full-screen element.
   */
  bool IsFullScreenAncestor() const {
    return mState.HasAtLeastOneOfStates(NS_EVENT_STATE_FULL_SCREEN_ANCESTOR |
                                        NS_EVENT_STATE_FULL_SCREEN);
  }

  /**
   * The style state of this element. This is the real state of the element
   * with any style locks applied for pseudo-class inspecting.
   */
  nsEventStates StyleState() const {
    if (!HasLockedStyleStates()) {
      return mState;
    }
    return StyleStateFromLocks();
  }

  /**
   * The style state locks applied to this element.
   */
  nsEventStates LockedStyleStates() const;

  /**
   * Add a style state lock on this element.
   */
  void LockStyleStates(nsEventStates aStates);

  /**
   * Remove a style state lock on this element.
   */
  void UnlockStyleStates(nsEventStates aStates);

  /**
   * Clear all style state locks on this element.
   */
  void ClearStyleStateLocks();

  /**
   * Get the inline style rule, if any, for this element.
   */
  virtual css::StyleRule* GetInlineStyleRule() = 0;

  /**
   * Set the inline style rule for this element. This will send an appropriate
   * AttributeChanged notification if aNotify is true.
   */
  virtual nsresult SetInlineStyleRule(css::StyleRule* aStyleRule,
                                      const nsAString* aSerialized,
                                      bool aNotify) = 0;

  /**
   * Get the SMIL override style rule for this element. If the rule hasn't been
   * created, this method simply returns null.
   */
  virtual css::StyleRule* GetSMILOverrideStyleRule() = 0;

  /**
   * Set the SMIL override style rule for this element. If aNotify is true, this
   * method will notify the document's pres context, so that the style changes
   * will be noticed.
   */
  virtual nsresult SetSMILOverrideStyleRule(css::StyleRule* aStyleRule,
                                            bool aNotify) = 0;

  /**
   * Returns a new nsISMILAttr that allows the caller to animate the given
   * attribute on this element.
   *
   * The CALLER OWNS the result and is responsible for deleting it.
   */
  virtual nsISMILAttr* GetAnimatedAttr(PRInt32 aNamespaceID, nsIAtom* aName) = 0;

  /**
   * Get the SMIL override style for this element. This is a style declaration
   * that is applied *after* the inline style, and it can be used e.g. to store
   * animated style values.
   *
   * Note: This method is analogous to the 'GetStyle' method in
   * nsGenericHTMLElement and nsStyledElement.
   */
  virtual nsICSSDeclaration* GetSMILOverrideStyle() = 0;

  /**
   * Returns if the element is labelable as per HTML specification.
   */
  virtual bool IsLabelable() const = 0;

  /**
   * Is the attribute named stored in the mapped attributes?
   *
   * // XXXbz we use this method in HasAttributeDependentStyle, so svg
   *    returns true here even though it stores nothing in the mapped
   *    attributes.
   */
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const = 0;

  /**
   * Get a hint that tells the style system what to do when
   * an attribute on this node changes, if something needs to happen
   * in response to the change *other* than the result of what is
   * mapped into style data via any type of style rule.
   */
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const = 0;

  /**
   * Returns an atom holding the name of the "class" attribute on this
   * content node (if applicable).  Returns null if there is no
   * "class" attribute for this type of content node.
   */
  virtual nsIAtom *GetClassAttributeName() const = 0;

protected:
  /**
   * Method to get the _intrinsic_ content state of this element.  This is the
   * state that is independent of the element's presentation.  To get the full
   * content state, use State().  See nsEventStates.h for
   * the possible bits that could be set here.
   */
  virtual nsEventStates IntrinsicState() const;

  /**
   * Method to add state bits.  This should be called from subclass
   * constructors to set up our event state correctly at construction
   * time and other places where we don't want to notify a state
   * change.
   */
  void AddStatesSilently(nsEventStates aStates) {
    mState |= aStates;
  }

  /**
   * Method to remove state bits.  This should be called from subclass
   * constructors to set up our event state correctly at construction
   * time and other places where we don't want to notify a state
   * change.
   */
  void RemoveStatesSilently(nsEventStates aStates) {
    mState &= ~aStates;
  }

private:
  // Need to allow the ESM, nsGlobalWindow, and the focus manager to
  // set our state
  friend class ::nsEventStateManager;
  friend class ::nsGlobalWindow;
  friend class ::nsFocusManager;

  // Also need to allow Link to call UpdateLinkState.
  friend class Link;

  void NotifyStateChange(nsEventStates aStates);

  void NotifyStyleStateChange(nsEventStates aStates);

  // Style state computed from element's state and style locks.
  nsEventStates StyleStateFromLocks() const;

  // Methods for the ESM to manage state bits.  These will handle
  // setting up script blockers when they notify, so no need to do it
  // in the callers unless desired.
  void AddStates(nsEventStates aStates) {
    NS_PRECONDITION(!aStates.HasAtLeastOneOfStates(INTRINSIC_STATES),
                    "Should only be adding ESM-managed states here");
    AddStatesSilently(aStates);
    NotifyStateChange(aStates);
  }
  void RemoveStates(nsEventStates aStates) {
    NS_PRECONDITION(!aStates.HasAtLeastOneOfStates(INTRINSIC_STATES),
                    "Should only be removing ESM-managed states here");
    RemoveStatesSilently(aStates);
    NotifyStateChange(aStates);
  }

  nsEventStates mState;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Element, NS_ELEMENT_IID)

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

#endif // mozilla_dom_Element_h__
