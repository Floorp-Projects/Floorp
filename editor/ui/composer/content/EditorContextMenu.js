/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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

  // The 'Save image (imagename)' menuitem:
  var isImage = (objectName == "img");
  ShowMenuItem("menu_saveImage_cm", isImage);

  // Remove separators if all items in immediate group above are hidden
  // A bit complicated to account if multiple groups are completely hidden!
  var haveUndo =
    IsMenuItemShowing("menu_undo_cm") ||
    IsMenuItemShowing("menu_redo_cm");

  var haveEdit =
    IsMenuItemShowing("menu_cut_cm")   ||
    IsMenuItemShowing("menu_copy_cm")  ||
    IsMenuItemShowing("menu_paste_cm") ||
    IsMenuItemShowing("menu_delete_cm");

  var haveStyle =
    IsMenuItemShowing("removeStylesMenuitem_cm") ||
    IsMenuItemShowing("createLink_cm") ||
    IsMenuItemShowing("removeLinksMenuitem_cm");

  var havePropsOrImage =
    IsMenuItemShowing("objectProperties_cm") ||
    IsMenuItemShowing("menu_saveImage_cm")

  ShowMenuItem("undoredo-separator", haveUndo && haveEdit);

  ShowMenuItem("edit-separator", haveEdit || haveUndo);

  if (isImage)   //we have an image
  {
     var saveImageMenuItem= document.getElementById("menu_saveImage_cm");

     var imagePtr = window.editorShell.GetSelectedElement(objectName);
     var imageName = extractFileNameFromUrl(imagePtr.getAttribute("src"));

     var menutext = window.editorShell.GetString("SaveImageAs").replace(/%NAME%/,imageName);

     saveImageMenuItem.setAttribute('label',menutext);

     var onCommand = "savePage('"+ imagePtr.getAttribute("src") + "',true)";
     saveImageMenuItem.setAttribute('oncommand',onCommand);
  }

  // Note: Item "menu_selectAll_cm" and
  // following separator are ALWAYS enabled,
  // so there will always be 1 separator here

  var showStyleSep = haveStyle && (havePropsOrImage || inCell);
  ShowMenuItem("styles-separator", showStyleSep);

  var showPropSep = (havePropsOrImage && inCell) || !haveStyle;
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
