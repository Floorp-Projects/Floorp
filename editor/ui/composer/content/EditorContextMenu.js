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

function fillContentContextMenu(event, contextMenuNode)
{
  if ( event.target != contextMenuNode )
    return;
   
	contextMenu = new nsContextMenu(contextMenuNode);

  HideDisabledItem("menu_undo_cm");
  HideDisabledItem("menu_redo_cm");
  HideDisabledItem("menu_cut_cm");
  HideDisabledItem("menu_copy_cm");
  HideDisabledItem("menu_paste_cm");
  HideDisabledItem("menu_delete_cm");
  HideDisabledItem("menu_selectAll_cm");
  HideDisabledItem("removeLinksMenuitem");

  ShowMenuItem("undoredo-separator", ShowUndoRedoSeparator());
  ShowMenuItem("menu_edit-separator", ShowCCPSeparator());
  ShowMenuItem("menu_selectAll-separator", ShowSelectAllSeparator());
  ShowMenuItem("menu_link-separator", ShowLinkSeparator());
  ShowMenuItem("tableMenu-separator", ShowTableMenuSeparator());
}

function cleanupContextMenu( event, contextMenuNode )
{
  if ( event.target != contextMenuNode )
    return;

  ShowHiddenItemOnCleanup("menu_undo_cm");
  ShowHiddenItemOnCleanup("menu_redo_cm");
  ShowHiddenItemOnCleanup("menu_cut_cm");
  ShowHiddenItemOnCleanup("menu_copy_cm");
  ShowHiddenItemOnCleanup("menu_paste_cm");
  ShowHiddenItemOnCleanup("menu_delete_cm");
  ShowHiddenItemOnCleanup("menu_selectAll_cm");
  ShowHiddenItemOnCleanup("removeLinksMenuitem");
}

function HideDisabledItem( id )
{
  var item = document.getElementById(id);
  if (!item)
    return false;

  var enabled = (item.getAttribute('disabled') !='true');
  if (!enabled)
  {
    item.setAttribute('hidden', 'true');
    item.setAttribute('contexthidden', 'true');
    return true;
  }
  
  return false;
}

function ShowHiddenItemOnCleanup( id )
{
  var item = document.getElementById(id);
  if (!item)
    return false;

  var isHidden = (item.getAttribute('contexthidden') == 'true');
  if (isHidden)
  {
    item.removeAttribute('hidden');
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
	  var showing = (item.getAttribute('hidden') !='true');
	  if(showItem != showing)
		  item.setAttribute('hidden', showItem ? '' : 'true');
  }
  else
    dump("ShowMenuItem: item id="+id+" not found\n");
}

function IsMenuItemShowing(menuID)
{
	var item = document.getElementById(menuID);
	if(item)
	{
		return(item.getAttribute('hidden') !='true');
	}
	return false;
}

function ShowUndoRedoSeparator()
{
	return(IsMenuItemShowing("menu_undo_cm")
		  || IsMenuItemShowing("menu_redo_cm")
		  );
}

function ShowCCPSeparator()
{
	return(IsMenuItemShowing("menu_cut_cm")
		  || IsMenuItemShowing("menu_copy_cm")
		  || IsMenuItemShowing("menu_paste_cm")
		  || IsMenuItemShowing("menu_delete_cm")
		  );
}

function ShowSelectAllSeparator()
{
	return(IsMenuItemShowing("menu_selectAll_cm")
		  );
}

function ShowLinkSeparator()
{
	return(IsMenuItemShowing("menu_preview_cm")
		  || IsMenuItemShowing("removeLinksMenuitem_cm")
		  );
}

function ShowTableMenuSeparator()
{
	return(IsMenuItemShowing("tableMenu_cm")
		  || IsMenuItemShowing("tableProperties_cm")
		  );
}

function EnableMenuItem(id, enableItem)
{
	var item = document.getElementById(id);
	var enabled = (item.getAttribute('disabled') !='true');
	if(item && (enableItem != enabled))
	{
		item.setAttribute('disabled', enableItem ? '' : 'true');
	}
}

function SetupCopyMenuItem(menuID, numSelected, forceHide)
{
	ShowMenuItem(menuID, !forceHide);
	EnableMenuItem(menuID, (numSelected > 0));
}
