/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (original author)
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

#ifndef nsEventStates_h__
#define nsEventStates_h__

#include "nsDebug.h"

/**
 * nsEventStates is the class used to represent the event states of nsIContent
 * instances. These states are calculated by IntrinsicState() and
 * ContentStatesChanged() has to be called when one of them changes thus
 * informing the layout/style engine of the change.
 * Event states are associated with pseudo-classes.
 */
class nsEventStates
{
public:
  typedef PRUint64 InternalType;

  nsEventStates()
    : mStates(0)
  { }

  // NOTE: the ideal scenario would be to have the default constructor public
  // setting mStates to 0 and this constructor (without = 0) private.
  // In that case, we could be sure that only macros at the end were creating
  // nsEventStates instances with mStates set to something else than 0.
  // Unfortunately, this constructor is needed at at least two places now.
  explicit nsEventStates(InternalType aStates)
    : mStates(aStates)
  { }

  nsEventStates(const nsEventStates& aEventStates) {
    mStates = aEventStates.mStates;
  }

  nsEventStates& operator=(const nsEventStates& aEventStates) {
    mStates = aEventStates.mStates;
    return *this;
  }

  nsEventStates operator|(const nsEventStates& aEventStates) const {
    return nsEventStates(mStates | aEventStates.mStates);
  }

  nsEventStates& operator|=(const nsEventStates& aEventStates) {
    mStates |= aEventStates.mStates;
    return *this;
  }

  // NOTE: calling if (eventStates1 & eventStates2) will not build.
  // This might work correctly if operator bool() is defined
  // but using HasState, HasAllStates or HasAtLeastOneOfStates is recommended.
  nsEventStates operator&(const nsEventStates& aEventStates) const {
    return nsEventStates(mStates & aEventStates.mStates);
  }

  nsEventStates& operator&=(const nsEventStates& aEventStates) {
    mStates &= aEventStates.mStates;
    return *this;
  }

  bool operator==(const nsEventStates& aEventStates) const {
    return mStates == aEventStates.mStates;
  }

  bool operator!=(const nsEventStates& aEventStates) const {
    return mStates != aEventStates.mStates;
  }

  nsEventStates operator~() const {
    return nsEventStates(~mStates);
  }

  nsEventStates operator^(const nsEventStates& aEventStates) const {
    return nsEventStates(mStates ^ aEventStates.mStates);
  }

  nsEventStates& operator^=(const nsEventStates& aEventStates) {
    mStates ^= aEventStates.mStates;
    return *this;
  }

  /**
   * Returns true if the nsEventStates instance is empty.
   * A nsEventStates instance is empty if it contains no state.
   *
   * @return Whether if the object is empty.
   */
  bool IsEmpty() const {
    return mStates == 0;
  }

  /**
   * Returns true if the nsEventStates instance contains the state
   * contained in aEventStates.
   * @note aEventStates should contain only one state.
   *
   * @param aEventStates The state to check.
   *
   * @return Whether the object has the state from aEventStates
   */
  bool HasState(nsEventStates aEventStates) const {
#ifdef DEBUG
    // If aEventStates.mStates is a power of two, it contains only one state
    // (or none, but we don't really care).
    if ((aEventStates.mStates & (aEventStates.mStates - 1))) {
      NS_ERROR("When calling HasState, "
               "nsEventStates object has to contain only one state!");
    }
#endif // DEBUG
    return mStates & aEventStates.mStates;
  }

  /**
   * Returns true if the nsEventStates instance contains one of the states
   * contained in aEventStates.
   *
   * @param aEventStates The states to check.
   *
   * @return Whether the object has at least one state from aEventStates
   */
  bool HasAtLeastOneOfStates(nsEventStates aEventStates) const {
    return mStates & aEventStates.mStates;
  }

  /**
   * Returns true if the nsEventStates instance contains all states
   * contained in aEventStates.
   *
   * @param aEventStates The states to check.
   *
   * @return Whether the object has all states from aEventStates
   */
  bool HasAllStates(nsEventStates aEventStates) const {
    return (mStates & aEventStates.mStates) == aEventStates.mStates;
  }

  // We only need that method for inDOMUtils::GetContentState.
  // If inDOMUtils::GetContentState is removed, this method should be removed.
  InternalType GetInternalValue() const {
    return mStates;
  }

private:
  InternalType mStates;
};

/**
 * The following macros are creating nsEventStates instance with different
 * values depending of their meaning.
 * Ideally, nsEventStates instance with values different than 0 should only be
 * created that way.
 */

// Helper to define a new nsEventStates macro.
#define NS_DEFINE_EVENT_STATE_MACRO(_val)               \
  (nsEventStates(nsEventStates::InternalType(1) << _val))

// Mouse is down on content.
#define NS_EVENT_STATE_ACTIVE        NS_DEFINE_EVENT_STATE_MACRO(0)
// Content has focus.
#define NS_EVENT_STATE_FOCUS         NS_DEFINE_EVENT_STATE_MACRO(1)
// Mouse is hovering over content.
#define NS_EVENT_STATE_HOVER         NS_DEFINE_EVENT_STATE_MACRO(2)
// Drag is hovering over content.
#define NS_EVENT_STATE_DRAGOVER      NS_DEFINE_EVENT_STATE_MACRO(3)
// Content is URL's target (ref).
#define NS_EVENT_STATE_URLTARGET     NS_DEFINE_EVENT_STATE_MACRO(4)
// Content is checked.
#define NS_EVENT_STATE_CHECKED       NS_DEFINE_EVENT_STATE_MACRO(5)
// Content is enabled (and can be disabled).
#define NS_EVENT_STATE_ENABLED       NS_DEFINE_EVENT_STATE_MACRO(6)
// Content is disabled.
#define NS_EVENT_STATE_DISABLED      NS_DEFINE_EVENT_STATE_MACRO(7)
// Content is required.
#define NS_EVENT_STATE_REQUIRED      NS_DEFINE_EVENT_STATE_MACRO(8)
// Content is optional (and can be required).
#define NS_EVENT_STATE_OPTIONAL      NS_DEFINE_EVENT_STATE_MACRO(9)
// Link has been visited.
#define NS_EVENT_STATE_VISITED       NS_DEFINE_EVENT_STATE_MACRO(10)
// Link hasn't been visited.
#define NS_EVENT_STATE_UNVISITED     NS_DEFINE_EVENT_STATE_MACRO(11)
// Content is valid (and can be invalid).
#define NS_EVENT_STATE_VALID         NS_DEFINE_EVENT_STATE_MACRO(12)
// Content is invalid.
#define NS_EVENT_STATE_INVALID       NS_DEFINE_EVENT_STATE_MACRO(13)
// Content value is in-range (and can be out-of-range).
#define NS_EVENT_STATE_INRANGE       NS_DEFINE_EVENT_STATE_MACRO(14)
// Content value is out-of-range.
#define NS_EVENT_STATE_OUTOFRANGE    NS_DEFINE_EVENT_STATE_MACRO(15)
// These two are temporary (see bug 302188)
// Content is read-only.
#define NS_EVENT_STATE_MOZ_READONLY  NS_DEFINE_EVENT_STATE_MACRO(16)
// Content is editable.
#define NS_EVENT_STATE_MOZ_READWRITE NS_DEFINE_EVENT_STATE_MACRO(17)
// Content is the default one (meaning depends of the context).
#define NS_EVENT_STATE_DEFAULT       NS_DEFINE_EVENT_STATE_MACRO(18)
// Content could not be rendered (image/object/etc).
#define NS_EVENT_STATE_BROKEN        NS_DEFINE_EVENT_STATE_MACRO(19)
// Content disabled by the user (images turned off, say).
#define NS_EVENT_STATE_USERDISABLED  NS_DEFINE_EVENT_STATE_MACRO(20)
// Content suppressed by the user (ad blocking, etc).
#define NS_EVENT_STATE_SUPPRESSED    NS_DEFINE_EVENT_STATE_MACRO(21)
// Content is still loading such that there is nothing to show the
// user (eg an image which hasn't started coming in yet).
#define NS_EVENT_STATE_LOADING       NS_DEFINE_EVENT_STATE_MACRO(22)
// Content is of a type that gecko can't handle.
#define NS_EVENT_STATE_TYPE_UNSUPPORTED NS_DEFINE_EVENT_STATE_MACRO(23)
#define NS_EVENT_STATE_INCREMENT_SCRIPT_LEVEL NS_DEFINE_EVENT_STATE_MACRO(24)
// Handler for the content has been blocked.
#define NS_EVENT_STATE_HANDLER_BLOCKED NS_DEFINE_EVENT_STATE_MACRO(25)
// Handler for the content has been disabled.
#define NS_EVENT_STATE_HANDLER_DISABLED NS_DEFINE_EVENT_STATE_MACRO(26)
// Content is in the indeterminate state.
#define NS_EVENT_STATE_INDETERMINATE NS_DEFINE_EVENT_STATE_MACRO(27)
// Handler for the content has crashed
#define NS_EVENT_STATE_HANDLER_CRASHED NS_DEFINE_EVENT_STATE_MACRO(28)
// Content has focus and should show a ring.
#define NS_EVENT_STATE_FOCUSRING     NS_DEFINE_EVENT_STATE_MACRO(29)
// Content shows its placeholder
#define NS_EVENT_STATE_MOZ_PLACEHOLDER NS_DEFINE_EVENT_STATE_MACRO(30)
// Content is a submit control and the form isn't valid.
#define NS_EVENT_STATE_MOZ_SUBMITINVALID NS_DEFINE_EVENT_STATE_MACRO(31)
// UI friendly version of :invalid pseudo-class.
#define NS_EVENT_STATE_MOZ_UI_INVALID NS_DEFINE_EVENT_STATE_MACRO(32)
// UI friendly version of :valid pseudo-class.
#define NS_EVENT_STATE_MOZ_UI_VALID NS_DEFINE_EVENT_STATE_MACRO(33)
// Content is the full screen element, or a frame containing the
// current full-screen element.
#define NS_EVENT_STATE_FULL_SCREEN   NS_DEFINE_EVENT_STATE_MACRO(34)
// Handler for click to play plugin
#define NS_EVENT_STATE_TYPE_CLICK_TO_PLAY NS_DEFINE_EVENT_STATE_MACRO(35)

/**
 * NOTE: do not go over 63 without updating nsEventStates::InternalType!
 */

#define ESM_MANAGED_STATES (NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_FOCUS |     \
                            NS_EVENT_STATE_HOVER | NS_EVENT_STATE_DRAGOVER |   \
                            NS_EVENT_STATE_URLTARGET | NS_EVENT_STATE_FOCUSRING | \
                            NS_EVENT_STATE_FULL_SCREEN)

#define INTRINSIC_STATES (~ESM_MANAGED_STATES)

#endif // nsEventStates_h__

