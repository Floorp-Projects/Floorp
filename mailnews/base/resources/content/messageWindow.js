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
var gNextMessageViewIndexAfterDelete = -2;

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
    else if (event.GetUnicode() == "msgLoaded") {
      OnMsgLoaded(folder, gCurrentMessageUri);
    }
  }
}

var messagepaneObserver = {

canHandleMultipleItems: false,

  onDrop: function (aEvent, aData, aDragSession)
  {
    var sourceUri = aData.data; 
    if (sourceUri != gCurrentMessageUri)
    {
      var msgHdr = GetMsgHdrFromUri(sourceUri);
      var folderUri = msgHdr.folder.URI;
      if (folderUri != gCurrentFolderUri)
        UpdateDBView(folderUri);
      SelectMessage(sourceUri);
    }
  },
 
  onDragOver: function (aEvent, aFlavour, aDragSession)
  {
    var messagepanebox = document.getElementById("messagepanebox");
    messagepanebox.setAttribute("dragover", "true");
  },
 
  onDragExit: function (aEvent, aDragSession)
  {
    var messagepanebox = document.getElementById("messagepanebox");
    messagepanebox.removeAttribute("dragover");
  },

  canDrop: function(aEvent, aDragSession)  //allow drop from mail:3pane window only - 4xp
  {
    var doc = aDragSession.sourceNode.ownerDocument;
    var elem = doc.getElementById("messengerWindow");
    return (elem && (elem.getAttribute("windowtype") == "mail:3pane"));
  },
  
  getSupportedFlavours: function ()
  {
    var flavourSet = new FlavourSet();
    flavourSet.appendFlavour("text/x-moz-message");
    return flavourSet;
  }
};

function UpdateDBView(folderUri)
{
  var dbview = GetDBView();  //close old folder view
  if (dbview) {
    dbview.close(); 
  }

  SelectFolder(folderUri);

  CreateView(null);   //create new folder view
} 
  

function nsMsgDBViewCommandUpdater()
{}

function UpdateStandAloneMessageCounts()
{
  // hook for extra toolbar items
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(window, "mail:updateStandAloneMessageCounts", "");
}

nsMsgDBViewCommandUpdater.prototype = 
{
  updateCommandStatus : function()
    {
      // the back end is smart and is only telling us to update command status
      // when the # of items in the selection has actually changed.
      UpdateMailToolbar("dbview, std alone window");
    },

  displayMessageChanged : function(aFolder, aSubject, aKeywords)
  {
    setTitleFromFolder(aFolder, aSubject);
    gCurrentMessageUri = gDBView.URIForFirstSelectedMessage;
    UpdateStandAloneMessageCounts();
    SetKeywords(aKeywords);
  },

  QueryInterface : function(iid)
  {
    if (iid.equals(Components.interfaces.nsIMsgDBViewCommandUpdater) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
	  
    throw Components.results.NS_NOINTERFACE;
  }
}

// from MailNewsTypes.h
const nsMsgKey_None = 0xFFFFFFFF;

function HandleDeleteOrMoveMsgCompleted(folder)
{
	dump("In HandleDeleteOrMoveMsgCompleted\n");
	var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
	if(!folderResource)
		return;

	var folderUri = folderResource.Value;
	if((folderUri == gCurrentFolderUri) && gCurrentMessageIsDeleted)
	{
    gDBView.onDeleteCompleted(true);
    gCurrentMessageIsDeleted = false;
    if (gNextMessageViewIndexAfterDelete != nsMsgKey_None) 
    {
      var nextMstKey = gDBView.getKeyAt(gNextMessageViewIndexAfterDelete);
      if (nextMstKey != nsMsgKey_None) {
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
  gDBView.onDeleteCompleted(false);
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
  AddToolBarPrefListener();
  ShowHideToolBarButtons()
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

  CreateView(originalView)
 
  setTimeout("var msgKey = extractMsgKeyFromURI(gCurrentMessageUri); gDBView.loadMessageByMsgKey(msgKey); gNextMessageViewIndexAfterDelete = gDBView.msgToSelectAfterDelete; UpdateStandAloneMessageCounts();", 0);
  
  SetupCommandUpdateHandlers();
  var messagePaneFrame = top.frames['messagepane'];
  if(messagePaneFrame)
	messagePaneFrame.focus();
}

function CreateView(originalView)
{
  
  var msgFolder = GetLoadedMsgFolder();

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
  else if (msgFolder)
  {
    var msgDatabase = msgFolder.getMsgDatabase(msgWindow);
    if (msgDatabase)
    {
      var dbFolderInfo = msgDatabase.dBFolderInfo;
      sortType = dbFolderInfo.sortType;
      sortOrder = dbFolderInfo.sortOrder;
      viewFlags = dbFolderInfo.viewFlags;
      viewType = dbFolderInfo.viewType;
      msgDatabase = null;
      dbFolderInfo = null;
   }
  }

  // create a db view
  CreateBareDBView(originalView, msgFolder, viewType, viewFlags, sortType, sortOrder); 

  var uri;
  if (gCurrentMessageUri)
    uri = gCurrentMessageUri;
  else if (gCurrentFolderUri)
    uri = gCurrentFolderUri;
  else
    uri = null;

  SetUpToolbarButtons(uri);

  // hook for extra toolbar items
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(window, "mail:setupToolbarItems", uri);
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
  RemoveToolBarPrefListener();
	// FIX ME - later we will be able to use onunload from the overlay
	OnUnloadMsgHeaderPane();

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

function GetSelectedIndices(dbView)
{
  try {
    var indicesArray = {}; 
    var length = {};
    dbView.getIndicesForSelection(indicesArray,length);
    return indicesArray.value;
  }
  catch (ex) {
    dump("ex = " + ex + "\n");
    return null;
  }
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
  UpdateMailToolbar("clear msg, std alone window");
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

function GetMsgHdrFromUri(messageUri)
{
  return messenger.messageServiceFromURI(messageUri).messageURIToMsgHdr(messageUri);
}

function SelectMessage(messageUri)
{
  var msgHdr = GetMsgHdrFromUri(messageUri);
  gDBView.loadMessageByMsgKey(msgHdr.messageKey);
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
      case "cmd_createFilterFromPopup":
      case "cmd_createFilterFromMenu":
			case "cmd_delete":
      case "cmd_undo":
      case "cmd_redo":
      case "cmd_killThread":
      case "cmd_watchThread":
			case "button_delete":
      case "button_junk":
			case "cmd_shiftDelete":
      case "button_print":
			case "cmd_print":
			case "cmd_printpreview":
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
			case "cmd_findPrev":
      case "cmd_search":
      case "button_mark":
			case "cmd_markAsRead":
			case "cmd_markAllRead":
			case "cmd_markThreadAsRead":
			case "cmd_markAsFlagged":
      case "cmd_label0":
      case "cmd_label1":
      case "cmd_label2":
      case "cmd_label3":
      case "cmd_label4":
      case "cmd_label5":
      case "button_file":
			case "cmd_file":
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
      case "cmd_synchronizeOffline":
			case "cmd_downloadFlagged":
			case "cmd_downloadSelected":
        return CheckOnline();
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
		switch ( command )
		{
      case "cmd_createFilterFromPopup":
      case "cmd_createFilterFromMenu":
        var loadedFolder = GetLoadedMsgFolder();
        if (!(loadedFolder && loadedFolder.server.canHaveFilters))
          return false;
			case "cmd_delete":
        UpdateDeleteCommand();
        // fall through
			case "button_delete":
			case "cmd_shiftDelete":
        var loadedFolder = GetLoadedMsgFolder();
        return gCurrentMessageUri && loadedFolder && (loadedFolder.canDeleteMessages || isNewsURI(gCurrentFolderUri));
      case "button_junk":
        return (!isNewsURI(gCurrentFolderUri));
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
			case "cmd_print":
			case "cmd_printpreview":
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
      case "cmd_label0":
      case "cmd_label1":
      case "cmd_label2":
      case "cmd_label3":
      case "cmd_label4":
      case "cmd_label5":
        return(true);
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
      case "cmd_synchronizeOffline":
                return CheckOnline();
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
			case "cmd_findPrev":
				return MsgCanFindAgain();
      case "cmd_search":
        var loadedFolder = GetLoadedMsgFolder();
        if (!loadedFolder)
          return false;
        return loadedFolder.server.canSearchMessages; 
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
      case "cmd_createFilterFromPopup":
				break;// This does nothing because the createfilter is invoked from the popupnode oncommand.        
      case "cmd_createFilterFromMenu":
        MsgCreateFilter();
				break;        
			case "cmd_delete":
				MsgDeleteMessageFromMessageWindow(false, false);
				break;
			case "cmd_shiftDelete":
				MsgDeleteMessageFromMessageWindow(true, false);
				break;
      case "button_junk":
        MsgJunk();
        break;
			case "button_delete":
				MsgDeleteMessageFromMessageWindow(false, true);
				break;
		  case "cmd_printSetup":
		    NSPrintSetup();
		    break;
			case "cmd_print":
				PrintEnginePrint();
				break;
			case "cmd_printpreview":
				PrintEnginePrintPreview();
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
				MsgFindAgain(false);
				break;
			case "cmd_findPrev":
				MsgFindAgain(true);
				break;
      case "cmd_search":
        MsgSearchMessages();
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
      case "cmd_label0":
        gDBView.doCommand(nsMsgViewCommandType.label0);
 				return;
      case "cmd_label1":
        gDBView.doCommand(nsMsgViewCommandType.label1);
        return; 
      case "cmd_label2":
        gDBView.doCommand(nsMsgViewCommandType.label2);
        return; 
      case "cmd_label3":
        gDBView.doCommand(nsMsgViewCommandType.label3);
        return; 
      case "cmd_label4":
        gDBView.doCommand(nsMsgViewCommandType.label4);
        return; 
      case "cmd_label5":
        gDBView.doCommand(nsMsgViewCommandType.label5);
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
      case "button_next":
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
  if ((resultId.value != nsMsgKey_None) && (resultIndex.value != nsMsgKey_None)) 
  {
    // load the message key
    gDBView.loadMessageByMsgKey(resultId.value);
    // if we changed folders, the message counts changed.
    UpdateStandAloneMessageCounts();
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

