/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Charles Manske (cmanske@netscape.com)
 */

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

