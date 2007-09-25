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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt (jgaunt@netscape.com)
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

#ifndef __nsAccessibilityService_h__
#define __nsAccessibilityService_h__

#include "nsIAccessibilityService.h"
#include "nsIObserver.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"

class nsIFrame;
class nsIWeakReference;
class nsIDOMNode;
class nsObjectFrame;
class nsIDocShell;
class nsIPresShell;
class nsIContent;

class nsAccessibilityService : public nsIAccessibilityService,
                               public nsIObserver,
                               public nsIWebProgressListener,
                               public nsSupportsWeakReference
{
public:
  nsAccessibilityService();
  virtual ~nsAccessibilityService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCESSIBLERETRIEVAL
  NS_DECL_NSIACCESSIBILITYSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIWEBPROGRESSLISTENER

  /**
   * Return presentation shell for the given node.
   *
   * @param aNode - the given DOM node.
   */
  static nsresult GetShellFromNode(nsIDOMNode *aNode,
                                   nsIWeakReference **weakShell);

  /**
   * Return accessibility service (static instance of this class).
   */
  static nsresult GetAccessibilityService(nsIAccessibilityService** aResult);

private:
  /**
   * Return presentation shell, DOM node for the given frame.
   *
   * @param aFrame - the given frame
   * @param aRealFrame [out] - the given frame casted to nsIFrame
   * @param aShell [out] - presentation shell for DOM node associated with the
   *                 given frame
   * @param aContent [out] - DOM node associated with the given frame
   */
  nsresult GetInfo(nsISupports *aFrame, nsIFrame **aRealFrame,
                   nsIWeakReference **aShell,
                   nsIDOMNode **aContent);

  /**
   * Initialize an accessible and cache it. The method should be called for
   * every created accessible.
   *
   * @param aAccessibleIn - accessible to initialize.
   */
  nsresult InitAccessible(nsIAccessible *aAccessibleIn, nsIAccessible **aAccessibleOut);

  /**
   * Return accessible object for elements implementing nsIAccessibleProvider
   * interface.
   *
   * @param aNode - DOM node that accessible is returned for.
   */
  nsresult GetAccessibleByType(nsIDOMNode *aNode, nsIAccessible **aAccessible);

  /**
   * Return accessible object if parent is a deck frame.
   *
   * @param aNode - DOMNode that accessible is returned for.
   */
  nsresult GetAccessibleForDeckChildren(nsIDOMNode *aNode,
                                        nsIAccessible **aAccessible);

  static nsAccessibilityService *gAccessibilityService;

  /**
   * Does this content node have a universal ARIA property set on it?
   * A universal ARIA property is one that can be defined on any element even if there is no role.
   *
   * @param aContent The content node to test
   * @param aWeakShell  A weak reference to the pres shell
   * @return PR_TRUE if there is a universal ARIA property set on the node
   */
  PRBool HasUniversalAriaProperty(nsIContent *aContent, nsIWeakReference *aWeakShell);
};

/**
 * Map nsIAccessibleRole constants to strings. Used by
 * nsIAccessibleRetrieval::getStringRole() method.
 */
static const char kRoleNames[][20] = {
  "nothing",             //ROLE_NOTHING
  "titlebar",            //ROLE_TITLEBAR
  "menubar",             //ROLE_MENUBAR
  "scrollbar",           //ROLE_SCROLLBAR 
  "grip",                //ROLE_GRIP
  "sound",               //ROLE_SOUND
  "cursor",              //ROLE_CURSOR
  "caret",               //ROLE_CARET
  "alert",               //ROLE_ALERT
  "window",              //ROLE_WINDOW
  "client",              //ROLE_CLIENT
  "menupopup",           //ROLE_MENUPOPUP
  "menuitem",            //ROLE_MENUITEM
  "tooltip",             //ROLE_TOOLTIP
  "application",         //ROLE_APPLICATION
  "document",            //ROLE_DOCUMENT
  "pane",                //ROLE_PANE
  "chart",               //ROLE_CHART
  "dialog",              //ROLE_DIALOG
  "border",              //ROLE_BORDER
  "grouping",            //ROLE_GROUPING
  "separator",           //ROLE_SEPARATOR
  "toolbar",             //ROLE_TOOLBAR
  "statusbar",           //ROLE_STATUSBAR
  "table",               //ROLE_TABLE
  "columnheader",        //ROLE_COLUMNHEADER
  "rowheader",           //ROLE_ROWHEADER
  "column",              //ROLE_COLUMN
  "row",                 //ROLE_ROW
  "cell",                //ROLE_CELL
  "link",                //ROLE_LINK
  "helpballoon",         //ROLE_HELPBALLOON
  "character",           //ROLE_CHARACTER
  "list",                //ROLE_LIST
  "listitem",            //ROLE_LISTITEM
  "outline",             //ROLE_OUTLINE
  "outlineitem",         //ROLE_OUTLINEITEM
  "pagetab",             //ROLE_PAGETAB
  "propertypage",        //ROLE_PROPERTYPAGE
  "indicator",           //ROLE_INDICATOR
  "graphic",             //ROLE_GRAPHIC
  "statictext",          //ROLE_STATICTEXT
  "text leaf",           //ROLE_TEXT_LEAF
  "pushbutton",          //ROLE_PUSHBUTTON
  "checkbutton",         //ROLE_CHECKBUTTON
  "radiobutton",         //ROLE_RADIOBUTTON
  "combobox",            //ROLE_COMBOBOX
  "droplist",            //ROLE_DROPLIST
  "progressbar",         //ROLE_PROGRESSBAR
  "dial",                //ROLE_DIAL
  "hotkeyfield",         //ROLE_HOTKEYFIELD
  "slider",              //ROLE_SLIDER
  "spinbutton",          //ROLE_SPINBUTTON
  "diagram",             //ROLE_DIAGRAM
  "animation",           //ROLE_ANIMATION
  "equation",            //ROLE_EQUATION
  "buttondropdown",      //ROLE_BUTTONDROPDOWN
  "buttonmenu",          //ROLE_BUTTONMENU
  "buttondropdowngrid",  //ROLE_BUTTONDROPDOWNGRID
  "whitespace",          //ROLE_WHITESPACE
  "pagetablist",         //ROLE_PAGETABLIST
  "clock",               //ROLE_CLOCK
  "splitbutton",         //ROLE_SPLITBUTTON
  "ipaddress",           //ROLE_IPADDRESS
  "accel label",         //ROLE_ACCEL_LABEL
  "arrow",               //ROLE_ARROW
  "canvas",              //ROLE_CANVAS
  "check menu item",     //ROLE_CHECK_MENU_ITEM
  "color chooser",       //ROLE_COLOR_CHOOSER
  "date editor",         //ROLE_DATE_EDITOR
  "desktop icon",        //ROLE_DESKTOP_ICON
  "desktop frame",       //ROLE_DESKTOP_FRAME
  "directory pane",      //ROLE_DIRECTORY_PANE
  "file chooser",        //ROLE_FILE_CHOOSER
  "font chooser",        //ROLE_FONT_CHOOSER
  "chrome window",       //ROLE_CHROME_WINDOW
  "glass pane",          //ROLE_GLASS_PANE
  "html container",      //ROLE_HTML_CONTAINER
  "icon",                //ROLE_ICON
  "label",               //ROLE_LABEL
  "layered pane",        //ROLE_LAYERED_PANE
  "option pane",         //ROLE_OPTION_PANE
  "password text",       //ROLE_PASSWORD_TEXT
  "popup menu",          //ROLE_POPUP_MENU
  "radio menu item",     //ROLE_RADIO_MENU_ITEM
  "root pane",           //ROLE_ROOT_PANE
  "scroll pane",         //ROLE_SCROLL_PANE
  "split pane",          //ROLE_SPLIT_PANE
  "table column header", //ROLE_TABLE_COLUMN_HEADER
  "table row header",    //ROLE_TABLE_ROW_HEADER
  "tear off menu item",  //ROLE_TEAR_OFF_MENU_ITEM
  "terminal",            //ROLE_TERMINAL
  "text container",      //ROLE_TEXT_CONTAINER
  "toggle button",       //ROLE_TOGGLE_BUTTON
  "tree table",          //ROLE_TREE_TABLE
  "viewport",            //ROLE_VIEWPORT
  "header",              //ROLE_HEADER
  "footer",              //ROLE_FOOTER
  "paragraph",           //ROLE_PARAGRAPH
  "ruler",               //ROLE_RULER
  "autocomplete",        //ROLE_AUTOCOMPLETE
  "editbar",             //ROLE_EDITBAR
  "entry",               //ROLE_ENTRY
  "caption",             //ROLE_CAPTION
  "document frame",      //ROLE_DOCUMENT_FRAME
  "heading",             //ROLE_HEADING
  "page",                //ROLE_PAGE
  "section",             //ROLE_SECTION
  "redundant object",    //ROLE_REDUNDANT_OBJECT
  "form",                //ROLE_FORM
  "ime",                 //ROLE_IME
  "app root",            //ROLE_APP_ROOT
  "parent menuitem",     //ROLE_PARENT_MENUITEM
  "calendar",            //ROLE_CALENDAR
  "combobox list",       //ROLE_COMBOBOX_LIST
  "combobox listitem",   //ROLE_COMBOBOX_LISTITEM
  "image map"            //ROLE_IMAGE_MAP
};

/**
 * Map nsIAccessibleEvents constants to strings. Used by
 * nsIAccessibleRetrieval::getStringEventType() method.
 */
static const char kEventTypeNames[][40] = {
  "unknown",                                 //
  "DOM node create",                         // EVENT_DOM_CREATE
  "DOM node destroy",                        // EVENT_DOM_DESTROY
  "DOM node significant change",             // EVENT_DOM_SIGNIFICANT_CHANGE
  "async show",                              // EVENT_ASYNCH_SHOW
  "async hide",                              // EVENT_ASYNCH_HIDE
  "async significant change",                // EVENT_ASYNCH_SIGNIFICANT_CHANGE
  "active decendent change",                 // EVENT_ACTIVE_DECENDENT_CHANGED
  "focus",                                   // EVENT_FOCUS
  "state change",                            // EVENT_STATE_CHANGE
  "location change",                         // EVENT_LOCATION_CHANGE
  "name changed",                            // EVENT_NAME_CHANGE
  "description change",                      // EVENT_DESCRIPTION_CHANGE
  "value change",                            // EVENT_VALUE_CHANGE
  "help change",                             // EVENT_HELP_CHANGE
  "default action change",                   // EVENT_DEFACTION_CHANGE
  "action change",                           // EVENT_ACTION_CHANGE
  "accelerator change",                      // EVENT_ACCELERATOR_CHANGE
  "selection",                               // EVENT_SELECTION
  "selection add",                           // EVENT_SELECTION_ADD
  "selection remove",                        // EVENT_SELECTION_REMOVE
  "selection within",                        // EVENT_SELECTION_WITHIN
  "alert",                                   // EVENT_ALERT
  "foreground",                              // EVENT_FOREGROUND
  "menu start",                              // EVENT_MENU_START
  "menu end",                                // EVENT_MENU_END
  "menupopup start",                         // EVENT_MENUPOPUP_START
  "menupopup end",                           // EVENT_MENUPOPUP_END
  "capture start",                           // EVENT_CAPTURE_START
  "capture end",                             // EVENT_CAPTURE_END
  "movesize start",                          // EVENT_MOVESIZE_START
  "movesize end",                            // EVENT_MOVESIZE_END
  "contexthelp start",                       // EVENT_CONTEXTHELP_START
  "contexthelp end",                         // EVENT_CONTEXTHELP_END
  "dragdrop start",                          // EVENT_DRAGDROP_START
  "dragdrop end",                            // EVENT_DRAGDROP_END
  "dialog start",                            // EVENT_DIALOG_START
  "dialog end",                              // EVENT_DIALOG_END
  "scrolling start",                         // EVENT_SCROLLING_START
  "scrolling end",                           // EVENT_SCROLLING_END
  "minimize start",                          // EVENT_MINIMIZE_START
  "minimize end",                            // EVENT_MINIMIZE_END
  "document load start",                     // EVENT_DOCUMENT_LOAD_START
  "document load complete",                  // EVENT_DOCUMENT_LOAD_COMPLETE
  "document reload",                         // EVENT_DOCUMENT_RELOAD
  "document load stopped",                   // EVENT_DOCUMENT_LOAD_STOPPED
  "document attributes changed",             // EVENT_DOCUMENT_ATTRIBUTES_CHANGED
  "document content changed",                // EVENT_DOCUMENT_CONTENT_CHANGED
  "property changed",                        // EVENT_PROPERTY_CHANGED
  "selection changed",                       // EVENT_SELECTION_CHANGED
  "text attribute changed",                  // EVENT_TEXT_ATTRIBUTE_CHANGED
  "text caret moved",                        // EVENT_TEXT_CARET_MOVED
  "text changed",                            // EVENT_TEXT_CHANGED
  "text inserted",                           // EVENT_TEXT_INSERTED
  "text removed",                            // EVENT_TEXT_REMOVED
  "text updated",                            // EVENT_TEXT_UPDATED
  "text selection changed",                  // EVENT_TEXT_SELECTION_CHANGED
  "visible data changed",                    // EVENT_VISIBLE_DATA_CHANGED
  "text column changed",                     // EVENT_TEXT_COLUMN_CHANGED
  "section changed",                         // EVENT_SECTION_CHANGED
  "table caption changed",                   // EVENT_TABLE_CAPTION_CHANGED
  "table model changed",                     // EVENT_TABLE_MODEL_CHANGED
  "table summary changed",                   // EVENT_TABLE_SUMMARY_CHANGED
  "table row description changed",           // EVENT_TABLE_ROW_DESCRIPTION_CHANGED
  "table row header changed",                // EVENT_TABLE_ROW_HEADER_CHANGED
  "table row insert",                        // EVENT_TABLE_ROW_INSERT
  "table row delete",                        // EVENT_TABLE_ROW_DELETE
  "table row reorder",                       // EVENT_TABLE_ROW_REORDER
  "table column description changed",        // EVENT_TABLE_COLUMN_DESCRIPTION_CHANGED
  "table column header changed",             // EVENT_TABLE_COLUMN_HEADER_CHANGED
  "table column insert",                     // EVENT_TABLE_COLUMN_INSERT
  "table column delete",                     // EVENT_TABLE_COLUMN_DELETE
  "table column reorder",                    // EVENT_TABLE_COLUMN_REORDER
  "window activate",                         // EVENT_WINDOW_ACTIVATE
  "window create",                           // EVENT_WINDOW_CREATE
  "window deactivate",                       // EVENT_WINDOW_DEACTIVATE
  "window destroy",                          // EVENT_WINDOW_DESTROY
  "window maximize",                         // EVENT_WINDOW_MAXIMIZE
  "window minimize",                         // EVENT_WINDOW_MINIMIZE
  "window resize",                           // EVENT_WINDOW_RESIZE
  "window restore",                          // EVENT_WINDOW_RESTORE
  "hyperlink end index changed",             // EVENT_HYPERLINK_END_INDEX_CHANGED
  "hyperlink number of anchors changed",     // EVENT_HYPERLINK_NUMBER_OF_ANCHORS_CHANGED
  "hyperlink selected link changed",         // EVENT_HYPERLINK_SELECTED_LINK_CHANGED
  "hypertext link activated",                // EVENT_HYPERTEXT_LINK_ACTIVATED
  "hypertext link selected",                 // EVENT_HYPERTEXT_LINK_SELECTED
  "hyperlink start index changed",           // EVENT_HYPERLINK_START_INDEX_CHANGED
  "hypertext changed",                       // EVENT_HYPERTEXT_CHANGED
  "hypertext links count changed",           // EVENT_HYPERTEXT_NLINKS_CHANGED
  "object attribute changed",                // EVENT_OBJECT_ATTRIBUTE_CHANGED
  "internal load",                           // EVENT_INTERNAL_LOAD
  "reorder"                                  // EVENT_REORDER
};

#endif /* __nsIAccessibilityService_h__ */
