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

 function fillThreadPaneContextMenu()
{
	var selectedMessages = GetSelectedMessages();
	var numSelected = selectedMessages ? selectedMessages.length : 0;

	var isNewsgroup = false;
	var selectedMessage = null;
	if(numSelected == 1)
	{
		selectedMessage = selectedMessages[0];
		isNewsgroup = GetMessageType(selectedMessage) == "news";
	}


	SetupNewMessageWindowMenuItem("threadPaneContext-openNewWindow", numSelected, false);
	SetupEditAsNewMenuItem("threadPaneContext-editAsNew", numSelected, false);

	ShowMenuItem("threadPaneContext-sep-open", (numSelected <= 1));

	SetupReplyToSenderMenuItem("threadPaneContext-replySender", numSelected, false);
	SetupReplyToNewsgroupMenuItem("threadPaneContext-replyNewsgroup", numSelected, isNewsgroup, false);
	SetupReplyAllMenuItem("threadPaneContext-replyAll", numSelected, false);
	SetupForwardMenuItem("threadPaneContext-forward", numSelected, false);

	ShowMenuItem("threadPaneContext-sep-reply", true);

	SetupMoveMenuItem("threadPaneContext-moveMenu", numSelected, isNewsgroup, false);
	SetupCopyMenuItem("threadPaneContext-copyMenu", numSelected, false);
	SetupSaveAsMenuItem("threadPaneContext-saveAs", numSelected, false);
	SetupPrintMenuItem("threadPaneContext-print", numSelected, false);
	SetupDeleteMenuItem("threadPaneContext-delete", numSelected, false);
	SetupAddSenderToABMenuItem("threadPaneContext-addSenderToAddressBook", numSelected, false);
	SetupAddAllToABMenuItem("threadPaneContext-addAllToAddressBook", numSelected, false);


	ShowMenuItem("threadPaneContext-sep-edit", (numSelected <= 1));

	return(true);
}

function GetMessageType(message)
{
	var compositeDataSource = GetCompositeDataSource("MessageProperty");
	var messageResource = message.QueryInterface(Components.interfaces.nsIRDFResource);
	if(messageResource && compositeDataSource)
	{
		var property =
			RDF.GetResource('http://home.netscape.com/NC-rdf#MessageType');
		if (!property) return null;
		var result = compositeDataSource.GetTarget(messageResource, property , true);
		if (!result) return null;
		result = result.QueryInterface(Components.interfaces.nsIRDFLiteral);
		if (!result) return null;
		return result.Value;
	}

	return null;

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

function fillFolderPaneContextMenu()
{
	var tree = GetFolderTree();
	var selectedItems = tree.selectedItems;
	var numSelected = selectedItems.length;

    var popupNode = document.getElementById('folderPaneContext');

	var targetFolder = document.popupNode.parentNode.parentNode;
	if (targetFolder.getAttribute('selected') != 'true')
	{
      tree.selectItem(targetFolder);
    }

	var isServer = targetFolder.getAttribute('IsServer') == 'true';
	var serverType = targetFolder.getAttribute('ServerType');
	var specialFolder = targetFolder.getAttribute('SpecialFolder');

	ShowMenuItem("folderPaneContext-getMessages", (numSelected <= 1) && (isServer && (serverType != 'nntp')));
	EnableMenuItem("folderPaneContext-getMessages", true);

	ShowMenuItem("folderPaneContext-openNewWindow", (numSelected <= 1) && !isServer);
	EnableMenuItem("folderPaneContext-openNewWindow", (true));

	SetupRenameMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder);
	SetupRemoveMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder);

	ShowMenuItem("folderPaneContext-emptyTrash", (numSelected <= 1) && (specialFolder == 'Trash'));
	EnableMenuItem("folderPaneContext-emptyTrash", true);

	ShowMenuItem("folderPaneContext-sendUnsentMessages", (numSelected <= 1) && (specialFolder == 'Unsent Messages'));
	EnableMenuItem("folderPaneContext-sendUnsentMessages", true);

	ShowMenuItem("folderPaneContext-unsubscribe", (numSelected <= 1) && ((serverType == 'nntp') && !isServer));
	EnableMenuItem("folderPaneContext-unsubscribe", false);

	ShowMenuItem("folderPaneContext-markFolderRead", (numSelected <= 1) && ((serverType == 'nntp') && !isServer));
	EnableMenuItem("folderPaneContext-markFolderRead", false);

	ShowMenuItem("folderPaneContext-sep-edit", (numSelected <= 1));

	SetupNewMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder);

	ShowMenuItem("folderPaneContext-subscribe", (numSelected <= 1) && (serverType == 'nntp'));
	EnableMenuItem("folderPaneContext-subscribe", true);

	ShowMenuItem("folderPaneContext-sep-new", ((numSelected<=1) && (specialFolder != "Unsent Messages")));

	ShowMenuItem("folderPaneContext-searchMessages", (numSelected<=1));
	EnableMenuItem("folderPaneContext-searchMessages", false);

	return(true);
}

function SetupRenameMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder)
{
	var isSpecialFolder = specialFolder != 'none';
	var isMail = serverType != 'nntp';
	var canRename = (targetFolder.getAttribute('CanRename') == "true");

	ShowMenuItem("folderPaneContext-rename", (numSelected <= 1) && (isServer || canRename));
	EnableMenuItem("folderPaneContext-rename", !isServer);

	if(isServer)
	{
		if(isMail)
		{
			SetMenuItemValue("folderPaneContext-rename", Bundle.GetStringFromName("renameAccount"));
		}
		else
		{
			SetMenuItemValue("folderPaneContext-rename", Bundle.GetStringFromName("renameNewsAccount"));
		}
	}
	else if(canRename)
	{
		SetMenuItemValue("folderPaneContext-rename", Bundle.GetStringFromName("renameFolder"));
	}
}

function SetupRemoveMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder)
{
	var isMail = serverType != 'nntp';
	var isInbox = specialFolder == "Inbox";
	var isTrash = specialFolder == "Trash";
	var isUnsent = specialFolder == "Unsent Messages";
	var showRemove = (numSelected <=1) && (isServer || (isMail && (!(isInbox || isTrash || isUnsent))));


	ShowMenuItem("folderPaneContext-remove", showRemove);
	EnableMenuItem("folderPaneContext-remove", false);

	if(isServer)
	{
		if(isMail)
		{
			SetMenuItemValue("folderPaneContext-remove", Bundle.GetStringFromName("removeAccount"));
		}
		else
		{
			SetMenuItemValue("folderPaneContext-remove", Bundle.GetStringFromName("removeNewsAccount"));
		}
	}
	else if(isMail && !(isInbox || isTrash || isUnsent))
	{
		SetMenuItemValue("folderPaneContext-remove", Bundle.GetStringFromName("removeFolder"));
	}
}

function SetupNewMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder)
{
	var canCreateNew = targetFolder.getAttribute('CanCreateSubfolders') == 'true';
	var showNew = (numSelected <=1) && (serverType != 'nntp') && canCreateNew;
	ShowMenuItem("folderPaneContext-new", showNew);
	EnableMenuItem("folderPaneContext-new", true);
	if(showNew)
	{
		if(isServer)
			SetMenuItemValue("folderPaneContext-new", Bundle.GetStringFromName("newFolder"));
		else
			SetMenuItemValue("folderPaneContext-new", Bundle.GetStringFromName("newSubfolder"));
	}

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

function SetMenuItemValue(id, value)
{
	var item = document.getElementById(id);
	if(item)
		item.setAttribute('value', value);

}


function fillMessagePaneContextMenu(contextMenuNode)
{
	contextMenu = new nsContextMenu(contextMenuNode);

	var message = GetLoadedMessage();
	var numSelected = (message) ? 1 : 0;

	var isNewsgroup = false;

	if(numSelected == 1)
		isNewsgroup = GetMessageType(message) == "news";


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


