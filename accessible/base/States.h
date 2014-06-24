/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set expandtab shiftwidth=2 tabstop=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _states_h_
#define _states_h_

#include <stdint.h>

namespace mozilla {
namespace a11y {
namespace states {

  /**
   * The object is disabled, opposite to enabled and sensitive.
   */
  const uint64_t UNAVAILABLE = ((uint64_t) 0x1) << 0;

  /**
   * The object is selected.
   */
  const uint64_t SELECTED = ((uint64_t) 0x1) << 1;

  /**
   * The object has the keyboard focus.
   */
  const uint64_t FOCUSED = ((uint64_t) 0x1) << 2;

  /**
   * The object is pressed.
   */
  const uint64_t PRESSED = ((uint64_t) 0x1) << 3;

  /**
   * The checkable object is checked, applied to check box controls,
   * @see CHECKABLE and MIXED states.
   */
  const uint64_t CHECKED = ((uint64_t) 0x1) << 4;

  /**
   * Indicates that the state of a three-state check box or tool bar button is
   * undetermined.  The check box is neither checked or unchecked, and is
   * in the third or mixed state.
   */
  const uint64_t MIXED = ((uint64_t) 0x1) << 5;

  /**
   * The object is designated read-only, so it can't be edited.
   */
  const uint64_t READONLY = ((uint64_t) 0x1) << 6;

  /**
   * The object is hot-tracked by the mouse, which means that its appearance
   * has changed to indicate that the mouse pointer is located over it.
   *
   * This is currently unused.
   */
  const uint64_t HOTTRACKED = ((uint64_t) 0x1) << 7;

  /**
   * This object is the default button in a window.
   */
  const uint64_t DEFAULT = ((uint64_t) 0x1) << 8;

  /**
   * The expandable object's children are displayed, the opposite of collapsed,
   * applied to trees, list and other controls.
   * @see COLLAPSED state
   */
  const uint64_t EXPANDED = ((uint64_t) 0x1) << 9;

  /**
   * The expandable object's children are not displayed, the opposite of
   * expanded, applied to tree lists and other controls, 
   * @see EXPANDED state.
   */
  const uint64_t COLLAPSED = ((uint64_t) 0x1) << 10;

  /**
   * The control or document can not accept input at this time.
   */
  const uint64_t BUSY = ((uint64_t) 0x1) << 11;

  /**
   * The object is out of normal flow, may be outside of boundaries of its
   * parent.
   */
  const uint64_t FLOATING = ((uint64_t) 0x1) << 12;

  /**
   * The object can be checked.
   */
  const uint64_t CHECKABLE = ((uint64_t) 0x1) << 13;

  /**
   * This object is a graphic which is rapidly changing appearance.
   */
  const uint64_t ANIMATED = ((uint64_t) 0x1) << 14;

  /**
   * The object is programmatically hidden.
   * So user action like scrolling or switching tabs won't make this visible.
   */
  const uint64_t INVISIBLE = ((uint64_t) 0x1) << 15;

  /**
   * The object is scrolled off screen.
   * User action such as scrolling or changing tab may make the object
   * visible.
   */
  const uint64_t OFFSCREEN = ((uint64_t) 0x1) << 16;

  /**
   * The object can be resized.
   */
  const uint64_t SIZEABLE = ((uint64_t) 0x1) << 17;

  /**
   * The object can be moved to a different position.
   */
  const uint64_t MOVEABLE = ((uint64_t) 0x1) << 18;

  /**
   * The object describes itself with speech.
   * Other speech related assistive technology may want to avoid speaking
   * information about this object, because the object is already doing this.
   */
  const uint64_t SELFVOICING = ((uint64_t) 0x1) << 19;

  /**
   * The object can have the focus and become focused.
   */
  const uint64_t FOCUSABLE = ((uint64_t) 0x1) << 20;

  /**
   * The object can be selected.
   */
  const uint64_t SELECTABLE = ((uint64_t) 0x1) << 21;

  /**
   * This object is a link.
   */
  const uint64_t LINKED = ((uint64_t) 0x1) << 22;

  /**
   * This is used for links that have been traversed
   * i.e. the linked page has been visited.
   */
  const uint64_t TRAVERSED = ((uint64_t) 0x1) << 23;

  /**
   * Supports multiple selection.
   */
  const uint64_t MULTISELECTABLE = ((uint64_t) 0x1) << 24;

  /**
   * Supports extended selection.
   * All objects supporting this are also multipselectable.
   * This only makes sense for msaa see bug 635690.
   */
  const uint64_t EXTSELECTABLE = ((uint64_t) 0x1) << 25;

  /**
   * The user is required to interact with this object.
   */
  const uint64_t REQUIRED = ((uint64_t) 0x1) << 26;

  /**
   * The object is an alert, notifying the user of something important.
   */
  const uint64_t ALERT = ((uint64_t) 0x1) << 27;

  /**
   * Used for text fields containing invalid values.
   */
  const uint64_t INVALID = ((uint64_t) 0x1) << 28;

  /**
   * The controls value can not be obtained, and is returned as a set of "*"s.
   */
  const uint64_t PROTECTED = ((uint64_t) 0x1) << 29;

  /**
   * The object can be invoked to show a pop up menu or window.
   */
 const uint64_t HASPOPUP = ((uint64_t) 0x1) << 30;

 /**
  * The editable area has some kind of autocompletion.
  */
  const uint64_t SUPPORTS_AUTOCOMPLETION = ((uint64_t) 0x1) << 31;

  /**
   * The object is no longer available to be queried.
   */
  const uint64_t DEFUNCT = ((uint64_t) 0x1) << 32;

  /**
   * The text is selectable, the object must implement the text interface.
   */
  const uint64_t SELECTABLE_TEXT = ((uint64_t) 0x1) << 33;

  /**
   * The text in this object can be edited.
   */
  const uint64_t EDITABLE = ((uint64_t) 0x1) << 34;

  /**
   * This window is currently the active window.
   */
  const uint64_t ACTIVE = ((uint64_t) 0x1) << 35;

  /**
   * Indicates that the object is modal. Modal objects have the behavior
   * that something must be done with the object before the user can
   * interact with an object in a different window.
   */
  const uint64_t MODAL = ((uint64_t) 0x1) << 36;

  /**
   * Edit control that can take multiple lines.
  */
  const uint64_t MULTI_LINE = ((uint64_t) 0x1) << 37;

  /**
   * Uses horizontal layout.
   */
  const uint64_t HORIZONTAL = ((uint64_t) 0x1) << 38;

  /**
   * Indicates this object paints every pixel within its rectangular region.
   */
  const uint64_t OPAQUE1 = ((uint64_t) 0x1) << 39;

  /**
   * This text object can only contain 1 line of text.
   */
  const uint64_t SINGLE_LINE = ((uint64_t) 0x1) << 40;

  /**
   * The parent object manages descendants, and this object may only exist
   * while it is visible or has focus.
   * For example the focused cell of a table or the current element of a list box may have this state.
   */
  const uint64_t TRANSIENT = ((uint64_t) 0x1) << 41;

  /**
   * Uses vertical layout.
   * Especially used for sliders and scrollbars.
   */
  const uint64_t VERTICAL = ((uint64_t) 0x1) << 42;

  /**
   * Object not dead, but not up-to-date either.
   */
  const uint64_t STALE = ((uint64_t) 0x1) << 43;

  /**
   * A widget that is not unavailable.
   */
  const uint64_t ENABLED = ((uint64_t) 0x1) << 44;

  /**
   * Same as ENABLED state for now see bug 636158
   */
  const uint64_t SENSITIVE = ((uint64_t) 0x1) << 45;

  /**
   * The object is expandable, provides a UI to expand/collapse its children
   * @see EXPANDED and COLLAPSED states.
   */
  const uint64_t EXPANDABLE = ((uint64_t) 0x1) << 46;

  /**
   * The object is pinned, usually indicating it is fixed in place and has permanence.
   */
  const uint64_t PINNED = ((uint64_t) 0x1) << 47;
} // namespace states
} // namespace a11y
} // namespace mozilla

#endif
	
