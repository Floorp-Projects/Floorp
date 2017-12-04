/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventStates_h_
#define mozilla_EventStates_h_

#include "mozilla/Attributes.h"
#include "nsDebug.h"

namespace mozilla {

/**
 * EventStates is the class used to represent the event states of nsIContent
 * instances. These states are calculated by IntrinsicState() and
 * ContentStatesChanged() has to be called when one of them changes thus
 * informing the layout/style engine of the change.
 * Event states are associated with pseudo-classes.
 */
class EventStates
{
public:
  typedef uint64_t InternalType;
  typedef uint64_t ServoType;

  constexpr EventStates()
    : mStates(0)
  {
  }

  // NOTE: the ideal scenario would be to have the default constructor public
  // setting mStates to 0 and this constructor (without = 0) private.
  // In that case, we could be sure that only macros at the end were creating
  // EventStates instances with mStates set to something else than 0.
  // Unfortunately, this constructor is needed at at least two places now.
  explicit constexpr EventStates(InternalType aStates)
    : mStates(aStates)
  {
  }

  EventStates constexpr operator|(const EventStates& aEventStates) const
  {
    return EventStates(mStates | aEventStates.mStates);
  }

  EventStates& operator|=(const EventStates& aEventStates)
  {
    mStates |= aEventStates.mStates;
    return *this;
  }

  // NOTE: calling if (eventStates1 & eventStates2) will not build.
  // This might work correctly if operator bool() is defined
  // but using HasState, HasAllStates or HasAtLeastOneOfStates is recommended.
  EventStates constexpr operator&(const EventStates& aEventStates) const
  {
    return EventStates(mStates & aEventStates.mStates);
  }

  EventStates& operator&=(const EventStates& aEventStates)
  {
    mStates &= aEventStates.mStates;
    return *this;
  }

  bool operator==(const EventStates& aEventStates) const
  {
    return mStates == aEventStates.mStates;
  }

  bool operator!=(const EventStates& aEventStates) const
  {
    return mStates != aEventStates.mStates;
  }

  EventStates operator~() const
  {
    return EventStates(~mStates);
  }

  EventStates operator^(const EventStates& aEventStates) const
  {
    return EventStates(mStates ^ aEventStates.mStates);
  }

  EventStates& operator^=(const EventStates& aEventStates)
  {
    mStates ^= aEventStates.mStates;
    return *this;
  }

  /**
   * Returns true if the EventStates instance is empty.
   * A EventStates instance is empty if it contains no state.
   *
   * @return Whether if the object is empty.
   */
  bool IsEmpty() const
  {
    return mStates == 0;
  }

  /**
   * Returns true if the EventStates instance contains the state
   * contained in aEventStates.
   * @note aEventStates should contain only one state.
   *
   * @param aEventStates The state to check.
   *
   * @return Whether the object has the state from aEventStates
   */
  bool HasState(EventStates aEventStates) const
  {
#ifdef DEBUG
    // If aEventStates.mStates is a power of two, it contains only one state
    // (or none, but we don't really care).
    if ((aEventStates.mStates & (aEventStates.mStates - 1))) {
      NS_ERROR("When calling HasState, "
               "EventStates object has to contain only one state!");
    }
#endif // DEBUG
    return mStates & aEventStates.mStates;
  }

  /**
   * Returns true if the EventStates instance contains one of the states
   * contained in aEventStates.
   *
   * @param aEventStates The states to check.
   *
   * @return Whether the object has at least one state from aEventStates
   */
  bool HasAtLeastOneOfStates(EventStates aEventStates) const
  {
    return mStates & aEventStates.mStates;
  }

  /**
   * Returns true if the EventStates instance contains all states
   * contained in aEventStates.
   *
   * @param aEventStates The states to check.
   *
   * @return Whether the object has all states from aEventStates
   */
  bool HasAllStates(EventStates aEventStates) const
  {
    return (mStates & aEventStates.mStates) == aEventStates.mStates;
  }

  // We only need that method for inDOMUtils::GetContentState.
  // If inDOMUtils::GetContentState is removed, this method should be removed.
  InternalType GetInternalValue() const {
    return mStates;
  }

  /**
   * Method used to get the appropriate state representation for Servo.
   */
  ServoType ServoValue() const
  {
    return mStates;
  }

private:
  InternalType mStates;
};

} // namespace mozilla

/**
 * The following macros are creating EventStates instance with different
 * values depending of their meaning.
 * Ideally, EventStates instance with values different than 0 should only be
 * created that way.
 */

// Helper to define a new EventStates macro.
#define NS_DEFINE_EVENT_STATE_MACRO(_val)               \
  (mozilla::EventStates(mozilla::EventStates::InternalType(1) << _val))

/*
 * In order to efficiently convert Gecko EventState values into Servo
 * ElementState values [1], we maintain the invariant that the low bits of
 * EventState can be masked off to form an ElementState (this works so
 * long as Servo never supports a state that Gecko doesn't).
 *
 * This is unfortunately rather fragile for now, but we should soon have
 * the infrastructure to statically-assert that these match up. If you
 * need to change these, please notify somebody involved with Stylo.
 *
 * [1] https://github.com/servo/servo/blob/master/components/style/element_state.rs
 */

// Mouse is down on content.
#define NS_EVENT_STATE_ACTIVE        NS_DEFINE_EVENT_STATE_MACRO(0)
// Content has focus.
#define NS_EVENT_STATE_FOCUS         NS_DEFINE_EVENT_STATE_MACRO(1)
// Mouse is hovering over content.
#define NS_EVENT_STATE_HOVER         NS_DEFINE_EVENT_STATE_MACRO(2)
// Content is enabled (and can be disabled).
#define NS_EVENT_STATE_ENABLED       NS_DEFINE_EVENT_STATE_MACRO(3)
// Content is disabled.
#define NS_EVENT_STATE_DISABLED      NS_DEFINE_EVENT_STATE_MACRO(4)
// Content is checked.
#define NS_EVENT_STATE_CHECKED       NS_DEFINE_EVENT_STATE_MACRO(5)
// Content is in the indeterminate state.
#define NS_EVENT_STATE_INDETERMINATE NS_DEFINE_EVENT_STATE_MACRO(6)
// Content shows its placeholder
#define NS_EVENT_STATE_PLACEHOLDERSHOWN NS_DEFINE_EVENT_STATE_MACRO(7)
// Content is URL's target (ref).
#define NS_EVENT_STATE_URLTARGET     NS_DEFINE_EVENT_STATE_MACRO(8)
// Content is the full screen element, or a frame containing the
// current full-screen element.
#define NS_EVENT_STATE_FULL_SCREEN   NS_DEFINE_EVENT_STATE_MACRO(9)
// Content is valid (and can be invalid).
#define NS_EVENT_STATE_VALID         NS_DEFINE_EVENT_STATE_MACRO(10)
// Content is invalid.
#define NS_EVENT_STATE_INVALID       NS_DEFINE_EVENT_STATE_MACRO(11)
// UI friendly version of :valid pseudo-class.
#define NS_EVENT_STATE_MOZ_UI_VALID NS_DEFINE_EVENT_STATE_MACRO(12)
// UI friendly version of :invalid pseudo-class.
#define NS_EVENT_STATE_MOZ_UI_INVALID NS_DEFINE_EVENT_STATE_MACRO(13)
// Content could not be rendered (image/object/etc).
#define NS_EVENT_STATE_BROKEN        NS_DEFINE_EVENT_STATE_MACRO(14)
// Content disabled by the user (images turned off, say).
#define NS_EVENT_STATE_USERDISABLED  NS_DEFINE_EVENT_STATE_MACRO(15)
// Content suppressed by the user (ad blocking, etc).
#define NS_EVENT_STATE_SUPPRESSED    NS_DEFINE_EVENT_STATE_MACRO(16)
// Content is still loading such that there is nothing to show the
// user (eg an image which hasn't started coming in yet).
#define NS_EVENT_STATE_LOADING       NS_DEFINE_EVENT_STATE_MACRO(17)
// Handler for the content has been blocked.
#define NS_EVENT_STATE_HANDLER_BLOCKED NS_DEFINE_EVENT_STATE_MACRO(18)
// Handler for the content has been disabled.
#define NS_EVENT_STATE_HANDLER_DISABLED NS_DEFINE_EVENT_STATE_MACRO(19)
// Handler for the content has crashed
#define NS_EVENT_STATE_HANDLER_CRASHED NS_DEFINE_EVENT_STATE_MACRO(20)
// Content is required.
#define NS_EVENT_STATE_REQUIRED      NS_DEFINE_EVENT_STATE_MACRO(21)
// Content is optional (and can be required).
#define NS_EVENT_STATE_OPTIONAL      NS_DEFINE_EVENT_STATE_MACRO(22)
// Free bit                          NS_DEFINE_EVENT_STATE_MACRO(23)
// Link has been visited.
#define NS_EVENT_STATE_VISITED       NS_DEFINE_EVENT_STATE_MACRO(24)
// Link hasn't been visited.
#define NS_EVENT_STATE_UNVISITED     NS_DEFINE_EVENT_STATE_MACRO(25)
// Drag is hovering over content.
#define NS_EVENT_STATE_DRAGOVER      NS_DEFINE_EVENT_STATE_MACRO(26)
// Content value is in-range (and can be out-of-range).
#define NS_EVENT_STATE_INRANGE       NS_DEFINE_EVENT_STATE_MACRO(27)
// Content value is out-of-range.
#define NS_EVENT_STATE_OUTOFRANGE    NS_DEFINE_EVENT_STATE_MACRO(28)
// These two are temporary (see bug 302188)
// Content is read-only.
#define NS_EVENT_STATE_MOZ_READONLY  NS_DEFINE_EVENT_STATE_MACRO(29)
// Content is editable.
#define NS_EVENT_STATE_MOZ_READWRITE NS_DEFINE_EVENT_STATE_MACRO(30)
// Content is the default one (meaning depends of the context).
#define NS_EVENT_STATE_DEFAULT       NS_DEFINE_EVENT_STATE_MACRO(31)
// Content is a submit control and the form isn't valid.
#define NS_EVENT_STATE_MOZ_SUBMITINVALID NS_DEFINE_EVENT_STATE_MACRO(32)
// Content is in the optimum region.
#define NS_EVENT_STATE_OPTIMUM NS_DEFINE_EVENT_STATE_MACRO(33)
// Content is in the suboptimal region.
#define NS_EVENT_STATE_SUB_OPTIMUM NS_DEFINE_EVENT_STATE_MACRO(34)
// Content is in the sub-suboptimal region.
#define NS_EVENT_STATE_SUB_SUB_OPTIMUM NS_DEFINE_EVENT_STATE_MACRO(35)
// Element is highlighted (devtools inspector)
#define NS_EVENT_STATE_DEVTOOLS_HIGHLIGHTED NS_DEFINE_EVENT_STATE_MACRO(36)
// Element is transitioning for rules changed by style editor
#define NS_EVENT_STATE_STYLEEDITOR_TRANSITIONING NS_DEFINE_EVENT_STATE_MACRO(37)
#define NS_EVENT_STATE_INCREMENT_SCRIPT_LEVEL NS_DEFINE_EVENT_STATE_MACRO(38)
// Content has focus and should show a ring.
#define NS_EVENT_STATE_FOCUSRING     NS_DEFINE_EVENT_STATE_MACRO(39)
// Handler for click to play plugin
#define NS_EVENT_STATE_TYPE_CLICK_TO_PLAY NS_DEFINE_EVENT_STATE_MACRO(40)
// Handler for click to play plugin (vulnerable w/update)
#define NS_EVENT_STATE_VULNERABLE_UPDATABLE NS_DEFINE_EVENT_STATE_MACRO(41)
// Handler for click to play plugin (vulnerable w/no update)
#define NS_EVENT_STATE_VULNERABLE_NO_UPDATE NS_DEFINE_EVENT_STATE_MACRO(42)
// Element has focus-within.
#define NS_EVENT_STATE_FOCUS_WITHIN NS_DEFINE_EVENT_STATE_MACRO(43)
// Element is ltr (for :dir pseudo-class)
#define NS_EVENT_STATE_LTR NS_DEFINE_EVENT_STATE_MACRO(44)
// Element is rtl (for :dir pseudo-class)
#define NS_EVENT_STATE_RTL NS_DEFINE_EVENT_STATE_MACRO(45)
// States for tracking the state of the "dir" attribute for HTML elements.  We
// use these to avoid having to get "dir" attributes all the time during
// selector matching for some parts of the UA stylesheet.
//
// These states are externally managed, because we also don't want to keep
// getting "dir" attributes in IntrinsicState.
//
// Element is HTML and has a "dir" attibute.  This state might go away depending
// on how https://github.com/whatwg/html/issues/2769 gets resolved.  The value
// could be anything.
#define NS_EVENT_STATE_HAS_DIR_ATTR NS_DEFINE_EVENT_STATE_MACRO(46)
// Element is HTML, has a "dir" attribute, and the attribute's value is
// case-insensitively equal to "ltr".
#define NS_EVENT_STATE_DIR_ATTR_LTR NS_DEFINE_EVENT_STATE_MACRO(47)
// Element is HTML, has a "dir" attribute, and the attribute's value is
// case-insensitively equal to "rtl".
#define NS_EVENT_STATE_DIR_ATTR_RTL NS_DEFINE_EVENT_STATE_MACRO(48)
// Element is HTML, and is either a <bdi> element with no valid-valued "dir"
// attribute or any HTML element which has a "dir" attribute whose value is
// "auto".
#define NS_EVENT_STATE_DIR_ATTR_LIKE_AUTO NS_DEFINE_EVENT_STATE_MACRO(49)
// Element is filled by Autofill feature.
#define NS_EVENT_STATE_AUTOFILL NS_DEFINE_EVENT_STATE_MACRO(50)
// Element is filled with preview data by Autofill feature.
#define NS_EVENT_STATE_AUTOFILL_PREVIEW NS_DEFINE_EVENT_STATE_MACRO(51)

// Event state that is used for values that need to be parsed but do nothing.
#define NS_EVENT_STATE_IGNORE NS_DEFINE_EVENT_STATE_MACRO(63)

/**
 * NOTE: do not go over 63 without updating EventStates::InternalType!
 */

#define DIRECTION_STATES (NS_EVENT_STATE_LTR | NS_EVENT_STATE_RTL)

#define DIR_ATTR_STATES (NS_EVENT_STATE_HAS_DIR_ATTR |          \
                         NS_EVENT_STATE_DIR_ATTR_LTR |          \
                         NS_EVENT_STATE_DIR_ATTR_RTL |          \
                         NS_EVENT_STATE_DIR_ATTR_LIKE_AUTO)

#define DISABLED_STATES (NS_EVENT_STATE_DISABLED | NS_EVENT_STATE_ENABLED)

#define REQUIRED_STATES (NS_EVENT_STATE_REQUIRED | NS_EVENT_STATE_OPTIONAL)

// Event states that can be added and removed through
// Element::{Add,Remove}ManuallyManagedStates.
//
// Take care when manually managing state bits.  You are responsible for
// setting or clearing the bit when an Element is added or removed from a
// document (e.g. in BindToTree and UnbindFromTree), if that is an
// appropriate thing to do for your state bit.
#define MANUALLY_MANAGED_STATES (             \
  NS_EVENT_STATE_AUTOFILL |                   \
  NS_EVENT_STATE_AUTOFILL_PREVIEW             \
)

// Event states that are managed externally to an element (by the
// EventStateManager, or by other code).  As opposed to those in
// INTRINSIC_STATES, which are are computed by the element itself
// and returned from Element::IntrinsicState.
#define EXTERNALLY_MANAGED_STATES (           \
  MANUALLY_MANAGED_STATES |                   \
  DIR_ATTR_STATES |                           \
  DISABLED_STATES |                           \
  REQUIRED_STATES |                           \
  NS_EVENT_STATE_ACTIVE |                     \
  NS_EVENT_STATE_DRAGOVER |                   \
  NS_EVENT_STATE_FOCUS |                      \
  NS_EVENT_STATE_FOCUSRING |                  \
  NS_EVENT_STATE_FOCUS_WITHIN |               \
  NS_EVENT_STATE_FULL_SCREEN |                \
  NS_EVENT_STATE_HOVER |                      \
  NS_EVENT_STATE_URLTARGET                    \
)

#define INTRINSIC_STATES (~EXTERNALLY_MANAGED_STATES)

#endif // mozilla_EventStates_h_
