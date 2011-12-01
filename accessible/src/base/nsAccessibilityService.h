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

#include "a11yGeneric.h"
#include "nsAccDocManager.h"

#include "mozilla/a11y/FocusManager.h"

#include "nsIObserver.h"

namespace mozilla {
namespace a11y {

/**
 * Return focus manager.
 */
FocusManager* FocusMgr();

} // namespace a11y
} // namespace mozilla

class nsAccessibilityService : public nsAccDocManager,
                               public mozilla::a11y::FocusManager,
                               public nsIAccessibilityService,
                               public nsIObserver
{
public:
  virtual ~nsAccessibilityService();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIACCESSIBLERETRIEVAL
  NS_DECL_NSIOBSERVER

  // nsIAccessibilityService
  virtual nsAccessible* GetAccessibleInShell(nsINode* aNode,
                                             nsIPresShell* aPresShell);
  virtual nsAccessible* GetRootDocumentAccessible(nsIPresShell* aPresShell,
                                                  bool aCanCreate);

  virtual already_AddRefed<nsAccessible>
    CreateHTMLBRAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTML4ButtonAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLButtonAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<nsAccessible>
    CreateHTMLCanvasAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLCaptionAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLCheckboxAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLComboboxAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  already_AddRefed<nsAccessible>
    CreateHTMLFileInputAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLGroupboxAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLHRAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLImageAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLLabelAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLLIAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLListboxAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLMediaAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLObjectFrameAccessible(nsObjectFrame* aFrame, nsIContent* aContent,
                                    nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLRadioButtonAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLTableAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLTableCellAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLTextAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHTMLTextFieldAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateHyperTextAccessible(nsIContent* aContent, nsIPresShell* aPresShell);
  virtual already_AddRefed<nsAccessible>
    CreateOuterDocAccessible(nsIContent* aContent, nsIPresShell* aPresShell);

  virtual nsAccessible* AddNativeRootAccessible(void* aAtkAccessible);
  virtual void RemoveNativeRootAccessible(nsAccessible* aRootAccessible);

  virtual void ContentRangeInserted(nsIPresShell* aPresShell,
                                    nsIContent* aContainer,
                                    nsIContent* aStartChild,
                                    nsIContent* aEndChild);

  virtual void ContentRemoved(nsIPresShell* aPresShell, nsIContent* aContainer,
                              nsIContent* aChild);

  virtual void UpdateText(nsIPresShell* aPresShell, nsIContent* aContent);

  /**
   * Update list bullet accessible.
   */
  virtual void UpdateListBullet(nsIPresShell* aPresShell,
                                nsIContent* aHTMLListItemContent,
                                bool aHasBullet);

  virtual void NotifyOfAnchorJumpTo(nsIContent *aTarget);

  virtual void PresShellDestroyed(nsIPresShell* aPresShell);

  /**
   * Notify that presshell is activated.
   */
  virtual void PresShellActivated(nsIPresShell* aPresShell);

  virtual void RecreateAccessible(nsIPresShell* aPresShell,
                                  nsIContent* aContent);

  virtual void FireAccessibleEvent(PRUint32 aEvent, nsAccessible* aTarget);

  // nsAccessibiltiyService

  /**
   * Return true if accessibility service has been shutdown.
   */
  static bool IsShutdown() { return gIsShutdown; }

  /**
   * Return an accessible for the given DOM node from the cache or create new
   * one.
   *
   * @param  aNode             [in] the given node
   * @param  aPresShell        [in] the pres shell of the node
   * @param  aWeakShell        [in] the weak shell for the pres shell
   * @param  aIsSubtreeHidden  [out, optional] indicates whether the node's
   *                             frame and its subtree is hidden
   */
  nsAccessible* GetOrCreateAccessible(nsINode* aNode, nsIPresShell* aPresShell,
                                      nsIWeakReference* aWeakShell,
                                      bool* aIsSubtreeHidden = nsnull);

  /**
   * Return an accessible for the given DOM node.
   */
  nsAccessible* GetAccessible(nsINode* aNode);

  /**
   * Return an accessible for a DOM node in the given presshell.
   *
   * @param aNode       [in] the given node
   * @param aWeakShell  [in] the presentation shell for the given node
   */
  inline nsAccessible* GetAccessibleInWeakShell(nsINode* aNode,
                                                nsIWeakReference* aWeakShell)
  {
    // XXX: weak shell is ignored until multiple shell documents are supported.
    return GetAccessible(aNode);
  }

  /**
   * Return an accessible for the given DOM node or container accessible if
   * the node is not accessible.
   */
  nsAccessible* GetAccessibleOrContainer(nsINode* aNode,
                                         nsIWeakReference* aWeakShell);

  /**
   * Return a container accessible for the given DOM node.
   */
  inline nsAccessible* GetContainerAccessible(nsINode* aNode,
                                              nsIWeakReference* aWeakShell)
  {
    return aNode ?
      GetAccessibleOrContainer(aNode->GetNodeParent(), aWeakShell) : nsnull;
  }

private:
  // nsAccessibilityService creation is controlled by friend
  // NS_GetAccessibilityService, keep constructors private.
  nsAccessibilityService();
  nsAccessibilityService(const nsAccessibilityService&);
  nsAccessibilityService& operator =(const nsAccessibilityService&);

private:
  /**
   * Initialize accessibility service.
   */
  bool Init();

  /**
   * Shutdowns accessibility service.
   */
  void Shutdown();

  /**
   * Create accessible for the element implementing nsIAccessibleProvider
   * interface.
   */
  already_AddRefed<nsAccessible>
    CreateAccessibleByType(nsIContent* aContent, nsIWeakReference* aWeakShell);

  /**
   * Create accessible for HTML node by tag name.
   */
  already_AddRefed<nsAccessible>
    CreateHTMLAccessibleByMarkup(nsIFrame* aFrame, nsIContent* aContent,
                                 nsIWeakReference* aWeakShell);

  /**
   * Create accessible if parent is a deck frame.
   */
  already_AddRefed<nsAccessible>
    CreateAccessibleForDeckChild(nsIFrame* aFrame, nsIContent* aContent,
                                 nsIWeakReference* aWeakShell);

#ifdef MOZ_XUL
  /**
   * Create accessible for XUL tree element.
   */
  already_AddRefed<nsAccessible>
    CreateAccessibleForXULTree(nsIContent* aContent, nsIWeakReference* aWeakShell);
#endif

  /**
   * Reference for accessibility service instance.
   */
  static nsAccessibilityService* gAccessibilityService;

  /**
   * Indicates whether accessibility service was shutdown.
   */
  static bool gIsShutdown;

  /**
   * Does this content node have a universal ARIA property set on it?
   * A universal ARIA property is one that can be defined on any element even if there is no role.
   *
   * @param aContent The content node to test
   * @return true if there is a universal ARIA property set on the node
   */
  bool HasUniversalAriaProperty(nsIContent *aContent);

  friend nsAccessibilityService* GetAccService();
  friend mozilla::a11y::FocusManager* mozilla::a11y::FocusMgr();

  friend nsresult NS_GetAccessibilityService(nsIAccessibilityService** aResult);
};

/**
 * Return the accessibility service instance. (Handy global function)
 */
inline nsAccessibilityService*
GetAccService()
{
  return nsAccessibilityService::gAccessibilityService;
}

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
  "internal frame",      //ROLE_INTERNAL_FRAME
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
  "combobox option",     //ROLE_COMBOBOX_OPTION
  "image map",           //ROLE_IMAGE_MAP
  "listbox option",      //ROLE_OPTION
  "listbox rich option", //ROLE_RICH_OPTION
  "listbox",             //ROLE_LISTBOX
  "flat equation",       //ROLE_FLAT_EQUATION
  "gridcell",            //ROLE_GRID_CELL
  "embedded object",     //ROLE_EMBEDDED_OBJECT
  "note"                 //ROLE_NOTE
};

/**
 * Map nsIAccessibleEvents constants to strings. Used by
 * nsIAccessibleRetrieval::getStringEventType() method.
 */
static const char kEventTypeNames[][40] = {
  "unknown",                                 //
  "show",                                    // EVENT_SHOW
  "hide",                                    // EVENT_HIDE
  "reorder",                                 // EVENT_REORDER
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
  "document load complete",                  // EVENT_DOCUMENT_LOAD_COMPLETE
  "document reload",                         // EVENT_DOCUMENT_RELOAD
  "document load stopped",                   // EVENT_DOCUMENT_LOAD_STOPPED
  "document attributes changed",             // EVENT_DOCUMENT_ATTRIBUTES_CHANGED
  "document content changed",                // EVENT_DOCUMENT_CONTENT_CHANGED
  "property changed",                        // EVENT_PROPERTY_CHANGED
  "page changed",                           // EVENT_PAGE_CHANGED
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
};

/**
 * Map nsIAccessibleRelation constants to strings. Used by
 * nsIAccessibleRetrieval::getStringRelationType() method.
 */
static const char kRelationTypeNames[][20] = {
  "unknown",             // RELATION_NUL
  "controlled by",       // RELATION_CONTROLLED_BY
  "controller for",      // RELATION_CONTROLLER_FOR
  "label for",           // RELATION_LABEL_FOR
  "labelled by",         // RELATION_LABELLED_BY
  "member of",           // RELATION_MEMBER_OF
  "node child of",       // RELATION_NODE_CHILD_OF
  "flows to",            // RELATION_FLOWS_TO
  "flows from",          // RELATION_FLOWS_FROM
  "subwindow of",        // RELATION_SUBWINDOW_OF
  "embeds",              // RELATION_EMBEDS
  "embedded by",         // RELATION_EMBEDDED_BY
  "popup for",           // RELATION_POPUP_FOR
  "parent window of",    // RELATION_PARENT_WINDOW_OF
  "described by",        // RELATION_DESCRIBED_BY
  "description for",     // RELATION_DESCRIPTION_FOR
  "default button"       // RELATION_DEFAULT_BUTTON
};

#endif /* __nsIAccessibilityService_h__ */

