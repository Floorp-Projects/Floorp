# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.

/* This is where functions related to the standalone message window are kept */

// from MailNewsTypes.h
const nsMsgKey_None = 0xFFFFFFFF;
const nsMsgViewIndex_None = 0xFFFFFFFF;

/* globals for a particular window */

var compositeDataSourceContractID        = datasourceContractIDPrefix + "composite-datasource";

var gCompositeDataSource;
var gCurrentMessageUri;
var gCurrentFolderUri;
var gThreadPaneCommandUpdater = null;
var gCurrentMessageIsDeleted = false;
var gNextMessageViewIndexAfterDelete = -2;
var gCurrentFolderToRerootForStandAlone;
var gRerootOnFolderLoadForStandAlone = false;
var gNextMessageAfterLoad = null;

// the folderListener object
var folderListener = {
  OnItemAdded: function(parentItem, item) {},

  OnItemRemoved: function(parentItem, item) {
    if (parentItem.Value != gCurrentFolderUri)
      return;

    if (item instanceof Components.interfaces.nsIMsgDBHdr &&
        extractMsgKeyFromURI() == item.messageKey)
      gCurrentMessageIsDeleted = true;
  },

  OnItemPropertyChanged: function(item, property, oldValue, newValue) {},
  OnItemIntPropertyChanged: function(item, property, oldValue, newValue) { 
    if (item.Value == gCurrentFolderUri) {
      if (property.toString() == "TotalMessages" || property.toString() == "TotalUnreadMessages") {
        UpdateStandAloneMessageCounts();
      }      
    }
  },
  OnItemBoolPropertyChanged: function(item, property, oldValue, newValue) {},
  OnItemUnicharPropertyChanged: function(item, property, oldValue, newValue){},
  OnItemPropertyFlagChanged: function(item, property, oldFlag, newFlag) {},

  OnItemEvent: function(folder, event) {
    var eventType = event.toString();

    if (eventType == "DeleteOrMoveMsgCompleted")
      HandleDeleteOrMoveMsgCompleted(folder);
    else if (eventType == "DeleteOrMoveMsgFailed")
      HandleDeleteOrMoveMsgFailed(folder);
    else if (eventType == "FolderLoaded") {
      if (folder) {
        var uri = folder.URI;
        if (uri == gCurrentFolderToRerootForStandAlone) {
          gCurrentFolderToRerootForStandAlone = null;
          var msgFolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
          if (msgFolder) {
            msgFolder.endFolderLoading();
            if (gRerootOnFolderLoadForStandAlone) {
              RerootFolderForStandAlone(uri);
            }
          }   
        }
      }
    }
    else if (eventType == "JunkStatusChanged") {
      HandleJunkStatusChanged(folder);
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
      SelectFolder(GetMsgHdrFromUri(sourceUri).folder.URI);
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
    ClearPendingReadTimer(); // we are loading / selecting a new message so kill the mark as read timer for the currently viewed message
    gCurrentMessageUri = gDBView.URIForFirstSelectedMessage;
    UpdateStandAloneMessageCounts();
    SetKeywords(aKeywords);
    goUpdateCommand("button_junk");
  },

  updateNextMessageAfterDelete : function()
  {
    SetNextMessageAfterDelete();
  },

  QueryInterface : function(iid)
  {
    if (iid.equals(Components.interfaces.nsIMsgDBViewCommandUpdater) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
	  
    throw Components.results.NS_NOINTERFACE;
  }
}

function HandleDeleteOrMoveMsgCompleted(folder)
{
	var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
	if (!folderResource)
		return;

	if ((folderResource.Value == gCurrentFolderUri) && gCurrentMessageIsDeleted)
	{
    gDBView.onDeleteCompleted(true);
    gCurrentMessageIsDeleted = false;
    if (gNextMessageViewIndexAfterDelete != nsMsgViewIndex_None) 
    {
      var nextMstKey = gDBView.getKeyAt(gNextMessageViewIndexAfterDelete);
      if (nextMstKey != nsMsgKey_None) {
        LoadMessageByViewIndex(gNextMessageViewIndexAfterDelete);
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
  if (!folderResource)
     return;

  gDBView.onDeleteCompleted(false);
  if ((folderResource.Value == gCurrentFolderUri) && gCurrentMessageIsDeleted)
    gCurrentMessageIsDeleted = false;
}

function IsCurrentLoadedFolder(folder)
{
  var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
  if (!folderResource)
     return false;

  return (folderResource.Value == gCurrentFolderUri);
}


// we won't show the window until the onload() handler is finished
// so we do this trick (suggested by hyatt / blaker)
function OnLoadMessageWindow()
{
  setTimeout(delayedOnLoadMessageWindow, 0); // when debugging, set this to 5000, so you can see what happens after the window comes up.
}

function delayedOnLoadMessageWindow()
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
    var notifyFlags = nsIFolderListener.removed | nsIFolderListener.event | nsIFolderListener.intPropertyChanged;
    mailSession.AddFolderListener(folderListener, notifyFlags);
  } catch (ex) {
    dump("Error adding to session: " +ex + "\n");
  }

  var originalView = null;
  var folder = null;
  var messageUri;
  var loadCustomMessage = false;       //set to true when either loading a message/rfc822 attachment or a .eml file
	if (window.arguments)
	{
		if (window.arguments[0])
    {
      try
      {
        messageUri = window.arguments[0];
        if (messageUri instanceof Components.interfaces.nsIURI)
        {
          loadCustomMessage = /type=x-message-display/.test(messageUri.spec);
          gCurrentMessageUri = messageUri.spec;
          if (messageUri instanceof Components.interfaces.nsIMsgMailNewsUrl)
            folder = messageUri.folder;
        }
      } 
      catch(ex) 
      {
        folder = null;
        dump("## ex=" + ex + "\n");
      }

      if (!gCurrentMessageUri)
			gCurrentMessageUri = window.arguments[0];
    }
		else
			gCurrentMessageUri = null;

		if (window.arguments[1])
			gCurrentFolderUri = window.arguments[1];
		else
      gCurrentFolderUri = folder ? folder.URI : null;

    if (window.arguments[2])
      originalView = window.arguments[2];      

	}	

  CreateView(originalView);
  
  // initialize the customizeDone method on the customizeable toolbar
  var toolbox = document.getElementById("mail-toolbox");
  toolbox.customizeDone = MailToolboxCustomizeDone;

  var toolbarset = document.getElementById('customToolbars');
  toolbox.toolbarset = toolbarset;

  setTimeout(OnLoadMessageWindowDelayed, 0, loadCustomMessage);
  
  SetupCommandUpdateHandlers();
}

function OnLoadMessageWindowDelayed(loadCustomMessage)
{
  if (loadCustomMessage)
    gDBView.loadMessageByUrl(gCurrentMessageUri);
  else
{
  var msgKey = extractMsgKeyFromURI(gCurrentMessageUri); 
  gDBView.loadMessageByMsgKey(msgKey); 
  }
  gNextMessageViewIndexAfterDelete = gDBView.msgToSelectAfterDelete; 
  UpdateStandAloneMessageCounts();
   
  // set focus to the message pane
  window.content.focus();

  // since we just changed the pane with focus we need to update the toolbar to reflect this
  // XXX TODO
  // can we optimize
  // and just update cmd_delete and button_delete?
  UpdateMailToolbar("focus");
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
	if (message_menuitem)
		message_menuitem.setAttribute("hidden", "true");

	var showSearchToolbar = document.getElementById('menu_showSearchToolbar');
	if (showSearchToolbar)
		showSearchToolbar.setAttribute("hidden", "true");

	var showSearch_showMessage_Separator = document.getElementById('menu_showSearch_showMessage_Separator');
	if (showSearch_showMessage_Separator)
		showSearch_showMessage_Separator.setAttribute("hidden", "true");

	var expandOrCollapseMenu = document.getElementById('menu_expandOrCollapse');
	if (expandOrCollapseMenu)
		expandOrCollapseMenu.setAttribute("hidden", "true");

	var renameFolderMenu = document.getElementById('menu_renameFolder');
	if (renameFolderMenu)
		renameFolderMenu.setAttribute("hidden", "true");

  var viewLayoutMenu = document.getElementById("menu_MessagePaneLayout");
  if (viewLayoutMenu)
    viewLayoutMenu.setAttribute("hidden", "true");

	var viewMessagesMenu = document.getElementById('viewMessagesMenu');
	if (viewMessagesMenu)
		viewMessagesMenu.setAttribute("hidden", "true");

	var viewMessageViewMenu = document.getElementById('viewMessageViewMenu');
	if (viewMessageViewMenu)
		viewMessageViewMenu.setAttribute("hidden", "true");

	var viewMessagesMenuSeparator = document.getElementById('viewMessagesMenuSeparator');
	if (viewMessagesMenuSeparator)
		viewMessagesMenuSeparator.setAttribute("hidden", "true");

	var openMessageMenu = document.getElementById('openMessageWindowMenuitem');
	if (openMessageMenu)
		openMessageMenu.setAttribute("hidden", "true");

  var viewSortMenuSeparator = document.getElementById('viewSortMenuSeparator');
  if (viewSortMenuSeparator)
    viewSortMenuSeparator.setAttribute("hidden", "true");

	var viewSortMenu = document.getElementById('viewSortMenu');
	if (viewSortMenu)
		viewSortMenu.setAttribute("hidden", "true");

	var emptryTrashMenu = document.getElementById('menu_emptyTrash');
	if (emptryTrashMenu)
		emptryTrashMenu.setAttribute("hidden", "true");

  var menuPropertiesSeparator = document.getElementById("editPropertiesSeparator");
  if (menuPropertiesSeparator)
    menuPropertiesSeparator.setAttribute("hidden", "true");

	var menuProperties = document.getElementById('menu_properties');
	if (menuProperties)
		menuProperties.setAttribute("hidden", "true");

	var compactFolderMenu = document.getElementById('menu_compactFolder');
	if (compactFolderMenu)
		compactFolderMenu.setAttribute("hidden", "true");

	var trashSeparator = document.getElementById('trashMenuSeparator');
	if (trashSeparator)
		trashSeparator.setAttribute("hidden", "true");

	var goStartPageSeparator = document.getElementById('goNextSeparator');
	if (goStartPageSeparator)
		goStartPageSeparator.hidden = true;

  var goStartPage = document.getElementById('goStartPage');
	if (goStartPage)
   goStartPage.hidden = true;
}

function OnUnloadMessageWindow()
{
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
	if (msgFolder)
		folderArray[0] = msgFolder;	

	return folderArray;
}

function GetFirstSelectedMessage()
{
	return GetLoadedMessage();
}

function GetNumSelectedMessages()
{
	if (gCurrentMessageUri)
		return 1;
	else
		return 0;
}

function GetSelectedMessages()
{
	var messageArray = new Array(1);
	var message = GetLoadedMessage();
	if (message)
		messageArray[0] = message;	

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
	if (gCurrentFolderUri)
		return RDF.GetResource(gCurrentFolderUri).QueryInterface(Components.interfaces.nsIMsgFolder);
	else
    return null;
}

function GetSelectedFolderURI()
{
  return gCurrentFolderUri;
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
  if (folderUri == gCurrentFolderUri)
    return;

  var msgfolder = RDF.GetResource(folderUri).QueryInterface(Components.interfaces.nsIMsgFolder);
  if (!msgfolder || msgfolder.isServer)
    return;

  // close old folder view
  var dbview = GetDBView();  
  if (dbview)
    dbview.close(); 

  gCurrentFolderToRerootForStandAlone = folderUri;

  if (msgfolder.manyHeadersToDownload)
  {
    gRerootOnFolderLoadForStandAlone = true;
    try
    {
      msgfolder.startFolderLoading();
      msgfolder.updateFolder(msgWindow);
    }
    catch(ex)
    {
      dump("Error loading with many headers to download: " + ex + "\n");
    }
  }
  else
  {
    RerootFolderForStandAlone(folderUri);
    gRerootOnFolderLoadForStandAlone = false;
    msgfolder.startFolderLoading();

    //Need to do this after rerooting folder.  Otherwise possibility of receiving folder loaded
    //notification before folder has actually changed.
    msgfolder.updateFolder(msgWindow);
  }    
}

function RerootFolderForStandAlone(uri)
{
  gCurrentFolderUri = uri;

  // create new folder view
  CreateView(null);
  
  // now do the work to load the appropriate message
  if (gNextMessageAfterLoad) {
    var type = gNextMessageAfterLoad;
    gNextMessageAfterLoad = null;
    LoadMessageByNavigationType(type);
  }
  
  SetUpToolbarButtons(gCurrentFolderUri);
  
  UpdateMailToolbar("reroot folder in stand alone window");
  
  // hook for extra toolbar items
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(window, "mail:setupToolbarItems", uri);
} 

function GetMsgHdrFromUri(messageUri)
{
  return messenger.messageServiceFromURI(messageUri).messageURIToMsgHdr(messageUri);
}

function SelectMessage(messageUri)
{
  var msgHdr = GetMsgHdrFromUri(messageUri);
  LoadMessageByMsgKey(msgHdr.messageKey);
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
      case "cmd_undo":
      case "cmd_redo":
      case "cmd_killThread":
      case "cmd_watchThread":
			case "button_delete":
      case "button_junk":
			case "cmd_shiftDelete":
			case "cmd_saveAsFile":
			case "cmd_saveAsTemplate":
			case "cmd_viewPageSource":
      case "cmd_getMsgsForAuthAccounts":
      case "button_mark":
			case "cmd_markAsRead":
			case "cmd_markAllRead":
			case "cmd_markThreadAsRead":
      case "cmd_markReadByDate":
			case "cmd_markAsFlagged":
      case "cmd_label0":
      case "cmd_label1":
      case "cmd_label2":
      case "cmd_label3":
      case "cmd_label4":
      case "cmd_label5":
      case "button_file":
			case "cmd_file":
      case "cmd_markAsJunk":
      case "cmd_markAsNotJunk":
      case "cmd_recalculateJunkScore":
      case "cmd_applyFilters":
      case "cmd_runJunkControls":
      case "cmd_deleteJunk":
			case "cmd_nextMsg":
      case "button_next":
      case "button_previous":
			case "cmd_nextUnreadMsg":
			case "cmd_nextFlaggedMsg":
			case "cmd_nextUnreadThread":
			case "cmd_previousMsg":
			case "cmd_previousUnreadMsg":
			case "cmd_previousFlaggedMsg":
        return !(gDBView.keyForFirstSelectedMessage == nsMsgKey_None);
      case "cmd_getNextNMessages":
      case "cmd_find":
      case "cmd_findAgain":
      case "cmd_findPrev":
      case "cmd_search":
      case "cmd_reload":
      case "cmd_getNewMessages":
      case "button_getNewMessages":
      case "button_print":
      case "cmd_print":
      case "cmd_printpreview":
      case "cmd_printSetup":
      case "cmd_close":
      case "cmd_settingsOffline":
      case "cmd_createFilterFromPopup":
      case "cmd_createFilterFromMenu":
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
        UpdateJunkToolbarButton();
        // fall through
      case "cmd_markAsJunk":
			case "cmd_markAsNotJunk":
      case "cmd_recalculateJunkScore":
        // can't do junk on news yet
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
      case "cmd_markReadByDate":
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
      case "button_previous":
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
      case "cmd_applyFilters":
      case "cmd_runJunkControls":
      case "cmd_deleteJunk":
        return false;
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
      case "cmd_markReadByDate":
        MsgMarkReadByDate();
        return;
			case "cmd_markAsFlagged":
				MsgMarkAsFlagged(null);
				return;
			case "cmd_markAsJunk":
        JunkSelectedMessages(true);
				return;
			case "cmd_markAsNotJunk":
        JunkSelectedMessages(false);
				return;
      case "cmd_recalculateJunkScore":
        analyzeMessagesForJunk();
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
      case "button_previous":
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

function LoadMessageByNavigationType(type)
{
  var resultId = new Object;
  var resultIndex = new Object;
  var threadIndex = new Object;

  gDBView.viewNavigate(type, resultId, resultIndex, threadIndex, true /* wrap */);

  // if we found something....display it.
  if ((resultId.value != nsMsgKey_None) && (resultIndex.value != nsMsgKey_None)) 
  {
    // load the message key
    LoadMessageByMsgKey(resultId.value);
    // if we changed folders, the message counts changed.
    UpdateStandAloneMessageCounts();

    // new message has been loaded
    return true;
  }

  // no message found to load
  return false;
}
   
function performNavigation(type)
{
  // Try to load a message by navigation type if we can find
  // the message in the same folder.
  if (LoadMessageByNavigationType(type))
    return;
   
  CrossFolderNavigation(type);
}

function SetupCommandUpdateHandlers()
{
	top.controllers.insertControllerAt(0, MessageWindowController);
}

function GetDBView()
{
  return gDBView;
}

function LoadMessageByMsgKey(messageKey)
{
  gDBView.loadMessageByMsgKey(messageKey);
  // we only want to update the toolbar if there was no previous selected message.
  if (nsMsgKey_None == gDBView.keyForFirstSelectedMessage)
    UpdateMailToolbar("update toolbar for message Window");
}

function LoadMessageByViewIndex(viewIndex)
{
  gDBView.loadMessageByViewIndex(viewIndex);
  // we only want to update the toolbar if there was no previous selected message.
  if (nsMsgKey_None == gDBView.keyForFirstSelectedMessage)
    UpdateMailToolbar("update toolbar for message Window");
}
