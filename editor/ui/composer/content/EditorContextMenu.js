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
  goUpdateCommand("cmd_delete");

  // Setup object property menuitem
  var objectName = InitObjectPropertiesMenuitem("objectProperties_cm");
  InitRemoveStylesMenuitems("removeStylesMenuitem_cm", "removeLinksMenuitem_cm");

  var inCell = IsInTableCell();
  // Set appropriate text for join cells command
  InitJoinCellMenuitem("joinTableCells_cm");

  goUpdateCommand("cmd_JoinTableCells");
  goUpdateCommand("cmd_SplitTableCell");
  goUpdateCommand("cmd_TableOrCellColor");

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
		IsMenuItemShowing("menu_clear_cm");

  var haveStyle = 
    IsMenuItemShowing("removeStylesMenuitem_cm") ||
    IsMenuItemShowing("createLink_cm") ||
    IsMenuItemShowing("removeLinksMenuitem_cm");

  var haveProps = IsMenuItemShowing("objectProperties_cm");

  ShowMenuItem("undoredo-separator", haveUndo && haveEdit);

  ShowMenuItem("edit-separator", haveEdit || haveUndo);
	             
  // Note: Item "menu_selectAll_cm" and 
  // folowing separator are ALWAYS enabled,
  // so there will always be 1 separator here

  ShowMenuItem("styles-separator", haveStyle && (haveProps || inCell));

  ShowMenuItem("property-separator", (haveProps && inCell) || !haveStyle);

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
  item.setAttribute("collapsed", enabled ? "" : "true");
  item.setAttribute('contexthidden', enabled ? "" : "true");
  return enabled;
}

function ShowHiddenItemOnCleanup( item )
{
  if (!item) return false;

  var isHidden = (item.getAttribute('contexthidden') == "true");
  if (isHidden)
  {
    item.removeAttribute("collapsed");
    item.removeAttribute('contexthidden');
    return true;
  }
  return false;
}

function ShowMenuItem(id, showItem)
{
  var item = document.getElementById(id);
  if (item)
  {
    //var showing = (item.getAttribute("collapsed") !="true");
    //if(showItem != showing ? "true" : "")
		  item.setAttribute("collapsed", showItem ? "" : "true");
  }
  else
    dump("ShowMenuItem: item id="+id+" not found\n");
}

function IsMenuItemShowing(menuID)
{
	var item = document.getElementById(menuID);
	if(item)
	{
    var show = item.getAttribute("collapsed") != "true";
		return(item.getAttribute("collapsed") !="true");
	}
	return false;
}
