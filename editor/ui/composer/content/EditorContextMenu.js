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

function fillContentContextMenu(contextMenuNode)
{
  dump("here!\n")
	contextMenu = new nsContextMenu(contextMenuNode);
  dump("herehere!\n")
  return;


	var message = GetLoadedMessage();
	var numSelected = (message) ? 1 : 0;

	var isNewsgroup = false;


	SetupNewMessageWindowMenuItem("messagePaneContext-openNewWindow", numSelected, (numSelected == 0));
	SetupEditAsNewMenuItem("messagePaneContext-editAsNew", numSelected, (numSelected == 0));
	SetupReplyToSenderMenuItem("messagePaneContext-replySender", numSelected, (numSelected == 0));
	SetupReplyToNewsgroupMenuItem("messagePaneContext-replyNewsgroup", numSelected, isNewsgroup, (numSelected == 0));
	SetupReplyAllMenuItem("messagePaneContext-replyAll" , numSelected, (numSelected == 0));
	SetupForwardMenuItem("messagePaneContext-forward", numSelected, (numSelected == 0));
	SetupMoveMenuItem("messagePaneContext-moveMenu", numSelected, isNewsgroup, (numSelected == 0));"context-copy"
	SetupCopyMenuItem("messagePaneContext-copyMenu", numSelected, (numSelected == 0));
	SetupSaveAsMenuItem("messagePaneContext-saveAs", numSelected, (numSelected == 0));
	SetupPrintMenuItem("messagePaneContext-print", numSelected, (numSelected == 0));
	SetupDeleteMenuItem("messagePaneContext-delete", numSelected, (numSelected == 0));
	SetupAddSenderToABMenuItem("messagePaneContext-addSenderToAddressBook", numSelected, (numSelected == 0));
	SetupAddAllToABMenuItem("messagePaneContext-addAllToAddressBook", numSelected, (numSelected == 0));

	//Figure out separators
	ShowMenuItem("messagePaneContext-sep-open", ShowMessagePaneOpenSeparator());
	ShowMenuItem("messagePaneContext-sep-reply", ShowMessagePaneReplySeparator());
	ShowMenuItem("messagePaneContext-sep-edit", ShowMessagePaneEditSeparator());
	ShowMenuItem("messagePaneContext-sep-addressBook", ShowMessagePaneABSeparator());
	ShowMenuItem("messagePaneContext-sep-link", ShowMessagePaneLinkSeparator());
	ShowMenuItem("messagePaneContext-sep-image", ShowMessagePaneImageSeparator());
	ShowMenuItem("messagePaneContext-sep-copy", ShowMessagePaneCopySeparator());
}

function ShowMenuItem(id, showItem)
{
	var item = document.getElementById(id);
	var showing = (item.getAttribute('hidden') !='true');
	if(item && (showItem != showing))
		item.setAttribute('hidden', showItem ? '' : 'true');
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

function SetupNewMessageWindowMenuItem(menuID, numSelected, forceHide)
{
	ShowMenuItem(menuID, (numSelected <= 1) && !forceHide);
	EnableMenuItem(menuID, true);
}

function SetupEditAsNewMenuItem(menuID, numSelected, forceHide)
{
	ShowMenuItem(menuID, (numSelected <= 1)&& !forceHide);
	EnableMenuItem(menuID, true);
}

function SetupReplyToSenderMenuItem(menuID, numSelected, forceHide)
{
	ShowMenuItem(menuID, (numSelected <= 1)&& !forceHide);
	EnableMenuItem(menuID, (numSelected == 1));
}

function SetupReplyToNewsgroupMenuItem(menuID, numSelected, isNewsgroup, forceHide)
{
	ShowMenuItem(menuID, (numSelected <= 1) && isNewsgroup && !forceHide);
	EnableMenuItem(menuID,  (numSelected == 1));
}

function SetupReplyAllMenuItem(menuID, numSelected, forceHide)
{
	ShowMenuItem(menuID, (numSelected <= 1) && !forceHide);
	EnableMenuItem(menuID, (numSelected == 1));
}

function SetupForwardMenuItem(menuID, numSelected, forceHide)
{
	ShowMenuItem(menuID, !forceHide);
	EnableMenuItem(menuID, (numSelected > 0));
}

function SetupMoveMenuItem(menuID, numSelected, isNewsgroup, forceHide)
{
	ShowMenuItem(menuID, !isNewsgroup && !forceHide);
	EnableMenuItem(menuID, (numSelected > 0));
}

function SetupCopyMenuItem(menuID, numSelected, forceHide)
{
	ShowMenuItem(menuID, !forceHide);
	EnableMenuItem(menuID, (numSelected > 0));
}

function SetupSaveAsMenuItem(menuID, numSelected, forceHide)
{
	ShowMenuItem(menuID, (numSelected <= 1) && !forceHide);
	EnableMenuItem(menuID, (numSelected == 1));
}

function SetupPrintMenuItem(menuID, numSelected, forceHide)
{
	ShowMenuItem(menuID, !forceHide);
	EnableMenuItem(menuID, (numSelected > 0));
}

function SetupDeleteMenuItem(menuID, numSelected, forceHide)
{
	ShowMenuItem(menuID, !forceHide);
	EnableMenuItem(menuID, (numSelected > 0));
}

function SetupAddSenderToABMenuItem(menuID, numSelected, forceHide)
{
	ShowMenuItem(menuID, (numSelected <= 1) && !forceHide);
	EnableMenuItem(menuID, false);
}

function SetupAddAllToABMenuItem(menuID, numSelected, forceHide)
{
	ShowMenuItem(menuID, (numSelected <= 1) && !forceHide);
	EnableMenuItem(menuID, false);
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

function IsMenuItemShowingWithStyle(menuID)
{
	var item = document.getElementById(menuID);
	if(item)
	{
		var style = item.getAttribute( "style" );
		return ( style.indexOf( "display:none;" ) == -1 )
	}
	return false;
}

function ShowMessagePaneOpenSeparator()
{
	return(IsMenuItemShowing("messagePaneContext-openNewWindow")  ||
		IsMenuItemShowingWithStyle("context-selectall") ||
		IsMenuItemShowingWithStyle("context-copy"));
}

function ShowMessagePaneReplySeparator()
{
	return (IsMenuItemShowing("messagePaneContext-replySender") ||
		IsMenuItemShowing("messagePaneContext-replyNewsgroup") ||
		IsMenuItemShowing("messagePaneContext-replyAll") ||
		IsMenuItemShowing("messagePaneContext-forward") ||
		IsMenuItemShowing("messagePaneContext-editAsNew"));
}

function ShowMessagePaneEditSeparator()
{
	return (IsMenuItemShowing("messagePaneContext-moveMenu") ||
		IsMenuItemShowing("messagePaneContext-copyMenu") ||
		IsMenuItemShowing("messagePaneContext-saveAs") ||
		IsMenuItemShowing("messagePaneContext-print") ||
		IsMenuItemShowing("messagePaneContext-delete"));
}

function ShowMessagePaneABSeparator()
{
	return (IsMenuItemShowing("messagePaneContext-addSenderToAddressBook") ||
		IsMenuItemShowing("messagePaneContext-addAllToAddressBook"));
}

function ShowMessagePaneLinkSeparator()
{
	return (IsMenuItemShowingWithStyle("context-openlink") ||
		IsMenuItemShowingWithStyle("context-editlink"));
}

function ShowMessagePaneImageSeparator()
{
	return (IsMenuItemShowingWithStyle("context-viewimage"));
}

function ShowMessagePaneCopySeparator()
{
	return (IsMenuItemShowingWithStyle("context-copylink") ||
		IsMenuItemShowingWithStyle("context-copyimage"));
}


