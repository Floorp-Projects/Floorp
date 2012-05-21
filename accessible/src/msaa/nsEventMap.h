/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <winuser.h>
#include "AccessibleEventId.h"

const PRUint32 kEVENT_WIN_UNKNOWN = 0x00000000;
const PRUint32 kEVENT_LAST_ENTRY  = 0xffffffff;

static const PRUint32 gWinEventMap[] = {
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent doesn't have 0 constant
  EVENT_OBJECT_SHOW,                                 // nsIAccessibleEvent::EVENT_SHOW
  EVENT_OBJECT_HIDE,                                 // nsIAccessibleEvent::EVENT_HIDE
  EVENT_OBJECT_REORDER,                              // nsIAccessibleEvent::EVENT_REORDER
  IA2_EVENT_ACTIVE_DECENDENT_CHANGED,                // nsIAccessibleEvent::EVENT_ACTIVE_DECENDENT_CHANGED
  EVENT_OBJECT_FOCUS,                                // nsIAccessibleEvent::EVENT_FOCUS
  EVENT_OBJECT_STATECHANGE,                          // nsIAccessibleEvent::EVENT_STATE_CHANGE
  EVENT_OBJECT_LOCATIONCHANGE,                       // nsIAccessibleEvent::EVENT_LOCATION_CHANGE
  EVENT_OBJECT_NAMECHANGE,                           // nsIAccessibleEvent::EVENT_NAME_CHANGE
  EVENT_OBJECT_DESCRIPTIONCHANGE,                    // nsIAccessibleEvent::EVENT_DESCRIPTION_CHANGE
  EVENT_OBJECT_VALUECHANGE,                          // nsIAccessibleEvent::EVENT_VALUE_CHANGE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_HELP_CHANGE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_DEFACTION_CHANGE
  IA2_EVENT_ACTION_CHANGED,                          // nsIAccessibleEvent::EVENT_ACTION_CHANGE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_ACCELERATOR_CHANGE
  EVENT_OBJECT_SELECTION,                            // nsIAccessibleEvent::EVENT_SELECTION
  EVENT_OBJECT_SELECTIONADD,                         // nsIAccessibleEvent::EVENT_SELECTION_ADD
  EVENT_OBJECT_SELECTIONREMOVE,                      // nsIAccessibleEvent::EVENT_SELECTION_REMOVE
  EVENT_OBJECT_SELECTIONWITHIN,                      // nsIAccessibleEvent::EVENT_SELECTION_WITHIN
  EVENT_SYSTEM_ALERT,                                // nsIAccessibleEvent::EVENT_ALERT
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_FOREGROUND
  EVENT_SYSTEM_MENUSTART,                            // nsIAccessibleEvent::EVENT_MENU_START
  EVENT_SYSTEM_MENUEND,                              // nsIAccessibleEvent::EVENT_MENU_END
  EVENT_SYSTEM_MENUPOPUPSTART,                       // nsIAccessibleEvent::EVENT_MENUPOPUP_START
  EVENT_SYSTEM_MENUPOPUPEND,                         // nsIAccessibleEvent::EVENT_MENUPOPUP_END
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_CAPTURE_START
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_CAPTURE_END
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_MOVESIZE_START
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_MOVESIZE_END
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_CONTEXT_HELP_START
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_CONTEXT_HELP_END
  EVENT_SYSTEM_DRAGDROPSTART,                        // nsIAccessibleEvent::EVENT_DRAGDROP_START
  EVENT_SYSTEM_DRAGDROPEND,                          // nsIAccessibleEvent::EVENT_DRAGDROP_END
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_DIALOG_START
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_DIALOG_END
  EVENT_SYSTEM_SCROLLINGSTART,                       // nsIAccessibleEvent::EVENT_SCROLLING_START
  EVENT_SYSTEM_SCROLLINGEND,                         // nsIAccessibleEvent::EVENT_SCROLLING_END
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_MINIMIZE_START
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_MINIMIZE_END
  IA2_EVENT_DOCUMENT_LOAD_COMPLETE,                  // nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE
  IA2_EVENT_DOCUMENT_RELOAD,                         // nsIAccessibleEvent::EVENT_DOCUMENT_RELOAD
  IA2_EVENT_DOCUMENT_LOAD_STOPPED,                   // nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_STOPPED
  IA2_EVENT_DOCUMENT_ATTRIBUTE_CHANGED,              // nsIAccessibleEvent::EVENT_DOCUMENT_ATTRIBUTES_CHANGED
  IA2_EVENT_DOCUMENT_CONTENT_CHANGED,                // nsIAccessibleEvent::EVENT_DOCUMENT_CONTENT_CHANGED
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_PROPERTY_CHANGED
  IA2_EVENT_PAGE_CHANGED,                            // nsIAccessibleEvent::IA2_EVENT_PAGE_CHANGED
  IA2_EVENT_TEXT_ATTRIBUTE_CHANGED,                  // nsIAccessibleEvent::EVENT_TEXT_ATTRIBUTE_CHANGED
  IA2_EVENT_TEXT_CARET_MOVED,                        // nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED
  IA2_EVENT_TEXT_CHANGED,                            // nsIAccessibleEvent::EVENT_TEXT_CHANGED
  IA2_EVENT_TEXT_INSERTED,                           // nsIAccessibleEvent::EVENT_TEXT_INSERTED
  IA2_EVENT_TEXT_REMOVED,                            // nsIAccessibleEvent::EVENT_TEXT_REMOVED
  IA2_EVENT_TEXT_UPDATED,                            // nsIAccessibleEvent::EVENT_TEXT_UPDATED
  IA2_EVENT_TEXT_SELECTION_CHANGED,                  // nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED
  IA2_EVENT_VISIBLE_DATA_CHANGED,                    // nsIAccessibleEvent::EVENT_VISIBLE_DATA_CHANGED
  IA2_EVENT_TEXT_COLUMN_CHANGED,                     // nsIAccessibleEvent::EVENT_TEXT_COLUMN_CHANGED
  IA2_EVENT_SECTION_CHANGED,                         // nsIAccessibleEvent::EVENT_SECTION_CHANGED
  IA2_EVENT_TABLE_CAPTION_CHANGED,                   // nsIAccessibleEvent::EVENT_TABLE_CAPTION_CHANGED
  IA2_EVENT_TABLE_MODEL_CHANGED,                     // nsIAccessibleEvent::EVENT_TABLE_MODEL_CHANGED
  IA2_EVENT_TABLE_SUMMARY_CHANGED,                   // nsIAccessibleEvent::EVENT_TABLE_SUMMARY_CHANGED
  IA2_EVENT_TABLE_ROW_DESCRIPTION_CHANGED,           // nsIAccessibleEvent::EVENT_TABLE_ROW_DESCRIPTION_CHANGED
  IA2_EVENT_TABLE_ROW_HEADER_CHANGED,                // nsIAccessibleEvent::EVENT_TABLE_ROW_HEADER_CHANGED
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_TABLE_ROW_INSERT
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_TABLE_ROW_DELETE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_TABLE_ROW_REORDER
  IA2_EVENT_TABLE_COLUMN_DESCRIPTION_CHANGED,        // nsIAccessibleEvent::EVENT_TABLE_COLUMN_DESCRIPTION_CHANGED
  IA2_EVENT_TABLE_COLUMN_HEADER_CHANGED,             // nsIAccessibleEvent::EVENT_TABLE_COLUMN_HEADER_CHANGED
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_TABLE_COLUMN_INSERT
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_TABLE_COLUMN_DELETE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_TABLE_COLUMN_REORDER
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_WINDOW_ACTIVATE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_WINDOW_CREATE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_WINDOW_DEACTIVATE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_WINDOW_DESTROY
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_WINDOW_MAXIMIZE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_WINDOW_MINIMIZE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_WINDOW_RESIZE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_WINDOW_RESTORE
  IA2_EVENT_HYPERLINK_END_INDEX_CHANGED,             // nsIAccessibleEvent::EVENT_HYPERLINK_END_INDEX_CHANGED
  IA2_EVENT_HYPERLINK_NUMBER_OF_ANCHORS_CHANGED,     // nsIAccessibleEvent::EVENT_HYPERLINK_NUMBER_OF_ANCHORS_CHANGED
  IA2_EVENT_HYPERLINK_SELECTED_LINK_CHANGED,         // nsIAccessibleEvent::EVENT_HYPERLINK_SELECTED_LINK_CHANGED
  IA2_EVENT_HYPERTEXT_LINK_ACTIVATED,                // nsIAccessibleEvent::EVENT_HYPERTEXT_LINK_ACTIVATED
  IA2_EVENT_HYPERTEXT_LINK_SELECTED,                 // nsIAccessibleEvent::EVENT_HYPERTEXT_LINK_SELECTED
  IA2_EVENT_HYPERLINK_START_INDEX_CHANGED,           // nsIAccessibleEvent::EVENT_HYPERLINK_START_INDEX_CHANGED
  IA2_EVENT_HYPERTEXT_CHANGED,                       // nsIAccessibleEvent::EVENT_HYPERTEXT_CHANGED
  IA2_EVENT_HYPERTEXT_NLINKS_CHANGED,                // nsIAccessibleEvent::EVENT_HYPERTEXT_NLINKS_CHANGED
  IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED,                // nsIAccessibleEvent::EVENT_OBJECT_ATTRIBUTE_CHANGED
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_VIRTUALCURSOR_CHANGED
  kEVENT_LAST_ENTRY                                  // nsIAccessibleEvent::EVENT_LAST_ENTRY
};

