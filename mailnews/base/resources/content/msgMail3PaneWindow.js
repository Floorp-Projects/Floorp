/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *
 * Contributor(s):
 *   Jan Varga (varga@utcru.sk)
 *   Håkan Waara (hwaara@chello.se)
 */

/* This is where functions related to the 3 pane window are kept */

var showPerformance = false;

var gFolderOutliner; 
var gMessagePane;
var gMessagePaneFrame;
var gThreadOutliner;

var gThreadAndMessagePaneSplitter = null;
var gUnreadCount = null;
var gTotalCount = null;

var gCurrentLoadingFolderURI;
var gCurrentFolderToReroot;
var gCurrentLoadingFolderSortType = 0;
var gCurrentLoadingFolderSortOrder = 0;
var gCurrentLoadingFolderViewType = 0;
var gCurrentLoadingFolderViewFlags = 0;

var gCurrentDisplayedMessage = null;
var gNextMessageAfterDelete = null;
var gNextMessageAfterLoad = null;
var gNextMessageViewIndexAfterDelete = -2;
var gCurrentlyDisplayedMessage=-1;

var gActiveThreadPaneSortColumn = "";

var gStartFolderUri = null;

//If we've loaded a message, set to true.  Helps us keep the start page around.
var gHaveLoadedMessage;

var gDisplayStartupPage = false;

// the folderListener object
var folderListener = {
    OnItemAdded: function(parentItem, item, view) {},

	OnItemRemoved: function(parentItem, item, view){},

	OnItemPropertyChanged: function(item, property, oldValue, newValue) {},

	OnItemIntPropertyChanged: function(item, property, oldValue, newValue)
	{
		var currentLoadedFolder = GetThreadPaneFolder();
        if (!currentLoadedFolder) return;

		var currentURI = currentLoadedFolder.URI;

		//if we don't have a folder loaded, don't bother.
		if(currentURI)
		{
			if(property.GetUnicode() == "TotalMessages" || property.GetUnicode() == "TotalUnreadMessages")
			{
				var folder = item.QueryInterface(Components.interfaces.nsIMsgFolder);
				if(folder)
				{
					var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
					if(folderResource)
					{
						var folderURI = folderResource.Value;
						if(currentURI == folderURI)
						{
							UpdateStatusMessageCounts(folder);
						}
					}
				}
			}      
		}
	},

	OnItemBoolPropertyChanged: function(item, property, oldValue, newValue) {},

    OnItemUnicharPropertyChanged: function(item, property, oldValue, newValue){},
	OnItemPropertyFlagChanged: function(item, property, oldFlag, newFlag) {},

    OnItemEvent: function(folder, event) {
        var eventType = event.GetUnicode();

        if (eventType == "FolderLoaded") {
		if(folder)
		{
			var resource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
			if(resource)
			{
				var uri = resource.Value;
				if(uri == gCurrentFolderToReroot)
				{
					gCurrentFolderToReroot="";
					var msgFolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
					if(msgFolder)
					{
						msgFolder.endFolderLoading();
						RerootFolder(uri, msgFolder, gCurrentLoadingFolderViewType, gCurrentLoadingFolderViewFlags, gCurrentLoadingFolderSortType, gCurrentLoadingFolderSortOrder);
						gIsEditableMsgFolder = IsSpecialFolder(msgFolder, MSG_FOLDER_FLAG_DRAFTS);

						gCurrentLoadingFolderSortType = 0;
						gCurrentLoadingFolderSortOrder = 0;
            gCurrentLoadingFolderViewType = 0;
            gCurrentLoadingFolderViewFlags = 0;

            SetFocusThreadPane();
            var scrolled = false;
            if (gNextMessageAfterLoad) 
            {
              var type = gNextMessageAfterLoad;
              gNextMessageAfterLoad = null;

              // scroll to and select the proper message
              scrolled = ScrollToMessage(type, true, true /* selectMessage */);
            }
					}
				}
				if(uri == gCurrentLoadingFolderURI)
				{
				  gCurrentLoadingFolderURI = "";
				  //Now let's select the first new message if there is one
                  var beforeScrollToNew;
				  if(showPerformance) {
				    beforeScrollToNew = new Date();
                  }
                  if (!scrolled) {
                    // if we didn't just scroll, scroll to the first new message
                    // don't select it though
				    scrolled = ScrollToMessage(nsMsgNavigationType.firstNew, true, false /* selectMessage */);
                    
                    // if we failed to find a new message, scroll to the top
                    if (!scrolled) {
                      EnsureRowInThreadOutlinerIsVisible(0);
                    }
                  }

				  if(showPerformance) {
				      var afterScrollToNew = new Date();
				      var timeToScroll = (afterScrollToNew.getTime() - beforeScrollToNew.getTime())/1000;

				      var afterFolderLoadTime = new Date();
				      var timeToLoad = (afterFolderLoadTime.getTime() - gBeforeFolderLoadTime.getTime())/1000;
					  dump("Time to load " + uri + " is " +  timeToLoad + " seconds\n");
				  	  dump("of which scrolling to new is " + timeToScroll + " seconds\n");
				  }
    				SetBusyCursor(window, false);
				}
			}

		}
        } else if (eventType == "DeleteOrMoveMsgCompleted") {
			HandleDeleteOrMoveMsgCompleted(folder);
        }     
          else if (eventType == "DeleteOrMoveMsgFailed") {
                        HandleDeleteOrMoveMsgFailed(folder);
        }
          else if (eventType == "CompactCompleted") {
                        HandleCompactCompleted(folder);
        }
    }
}

var folderObserver = {
    canDropOn: function(index)
    {
        return CanDropOnFolderOutliner(index);
    },

    canDropBeforeAfter: function(index, before)
    {
        return CanDropBeforeAfterFolderOutliner(index, before);
    },

    onDrop: function(row, orientation)
    {
        DropOnFolderOutliner(row, orientation);
    },

    onToggleOpenState: function()
    {
    },

    onCycleHeader: function(colID, elt)
    {
    },

    onCycleCell: function(row, colID)
    {
    },

    onSelectionChanged: function()
    {
    },

    isEditable: function(row, colID)
    {
        return false;
    },

    onSetCellText: function(row, colID, value)
    {
    },

    onPerformAction: function(action)
    {
    },

    onPerformActionOnRow: function(action, row)
    {
    },

    onPerformActionOnCell: function(action, row, colID)
    {
    }
}

function HandleDeleteOrMoveMsgFailed(folder)
{
  if(IsCurrentLoadedFolder(folder)) {
    if(gNextMessageAfterDelete) {
      gNextMessageAfterDelete = null;
      gNextMessageViewIndexAfterDelete = -2;
    }
  }

  // fix me???
  // ThreadPaneSelectionChange(true);
}

function HandleDeleteOrMoveMsgCompleted(folder)
{
  if (gNextMessageViewIndexAfterDelete != -2) 
  {
    if (IsCurrentLoadedFolder(folder)) 
    {
      var outlinerView = gDBView.QueryInterface(Components.interfaces.nsIOutlinerView);
      var outlinerSelection = outlinerView.selection;
      if (gNextMessageViewIndexAfterDelete != -1) 
      {
        viewSize = outlinerView.rowCount;
        if (gNextMessageViewIndexAfterDelete >= viewSize) 
        {
          if (viewSize > 0)
            gNextMessageViewIndexAfterDelete = viewSize - 1;
          else
            gNextMessageViewIndexAfterDelete = -1;
        }
      }

      // if we are about to set the selection with a new element then DON'T clear
      // the selection then add the next message to select. This just generates
      // an extra round of command updating notifications that we are trying to
      // optimize away.
      if (gNextMessageViewIndexAfterDelete != -1) 
      {
        outlinerSelection.select(gNextMessageViewIndexAfterDelete);
        // since gNextMessageViewIndexAfterDelete probably has the same value
        // as the last index we had selected, the outliner isn't generating a new
        // selectionChanged notification for the outliner view. So we aren't loading the 
        // next message. to fix this, force the selection changed update.
        if (outlinerView)
          outlinerView.selectionChanged();
        EnsureRowInThreadOutlinerIsVisible(gNextMessageViewIndexAfterDelete); 
      }
      else 
      {
        outlinerSelection.clearSelection(); /* clear selection in either case  */
        setTitleFromFolder(folder,null);
        ClearMessagePane();
      }
    }
      gNextMessageViewIndexAfterDelete = -2;  
     //default value after delete/move/copy is over
  }
}

function HandleCompactCompleted (folder)
{
  if(folder)
  {
    var resource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
    if(resource)
    {
      var uri = resource.Value;
      var msgFolder = msgWindow.openFolder;
      if (msgFolder && uri == msgFolder.URI)
      {
        var msgdb = msgFolder.getMsgDatabase(msgWindow);
        if (msgdb)
        {
          var dbFolderInfo = msgdb.dBFolderInfo;
          sortType = dbFolderInfo.sortType;
          sortOrder = dbFolderInfo.sortOrder;
          viewFlags = dbFolderInfo.viewFlags;
          viewType = dbFolderInfo.viewType;
        }
        RerootFolder(uri, msgFolder, viewType, viewFlags, sortType, sortOrder);
        SetFocusThreadPane();
        if (gCurrentlyDisplayedMessage != -1)
        {
          var outlinerView = gDBView.QueryInterface(Components.interfaces.nsIOutlinerView);
          var outlinerSelection = outlinerView.selection;
          outlinerSelection.select(gCurrentlyDisplayedMessage);
          if (outlinerView)
            outlinerView.selectionChanged();
          EnsureRowInThreadOutlinerIsVisible(gCurrentlyDisplayedMessage);
        }
        gCurrentlyDisplayedMessage = -1; //reset
      }
    }
  }
}

function IsCurrentLoadedFolder(folder)
{
	var msgfolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
	if(msgfolder)
	{
		var folderResource = msgfolder.QueryInterface(Components.interfaces.nsIRDFResource);
		if(folderResource)
		{
			var folderURI = folderResource.Value;
			var currentLoadedFolder = GetThreadPaneFolder();
			var currentURI = currentLoadedFolder.URI;
			return(currentURI == folderURI);
		}
	}

	return false;
}

function ServerContainsFolder(server, folder)
{
  if (!folder || !server)
    return false;

  return server.equals(folder.server);
}

function SelectServer(server)
{
  SelectFolder(server.RootFolder.URI);
}

// we have this incoming server listener in case we need to
// alter the folder pane selection when a server is removed
// or changed (currently, when the real username or real hostname change)
var gThreePaneIncomingServerListener = {
    onServerLoaded: function(server) {},
    onServerUnloaded: function(server) {
      var selectedFolders = GetSelectedMsgFolders();
      for (var i = 0; i < selectedFolders.length; i++) {
        if (ServerContainsFolder(server, selectedFolders[i])) {
          SelectServer(accountManager.defaultAccount.incomingServer);
          // we've made a new selection, we're done
          return;
        }
      }
   
      // if nothing is selected at this point, better go select the default
      // this could happen if nothing was selected when the server was removed
      selectedFolders = GetSelectedMsgFolders();
      if (selectedFolders.length == 0) {
        SelectServer(accountManager.defaultAccount.incomingServer);
      }
    },
    onServerChanged: function(server) {
      // if the current selected folder is on the server that changed
      // and that server is an imap or news server, 
      // we need to update the selection. 
      // on those server types, we'll be reconnecting to the server
      // and our currently selected folder will need to be reloaded
      // or worse, be invalid.
      if (server.type != "imap" && server.type !="nntp")
        return;

      var selectedFolders = GetSelectedMsgFolders();
      for (var i = 0; i < selectedFolders.length; i++) {
        // if the selected item is a server, we don't have to update
        // the selection
        if (!(selectedFolders[i].isServer) && ServerContainsFolder(server, selectedFolders[i])) {
          SelectServer(server);
          // we've made a new selection, we're done
          return;
        }
      }
    }
}


/* Functions related to startup */
function OnLoadMessenger()
{
  showPerformance = pref.GetBoolPref('mail.showMessengerPerformance');
  var beforeLoadMessenger;
  if(showPerformance) {
      beforeLoadMessenger = new Date();
  }

  AddMailOfflineObserver();
  CreateMailWindowGlobals();
  Create3PaneGlobals();
  verifyAccounts(null);
    
  HideAccountCentral();
  loadStartPage();
  InitMsgWindow();

  messenger.SetWindow(window, msgWindow);

  InitializeDataSources();
  InitPanes();

  accountManager.SetSpecialFoldersForIdentities();
  accountManager.addIncomingServerListener(gThreePaneIncomingServerListener);

  AddToSession();
  //need to add to session before trying to load start folder otherwise listeners aren't
  //set up correctly.
  if ("arguments" in window && window.arguments[0])
  {
	gStartFolderUri = window.arguments[0];
  }
  else
  {
    gStartFolderUri = null;
  }

  setTimeout("loadStartFolder(gStartFolderUri);", 0);

  // FIX ME - later we will be able to use onload from the overlay
  OnLoadMsgHeaderPane();

	var id = null;
	var headerchoice = null;

	try {
		headerchoice = pref.GetIntPref("mail.show_headers");
	}
	catch (ex) {
		dump("failed to get the header pref\n");
	}

	switch (headerchoice) {
		case 2:	
			id = "viewallheaders";
			break;
		case 0:
			id = "viewbriefheaders";
			break;
		case 1:	
			id = "viewnormalheaders";
			break;
		default:
			id = "viewnormalheaders";
			break;
	}

	var menuitem = document.getElementById(id);

	try {
		// not working right yet.  see bug #??????
		// menuitem.setAttribute("checked", "true"); 
	}
	catch (ex) {
		dump("failed to set the view headers menu item\n");
	}

	gHaveLoadedMessage = false;

	//Set focus to the Thread Pane the first time the window is opened.
	SetFocusThreadPane();

	if(showPerformance) {
	  var afterLoadMessenger = new Date();
	  var timeToLoad = (afterLoadMessenger.getTime() - beforeLoadMessenger.getTime())/1000;
	  dump("Time in OnLoadMessger is " +  timeToLoad + " seconds\n");
	}
}

function OnUnloadMessenger()
{
  accountManager.removeIncomingServerListener(gThreePaneIncomingServerListener);
  OnMailWindowUnload();
}

function Create3PaneGlobals()
{
}
 
// because the "open" state persists, we'll call
// PerformExpand() for all servers that are open at startup.            
function PerformExpandForAllOpenServers()
{
    var folderOutliner = GetFolderOutliner();
    var view = folderOutliner.outlinerBoxObject.view;
    for (var i = 0; i < view.rowCount; i++)
    {
        if (view.isContainer(i))
        {
            var folderResource = GetFolderResource(folderOutliner, i);
            var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
            var isServer = GetFolderAttribute(folderOutliner, folderResource, "IsServer"); 
            if (isServer == "true")
            {
                if (view.isContainerOpen(i))
                {
                    var server = msgFolder.server;
                    // Don't do this for imap servers. See bug #41943
                    if (server.type != "imap")
                        server.performExpand(msgWindow);
                }
            }
        }
    }
}

function loadStartFolder(initialUri)
{
    var folderOutliner = GetFolderOutliner();
    var defaultServer = null;
    var startFolderResource = null;
    var isLoginAtStartUpEnabled = false;
    var enabledNewMailCheckOnce = false;
    var mailCheckOncePref = "mail.startup.enabledMailCheckOnce";

    //First get default account
    try
    {
        if(initialUri)
            startFolderResource = RDF.GetResource(initialUri);
        else
        {
            var defaultAccount = accountManager.defaultAccount;

            defaultServer = defaultAccount.incomingServer;
            var rootFolder = defaultServer.RootFolder;
            var rootMsgFolder = rootFolder.QueryInterface(Components.interfaces.nsIMsgFolder);

            startFolderResource = rootMsgFolder.QueryInterface(Components.interfaces.nsIRDFResource);

            enabledNewMailCheckOnce = pref.GetBoolPref(mailCheckOncePref);

            // Enable checknew mail once by turning checkmail pref 'on' to bring 
            // all users to one plane. This allows all users to go to Inbox. User can 
            // always go to server settings panel and turn off "Check for new mail at startup"
            if (!enabledNewMailCheckOnce)
            {
                pref.SetBoolPref(mailCheckOncePref, true);
                defaultServer.loginAtStartUp = true;
            }

            // Get the user pref to see if the login at startup is enabled for default account
            isLoginAtStartUpEnabled = defaultServer.loginAtStartUp;

            // Get Inbox only if when we have to login 
            if (isLoginAtStartUpEnabled) 
            {
                //now find Inbox
                var outNumFolders = new Object();
                var inboxFolder = rootMsgFolder.getFoldersWithFlag(0x1000, 1, outNumFolders); 
                if (!inboxFolder) return;

                startFolderResource = inboxFolder.QueryInterface(Components.interfaces.nsIRDFResource);
            }
            else
            {
                // set the startFolderResource to the server, so we select it
                // so we'll get account central
                startFolderResource = RDF.GetResource(defaultServer.serverURI);
            }
        }

        var startFolder = startFolderResource.QueryInterface(Components.interfaces.nsIFolder);
        SelectFolder(startFolder.URI);
                
        // only do this on startup, when we pass in null
        if (!initialUri && isLoginAtStartUpEnabled)
        {
            // Perform biff on the server to check for new mail, except for imap
            if (defaultServer.type != "imap")
            {
               var localFolder = inboxFolder.QueryInterface(Components.interfaces.nsIMsgLocalMailFolder);
               if (localFolder)
               { 
                   if (!localFolder.parsingInbox)
                       defaultServer.PerformBiff();
                   else
                       localFolder.checkForNewMessagesAfterParsing = true;
               }              
               else  //it can be only nntp
                   defaultServer.PerformBiff();
            }
        } 

        // because the "open" state persists, we'll call
        // PerformExpand() for all servers that are open at startup.
        PerformExpandForAllOpenServers();
    }
    catch(ex)
    {
        dump(ex);
        dump('Exception in LoadStartFolder caused by no default account.  We know about this\n');
    }

    if (!initialUri) 
    {
        MsgGetMessagesForAllServers(defaultServer);
    }
}

function TriggerGetMessages(server)
{
    // downloadMessagesAtStartup for a given server type indicates whether 
    // or not there is a need to Trigger GetMessages action
    if (server.downloadMessagesAtStartup)
        MsgGetMessage();
}

function AddToSession()
{
    try {
        var mailSession = Components.classes[mailSessionContractID].getService(Components.interfaces.nsIMsgMailSession);
        
        mailSession.AddFolderListener(folderListener);
	} catch (ex) {
        dump("Error adding to session\n");
    }
}

function InitPanes()
{
    OnLoadFolderPane();
    SetupCommandUpdateHandlers();
}

function InitializeDataSources()
{
	//Setup common mailwindow stuff.
	AddDataSources();

	//To threadpane move context menu
	SetupMoveCopyMenus('threadPaneContext-moveMenu', accountManagerDataSource, folderDataSource);

	//To threadpane copy content menu
	SetupMoveCopyMenus('threadPaneContext-copyMenu', accountManagerDataSource, folderDataSource);
}

function OnFolderUnreadColAttrModified(event)
{
    if (event.attrName == "hidden")
    {
        var folderNameCell = document.getElementById("folderNameCell");
        if (event.newValue == "true")
        {
            folderNameCell.setAttribute("label", "?folderTreeName");
        }
        else if (event.attrChange == Components.interfaces.nsIDOMMutationEvent.REMOVAL)
        {
            folderNameCell.setAttribute("label", "?folderTreeSimpleName");
        }
    }
}

// builds prior to 8-14-2001 did not have the unread and total columns
// in the folder pane.  so if a user ran an old build, and then
// upgraded, they get the new columns, and this causes problems
// because it looks like all the folder names are gone (see bug #96979)
// to work around this, we hide those columns once, using the 
// "mail.ui.folderpane.version" pref.
function UpgradeFolderPaneUI()
{
  var folderPaneUIVersion = pref.GetIntPref("mail.ui.folderpane.version");

  if (folderPaneUIVersion == 1) {
    var folderUnreadCol = document.getElementById("folderUnreadCol");
    folderUnreadCol.setAttribute("hidden", "true");
    var folderTotalCol = document.getElementById("folderTotalCol");
    folderTotalCol.setAttribute("hidden", "true");
    pref.SetIntPref("mail.ui.folderpane.version", 2);
  }
}

function OnLoadFolderPane()
{
    UpgradeFolderPaneUI();

    var folderUnreadCol = document.getElementById("folderUnreadCol");
    var hidden = folderUnreadCol.getAttribute("hidden");
    if (!hidden)
    {
        var folderNameCell = document.getElementById("folderNameCell");
        folderNameCell.setAttribute("label", "?folderTreeSimpleName");
    }
    folderUnreadCol.addEventListener("DOMAttrModified", OnFolderUnreadColAttrModified, false);

    SortFolderPane("folderNameCol");

    var folderOutliner = GetFolderOutliner();
    var folderOutlinerBuilder = folderOutliner.outlinerBoxObject.outlinerBody.builder.QueryInterface(Components.interfaces.nsIXULOutlinerBuilder);
    folderOutlinerBuilder.addObserver(folderObserver);
    folderOutliner.addEventListener("click",FolderPaneOnClick,true);
}

function GetFolderDatasource()
{
    var folderOutliner = GetFolderOutliner();
    return folderOutliner.outlinerBoxObject.outlinerBody.database;
}

/* Functions for accessing particular parts of the window*/
function GetFolderOutliner()
{
    if (! gFolderOutliner)
        gFolderOutliner = document.getElementById("folderOutliner");
    return gFolderOutliner;
}

function GetMessagePane()
{
    if (gMessagePane) return gMessagePane;
    gMessagePane = document.getElementById("messagepanebox");
    return gMessagePane;
}

function GetMessagePaneFrame()
{
    if (gMessagePaneFrame) return gMessagePaneFrame;
    gMessagePaneFrame = top.frames['messagepane'];
    return gMessagePaneFrame;
}

function FindInSidebar(currentWindow, id)
{
	var item = currentWindow.document.getElementById(id);
	if(item)
		return item;

	for(var i = 0; i < currentWindow.frames.length; i++)
	{
		var frameItem = FindInSidebar(currentWindow.frames[i], id);
		if(frameItem)
			return frameItem;
	}

	return null;
}

function GetThreadAndMessagePaneSplitter()
{
	if(gThreadAndMessagePaneSplitter) return gThreadAndMessagePaneSplitter;
	var splitter = document.getElementById('threadpane-splitter');
	gThreadAndMessagePaneSplitter = splitter;
	return splitter;
}

function GetUnreadCountElement()
{
	if(gUnreadCount) return gUnreadCount;
	var unreadCount = document.getElementById('unreadMessageCount');
	gUnreadCount = unreadCount;
	return unreadCount;
}
function GetTotalCountElement()
{
	if(gTotalCount) return gTotalCount;
	var totalCount = document.getElementById('totalMessageCount');
	gTotalCount = totalCount;
	return totalCount;
}

function IsThreadAndMessagePaneSplitterCollapsed()
{
  var messagePane = GetMessagePane();
  try {
    return (messagePane.getAttribute("collapsed") == "true");
	}
  catch (ex) {
		return false;
  }
}

function FindMessenger()
{
  return messenger;
}

function ClearThreadPaneSelection()
{
  try {
    if (gDBView) {
      var outlinerView = gDBView.QueryInterface(Components.interfaces.nsIOutlinerView);
      var outlinerSelection = outlinerView.selection;
      if (outlinerSelection) 
        outlinerSelection.clearSelection(); 
    }
  }
  catch (ex) {
    dump("ClearThreadPaneSelection: ex = " + ex + "\n");
  }
}

function ClearMessagePane()
{
	if(gHaveLoadedMessage)
	{	
    gHaveLoadedMessage = false;
		gCurrentDisplayedMessage = null;
		if (window.frames["messagepane"].location != "about:blank")
		    window.frames["messagepane"].location = "about:blank";
		// hide the message header view AND the message pane...
		HideMessageHeaderPane();
	}
}


function GetSelectedFolderIndex()
{
    var folderOutliner = GetFolderOutliner();
    var startIndex = {};
    var endIndex = {};
    folderOutliner.outlinerBoxObject.selection.getRangeAt(0, startIndex, endIndex);
    return startIndex.value;
}

function FolderPaneOnClick(event)
{
    // we only care about button 0 (left click) events
    if (event.button != 0)
        return;

    var folderOutliner = GetFolderOutliner();
    var row = {};
    var col = {};
    var elt = {};
    folderOutliner.outlinerBoxObject.getCellAt(event.clientX, event.clientY, row, col, elt);

    if (elt.value == "twisty")
    {
        var folderResource = GetFolderResource(folderOutliner, row.value);
        var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);

        if (!(folderOutliner.outlinerBoxObject.view.isContainerOpen(row.value)))
        {
            var isServer = GetFolderAttribute(folderOutliner, folderResource, "IsServer");
            if (isServer == "true")
            {
                var server = msgFolder.server;
                server.performExpand(msgWindow);
            }
            else
            {
                var serverType = GetFolderAttribute(folderOutliner, folderResource, "ServerType");
                if (serverType == "imap")
                {
                    var imapFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
                    imapFolder.performExpand(msgWindow);
                }
            }
        }
    }
    else if ((event.originalTarget.localName == "outlinercol") ||
             (event.originalTarget.localName == "slider") ||
             (event.originalTarget.localName == "scrollbarbutton")) {
      // clicking on the name column in the folder pane should not sort
      event.preventBubble();
    }
    else if (event.detail == 2) {
      FolderPaneDoubleClick(row.value, event);
    }
}

function FolderPaneDoubleClick(folderIndex, event)
{
    var folderOutliner = GetFolderOutliner();
    var folderResource = GetFolderResource(folderOutliner, folderIndex);
    var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    var isServer = GetFolderAttribute(folderOutliner, folderResource, "IsServer");

    if (isServer == "true")
    {
      if (!(folderOutliner.outlinerBoxObject.view.isContainerOpen(folderIndex)))
      {
        var server = msgFolder.server;
        server.performExpand(msgWindow);
      }
    }
    else 
    {
      // Open a new msg window only if we are double clicking on 
      // folders or newsgroups.
      MsgOpenNewWindowForFolder(folderResource.Value);

      // double clicking should not toggle the open / close state of the
      // folder.  this will happen if we don't prevent the event from
      // bubbling to the default handler in outliner.xml
      event.preventBubble();
    }
}

function ChangeSelection(outliner, newIndex)
{
    if(newIndex >= 0)
    {
        outliner.outlinerBoxObject.selection.select(newIndex);
        outliner.outlinerBoxObject.ensureRowIsVisible(newIndex);
    }
}

function SetActiveThreadPaneSortColumn(column)
{
	gActiveThreadPaneSortColumn = column;
}

function GetActiveThreadPaneSortColumn()
{
	return gActiveThreadPaneSortColumn;
}

function ClearActiveThreadPaneSortColumn()
{
	var activeColumn = document.getElementById(gActiveThreadPaneSortColumn);
	if(activeColumn)
	{
		activeColumn.removeAttribute("sortActive");
		activeColumn = "";
	}

}

function GetSelectedFolders()
{
    var folderArray = [];
    var k = 0;
    var folderOutliner = GetFolderOutliner();
    var rangeCount = folderOutliner.outlinerBoxObject.selection.getRangeCount();

    for(var i = 0; i < rangeCount; i++)
    {
        var startIndex = {};
        var endIndex = {};
        folderOutliner.outlinerBoxObject.selection.getRangeAt(i, startIndex, endIndex);
        for (var j = startIndex.value; j <= endIndex.value; j++)
        {
            var folderResource = GetFolderResource(folderOutliner, j);
            folderArray[k++] = folderResource.Value;
        }
    }

    return folderArray;
}

function GetSelectedMsgFolders()
{
    var folderArray = [];
    var k = 0;
    var folderOutliner = GetFolderOutliner();
    var rangeCount = folderOutliner.outlinerBoxObject.selection.getRangeCount();

    for(var i = 0; i < rangeCount; i++)
    {
        var startIndex = {};
        var endIndex = {};
        folderOutliner.outlinerBoxObject.selection.getRangeAt(i, startIndex, endIndex);
        for (var j = startIndex.value; j <= endIndex.value; j++)
        {
            var msgFolder = GetFolderResource(folderOutliner, j).QueryInterface(Components.interfaces.nsIMsgFolder);
            if(msgFolder)
                folderArray[k++] = msgFolder;
        }
    }

    return folderArray;
}

function GetFirstSelectedMessage()
{
    try {
        return gDBView.URIForFirstSelectedMessage;
    }
    catch (ex) {
        return null;
    }
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

function GetSelectedMessages()
{
  try {
    var messageArray = {}; 
    var length = {};
    gDBView.getURIsForSelection(messageArray,length);
    return messageArray.value;
  }
  catch (ex) {
    dump("ex = " + ex + "\n");
    return null;
  }
}

function GetLoadedMsgFolder()
{
    if (!gDBView) return null;
    return gDBView.msgFolder;
}

function GetLoadedMessage()
{
    try {
        return gDBView.URIForFirstSelectedMessage;
    }
    catch (ex) {
        return null;
    }
}

//Clear everything related to the current message. called after load start page.
function ClearMessageSelection()
{
	ClearThreadPaneSelection();
}

function GetCompositeDataSource(command)
{
	if (command == "GetNewMessages" || command == "NewFolder" || command == "MarkAllMessagesRead")
        return GetFolderDatasource();

	return null;
}

function SetNextMessageAfterDelete()
{
    gNextMessageViewIndexAfterDelete = gDBView.msgToSelectAfterDelete;
}

function EnsureAllAncestorsAreExpanded(outliner, resource)
{
    // get the parent of the desired folder, and then try to get
    // the index of the parent in the outliner
    var folder = resource.QueryInterface(Components.interfaces.nsIFolder);
    
    // if this is a server, there are no ancestors, so stop.
    var msgFolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
    if (msgFolder.isServer)
      return;

    var parentFolderResource = RDF.GetResource(folder.parent.URI);
    var folderIndex = GetFolderIndex(outliner, parentFolderResource);

    if (folderIndex == -1) {
      // if we couldn't find the parent, recurse
      EnsureAllAncestorsAreExpanded(outliner, parentFolderResource);
      // ok, now we should be able to find the parent
      folderIndex = GetFolderIndex(outliner, parentFolderResource);
    }

    // if the parent isn't open, open it
    if (!(outliner.outlinerBoxObject.view.isContainerOpen(folderIndex)))
      outliner.outlinerBoxObject.view.toggleOpenState(folderIndex);
}

function SelectFolder(folderUri)
{
    var folderOutliner = GetFolderOutliner();
    var folderResource = RDF.GetResource(folderUri);

    // before we can select a folder, we need to make sure it is "visible"
    // in the outliner.  to do that, we need to ensure that all its
    // ancestors are expanded
    EnsureAllAncestorsAreExpanded(folderOutliner, folderResource);
    var folderIndex = GetFolderIndex(folderOutliner, folderResource);
    ChangeSelection(folderOutliner, folderIndex);
}

function SelectMessage(messageUri)
{
  // this isn't going to work anymore
  dump("XXX fix this or remove SelectMessage()\n");
}

function ReloadMessage()
{
  gDBView.reloadMessage();
}

function SetBusyCursor(window, enable)
{
	if(enable)
		window.setCursor("wait");
	else
		window.setCursor("auto");

	var numFrames = window.frames.length;
	for(var i = 0; i < numFrames; i++)
		SetBusyCursor(window.frames[i], enable);
}

function GetDBView()
{
    return gDBView;
}

function GetFolderResource(outliner, index)
{
    var outlinerBuilder = outliner.outlinerBoxObject.outlinerBody.builder.QueryInterface(Components.interfaces.nsIXULOutlinerBuilder);
    return outlinerBuilder.getResourceAtIndex(index);
}

function GetFolderIndex(outliner, resource)
{
    var outlinerBuilder = outliner.outlinerBoxObject.outlinerBody.builder.QueryInterface(Components.interfaces.nsIXULOutlinerBuilder);
    return outlinerBuilder.getIndexOfResource(resource);
}

function GetFolderAttribute(outliner, source, attribute)
{
    var property = RDF.GetResource("http://home.netscape.com/NC-rdf#" + attribute);
    var target = outliner.outlinerBoxObject.outlinerBody.database.GetTarget(source, property, true);
    if (target)
        target = target.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    return target;
}
