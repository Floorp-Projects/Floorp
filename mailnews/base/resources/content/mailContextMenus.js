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
	var tree = GetThreadTree();
	var selectedItems = tree.selectedItems;
	var numSelected = selectedItems.length;

	ShowMenuItem("threadPaneContext-openNewWindow", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-openNewWindow", false);

	ShowMenuItem("threadPaneContext-editAsNew", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-editAsNew", false);
	
	ShowMenuItem("threadPaneContext-sep-open", (numSelected <= 1));

	ShowMenuItem("threadPaneContext-replySender", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-replySender", (numSelected == 1));

	ShowMenuItem("threadPaneContext-replyAll", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-replyAll", (numSelected == 1));

	ShowMenuItem("threadPaneContext-forward", true);
	EnableMenuItem("threadPaneContext-forward", (numSelected > 0));

	ShowMenuItem("threadPaneContext-sep-reply", true);

	ShowMenuItem("threadPaneContext-moveMenu", true);
	EnableMenuItem("threadPaneContext-moveMenu", (numSelected > 0));

	ShowMenuItem("threadPaneContext-copyMenu", true);
	EnableMenuItem("threadPaneContext-copyMenu", (numSelected > 0));

	ShowMenuItem("threadPaneContext-saveAs", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-saveAs", (numSelected == 1));

	ShowMenuItem("threadPaneContext-print", true);
	EnableMenuItem("threadPaneContext-print", (numSelected > 0));

	ShowMenuItem("threadPaneContext-delete", true);
	EnableMenuItem("threadPaneContext-delete", (numSelected > 0));

	ShowMenuItem("threadPaneContext-sep-edit", (numSelected <= 1));

	ShowMenuItem("threadPaneContext-addSenderToAddressBook", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-addSenderToAddressBook", false);

	ShowMenuItem("threadPaneContext-addAllToAddressBook", (numSelected <= 1));
	EnableMenuItem("threadPaneContext-addAllToAddressBook", false);

	return(true);
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

	ShowMenuItem("folderPaneContext-sep-search", (numSelected<=1));

	SetupPropertiesMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder);
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

function SetupPropertiesMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder)
{
	ShowMenuItem("folderPaneContext-properties", (numSelected <=1));
	EnableMenuItem("folderPaneContext-properties", false);

	if(isServer)
	{
		SetMenuItemValue("folderPaneContext-properties", Bundle.GetStringFromName("accountProperties"));
	}
	else if(specialFolder == 'Inbox')
	{
		SetMenuItemValue("folderPaneContext-properties", Bundle.GetStringFromName("inboxProperties"));
	}
	else if(specialFolder == 'Trash')
	{
		SetMenuItemValue("folderPaneContext-properties", Bundle.GetStringFromName("trashProperties"));
	}
	else if(specialFolder == 'Unsent Messages')
	{
		SetMenuItemValue("folderPaneContext-properties", Bundle.GetStringFromName("unsentMessagesProperties"));
	}
	else if(serverType == 'nntp')
	{
		SetMenuItemValue("folderPaneContext-properties", Bundle.GetStringFromName("newsgroupProperties"));
	}
	else
	{
		SetMenuItemValue("folderPaneContext-properties", Bundle.GetStringFromName("folderProperties"));
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