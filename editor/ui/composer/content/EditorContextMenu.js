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

  goUpdateCommand("cmd_undo");
  goUpdateCommand("cmd_redo");
  goUpdateCommand("cmd_cut");
  goUpdateCommand("cmd_copy");
  goUpdateCommand("cmd_paste");
  goUpdateCommand("cmd_pasteNoFormatting");
  goUpdateCommand("cmd_delete");
  goUpdateCommand("cmd_link");

  // Setup object property menuitem
  var objectName = InitObjectPropertiesMenuitem("objectProperties_cm");
  var isInLink = objectName == "href";

  // Special case of an image inside a link
  if (objectName == "img")
    isInLink = gEditor.getElementOrParentByTagName("href", GetObjectForProperties());

  // Disable "Create Link" if in a link
  SetElementEnabledById("createLink_cm", !isInLink);

  // Enable "Edit link in new Composer" if in a link
  SetElementEnabledById("editLink_cm", isInLink);

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
      HideDisabledItem(children.item(i));
  }

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

function EditorCleanupContextMenu( event, contextMenuNode )
{
  if ( event.target != contextMenuNode )
    return;

  var children = contextMenuNode.childNodes;
  if (children)
  {
    var count = children.length;
    for (var i = 0; i < count; i++)
    ShowHiddenItemOnCleanup(children.item(i));
  }
}

function HideDisabledItem( item )
{
  if (!item) return false;

  var enabled = (item.getAttribute('disabled') !="true");
  item.setAttribute("hidden", enabled ? "" : "true");
  item.setAttribute("contexthidden", enabled ? "" : "true");
  return enabled;
}

function ShowHiddenItemOnCleanup( item )
{
  if (!item) return false;

  var isHidden = (item.getAttribute("contexthidden") == "true");
  if (isHidden)
  {
    item.removeAttribute("hidden");
    item.removeAttribute("contexthidden");
    return true;
  }
  return false;
}

function ShowMenuItem(id, showItem)
{
  var item = document.getElementById(id);
  if (item)
  {
    item.setAttribute("hidden", showItem ? "" : "true");
    item.setAttribute("contexthidden", showItem ? "" : "true");
  }
  else
    dump("ShowMenuItem: item id="+id+" not found\n");
}

function IsMenuItemShowing(menuID)
{
  var item = document.getElementById(menuID);
  if(item)
    return(item.getAttribute("contexthidden") != "true");

  return false;
}

