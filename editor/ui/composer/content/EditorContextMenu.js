/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Charles Manske (cmanske@netscape.com)
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

function EditorFillContextMenu(event, contextMenuNode)
{
  if ( event.target != contextMenuNode )
    return;

  // Setup object property menuitem
  var objectName = InitObjectPropertiesMenuitem("objectProperties_cm");
  var isInLink = objectName == "href";

  // Special case of an image inside a link
  if (objectName == "img")
  try {
    isInLink = GetCurrentEditor().getElementOrParentByTagName("href", GetObjectForProperties());
  } catch (e) {}

  InitRemoveStylesMenuitems("removeStylesMenuitem_cm", "removeLinksMenuitem_cm", "removeNamedAnchorsMenuitem_cm");

  var inCell = IsInTableCell();
  // Set appropriate text for join cells command
  InitJoinCellMenuitem("joinTableCells_cm");

  // Update enable states for all table commands
  goUpdateTableMenuItems(document.getElementById("composerTableMenuItems"));

  // Loop through all children to hide disabled items
  var children = contextMenuNode.childNodes;
  if (children)
  {
    var count = children.length;
    for (var i = 0; i < count; i++)
      HideDisabledItem(children[i]);
  }

  // The above loop will always show all separators and the next two items
  // Hide "Create Link" if in a link
  ShowMenuItem("createLink_cm", !isInLink);

  // Hide "Edit link in new Composer" unless in a link
  ShowMenuItem("editLink_cm", isInLink);

  // Remove separators if all items in immediate group above are hidden
  // A bit complicated to account if multiple groups are completely hidden!
  var haveUndo =
    IsMenuItemShowing("menu_undo_cm") ||
    IsMenuItemShowing("menu_redo_cm");

  var haveEdit =
    IsMenuItemShowing("menu_cut_cm")   ||
    IsMenuItemShowing("menu_copy_cm")  ||
    IsMenuItemShowing("menu_paste_cm") ||
    IsMenuItemShowing("menu_pasteNoFormatting_cm") ||
    IsMenuItemShowing("menu_delete_cm");

  var haveStyle =
    IsMenuItemShowing("removeStylesMenuitem_cm") ||
    IsMenuItemShowing("createLink_cm") ||
    IsMenuItemShowing("removeLinksMenuitem_cm") ||
    IsMenuItemShowing("removeNamedAnchorsMenuitem_cm");

  var haveProps =
    IsMenuItemShowing("objectProperties_cm");

  ShowMenuItem("undoredo-separator", haveUndo && haveEdit);

  ShowMenuItem("edit-separator", haveEdit || haveUndo);

  // Note: Item "menu_selectAll_cm" and
  // following separator are ALWAYS enabled,
  // so there will always be 1 separator here

  var showStyleSep = haveStyle && (haveProps || inCell);
  ShowMenuItem("styles-separator", showStyleSep);

  var showPropSep = (haveProps && inCell);
  ShowMenuItem("property-separator", showPropSep);

  // Remove table submenus if not in table
  ShowMenuItem("tableInsertMenu_cm",  inCell);
  ShowMenuItem("tableSelectMenu_cm",  inCell);
  ShowMenuItem("tableDeleteMenu_cm",  inCell);

  // if we have a mispelled word, show spellchecker context
  // menuitems as well as the usual context menu
  InlineSpellCheckerUI.clearSuggestionsFromMenu();
  InlineSpellCheckerUI.initFromEvent(document.popupRangeParent, document.popupRangeOffset);
  var onMisspelling = InlineSpellCheckerUI.overMisspelling;
  document.getElementById('spellCheckSuggestionsSeparator').hidden = !onMisspelling;
  document.getElementById('spellCheckAddToDictionary').hidden = !onMisspelling;
  document.getElementById('spellCheckIgnoreWord').hidden = !onMisspelling;
  var separator = document.getElementById('spellCheckAddSep');
  separator.hidden = !onMisspelling;
  document.getElementById('spellCheckNoSuggestions').hidden = !onMisspelling ||
      InlineSpellCheckerUI.addSuggestionsToMenu(contextMenuNode, separator, 5);
}

function IsItemOrCommandEnabled( item )
{
  var command = item.getAttribute("command");
  if (command) {
    // If possible, query the command controller directly
    var controller = document.commandDispatcher.getControllerForCommand(command);
    if (controller)
      return controller.isCommandEnabled(command);
  }

  // Fall back on the inefficient observed disabled attribute
  return item.getAttribute("disabled") != "true";
}

function HideDisabledItem( item )
{
  item.hidden = !IsItemOrCommandEnabled(item);
}

function ShowMenuItem(id, showItem)
{
  var item = document.getElementById(id);
  if (item && !showItem)
  {
    item.hidden = true;
  }
  // else HideDisabledItem showed the item anyway
}

function IsMenuItemShowing(menuID)
{
  var item = document.getElementById(menuID);
  if (item)
    return !item.hidden;

  return false;
}

