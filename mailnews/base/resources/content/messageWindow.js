/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

/* This is where functions related to the standalone message window are kept */

/* globals for a particular window */

var compositeDataSourceContractID        = datasourceContractIDPrefix + "composite-datasource";

var gCompositeDataSource;
var gCurrentMessageUri;
var gCurrentFolderUri;

var gCurrentMessageIsDeleted = false;

// the folderListener object
var folderListener = {
    OnItemAdded: function(parentItem, item, view) {},

	OnItemRemoved: function(parentItem, item, view)
	{
		var parentFolderResource = parentItem.QueryInterface(Components.interfaces.nsIRDFResource);
		if(!parentFolderResource)
			return;

		var parentURI = parentFolderResource.Value;
		if(parentURI != gCurrentFolderUri)
			return;

		var deletedMessageResource = item.QueryInterface(Components.interfaces.nsIRDFResource);
		var deletedUri = deletedMessageResource.Value;

		//If the deleted message is our message then we know we're about to be deleted.
		if(deletedUri == gCurrentMessageUri)
		{
			gCurrentMessageIsDeleted = true;
		}

	},

	OnItemPropertyChanged: function(item, property, oldValue, newValue) {},

	OnItemIntPropertyChanged: function(item, property, oldValue, newValue)
	{
	},

	OnItemBoolPropertyChanged: function(item, property, oldValue, newValue) {},

    OnItemUnicharPropertyChanged: function(item, property, oldValue, newValue){},
	OnItemPropertyFlagChanged: function(item, property, oldFlag, newFlag) {},

    OnItemEvent: function(folder, event) {
		if (event.GetUnicode() == "DeleteOrMoveMsgCompleted") {
			HandleDeleteOrMoveMsgCompleted(folder);
		}     
                else if (event.GetUnicode() == "DeleteOrMoveMsgFailed") {
                        HandleDeleteOrMoveMsgFailed(folder);
                }
    }
}

function HandleDeleteOrMoveMsgCompleted(folder)
{
	dump("In HandleDeleteOrMoveMsgCompleted\n");
	var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
	if(!folderResource)
		return;

	var folderUri = folderResource.Value;
	if((folderUri == gCurrentFolderUri) && gCurrentMessageIsDeleted)
	{
		//If we knew we were going to be deleted and the deletion has finished, close the window.
		gCurrentMessageIsDeleted = false;
		//Use timeout to make sure all folder listeners get event before removing them.  Messes up
		//folder listener iterator if we don't do this.
		setTimeout("window.close();",0);

	}
}

function HandleDeleteOrMoveMsgFailed(folder)
{
        var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
        if(!folderResource)
                return;

        var folderUri = folderResource.Value;
        if((folderUri == gCurrentFolderUri) && gCurrentMessageIsDeleted)
        {
                gCurrentMessageIsDeleted = false;
        }	
}

function OnLoadMessageWindow()
{
	HideMenus();
	HideToolbarButtons();
	CreateMailWindowGlobals();
	CreateMessageWindowGlobals();
	verifyAccounts();

	InitMsgWindow();

	messenger.SetWindow(window, msgWindow);
	InitializeDataSources();
	// FIX ME - later we will be able to use onload from the overlay
	OnLoadMsgHeaderPane();

    try {
        mailSession.AddFolderListener(folderListener);
	} catch (ex) {
        dump("Error adding to session\n");
    }

	if(window.arguments && window.arguments.length == 2)
	{
		if(window.arguments[0])
		{
			gCurrentMessageUri = window.arguments[0];
		}
		else
		{
			gCurrentMessageUri = null;
		}

		if(window.arguments[1])
		{
			gCurrentFolderUri = window.arguments[1];
		}
		else
		{
			gCurrentFolderUri = null;
		}
	}	

  setTimeout("OpenURL(gCurrentMessageUri);", 0);
  SetupCommandUpdateHandlers();

}

function HideMenus()
{
	var message_menuitem=document.getElementById('menu_showMessage');
	if(message_menuitem)
		message_menuitem.setAttribute("hidden", "true");

	var expandOrCollapseMenu = document.getElementById('menu_expandOrCollapse');
	if(expandOrCollapseMenu)
		expandOrCollapseMenu.setAttribute("hidden", "true");

	var renameFolderMenu = document.getElementById('menu_renameFolder');
	if(renameFolderMenu)
		renameFolderMenu.setAttribute("hidden", "true");

	var viewMessagesMenu = document.getElementById('viewMessagesMenu');
	if(viewMessagesMenu)
		viewMessagesMenu.setAttribute("hidden", "true");

	var openMessageMenu = document.getElementById('openMessageWindowMenuitem');
	if(openMessageMenu)
		openMessageMenu.setAttribute("hidden", "true");

	var viewSortMenu = document.getElementById('viewSortMenu');
	if(viewSortMenu)
		viewSortMenu.setAttribute("hidden", "true");

	var viewThreadedMenu = document.getElementById('menu_showThreads');
	if(viewThreadedMenu)
		viewThreadedMenu.setAttribute("hidden", "true");

	var goNextMenu = document.getElementById('goNextMenu');
	if(goNextMenu)
		goNextMenu.setAttribute("hidden", "true");

	var goPreviousMenu = document.getElementById('goPreviousMenu');
	if(goPreviousMenu)
		goPreviousMenu.setAttribute("hidden", "true");

	var goNextSeparator = document.getElementById('goNextSeparator');
	if(goNextSeparator)
		goNextSeparator.setAttribute("hidden", "true");

	var emptryTrashMenu = document.getElementById('menu_emptyTrash');
	if(emptryTrashMenu)
		emptryTrashMenu.setAttribute("hidden", "true");

	var compactFolderMenu = document.getElementById('menu_compactFolder');
	if(compactFolderMenu)
		compactFolderMenu.setAttribute("hidden", "true");

	var trashSeparator = document.getElementById('trashMenuSeparator');
	if(trashSeparator)
		trashSeparator.setAttribute("hidden", "true");

}

function HideToolbarButtons()
{
	var nextButton = document.getElementById('button-next');
	if(nextButton)
		nextButton.setAttribute("hidden", "true");

}

function OnUnloadMessageWindow()
{
	OnMailWindowUnload();
}

function CreateMessageWindowGlobals()
{
	gCompositeDataSource = Components.classes[compositeDataSourceContractID].createInstance();
	gCompositeDataSource = gCompositeDataSource.QueryInterface(Components.interfaces.nsIRDFCompositeDataSource);

}

function InitializeDataSources()
{
	AddDataSources();
	//Now add datasources to composite datasource
	gCompositeDataSource.AddDataSource(accountManagerDataSource);
    gCompositeDataSource.AddDataSource(folderDataSource);
	gCompositeDataSource.AddDataSource(messageDataSource);
}

function GetSelectedMsgFolders()
{
	var folderArray = new Array(1);
	var msgFolder = GetLoadedMsgFolder();
	if(msgFolder)
	{
		folderArray[0] = msgFolder;	
	}
	return folderArray;
}

function GetSelectedMessage(index)
{
	return GetLoadedMessage();
}

function GetNumSelectedMessages()
{
	if(gCurrentMessageUri)
		return 1;
	else
		return 0;
}

function GetSelectedMessages()
{
	var messageArray = new Array(1);
	var message = GetLoadedMessage();
	if(message)
	{
		messageArray[0] = message;	
	}
	return messageArray;
}

function GetLoadedMsgFolder()
{
	if(gCurrentFolderUri)
	{
		var folderResource = RDF.GetResource(gCurrentFolderUri);
		if(folderResource)
		{
			var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
			return msgFolder;
		}
	}
	return null;
}

function GetLoadedMessage()
{
	if(gCurrentMessageUri)
	{
		var messageResource = RDF.GetResource(gCurrentMessageUri);
		if(messageResource)
		{
			var message = messageResource.QueryInterface(Components.interfaces.nsIMessage);
			return message;
		}
	}
	return null;

}

//Clear everything related to the current message. called after load start page.
function ClearMessageSelection()
{
	gCurrentMessageUri = null;
	gCurrentFolderUri = null;
	CommandUpdate_Mail();
}

function GetCompositeDataSource(command)
{
	return gCompositeDataSource;	
}

//Sets the next message after a delete.  If useSelection is true then use the
//current selection to determine this.  Otherwise use messagesToCheck which will
//be an array of nsIMessage's.
function SetNextMessageAfterDelete(messagesToCheck, useSelection)
{
	gCurrentMessageIsDeleted = true;
}

function SelectFolder(folderUri)
{
	gCurrentFolderUri = folderUri;
}

function SelectMessage(messageUri)
{
	gCurrentMessageUri = messageUri;
	OpenURL(gCurrentMessageUri);
}

function ReloadMessage()
{
	OpenURL(gCurrentMessageUri);
}

// MessageWindowController object (handles commands when one of the trees does not have focus)
var MessageWindowController =
{
   supportsCommand: function(command)
	{

		switch ( command )
		{
			case "cmd_reply":
			case "button_reply":
			case "cmd_replySender":
			case "cmd_replyGroup":
			case "cmd_replyall":
			case "button_replyall":
			case "cmd_forward":
			case "button_forward":
			case "cmd_forwardInline":
			case "cmd_forwardAttachment":
			case "cmd_editAsNew":
			case "cmd_delete":
			case "button_delete":
			case "cmd_shiftDelete":
			case "cmd_print":
			case "cmd_saveAsFile":
			case "cmd_saveAsTemplate":
			case "cmd_viewPageSource":
			case "cmd_reload":
			case "cmd_getNewMessages":
			case "cmd_getNextNMessages":
			case "cmd_find":
			case "cmd_findAgain":
			case "cmd_markAsRead":
			case "cmd_markAllRead":
			case "cmd_markThreadAsRead":
			case "cmd_markAsFlagged":
			case "cmd_file":
				return true;
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
		switch ( command )
		{
			case "cmd_reply":
			case "button_reply":
			case "cmd_replySender":
			case "cmd_replyGroup":
			case "cmd_replyall":
			case "button_replyall":
			case "cmd_forward":
			case "button_forward":
			case "cmd_forwardInline":
			case "cmd_forwardAttachment":
			case "cmd_editAsNew":
			case "cmd_delete":
			case "button_delete":
			case "cmd_shiftDelete":
			case "cmd_print":
			case "cmd_saveAsFile":
			case "cmd_saveAsTemplate":
			case "cmd_viewPageSource":
			case "cmd_reload":
			case "cmd_find":
			case "cmd_findAgain":
			case "cmd_markAsRead":
			case "cmd_markAllRead":
			case "cmd_markThreadAsRead":
			case "cmd_markAsFlagged":
			case "cmd_file":
				if ( command == "cmd_delete")
				{
					goSetMenuValue(command, 'valueMessage');
				}
				return ( gCurrentMessageUri != null);
			case "cmd_getNewMessages":
				return IsGetNewMessagesEnabled();
			case "cmd_getNextNMessages":
				return IsGetNextNMessagesEnabled();
			default:
				return false;
		}
	},

	doCommand: function(command)
	{
   		//dump("MessageWindowController.doCommand(" + command + ")\n");

		switch ( command )
		{
			case "cmd_getNewMessages":
				MsgGetMessage();
				break;
			case "cmd_getNextNMessages":
				MsgGetNextNMessages();
				break;
			case "cmd_reply":
				MsgReplyMessage(null);
				break;
			case "cmd_replySender":
				MsgReplySender(null);
				break;
			case "cmd_replyGroup":
				MsgReplyGroup(null);
				break;
			case "cmd_replyall":
				MsgReplyToAllMessage(null);
				break;
			case "cmd_forward":
				MsgForwardMessage(null);
				break;
			case "cmd_forwardInline":
				MsgForwardAsInline(null);
				break;
			case "cmd_forwardAttachment":
				MsgForwardAsAttachment(null);
				break;
			case "cmd_editAsNew":
				MsgEditMessageAsNew();
				break;
			case "cmd_delete":
				MsgDeleteMessage(false, false);
				break;
			case "cmd_shiftDelete":
				MsgDeleteMessage(true, false);
				break;
			case "button_delete":
				MsgDeleteMessage(false, true);
				break;
			case "cmd_print":
				PrintEnginePrint();
				break;
			case "cmd_saveAsFile":
				MsgSaveAsFile();
				break;
			case "cmd_saveAsTemplate":
				MsgSaveAsTemplate();
				break;
			case "cmd_viewPageSource":
				MsgViewPageSource();
				break;
			case "cmd_reload":
				MsgReload();
				break;
			case "cmd_find":
				MsgFind();
				break;
			case "cmd_findAgain":
				MsgFindAgain();
				break;
			case "cmd_markAsRead":
				MsgMarkMsgAsRead(null);
				return;
			case "cmd_markThreadAsRead":
				MsgMarkThreadAsRead();
				return;
			case "cmd_markAllRead":
				MsgMarkAllRead();
				return;
			case "cmd_markAsFlagged":
				MsgMarkAsFlagged(null);
				return;
		}
	},
	
	onEvent: function(event)
	{
	}
};


function CommandUpdate_Mail()
{
	goUpdateCommand('cmd_reply');
	goUpdateCommand('cmd_replySender');
	goUpdateCommand('cmd_replyGroup');
	goUpdateCommand('cmd_replyall');
	goUpdateCommand('cmd_forward');
	goUpdateCommand('cmd_forwardInline');
	goUpdateCommand('cmd_forwardAttachment');
	goUpdateCommand('button_reply');
	goUpdateCommand('button_replyall');
	goUpdateCommand('button_forward');
	goUpdateCommand('cmd_editAsNew');
	goUpdateCommand('cmd_delete');
	goUpdateCommand('button_delete');
	goUpdateCommand('cmd_shiftDelete');
	goUpdateCommand('cmd_print');
	goUpdateCommand('cmd_saveAsFile');
	goUpdateCommand('cmd_saveAsTemplate');
	goUpdateCommand('cmd_viewPageSource');
	goUpdateCommand('cmd_reload');
	goUpdateCommand('cmd_getNewMessages');
	goUpdateCommand('cmd_getNextNMessages');
	goUpdateCommand('cmd_find');
	goUpdateCommand('cmd_findAgain');
	goUpdateCommand('cmd_markAsRead');
	goUpdateCommand('cmd_markThreadAsRead');
	goUpdateCommand('cmd_markAllRead');
	goUpdateCommand('cmd_markAsFlagged');
	goUpdateCommand('cmd_file');

}

function SetupCommandUpdateHandlers()
{
	top.controllers.insertControllerAt(0, MessageWindowController);
}

function CommandUpdate_UndoRedo()
{

}

