/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "winable.h"
#include "AccessibleEventId.h"

const PRUint32 kEVENT_WIN_UNKNOWN = 0x00000000;

static const PRUint32 gWinEventMap[] = {
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_CREATE
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_DESTROY
  EVENT_OBJECT_SHOW,                                 // nsIAccessibleEvent::EVENT_SHOW
  EVENT_OBJECT_HIDE,                                 // nsIAccessibleEvent::EVENT_HIDE
  EVENT_OBJECT_REORDER,                              // nsIAccessibleEvent::EVENT_REORDER
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_PARENT_CHANGE
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
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_DRAGDROP_START
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_DRAGDROP_END
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_DIALOG_START
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_DIALOG_END
  EVENT_SYSTEM_SCROLLINGSTART,                       // nsIAccessibleEvent::EVENT_SCROLLING_START
  EVENT_SYSTEM_SCROLLINGEND,                         // nsIAccessibleEvent::EVENT_SCROLLING_END
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_MINIMIZE_START
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_MINIMIZE_END
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_START
  IA2_EVENT_DOCUMENT_LOAD_COMPLETE,                  // nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE
  IA2_EVENT_DOCUMENT_RELOAD,                         // nsIAccessibleEvent::EVENT_DOCUMENT_RELOAD
  IA2_EVENT_DOCUMENT_LOAD_STOPPED,                   // nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_STOPPED
  IA2_EVENT_DOCUMENT_ATTRIBUTE_CHANGED,              // nsIAccessibleEvent::EVENT_DOCUMENT_ATTRIBUTES_CHANGED
  IA2_EVENT_DOCUMENT_CONTENT_CHANGED,                // nsIAccessibleEvent::EVENT_DOCUMENT_CONTENT_CHANGED
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_PROPERTY_CHANGED
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_SELECTION_CHANGED
  IA2_EVENT_TEXT_ATTRIBUTE_CHANGED,                  // nsIAccessibleEvent::EVENT_TEXT_ATTRIBUTE_CHANGED
  IA2_EVENT_TEXT_CARET_MOVED,                        // nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED
  IA2_EVENT_TEXT_CHANGED,                            // nsIAccessibleEvent::EVENT_TEXT_CHANGED
  IA2_EVENT_TEXT_INSERTED,                           // nsIAccessibleEvent::EVENT_TEXT_INSERTED
  IA2_EVENT_TEXT_REMOVED,                            // nsIAccessibleEvent::EVENT_TEXT_REMOVED
  IA2_EVENT_TEXT_UPDATED,                            // nsIAccessibleEvent::EVENT_TEXT_UPDATED
  IA2_EVENT_TEXT_SELECTION_CHANGED,                  // nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED
  IA2_EVENT_VISIBLE_DATA_CHANGED,                    // nsIAccessibleEvent::EVENT_VISIBLE_DATA_CHANGED
  IA2_EVENT_COLUMN_CHANGED,                          // nsIAccessibleEvent::EVENT_COLUMN_CHANGED
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
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_INTERNAL_LOAD
  IA2_EVENT_HYPERLINK_END_INDEX_CHANGED,             // nsIAccessibleEvent::EVENT_HYPERLINK_END_INDEX_CHANGED
  IA2_EVENT_HYPERLINK_NUMBER_OF_ANCHORS_CHANGED,     // nsIAccessibleEvent::EVENT_HYPERLINK_NUMBER_OF_ANCHORS_CHANGED
  IA2_EVENT_HYPERLINK_SELECTED_LINK_CHANGED,         // nsIAccessibleEvent::EVENT_HYPERLINK_SELECTED_LINK_CHANGED
  IA2_EVENT_HYPERTEXT_LINK_ACTIVATED,                // nsIAccessibleEvent::EVENT_HYPERTEXT_LINK_ACTIVATED
  IA2_EVENT_HYPERTEXT_LINK_SELECTED,                 // nsIAccessibleEvent::EVENT_HYPERTEXT_LINK_SELECTED
  IA2_EVENT_HYPERLINK_START_INDEX_CHANGED,           // nsIAccessibleEvent::EVENT_HYPERLINK_START_INDEX_CHANGED
  IA2_EVENT_HYPERTEXT_CHANGED,                       // nsIAccessibleEvent::EVENT_HYPERTEXT_CHANGED
  IA2_EVENT_HYPERTEXT_NLINKS_CHANGED,                // nsIAccessibleEvent::EVENT_HYPERTEXT_NLINKS_CHANGED
  IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED,                // nsIAccessibleEvent::EVENT_OBJECT_ATTRIBUTE_CHANGED
  IA2_EVENT_PAGE_CHANGED,                            // nsIAccessibleEvent::EVENT_PAGE_CHANGED
  IA2_EVENT_ROLE_CHANGED,                            // nsIAccessibleEvent::EVENT_ROLE_CHANGED
  kEVENT_WIN_UNKNOWN,                                // nsIAccessibleEvent::EVENT_INTERNAL_LOAD
};

