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
	if(numSelected >= 0)
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
	SetupForwardAsAttachmentMenuItem("threadPaneContext-forwardAsAttachment", numSelected, false);

	ShowMenuItem("threadPaneContext-sep-reply", true);

	SetupMoveMenuItem("threadPaneContext-moveMenu", numSelected, isNewsgroup, false);
	SetupCopyMenuItem("threadPaneContext-copyMenu", numSelected, false);
	SetupSaveAsMenuItem("threadPaneContext-saveAs", numSelected, false);
	SetupPrintMenuItem("threadPaneContext-print", numSelected, false);
	SetupDeleteMenuItem("threadPaneContext-delete", numSelected, isNewsgroup, false);
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
	ShowMenuItem(menuID,  (numSelected <= 1) && !forceHide);
	EnableMenuItem(menuID, (numSelected > 0));
}

function SetupForwardAsAttachmentMenuItem(menuID, numSelected, forceHide)
{
	ShowMenuItem(menuID,  (numSelected > 1) && !forceHide);
	EnableMenuItem(menuID, (numSelected > 1));
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

function SetupDeleteMenuItem(menuID, numSelected, isNewsgroup, forceHide)
{
	var showMenuItem = !forceHide;

	ShowMenuItem(menuID, showMenuItem);
	if(showMenuItem)
	{
		EnableMenuItem(menuID, (numSelected > 0));
		if(!isNewsgroup)
		{
			SetMenuItemValue(menuID, Bundle.GetStringFromName("delete"));
			SetMenuItemAccessKey(menuID, Bundle.GetStringFromName("deleteAccessKey"));
		}
		else
		{
			SetMenuItemValue(menuID, Bundle.GetStringFromName("cancel"));
			SetMenuItemAccessKey(menuID, Bundle.GetStringFromName("cancelAccessKey"));
		}
	}
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
	var canSubscribeToFolder = (serverType == "nntp") || (serverType == "imap");
	var canGetMessages =  isServer && (serverType != "nntp") && (serverType !="none");

	ShowMenuItem("folderPaneContext-getMessages", (numSelected <= 1) && canGetMessages);
	EnableMenuItem("folderPaneContext-getMessages", true);

	ShowMenuItem("folderPaneContext-openNewWindow", (numSelected <= 1) && !isServer);
	EnableMenuItem("folderPaneContext-openNewWindow", (true));

	SetupRenameMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder);
	SetupRemoveMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder);

	ShowMenuItem("folderPaneContext-emptyTrash", (numSelected <= 1) && (specialFolder == 'Trash'));
	EnableMenuItem("folderPaneContext-emptyTrash", true);

	ShowMenuItem("folderPaneContext-sendUnsentMessages", (numSelected <= 1) && (specialFolder == 'Unsent Messages'));
	EnableMenuItem("folderPaneContext-sendUnsentMessages", true);

	ShowMenuItem("folderPaneContext-sep-edit", (numSelected <= 1));

	SetupNewMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder);

	ShowMenuItem("folderPaneContext-subscribe", (numSelected <= 1) && canSubscribeToFolder && serverType != 'nntp');
	EnableMenuItem("folderPaneContext-subscribe", true);

	ShowMenuItem("folderPaneContext-newsSubscribe", (numSelected <= 1) && canSubscribeToFolder && isServer && serverType == 'nntp');
	EnableMenuItem("folderPaneContext-subscribe", true);

	ShowMenuItem("folderPaneContext-searchMessages", (numSelected<=1));
	EnableMenuItem("folderPaneContext-searchMessages", true);

	return(true);
}

function SetupRenameMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder)
{
	var isSpecialFolder = specialFolder != 'none';
	var isMail = serverType != 'nntp';
	var canRename = (targetFolder.getAttribute('CanRename') == "true");

	ShowMenuItem("folderPaneContext-rename", (numSelected <= 1) && !isServer && (specialFolder == "none") && canRename);
	EnableMenuItem("folderPaneContext-rename", !isServer);

	if(canRename)
	{
		SetMenuItemValue("folderPaneContext-rename", Bundle.GetStringFromName("renameFolder"));
	}
}

function SetupRemoveMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder)
{
	var isMail = serverType != 'nntp';
	var isSpecialFolder = specialFolder != "none";
	//Can't currently delete Accounts or special folders.
	var showRemove = (numSelected <=1) && (isMail && !isSpecialFolder) && !isServer;


	ShowMenuItem("folderPaneContext-remove", showRemove);
	EnableMenuItem("folderPaneContext-remove", true);

	if(isMail && !isSpecialFolder)
	{
		SetMenuItemValue("folderPaneContext-remove", Bundle.GetStringFromName("removeFolder"));
	}
}

function SetupNewMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder)
{
	var canCreateNew = targetFolder.getAttribute('CanCreateSubfolders') == 'true';
	var isInbox = specialFolder == "Inbox";

	var showNew = ((numSelected <=1) && (serverType != 'nntp') && canCreateNew) || isInbox;
	ShowMenuItem("folderPaneContext-new", showNew);
	EnableMenuItem("folderPaneContext-new", true);
	if(showNew)
	{
		if(isServer || isInbox)
			SetMenuItemValue("folderPaneContext-new", Bundle.GetStringFromName("newFolder"));
		else
			SetMenuItemValue("folderPaneContext-new", Bundle.GetStringFromName("newSubfolder"));
	}

}

function ShowMenuItem(id, showItem)
{
	var item = document.getElementById(id);
	if(item)
	{
		var showing = (item.getAttribute('hidden') !='true');
		if(showItem != showing)
			item.setAttribute('hidden', showItem ? '' : 'true');
	}
}

function EnableMenuItem(id, enableItem)
{
	var item = document.getElementById(id);
	if(item)
	{
		var enabled = (item.getAttribute('disabled') !='true');
		if(enableItem != enabled)
		{
			item.setAttribute('disabled', enableItem ? '' : 'true');
		}
	}
}

function SetMenuItemValue(id, value)
{
	var item = document.getElementById(id);
	if(item)
		item.setAttribute('value', value);

}

function SetMenuItemAccessKey(id, accessKey)
{
	var item = document.getElementById(id);
	if(item)
		item.setAttribute('accesskey', accessKey);

}

function fillMessagePaneContextMenu(contextMenuNode)
{
	contextMenu = new nsContextMenu(contextMenuNode);

	var message = GetLoadedMessage();
	var numSelected = (message) ? 1 : 0;

	var isNewsgroup = false;

	if(numSelected == 1)
		isNewsgroup = GetMessageType(message) == "news";

	var hideMailItems = AreBrowserItemsShowing();

	SetupEditAsNewMenuItem("messagePaneContext-editAsNew", numSelected, (numSelected == 0 || hideMailItems));
	SetupReplyToSenderMenuItem("messagePaneContext-replySender", numSelected, (numSelected == 0 || hideMailItems));
	SetupReplyToNewsgroupMenuItem("messagePaneContext-replyNewsgroup", numSelected, isNewsgroup, (numSelected == 0 || hideMailItems));
	SetupReplyAllMenuItem("messagePaneContext-replyAll" , numSelected, (numSelected == 0 || hideMailItems));
	SetupForwardMenuItem("messagePaneContext-forward", numSelected, (numSelected == 0 || hideMailItems));
	SetupForwardAsAttachmentMenuItem("threadPaneContext-forwardAsAttachment", numSelected, hideMailItems);
	SetupMoveMenuItem("messagePaneContext-moveMenu", numSelected, isNewsgroup, (numSelected == 0 || hideMailItems));
	SetupCopyMenuItem("messagePaneContext-copyMenu", numSelected, (numSelected == 0 || hideMailItems));
	SetupSaveAsMenuItem("messagePaneContext-saveAs", numSelected, (numSelected == 0 || hideMailItems));
	SetupPrintMenuItem("messagePaneContext-print", numSelected, (numSelected == 0 || hideMailItems));
	SetupDeleteMenuItem("messagePaneContext-delete", numSelected, isNewsgroup, (numSelected == 0 || hideMailItems));
	SetupAddSenderToABMenuItem("messagePaneContext-addSenderToAddressBook", numSelected, (numSelected == 0 || hideMailItems));
	SetupAddAllToABMenuItem("messagePaneContext-addAllToAddressBook", numSelected, (numSelected == 0 || hideMailItems));

	//Figure out separators
	ShowMenuItem("messagePaneContext-sep-open", ShowMessagePaneOpenSeparator());
	ShowMenuItem("messagePaneContext-sep-reply", ShowMessagePaneReplySeparator());
	ShowMenuItem("messagePaneContext-sep-edit", ShowMessagePaneEditSeparator());
	ShowMenuItem("messagePaneContext-sep-link", ShowMessagePaneLinkSeparator());
	ShowMenuItem("messagePaneContext-sep-image", ShowMessagePaneImageSeparator());
	ShowMenuItem("messagePaneContext-sep-copy", ShowMessagePaneCopySeparator());
}

function AreBrowserItemsShowing()
{
	return(IsMenuItemShowingWithStyle("context-openlink") ||
		IsMenuItemShowingWithStyle("context-editlink") ||
		IsMenuItemShowingWithStyle("context-viewimage") ||
		IsMenuItemShowingWithStyle("context-copylink") ||
		IsMenuItemShowingWithStyle("context-copyimage") ||
		IsMenuItemShowingWithStyle("context-savelink") ||
		IsMenuItemShowingWithStyle("context-saveimage") ||
		IsMenuItemShowingWithStyle("context-bookmarklink"));
}

function ShowMessagePaneOpenSeparator()
{
	return(IsMenuItemShowingWithStyle("context-selectall") ||
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




