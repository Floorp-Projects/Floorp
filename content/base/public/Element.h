/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Boris Zbarsky <bzbarsky@mit.edu> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_dom_Element_h__
#define mozilla_dom_Element_h__

#include "nsIContent.h"
#include "nsEventStates.h"
#include "nsDOMMemoryReporter.h"

class nsEventStateManager;
class nsGlobalWindow;
class nsFocusManager;

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

class Element : public nsIContent
{
public:
#ifdef MOZILLA_INTERNAL_API
  Element(already_AddRefed<nsINodeInfo> aNodeInfo) :
    nsIContent(aNodeInfo),
    mState(NS_EVENT_STATE_MOZ_READONLY)
  {}
#endif // MOZILLA_INTERNAL_API

  NS_DECL_AND_IMPL_DOM_MEMORY_REPORTER_SIZEOF(Element, nsIContent)

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

} // namespace dom
} // namespace mozilla

inline mozilla::dom::Element* nsINode::AsElement() {
  NS_ASSERTION(IsElement(), "Not an element?");
  return static_cast<mozilla::dom::Element*>(this);
}

#endif // mozilla_dom_Element_h__
