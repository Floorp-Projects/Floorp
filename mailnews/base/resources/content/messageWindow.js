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
var gThreadPaneCommandUpdater = null;
var gCurrentMessageIsDeleted = false;
var gNextMessageViewIndexAfterDelete = -1;

// the folderListener object
var folderListener = {
  OnItemAdded: function(parentItem, item, view) {},

  OnItemRemoved: function(parentItem, item, view) {
    var parentFolderResource = parentItem.QueryInterface(Components.interfaces.nsIRDFResource);
    if(!parentFolderResource)
      return;

    var parentURI = parentFolderResource.Value;
    if(parentURI != gCurrentFolderUri)
      return;

    var deletedMessageHdr = item.QueryInterface(Components.interfaces.nsIMsgDBHdr);
    if (extractMsgKeyFromURI() == deletedMessageHdr.messageKey)
      gCurrentMessageIsDeleted = true;
  },

  OnItemPropertyChanged: function(item, property, oldValue, newValue) {},
  OnItemIntPropertyChanged: function(item, property, oldValue, newValue) { },
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

function nsMsgDBViewCommandUpdater()
{}

nsMsgDBViewCommandUpdater.prototype = 
{
  updateCommandStatus : function()
    {
      // the back end is smart and is only telling us to update command status
      // when the # of items in the selection has actually changed.
		  document.commandDispatcher.updateCommands('mail-toolbar');
    },

  displayMessageChanged : function(aFolder, aSubject)
  {
    setTitleFromFolder(aFolder, aSubject);
    gCurrentMessageUri = gDBView.URIForFirstSelectedMessage;
  },

  QueryInterface : function(iid)
  {
    if(iid.equals(Components.interfaces.nsIMsgDBViewCommandUpdater))
	    return this;
	  
    throw Components.results.NS_NOINTERFACE;
    return null;
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
    gCurrentMessageIsDeleted = false;
    if (gNextMessageViewIndexAfterDelete != -1) 
    {
      var nextMstKey = gDBView.getKeyAt(gNextMessageViewIndexAfterDelete);
      if (nextMstKey != -1) {
        gDBView.loadMessageByMsgKey(nextMstKey);
      }
      else {
        window.close();
      }
    }
    else
    {
      // close the stand alone window because there are no more messages in the folder
      window.close();
    }
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
  	AddMailOfflineObserver();
	CreateMailWindowGlobals();
	CreateMessageWindowGlobals();
	verifyAccounts(null);

	InitMsgWindow();

	messenger.SetWindow(window, msgWindow);
	InitializeDataSources();
	// FIX ME - later we will be able to use onload from the overlay
	OnLoadMsgHeaderPane();

  try {
    var nsIFolderListener = Components.interfaces.nsIFolderListener;
    var notifyFlags = nsIFolderListener.removed | nsIFolderListener.event;
    mailSession.AddFolderListener(folderListener, notifyFlags);
  } catch (ex) {
    dump("Error adding to session: " +ex + "\n");
  }

  var originalView = null;

	if(window.arguments)
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

    if (window.arguments[2])
      originalView = window.arguments[2];      
	}	

  // extract the sort type, the sort order, 
  var sortType;
  var sortOrder;
  var viewFlags;
  var viewType;

  if (originalView)
  {
    viewType = originalView.viewType;
    viewFlags = originalView.viewFlags;
    sortType = originalView.sortType;
    sortOrder = originalView.sortOrder;
  }
  
  var msgFolder = GetLoadedMsgFolder();
  CreateBareDBView(msgFolder,viewType, viewFlags, sortType, sortOrder); // create a db view for 

  if (gCurrentMessageUri) {
    SetUpToolbarButtons(gCurrentMessageUri);
  }
  else if (gCurrentFolderUri) {
    SetUpToolbarButtons(gCurrentFolderUri);
  }
 
  setTimeout("var msgKey = extractMsgKeyFromURI(gCurrentMessageUri); gDBView.loadMessageByMsgKey(msgKey); gNextMessageViewIndexAfterDelete = gDBView.msgToSelectAfterDelete;", 0);

  SetupCommandUpdateHandlers();
  var messagePaneFrame = top.frames['messagepane'];
  if(messagePaneFrame)
	messagePaneFrame.focus();
}

function extractMsgKeyFromURI()
{
  var msgKey = -1;
  var msgService = messenger.messageServiceFromURI(gCurrentMessageUri);
  if (msgService)
  {
    var msgHdr = msgService.messageURIToMsgHdr(gCurrentMessageUri);
    if (msgHdr)
      msgKey = msgHdr.messageKey;
  }

  return msgKey;
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

	var emptryTrashMenu = document.getElementById('menu_emptyTrash');
	if(emptryTrashMenu)
		emptryTrashMenu.setAttribute("hidden", "true");

	var menuProperties = document.getElementById('menu_properties');
	if(menuProperties)
		menuProperties.setAttribute("hidden", "true");

	var compactFolderMenu = document.getElementById('menu_compactFolder');
	if(compactFolderMenu)
		compactFolderMenu.setAttribute("hidden", "true");

	var trashSeparator = document.getElementById('trashMenuSeparator');
	if(trashSeparator)
		trashSeparator.setAttribute("hidden", "true");

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

function GetFirstSelectedMessage()
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
	if(message) {
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
  return gCurrentMessageUri;
}

//Clear everything related to the current message. called after load start page.
function ClearMessageSelection()
{
	gCurrentMessageUri = null;
	gCurrentFolderUri = null;
  document.commandDispatcher.updateCommands('mail-toolbar');	
}

function GetCompositeDataSource(command)
{
	return gCompositeDataSource;	
}

function SetNextMessageAfterDelete()
{
  gNextMessageViewIndexAfterDelete = gDBView.msgToSelectAfterDelete;
}

function SelectFolder(folderUri)
{
	gCurrentFolderUri = folderUri;
}

function ReloadMessage()
{
  gDBView.reloadMessage();
}

function MsgDeleteMessageFromMessageWindow(reallyDelete, fromToolbar)
{
  // if from the toolbar, return right away if this is a news message
  // only allow cancel from the menu:  "Edit | Cancel / Delete Message"
  if (fromToolbar)
  {
    if (isNewsURI(gCurrentFolderUri)) 
    {
        // if news, don't delete
        return;
    }
  }
  
  // before we delete 
  SetNextMessageAfterDelete();

  if (reallyDelete)
      gDBView.doCommand(nsMsgViewCommandType.deleteNoTrash);
  else
      gDBView.doCommand(nsMsgViewCommandType.deleteMsg);
}

// MessageWindowController object (handles commands when one of the trees does not have focus)
var MessageWindowController =
{
   supportsCommand: function(command)
	{

		switch ( command )
		{
			case "cmd_close":
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
      case "cmd_canHaveFilter":
			case "cmd_delete":
      case "cmd_undo":
      case "cmd_redo":
      case "cmd_killThread":
      case "cmd_watchThread":
			case "button_delete":
			case "cmd_shiftDelete":
      case "button_print":
			case "cmd_print":
		  case "cmd_printSetup":
			case "cmd_saveAsFile":
			case "cmd_saveAsTemplate":
			case "cmd_viewPageSource":
			case "cmd_reload":
			case "cmd_getNewMessages":
      case "button_getNewMessages":
      case "cmd_getMsgsForAuthAccounts":
			case "cmd_getNextNMessages":
			case "cmd_find":
			case "cmd_findAgain":
      case "button_mark":
			case "cmd_markAsRead":
			case "cmd_markAllRead":
			case "cmd_markThreadAsRead":
			case "cmd_markAsFlagged":
      case "button_file":
			case "cmd_file":
      case "cmd_synchronizeOffline":
			case "cmd_downloadFlagged":
			case "cmd_downloadSelected":
			case "cmd_settingsOffline":
			case "cmd_nextMsg":
      case "button_next":
			case "cmd_nextUnreadMsg":
			case "cmd_nextFlaggedMsg":
			case "cmd_nextUnreadThread":
			case "cmd_previousMsg":
			case "cmd_previousUnreadMsg":
			case "cmd_previousFlaggedMsg":
				return true;
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
    var enabled = new Object();
    var checkStatus = new Object();
		switch ( command )
		{
			case "cmd_delete":
				if (isNewsURI(gCurrentMessageUri))
				{
					goSetMenuValue(command, 'valueNewsMessage');
					goSetAccessKey(command, 'valueNewsMessageAccessKey');
				}
				else
				{
					goSetMenuValue(command, 'valueMessage');
					goSetAccessKey(command, 'valueMessageAccessKey');
				}

				if (gDBView)
				{
					gDBView.getCommandStatus(nsMsgViewCommandType.deleteMsg, enabled, checkStatus);
					return enabled.value;
				}
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
      case "cmd_canHaveFilter":
        var loadedFolder = GetLoadedMsgFolder();
        if (!(loadedFolder && loadedFolder.server.canHaveFilters))
          return false;

			case "button_delete":
			case "cmd_shiftDelete":
			case "cmd_print":
      case "button_print":
			case "cmd_saveAsFile":
			case "cmd_saveAsTemplate":
			case "cmd_viewPageSource":
			case "cmd_reload":
			case "cmd_find":
      case "button_mark":
			case "cmd_markAsRead":
			case "cmd_markAllRead":
			case "cmd_markThreadAsRead":
			case "cmd_markAsFlagged":
      case "button_file":
			case "cmd_file":
				return ( gCurrentMessageUri != null);
			case "cmd_printSetup":
			  return true;
			case "cmd_getNewMessages":
      case "button_getNewMessages":
      case "cmd_getMsgsForAuthAccounts":
				return IsGetNewMessagesEnabled();
			case "cmd_getNextNMessages":
				return IsGetNextNMessagesEnabled();		
			case "cmd_downloadFlagged":
			case "cmd_downloadSelected":
                return true;
            case "cmd_synchronizeOffline":
			case "cmd_settingsOffline":
                return IsAccountOfflineEnabled();
			case "cmd_close":
			case "cmd_nextMsg":
      case "button_next":
			case "cmd_nextUnreadMsg":
			case "cmd_nextUnreadThread":
			case "cmd_previousMsg":
			case "cmd_previousUnreadMsg":
				return true;
			case "cmd_findAgain":
				return MsgCanFindAgain();
      case "cmd_undo":
      case "cmd_redo":
                return SetupUndoRedoCommand(command);
			default:
				return false;
		}
	},

	doCommand: function(command)
	{
    // if the user invoked a key short cut then it is possible that we got here for a command which is
    // really disabled. kick out if the command should be disabled.
    if (!this.isCommandEnabled(command)) return;

    var navigationType = nsMsgNavigationType.nextUnreadMessage;

		switch ( command )
		{
			case "cmd_close":
				CloseMailWindow();
				break;
			case "cmd_getNewMessages":
				MsgGetMessage();
				break;
            case "cmd_undo":
                messenger.Undo(msgWindow);
                break;
            case "cmd_redo":
                messenger.Redo(msgWindow);
                break;
      case "cmd_getMsgsForAuthAccounts":
        MsgGetMessagesForAllAuthenticatedAccounts();
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
      case "cmd_canHaveFilter":
        MsgCreateFilter();
				break;        
			case "cmd_delete":
				MsgDeleteMessageFromMessageWindow(false, false);
				break;
			case "cmd_shiftDelete":
				MsgDeleteMessageFromMessageWindow(true, false);
				break;
			case "button_delete":
				MsgDeleteMessageFromMessageWindow(false, true);
				break;
		  case "cmd_printSetup":
		    goPageSetup();
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
      case "button_mark":
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
      case "cmd_downloadFlagged":
        MsgDownloadFlagged();
        return;
      case "cmd_downloadSelected":
        MsgDownloadSelected();
        return;
      case "cmd_synchronizeOffline":
        MsgSynchronizeOffline();
        return;
			case "cmd_settingsOffline":
				MsgSettingsOffline();
				return;
			case "cmd_nextUnreadMsg":
        performNavigation(nsMsgNavigationType.nextUnreadMessage);
				break;
			case "cmd_nextUnreadThread":      
        performNavigation(nsMsgNavigationType.nextUnreadThread);
				break;
			case "cmd_nextMsg":
        performNavigation(nsMsgNavigationType.nextMessage);
				break;
			case "cmd_nextFlaggedMsg":
        performNavigation(nsMsgNavigationType.nextFlagged);
				break;
			case "cmd_previousMsg":
        performNavigation(nsMsgNavigationType.previousMessage);
				break;
			case "cmd_previousUnreadMsg":
        performNavigation(nsMsgNavigationType.previousUnreadMessage);
				break;
			case "cmd_previousFlaggedMsg":
        performNavigation(nsMsgNavigationType.previousFlagged);
				break;
		}
	},
	
	onEvent: function(event)
	{
	}
};

function performNavigation(type)
{
  var resultId = new Object;
  var resultIndex = new Object;
  var threadIndex = new Object;

  gDBView.viewNavigate(type, resultId, resultIndex, threadIndex, true /* wrap */);
  
  // if we found something....display it.
  if ((resultId.value != nsMsgViewIndex_None) && (resultIndex.value != nsMsgViewIndex_None)) 
  {
    // load the message key
    gDBView.loadMessageByMsgKey(resultId.value);
    return;
  }
   
  // we need to span another folder 
  CrossFolderNavigation(type, false);
}

function SetupCommandUpdateHandlers()
{
	top.controllers.insertControllerAt(0, MessageWindowController);
}

function GetDBView()
{
  return gDBView;
}

