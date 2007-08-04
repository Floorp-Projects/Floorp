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

  static nsresult GetShellFromNode(nsIDOMNode *aNode, nsIWeakReference **weakShell);
  static nsresult GetAccessibilityService(nsIAccessibilityService** aResult);

private:
  nsresult GetInfo(nsISupports* aFrame, nsIFrame** aRealFrame, nsIWeakReference** aShell, nsIDOMNode** aContent);
  void GetOwnerFor(nsIPresShell *aPresShell, nsIPresShell **aOwnerShell, nsIContent **aOwnerContent);
  nsIContent* FindContentForDocShell(nsIPresShell* aPresShell, nsIContent* aContent, nsIDocShell*  aDocShell);
  static nsAccessibilityService *gAccessibilityService;
  nsresult InitAccessible(nsIAccessible *aAccessibleIn, nsIAccessible **aAccessibleOut);

  /**
   * Return accessible object for elements implementing nsIAccessibleProvider
   * interface.
   */
  nsresult GetAccessibleByType(nsIDOMNode *aNode, nsIAccessible **aAccessible);
  PRBool HasListener(nsIContent *aContent, nsAString& aEventType);

  /**
   *  Return accessible object if parent is a deck frame
   */
  nsresult GetAccessibleForDeckChildren(nsIDOMNode *aNode, nsIAccessible **aAccessible);
};

#endif /* __nsIAccessibilityService_h__ */
