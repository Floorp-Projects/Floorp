/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <winuser.h>
#include "AccessibleEventId.h"

const uint32_t kEVENT_WIN_UNKNOWN = 0x00000000;

static const uint32_t gWinEventMap[] = {
    // clang-format off
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent doesn't have 0 constant
  EVENT_OBJECT_SHOW,                                 // nsIAccessibleEvent::EVENT_SHOW
  EVENT_OBJECT_HIDE,                                 // nsIAccessibleEvent::EVENT_HIDE
  EVENT_OBJECT_REORDER,                              // nsIAccessibleEvent::EVENT_REORDER
  EVENT_OBJECT_FOCUS,                                // nsIAccessibleEvent::EVENT_FOCUS
  EVENT_OBJECT_STATECHANGE,                          // nsIAccessibleEvent::EVENT_STATE_CHANGE
  EVENT_OBJECT_NAMECHANGE,                           // nsIAccessibleEvent::EVENT_NAME_CHANGE
  EVENT_OBJECT_DESCRIPTIONCHANGE,                    // nsIAccessibleEvent::EVENT_DESCRIPTION_CHANGE
  EVENT_OBJECT_VALUECHANGE,                          // nsIAccessibleEvent::EVENT_VALUE_CHANGE
  EVENT_OBJECT_SELECTION,                            // nsIAccessibleEvent::EVENT_SELECTION
  EVENT_OBJECT_SELECTIONADD,                         // nsIAccessibleEvent::EVENT_SELECTION_ADD
  EVENT_OBJECT_SELECTIONREMOVE,                      // nsIAccessibleEvent::EVENT_SELECTION_REMOVE
  EVENT_OBJECT_SELECTIONWITHIN,                      // nsIAccessibleEvent::EVENT_SELECTION_WITHIN
  EVENT_SYSTEM_ALERT,                                // nsIAccessibleEvent::EVENT_ALERT
  EVENT_SYSTEM_MENUSTART,                            // nsIAccessibleEvent::EVENT_MENU_START
  EVENT_SYSTEM_MENUEND,                              // nsIAccessibleEvent::EVENT_MENU_END
  EVENT_SYSTEM_MENUPOPUPSTART,                       // nsIAccessibleEvent::EVENT_MENUPOPUP_START
  EVENT_SYSTEM_MENUPOPUPEND,                         // nsIAccessibleEvent::EVENT_MENUPOPUP_END
  EVENT_SYSTEM_DRAGDROPSTART,                        // nsIAccessibleEvent::EVENT_DRAGDROP_START
  EVENT_SYSTEM_SCROLLINGSTART,                       // nsIAccessibleEvent::EVENT_SCROLLING_START
  EVENT_SYSTEM_SCROLLINGEND,                         // nsIAccessibleEvent::EVENT_SCROLLING_END
  IA2_EVENT_DOCUMENT_LOAD_COMPLETE,                  // nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE
  IA2_EVENT_DOCUMENT_RELOAD,                         // nsIAccessibleEvent::EVENT_DOCUMENT_RELOAD
  IA2_EVENT_DOCUMENT_LOAD_STOPPED,                   // nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_STOPPED
  IA2_EVENT_TEXT_ATTRIBUTE_CHANGED,                  // nsIAccessibleEvent::EVENT_TEXT_ATTRIBUTE_CHANGED
  IA2_EVENT_TEXT_CARET_MOVED,                        // nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED
  IA2_EVENT_TEXT_INSERTED,                           // nsIAccessibleEvent::EVENT_TEXT_INSERTED
  IA2_EVENT_TEXT_REMOVED,                            // nsIAccessibleEvent::EVENT_TEXT_REMOVED
  IA2_EVENT_TEXT_SELECTION_CHANGED,                  // nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_WINDOW_ACTIVATE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_WINDOW_DEACTIVATE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_WINDOW_MAXIMIZE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_WINDOW_MINIMIZE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_WINDOW_RESTORE
  IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED,                // nsIAccessibleEvent::EVENT_OBJECT_ATTRIBUTE_CHANGED
  EVENT_OBJECT_VALUECHANGE,                          // nsIAccessibleEvent::EVENT_TEXT_VALUE_CHANGE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_SCROLLING
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_ANNOUNCEMENT
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_LIVE_REGION_ADDED
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_LIVE_REGION_REMOVED
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_INNER_REORDER
    // clang-format on
};
