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

// from MailNewsTypes.h
const nsMsgViewIndex_None = 0xFFFFFFFF;


var gFolderTree; 
var gMessagePane;
var gThreadTree;
var gSearchInput;

var gThreadAndMessagePaneSplitter = null;
var gUnreadCount = null;
var gTotalCount = null;

// cache these services
var gRDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService().QueryInterface(Components.interfaces.nsIRDFService);
var gDragService = null;
var nsIDragService = Components.interfaces.nsIDragService;


var gCurrentLoadingFolderURI;
var gCurrentFolderToReroot;
var gCurrentLoadingFolderSortType = 0;
var gCurrentLoadingFolderSortOrder = 0;
var gCurrentLoadingFolderViewType = 0;
var gCurrentLoadingFolderViewFlags = 0;
var gRerootOnFolderLoad = false;
var gCurrentDisplayedMessage = null;
var gNextMessageAfterDelete = null;
var gNextMessageAfterLoad = null;
var gNextMessageViewIndexAfterDelete = -2;
var gCurrentlyDisplayedMessage=nsMsgViewIndex_None;
var gStartFolderUri = null;
var gStartMsgKey = -1;
var gRightMouseButtonDown = false;
// Global var to keep track of which row in the thread pane has been selected
// This is used to make sure that the row with the currentIndex has the selection
// after a Delete or Move of a message that has a row index less than currentIndex.
var gThreadPaneCurrentSelectedIndex = -1;

// Global var to keep track of if the 'Delete Message' or 'Move To' thread pane
// context menu item was triggered.  This helps prevent the tree view from
// not updating on one of those menu item commands.
var gThreadPaneDeleteOrMoveOccurred = false;

//If we've loaded a message, set to true.  Helps us keep the start page around.
var gHaveLoadedMessage;

var gDisplayStartupPage = false;

// the folderListener object
var folderListener = {
    OnItemAdded: function(parentItem, item, view) { },

    OnItemRemoved: function(parentItem, item, view) { },

    OnItemPropertyChanged: function(item, property, oldValue, newValue) { },

    OnItemIntPropertyChanged: function(item, property, oldValue, newValue) {
      var currentLoadedFolder = GetThreadPaneFolder();
      if (!currentLoadedFolder) return;
      var currentURI = currentLoadedFolder.URI;

      //if we don't have a folder loaded, don't bother.
      if(currentURI) {
        if(property.GetUnicode() == "TotalMessages" || property.GetUnicode() == "TotalUnreadMessages") {
          var folder = item.QueryInterface(Components.interfaces.nsIMsgFolder);
          if(folder) {
            var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource); 
            if(folderResource) {
              var folderURI = folderResource.Value;
              if(currentURI == folderURI) {
                UpdateStatusMessageCounts(folder);
              }
            }
          }
        }      
      }
    },

    OnItemBoolPropertyChanged: function(item, property, oldValue, newValue) { },

    OnItemUnicharPropertyChanged: function(item, property, oldValue, newValue) { },
    OnItemPropertyFlagChanged: function(item, property, oldFlag, newFlag) { },

    OnItemEvent: function(folder, event) {
       var eventType = event.GetUnicode();
       if (eventType == "FolderLoaded") {
         if (folder) {
           var resource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
           if(resource) {
             var uri = resource.Value;
             if(uri == gCurrentFolderToReroot) {
               gCurrentFolderToReroot="";
               var msgFolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
               if(msgFolder) {
                 msgFolder.endFolderLoading();
                 // suppress command updating when rerooting the folder
                 // when rerooting, we'll be clearing the selection
                 // which will cause us to update commands.
                 if (gDBView) {
                   gDBView.suppressCommandUpdating = true;
                   // if the db's view isn't set, something went wrong and we should reroot
                   // the folder, which will re-open the view.
                   if (!gDBView.db)
                    gRerootOnFolderLoad = true;
                 }
                 if (gRerootOnFolderLoad)
                   RerootFolder(uri, msgFolder, gCurrentLoadingFolderViewType, gCurrentLoadingFolderViewFlags, gCurrentLoadingFolderSortType, gCurrentLoadingFolderSortOrder);

                 var db = msgFolder.getMsgDatabase(msgWindow);
                 if (db) 
                   db.resetHdrCacheSize(100);
                 
                 if (gDBView) {
                   gDBView.suppressCommandUpdating = false;
                 }

                 gIsEditableMsgFolder = IsSpecialFolder(msgFolder, MSG_FOLDER_FLAG_DRAFTS);

                 gCurrentLoadingFolderSortType = 0;
                 gCurrentLoadingFolderSortOrder = 0;
                 gCurrentLoadingFolderViewType = 0;
                 gCurrentLoadingFolderViewFlags = 0;

                 var scrolled = false;

                 LoadCurrentlyDisplayedMessage();  //used for rename folder msg loading after folder is loaded.

                 if (gStartMsgKey != -1) { 
                   // select the desired message
                   gDBView.selectMsgByKey(gStartMsgKey);
                   gStartMsgKey = -1;

                   // now scroll to it
	           var indicies = GetSelectedIndices(gDBView);
                   EnsureRowInThreadTreeIsVisible(indicies[0]);
                   scrolled = true;
                 }
                 if (gNextMessageAfterLoad) {
                   var type = gNextMessageAfterLoad;
                   gNextMessageAfterLoad = null;

                   // scroll to and select the proper message
                   scrolled = ScrollToMessage(type, true, true /* selectMessage */);
                 }
               }
             }
             if(uri == gCurrentLoadingFolderURI) {
               gCurrentLoadingFolderURI = "";
               //Now let's select the first new message if there is one
               if (!scrolled) {
                 // if we didn't just scroll, scroll to the first new message
                 // don't select it though
                 scrolled = ScrollToMessage(nsMsgNavigationType.firstNew, true, false /* selectMessage */);
                    
                 // if we failed to find a new message, scroll to the top
                 if (!scrolled) {
                   EnsureRowInThreadTreeIsVisible(0);
                 }
               }
               SetBusyCursor(window, false);
             }
           }
         }
       } 
       else if (eventType == "ImapHdrDownloaded") {
         if (folder) {
           var imapFolder = folder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
           if (imapFolder) {
            var hdrParser = imapFolder.hdrParser;
            if (hdrParser) {
              var msgHdr = hdrParser.GetNewMsgHdr();
              if (msgHdr)
              {
                var hdrs = hdrParser.headers;
                if (hdrs && hdrs.indexOf("X-attachment-size:") > 0) {
                  msgHdr.OrFlags(0x10000000);  // 0x10000000 is MSG_FLAG_ATTACHMENT
                }
                if (hdrs && hdrs.indexOf("X-image-size:") > 0) {
                  msgHdr.setStringProperty("imageSize", "1");
                }
              }
            }
           }
         }
       }
       else if (eventType == "DeleteOrMoveMsgCompleted") {
         HandleDeleteOrMoveMsgCompleted(folder);
       }     
       else if (eventType == "DeleteOrMoveMsgFailed") {
         HandleDeleteOrMoveMsgFailed(folder);
       }
       else if (eventType == "CompactCompleted") {
         HandleCompactCompleted(folder);
       }
       else if(eventType == "RenameCompleted") {
         SelectFolder(folder.URI);
       }
       else if (eventType == "msgLoaded") {
        OnMsgLoaded(folder, gCurrentDisplayedMessage);
       }
    }
}

var folderObserver = {
    canDropOn: function(index)
    {
        return CanDropOnFolderTree(index);
    },

    canDropBeforeAfter: function(index, before)
    {
        return CanDropBeforeAfterFolderTree(index, before);
    },

    onDrop: function(row, orientation)
    {
        DropOnFolderTree(row, orientation);
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
  gDBView.onDeleteCompleted(false);
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
  // you might not have a db view.  this can happen if
  // biff fires when the 3 pane is set to account central.
  if (!gDBView)
    return;

  gDBView.onDeleteCompleted(true);
  if (gNextMessageViewIndexAfterDelete != -2) 
  {
    if (IsCurrentLoadedFolder(folder)) 
    {
      var treeView = gDBView.QueryInterface(Components.interfaces.nsITreeView);
      var treeSelection = treeView.selection;
      if (gNextMessageViewIndexAfterDelete != nsMsgViewIndex_None) 
      {
        viewSize = treeView.rowCount;
        if (gNextMessageViewIndexAfterDelete >= viewSize) 
        {
          if (viewSize > 0)
            gNextMessageViewIndexAfterDelete = viewSize - 1;
          else
          {           
            gNextMessageViewIndexAfterDelete = nsMsgViewIndex_None;

            //there is nothing to select viewSize is 0

            treeSelection.clearSelection();
            setTitleFromFolder(folder,null);
            ClearMessagePane();
          }
        }
      }

      // if we are about to set the selection with a new element then DON'T clear
      // the selection then add the next message to select. This just generates
      // an extra round of command updating notifications that we are trying to
      // optimize away.
      if (gNextMessageViewIndexAfterDelete != nsMsgViewIndex_None) 
      {
        // when deleting a message we don't update the commands when the selection goes to 0
        // (we have a hack in nsMsgDBView which prevents that update) so there is no need to
        // update commands when we select the next message after the delete; the commands already
        // have the right update state...
        gDBView.suppressCommandUpdating = true;

        // This check makes sure that the tree does not perform a
        // selection on a non selected row (row < 0), else assertions will
        // be thrown.
        if (gNextMessageViewIndexAfterDelete >= 0)
          treeSelection.select(gNextMessageViewIndexAfterDelete);
        
        // if gNextMessageViewIndexAfterDelete has the same value 
        // as the last index we had selected, the tree won't generate a
        // selectionChanged notification for the tree view. So force a manual
        // selection changed call. (don't worry it's cheap if we end up calling it twice).
        if (treeView)
          treeView.selectionChanged();

        EnsureRowInThreadTreeIsVisible(gNextMessageViewIndexAfterDelete); 
        gDBView.suppressCommandUpdating = false;

        // hook for extra toolbar items
        // XXX I think there is a bug in the suppression code above.
        // what if I have two rows selected, and I hit delete, and so we load the next row.
        // what if I have commands that only enable where exactly one row is selected?
        var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
        observerService.notifyObservers(window, "mail:updateToolbarItems", null);
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
          dbFolderInfo = null;
        }
        RerootFolder(uri, msgFolder, viewType, viewFlags, sortType, sortOrder);
        LoadCurrentlyDisplayedMessage();
      }
    }
  }
}

function LoadCurrentlyDisplayedMessage()
{
  if (gCurrentlyDisplayedMessage != nsMsgViewIndex_None)
  {
    var treeView = gDBView.QueryInterface(Components.interfaces.nsITreeView);
    var treeSelection = treeView.selection;
    treeSelection.select(gCurrentlyDisplayedMessage);
    if (treeView)
      treeView.selectionChanged();
    EnsureRowInThreadTreeIsVisible(gCurrentlyDisplayedMessage);
    SetFocusThreadPane();
    gCurrentlyDisplayedMessage = nsMsgViewIndex_None; //reset
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
  SelectFolder(server.rootFolder.URI);
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
  setTimeout(delayedLoad, 0);
}

function delayedLoad() {
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
  // argument[0] --> folder uri
  // argument[1] --> optional message key

  if ("arguments" in window && window.arguments[0])
  {
    gStartFolderUri = window.arguments[0];
    gStartMsgKey = window.arguments[1];
  }
  else
  {
    gStartFolderUri = null;
    gStartMsgKey = -1;
  }

  setTimeout("loadStartFolder(gStartFolderUri);", 0);

  // FIX ME - later we will be able to use onload from the overlay
  OnLoadMsgHeaderPane();

  gHaveLoadedMessage = false;

  ThreadPaneOnLoad();

  //Set focus to the Thread Pane the first time the window is opened.
  SetFocusThreadPane();
}

function OnUnloadMessenger()
{
  accountManager.removeIncomingServerListener(gThreePaneIncomingServerListener);

  // FIX ME - later we will be able to use onload from the overlay
  OnUnloadMsgHeaderPane();

  OnMailWindowUnload();
}

function Create3PaneGlobals()
{
}
 
// because the "open" state persists, we'll call
// PerformExpand() for all servers that are open at startup.            
function PerformExpandForAllOpenServers()
{
    var folderTree = GetFolderTree();
    var view = folderTree.treeBoxObject.view;
    for (var i = 0; i < view.rowCount; i++)
    {
        if (view.isContainer(i))
        {
            var folderResource = GetFolderResource(folderTree, i);
            var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
            var isServer = GetFolderAttribute(folderTree, folderResource, "IsServer"); 
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
    var folderTree = GetFolderTree();
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
            var rootMsgFolder = defaultServer.rootMsgFolder;

            startFolderResource = rootMsgFolder.QueryInterface(Components.interfaces.nsIRDFResource);

            enabledNewMailCheckOnce = pref.getBoolPref(mailCheckOncePref);

            // Enable checknew mail once by turning checkmail pref 'on' to bring 
            // all users to one plane. This allows all users to go to Inbox. User can 
            // always go to server settings panel and turn off "Check for new mail at startup"
            if (!enabledNewMailCheckOnce)
            {
                pref.setBoolPref(mailCheckOncePref, true);
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
              defaultServer.PerformBiff();         
        } 

        // because the "open" state persists, we'll call
        // PerformExpand() for all servers that are open at startup.
        // note, because of the "news.persist_server_open_state_in_folderpane" pref
        // we don't persist the "open" state of news servers across sessions, 
        // but we do within a session, so if you open another 3 pane
        // and a news server is "open", we'll update the unread counts.
        PerformExpandForAllOpenServers();
    }
    catch(ex)
    {
        dump(ex);
        dump('Exception in LoadStartFolder caused by no default account.  We know about this\n');
    }

    if (!initialUri) 
    {
        MsgGetMessagesForAllServers(null);
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
        
        var nsIFolderListener = Components.interfaces.nsIFolderListener;
        var notifyFlags = nsIFolderListener.intPropertyChanged | nsIFolderListener.event;
        mailSession.AddFolderListener(folderListener, notifyFlags);
	} catch (ex) {
        dump("Error adding to session\n");
    }
}

function InitPanes()
{
    OnLoadFolderPane();
    OnLoadThreadPane();
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
        var label = {"true": "?folderTreeName", "false": "?folderTreeSimpleName"};
        folderNameCell.setAttribute("label", label[event.newValue]);
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
  var folderPaneUIVersion = pref.getIntPref("mail.ui.folderpane.version");

  if (folderPaneUIVersion == 1) {
    var folderUnreadCol = document.getElementById("folderUnreadCol");
    folderUnreadCol.setAttribute("hidden", "true");
    var folderTotalCol = document.getElementById("folderTotalCol");
    folderTotalCol.setAttribute("hidden", "true");
    pref.setIntPref("mail.ui.folderpane.version", 2);
  }
}

function OnLoadFolderPane()
{
    UpgradeFolderPaneUI();

    var folderUnreadCol = document.getElementById("folderUnreadCol");
    var hidden = folderUnreadCol.getAttribute("hidden");
    if (hidden != "true")
    {
        var folderNameCell = document.getElementById("folderNameCell");
        folderNameCell.setAttribute("label", "?folderTreeSimpleName");
    }
    folderUnreadCol.addEventListener("DOMAttrModified", OnFolderUnreadColAttrModified, false);

    //Add folderDataSource and accountManagerDataSource to folderPane
    accountManagerDataSource = accountManagerDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
    folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
    var database = GetFolderDatasource();

    database.AddDataSource(accountManagerDataSource);
    database.AddDataSource(folderDataSource);
    var folderTree = GetFolderTree();
    folderTree.setAttribute("ref", "msgaccounts:/");

    var folderTreeBuilder = folderTree.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder);
    folderTreeBuilder.addObserver(folderObserver);
    folderTree.addEventListener("click",FolderPaneOnClick,true);
    folderTree.addEventListener("mousedown",TreeOnMouseDown,true);
}

// builds prior to 12-08-2001 did not have the labels column
// in the thread pane.  so if a user ran an old build, and then
// upgraded, they get the new column, and this causes problems.
// We're trying to avoid a similar problem to bug #96979.
// to work around this, we hide the column once, using the 
// "mailnews.ui.threadpane.version" pref.
function UpgradeThreadPaneUI()
{
  var labelCol;
  var threadPaneUIVersion;

  try {
    threadPaneUIVersion = pref.getIntPref("mailnews.ui.threadpane.version");
    if (threadPaneUIVersion == 1) {
      labelCol = document.getElementById("labelCol");
      labelCol.setAttribute("hidden", "true");
      pref.setIntPref("mailnews.ui.threadpane.version", 2);
    }
	}
  catch (ex) {
    dump("UpgradeThreadPane: ex = " + ex + "\n");
  }
}

function OnLoadThreadPane()
{
    UpgradeThreadPaneUI();
}

function GetFolderDatasource()
{
    var folderTree = GetFolderTree();
    return folderTree.database;
}

/* Functions for accessing particular parts of the window*/
function GetFolderTree()
{
    if (! gFolderTree)
        gFolderTree = document.getElementById("folderTree");
    return gFolderTree;
}

function GetSearchInput()
{
    if (gSearchInput) return gSearchInput;
    gSearchInput = document.getElementById("searchInput");
    return gSearchInput;
}

function GetMessagePane()
{
    if (gMessagePane) return gMessagePane;
    gMessagePane = document.getElementById("messagepanebox");
    return gMessagePane;
}

function GetMessagePaneFrame()
{
    return window.content;
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

function IsFolderPaneCollapsed()
{
  var folderPaneBox = GetFolderTree().parentNode;
  return folderPaneBox.getAttribute("collapsed") == "true"
    || folderPaneBox.getAttribute("hidden") == "true";
}

function FindMessenger()
{
  return messenger;
}

function ClearThreadPaneSelection()
{
  try {
    if (gDBView) {
      var treeView = gDBView.QueryInterface(Components.interfaces.nsITreeView);
      var treeSelection = treeView.selection;
      if (treeSelection) 
        treeSelection.clearSelection(); 
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
    if (GetMessagePaneFrame().location != "about:blank")
        GetMessagePaneFrame().location = "about:blank";
    // hide the message header view AND the message pane...
		HideMessageHeaderPane();
	}
}


function GetSelectedFolderIndex()
{
    var folderTree = GetFolderTree();
    var startIndex = {};
    var endIndex = {};
    folderTree.treeBoxObject.selection.getRangeAt(0, startIndex, endIndex);
    return startIndex.value;
}

// Function to change the highlighted row to where the mouse was clicked
// without loading the contents of the selected row.
// It will also keep the outline/dotted line in the original row.
function ChangeSelectionWithoutContentLoad(event, tree)
{
    var row = {};
    var col = {};
    var elt = {};
    var treeBoxObj = tree.treeBoxObject;
    var treeSelection = treeBoxObj.selection;

    treeBoxObj.getCellAt(event.clientX, event.clientY, row, col, elt);
    // make sure that row.value is valid so that it doesn't mess up
    // the call to ensureRowIsVisible().
    if((row.value >= 0) && !treeSelection.isSelected(row.value))
    {
        var saveCurrentIndex = treeSelection.currentIndex;
        treeSelection.selectEventsSuppressed = true;
        treeSelection.select(row.value);
        treeSelection.currentIndex = saveCurrentIndex;
        treeBoxObj.ensureRowIsVisible(row.value);
        treeSelection.selectEventsSuppressed = false;

        // Keep track of which row in the thread pane is currently selected.
        if(tree.id == "threadTree")
          gThreadPaneCurrentSelectedIndex = row.value;
    }
    event.preventBubble();
}

function TreeOnMouseDown(event)
{
    // Detect right mouse click and change the highlight to the row
    // where the click happened without loading the message headers in
    // the Folder or Thread Pane.
    if (event.button == 2)
    {
      gRightMouseButtonDown = true;
      ChangeSelectionWithoutContentLoad(event, event.target.parentNode);
    }
    else
      gRightMouseButtonDown = false;
}

function FolderPaneOnClick(event)
{
    // we only care about button 0 (left click) events
    if (event.button != 0)
        return;

    var folderTree = GetFolderTree();
    var row = {};
    var col = {};
    var elt = {};
    folderTree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, col, elt);
    if (row.value == -1)
      return;

    if (elt.value == "twisty")
    {
        var folderResource = GetFolderResource(folderTree, row.value);
        var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);

        if (!(folderTree.treeBoxObject.view.isContainerOpen(row.value)))
        {
            var isServer = GetFolderAttribute(folderTree, folderResource, "IsServer");
            if (isServer == "true")
            {
                var server = msgFolder.server;
                server.performExpand(msgWindow);
            }
            else
            {
                var serverType = GetFolderAttribute(folderTree, folderResource, "ServerType");
                if (serverType == "imap")
                {
                    var imapFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
                    imapFolder.performExpand(msgWindow);
                }
            }
        }
    }
    else if ((event.originalTarget.localName == "treecol") ||
             (event.originalTarget.localName == "slider") ||
             (event.originalTarget.localName == "scrollbarbutton")) {
      // clicking on the name column in the folder pane should not sort
      event.preventBubble();
    }
    else if (event.detail == 2) {
      FolderPaneDoubleClick(row.value, event);
    }
    else if (gDBView && gDBView.isSearchView)
    {
      onClearSearch();
    }

}

function FolderPaneDoubleClick(folderIndex, event)
{
    var folderTree = GetFolderTree();
    var folderResource = GetFolderResource(folderTree, folderIndex);
    var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    var isServer = GetFolderAttribute(folderTree, folderResource, "IsServer");

    if (isServer == "true")
    {
      if (!(folderTree.treeBoxObject.view.isContainerOpen(folderIndex)))
      {
        var server = msgFolder.server;
        server.performExpand(msgWindow);
      }
    }
    else 
    {
      // Open a new msg window only if we are double clicking on 
      // folders or newsgroups.
      MsgOpenNewWindowForFolder(folderResource.Value, -1 /* key */);

      // double clicking should not toggle the open / close state of the
      // folder.  this will happen if we don't prevent the event from
      // bubbling to the default handler in tree.xml
      event.preventBubble();
    }
}

function ChangeSelection(tree, newIndex)
{
    if(newIndex >= 0)
    {
        tree.treeBoxObject.selection.select(newIndex);
        tree.treeBoxObject.ensureRowIsVisible(newIndex);
    }
}

function GetSelectedFolders()
{
    var folderArray = [];
    var k = 0;
    var folderTree = GetFolderTree();
    var rangeCount = folderTree.treeBoxObject.selection.getRangeCount();

    for(var i = 0; i < rangeCount; i++)
    {
        var startIndex = {};
        var endIndex = {};
        folderTree.treeBoxObject.selection.getRangeAt(i, startIndex, endIndex);
        for (var j = startIndex.value; j <= endIndex.value; j++)
        {
            var folderResource = GetFolderResource(folderTree, j);
            folderArray[k++] = folderResource.Value;
        }
    }

    return folderArray;
}

function GetSelectedMsgFolders()
{
    var folderArray = [];
    var k = 0;
    var folderTree = GetFolderTree();
    var rangeCount = folderTree.treeBoxObject.selection.getRangeCount();

    for(var i = 0; i < rangeCount; i++)
    {
        var startIndex = {};
        var endIndex = {};
        folderTree.treeBoxObject.selection.getRangeAt(i, startIndex, endIndex);
        for (var j = startIndex.value; j <= endIndex.value; j++)
        {
          var folderResource = GetFolderResource(folderTree, j); 
          if (folderResource.Value != "http://home.netscape.com/NC-rdf#PageTitleFakeAccount") {
            var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);

            if(msgFolder)
              folderArray[k++] = msgFolder;
          }
        }
    }

    return folderArray;
}

function GetFirstSelectedMessage()
{
    try {
        // Use this instead of gDBView.URIForFirstSelectedMessage, else it
        // will return the currentIndex message instead of the highlighted
        // message.
        //
        // note, there may not be any selected messages
        //
        // XXX todo
        // is this inefficient when we've got a lot of message selected?
        var selectedMessages = GetSelectedMessages();
        if (selectedMessages)
          return selectedMessages[0];
        else
          return null;
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
  var treeSelection = GetThreadTree().treeBoxObject.selection;

  gThreadPaneDeleteOrMoveOccurred = true;
  if (treeSelection.isSelected(treeSelection.currentIndex))
    gNextMessageViewIndexAfterDelete = gDBView.msgToSelectAfterDelete;
  else if (treeSelection.currentIndex > gThreadPaneCurrentSelectedIndex)
    // Since the currentIndex (the row with the outline/dotted line) is greater
    // than the currently selected row (the row that is highlighted), we need to
    // make sure that upon a Delete or Move of the selected row, the highlight
    // returns to the currentIndex'ed row.  It is necessary to subtract 1
    // because the row being deleted is above the row with the currentIndex.
    // If the subtraction is not done, then the highlight will end up on the
    // row listed after the currentIndex'ed row.
    gNextMessageViewIndexAfterDelete = treeSelection.currentIndex - 1;
  else
    gNextMessageViewIndexAfterDelete = treeSelection.currentIndex;
}

function EnsureAllAncestorsAreExpanded(tree, resource)
{
    // get the parent of the desired folder, and then try to get
    // the index of the parent in the tree
    var folder = resource.QueryInterface(Components.interfaces.nsIFolder);
    
    // if this is a server, there are no ancestors, so stop.
    var msgFolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
    if (msgFolder.isServer)
      return;

    var parentFolderResource = RDF.GetResource(folder.parent.URI);
    var folderIndex = GetFolderIndex(tree, parentFolderResource);

    if (folderIndex == -1) {
      // if we couldn't find the parent, recurse
      EnsureAllAncestorsAreExpanded(tree, parentFolderResource);
      // ok, now we should be able to find the parent
      folderIndex = GetFolderIndex(tree, parentFolderResource);
    }

    // if the parent isn't open, open it
    if (!(tree.treeBoxObject.view.isContainerOpen(folderIndex)))
      tree.treeBoxObject.view.toggleOpenState(folderIndex);
}

function SelectFolder(folderUri)
{
    var folderTree = GetFolderTree();
    var folderResource = RDF.GetResource(folderUri);

    // before we can select a folder, we need to make sure it is "visible"
    // in the tree.  to do that, we need to ensure that all its
    // ancestors are expanded
    EnsureAllAncestorsAreExpanded(folderTree, folderResource);
    var folderIndex = GetFolderIndex(folderTree, folderResource);
    ChangeSelection(folderTree, folderIndex);
}

function SelectMessage(messageUri)
{
  var msgHdr = messenger.messageServiceFromURI(messageUri).messageURIToMsgHdr(messageUri);
  if (msgHdr)
    gDBView.selectMsgByKey(msgHdr.messageKey);
}

function ReloadWithAllParts()
{
  gDBView.reloadMessageWithAllParts();
}

function ReloadMessage()
{
  gDBView.reloadMessage();
}

function SetBusyCursor(window, enable)
{
    // setCursor() is only available for chrome windows.
    // However one of our frames is the start page which 
    // is a non-chrome window, so check if this window has a
    // setCursor method
    if ("setCursor" in window) {
        if (enable)
            window.setCursor("wait");
        else
            window.setCursor("auto");
    }

	var numFrames = window.frames.length;
	for(var i = 0; i < numFrames; i++)
		SetBusyCursor(window.frames[i], enable);
}

function GetDBView()
{
    return gDBView;
}

function GetFolderResource(tree, index)
{
    return tree.builderView.getResourceAtIndex(index);
}

function GetFolderIndex(tree, resource)
{
    return tree.builderView.getIndexOfResource(resource);
}

function GetFolderAttribute(tree, source, attribute)
{
    var property = RDF.GetResource("http://home.netscape.com/NC-rdf#" + attribute);
    var target = tree.database.GetTarget(source, property, true);
    if (target)
        target = target.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    return target;
}

// Controller object for folder pane
var FolderPaneController =
{
   supportsCommand: function(command)
	{
		switch ( command )
		{
			case "cmd_delete":
			case "button_delete":
			case "cmd_selectAll":
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return true;
				
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
    if (IsFakeAccount()) 
      return false;

		switch ( command )
		{
			case "cmd_selectAll":
                                // the folder pane (currently)
                                // only handles single selection
                                // so we forward select all to the thread pane
                                // if there is no DBView
                                // don't bother sending to the thread pane
                                // this can happen when we've selected a server
                                // and account central is displayed
                                return (gDBView != null);
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return false;
			case "cmd_delete":
			case "button_delete":
			if ( command == "cmd_delete" )
				goSetMenuValue(command, 'valueFolder');
      var folderTree = GetFolderTree();
      var startIndex = {};
      var endIndex = {};
      folderTree.treeBoxObject.selection.getRangeAt(0, startIndex, endIndex);
      if (startIndex.value >= 0) {
        var canDeleteThisFolder;
				var specialFolder = null;
				var isServer = null;
				var serverType = null;
				try {
          var folderResource = GetFolderResource(folderTree, startIndex.value);
          specialFolder = GetFolderAttribute(folderTree, folderResource, "SpecialFolder");
          isServer = GetFolderAttribute(folderTree, folderResource, "IsServer");
          serverType = GetFolderAttribute(folderTree, folderResource, "ServerType");
          if (serverType == "nntp") {
			     	if ( command == "cmd_delete" ) {
					      goSetMenuValue(command, 'valueNewsgroup');
				    	  goSetAccessKey(command, 'valueNewsgroupAccessKey');
            }
          }
				}
				catch (ex) {
					//dump("specialFolder failure: " + ex + "\n");
				} 
        if (specialFolder == "Inbox" || specialFolder == "Trash" || specialFolder == "Drafts" || specialFolder == "Sent" || specialFolder == "Templates" || specialFolder == "Unsent Messages" || isServer == "true")
          canDeleteThisFolder = false;
        else
          canDeleteThisFolder = true;
        return canDeleteThisFolder && isCommandEnabled(command);
      }
			else
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

		switch ( command )
		{
			case "cmd_delete":
			case "button_delete":
				MsgDeleteFolder();
				break;
			case "cmd_selectAll":
                                // the folder pane (currently)
                                // only handles single selection
                                // so we forward select all to the thread pane
                                SendCommandToThreadPane(command);
                                break;
		}
	},
	
	onEvent: function(event)
	{
		// on blur events set the menu item texts back to the normal values
		if ( event == 'blur' )
        {
			goSetMenuValue('cmd_delete', 'valueDefault');
        }
	}
};


// Controller object for thread pane
var ThreadPaneController =
{
   supportsCommand: function(command)
	{
		switch ( command )
		{
			case "cmd_selectAll":
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return true;
				
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
		switch ( command )
		{
			case "cmd_selectAll":
				return true;
			
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
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
    if (!gDBView) return;

		switch ( command )
		{
			case "cmd_selectAll":
                // if in threaded mode, the view will expand all before selecting all
                gDBView.doCommand(nsMsgViewCommandType.selectAll)
                if (gDBView.numSelected != 1) {
                    setTitleFromFolder(gDBView.msgFolder,null);
                    ClearMessagePane();
                }
                break;
		}
	},
	
	onEvent: function(event)
	{
	}
};

// DefaultController object (handles commands when one of the trees does not have focus)
var DefaultController =
{
   supportsCommand: function(command)
	{

		switch ( command )
		{
      case "cmd_createFilterFromPopup":
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
      case "cmd_createFilterFromMenu":
			case "cmd_delete":
			case "button_delete":
			case "cmd_shiftDelete":
			case "cmd_nextMsg":
      case "button_next":
			case "cmd_nextUnreadMsg":
			case "cmd_nextFlaggedMsg":
			case "cmd_nextUnreadThread":
			case "cmd_previousMsg":
			case "cmd_previousUnreadMsg":
			case "cmd_previousFlaggedMsg":
			case "cmd_viewAllMsgs":
			case "cmd_viewUnreadMsgs":
      case "cmd_viewThreadsWithUnread":
      case "cmd_viewWatchedThreadsWithUnread":
      case "cmd_viewIgnoredThreads":
      case "cmd_undo":
      case "cmd_redo":
			case "cmd_expandAllThreads":
			case "cmd_collapseAllThreads":
			case "cmd_renameFolder":
			case "cmd_sendUnsentMsgs":
			case "cmd_openMessage":
      case "button_print":
			case "cmd_print":
			case "cmd_printSetup":
			case "cmd_saveAsFile":
			case "cmd_saveAsTemplate":
            case "cmd_properties":
			case "cmd_viewPageSource":
			case "cmd_setFolderCharset":
			case "cmd_reload":
      case "button_getNewMessages":
			case "cmd_getNewMessages":
      case "cmd_getMsgsForAuthAccounts":
			case "cmd_getNextNMessages":
			case "cmd_find":
			case "cmd_findAgain":
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
			case "cmd_emptyTrash":
			case "cmd_compactFolder":
			case "cmd_sortByThread":
  	  case "cmd_settingsOffline":
      case "cmd_close":
      case "cmd_selectThread":
				return true;
      case "cmd_downloadFlagged":
      case "cmd_downloadSelected":
      case "cmd_synchronizeOffline":
        return(CheckOnline());

      case "cmd_watchThread":
      case "cmd_killThread":
        return(isNewsURI(GetFirstSelectedMessage()));

			default:
				return false;
		}
	},

  isCommandEnabled: function(command)
  {
    var enabled = new Object();
    enabled.value = false;
    var checkStatus = new Object();

    if (IsFakeAccount()) 
      return false;

    // note, all commands that get fired on a single key need to check MailAreaHasFocus() as well
    switch ( command )
    {
      case "cmd_delete":
        UpdateDeleteCommand();
        // fall through
      case "button_delete":
        if (gDBView)
          gDBView.getCommandStatus(nsMsgViewCommandType.deleteMsg, enabled, checkStatus);
        return enabled.value;
      case "cmd_shiftDelete":
        if (gDBView)
          gDBView.getCommandStatus(nsMsgViewCommandType.deleteNoTrash, enabled, checkStatus);
        return enabled.value;
      case "cmd_killThread":
        return ((GetNumSelectedMessages() == 1) && MailAreaHasFocus() && IsViewNavigationItemEnabled());
      case "cmd_watchThread":
        if (MailAreaHasFocus() && (GetNumSelectedMessages() == 1) && gDBView)
          gDBView.getCommandStatus(nsMsgViewCommandType.toggleThreadWatched, enabled, checkStatus);
        return enabled.value;
      case "cmd_createFilterFromPopup":
        var loadedFolder = GetLoadedMsgFolder();
        if (!(loadedFolder && loadedFolder.server.canHaveFilters))
          return false;
      case "cmd_createFilterFromMenu":
        loadedFolder = GetLoadedMsgFolder();
        if (!(loadedFolder && loadedFolder.server.canHaveFilters) || !(IsMessageDisplayedInMessagePane()))
          return false;
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
      case "cmd_openMessage":
      case "button_print":
      case "cmd_print":
      case "cmd_saveAsFile":
      case "cmd_saveAsTemplate":
      case "cmd_viewPageSource":
      case "cmd_reload":
	      if ( GetNumSelectedMessages() > 0)
        {
          if (gDBView)
          {
             gDBView.getCommandStatus(nsMsgViewCommandType.cmdRequiringMsgBody, enabled, checkStatus);
              return enabled.value;
          }
        }
        return false;
      case "cmd_printSetup":
        return true;
      case "cmd_markAsFlagged":
      case "button_file":
      case "cmd_file":
        return (GetNumSelectedMessages() > 0 );
      case "button_mark":
      case "cmd_markAsRead":
      case "cmd_markThreadAsRead":
      case "cmd_label0":
      case "cmd_label1":
      case "cmd_label2":
      case "cmd_label3":
      case "cmd_label4":
      case "cmd_label5":
        return(MailAreaHasFocus() && GetNumSelectedMessages() > 0);
      case "button_next":
        return IsViewNavigationItemEnabled();
      case "cmd_nextMsg":
      case "cmd_nextUnreadMsg":
      case "cmd_nextUnreadThread":
      case "cmd_previousMsg":
      case "cmd_previousUnreadMsg":
        return (MailAreaHasFocus() && IsViewNavigationItemEnabled());
      case "cmd_markAllRead":
        return (MailAreaHasFocus() && IsFolderSelected());
      case "cmd_find":
      case "cmd_findAgain":
        return IsMessageDisplayedInMessagePane();
        break;
      case "cmd_search":
        return IsCanSearchMessagesEnabled();
      // these are enabled on when we are in threaded mode
      case "cmd_selectThread":
        if (GetNumSelectedMessages() <= 0) return false;
      case "cmd_expandAllThreads":
      case "cmd_collapseAllThreads":
        if (!gDBView) return false;
          return (gDBView.sortType == nsMsgViewSortType.byThread);
        break;
      case "cmd_nextFlaggedMsg":
      case "cmd_previousFlaggedMsg":
        return IsViewNavigationItemEnabled();
      case "cmd_viewAllMsgs":
      case "cmd_sortByThread":
      case "cmd_viewUnreadMsgs":
      case "cmd_viewThreadsWithUnread":
      case "cmd_viewWatchedThreadsWithUnread":
      case "cmd_viewIgnoredThreads":
      case "cmd_stop":
        return true;
      case "cmd_undo":
      case "cmd_redo":
          return SetupUndoRedoCommand(command);
      case "cmd_renameFolder":
        return IsRenameFolderEnabled();
      case "cmd_sendUnsentMsgs":
        return IsSendUnsentMsgsEnabled(null);
      case "cmd_properties":
        return IsPropertiesEnabled(command);
      case "button_getNewMessages":
      case "cmd_getNewMessages":
      case "cmd_getMsgsForAuthAccounts":
        return IsGetNewMessagesEnabled();
      case "cmd_getNextNMessages":
        return IsGetNextNMessagesEnabled();
      case "cmd_emptyTrash":
        return IsEmptyTrashEnabled();
      case "cmd_compactFolder":
        return IsCompactFolderEnabled();
      case "cmd_setFolderCharset":
        return IsFolderCharsetEnabled();
      case "cmd_close":
        return true;
      case "cmd_downloadFlagged":
        return(CheckOnline());
      case "cmd_downloadSelected":
        return(MailAreaHasFocus() && IsFolderSelected() && CheckOnline() && GetNumSelectedMessages() > 0);
      case "cmd_synchronizeOffline":
        return CheckOnline() && IsAccountOfflineEnabled();       
      case "cmd_settingsOffline":
        return (MailAreaHasFocus() && IsAccountOfflineEnabled());
      default:
        return false;
    }
    return false;
  },

	doCommand: function(command)
	{
    // if the user invoked a key short cut then it is possible that we got here for a command which is
    // really disabled. kick out if the command should be disabled.
    if (!this.isCommandEnabled(command)) return;
   
		switch ( command )
		{
			case "cmd_close":
				CloseMailWindow();
				break;
      case "button_getNewMessages":
			case "cmd_getNewMessages":
				MsgGetMessage();
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
      case "cmd_createFilterFromMenu":
        MsgCreateFilter();
        break;        
      case "cmd_createFilterFromPopup":
        break;// This does nothing because the createfilter is invoked from the popupnode oncommand.
			case "button_delete":
			case "cmd_delete":
        SetNextMessageAfterDelete();
        gDBView.doCommand(nsMsgViewCommandType.deleteMsg);
				break;
			case "cmd_shiftDelete":
        SetNextMessageAfterDelete();
        gDBView.doCommand(nsMsgViewCommandType.deleteNoTrash);
				break;
      case "cmd_killThread":
        /* kill thread kills the thread and then does a next unread */
      	GoNextMessage(nsMsgNavigationType.toggleThreadKilled, true);
        break;
      case "cmd_watchThread":
        gDBView.doCommand(nsMsgViewCommandType.toggleThreadWatched);
        break;
      case "button_next":
			case "cmd_nextUnreadMsg":
				MsgNextUnreadMessage();
				break;
			case "cmd_nextUnreadThread":
				MsgNextUnreadThread();
				break;
			case "cmd_nextMsg":
				MsgNextMessage();
				break;
			case "cmd_nextFlaggedMsg":
				MsgNextFlaggedMessage();
				break;
			case "cmd_previousMsg":
				MsgPreviousMessage();
				break;
			case "cmd_previousUnreadMsg":
				MsgPreviousUnreadMessage();
				break;
			case "cmd_previousFlaggedMsg":
				MsgPreviousFlaggedMessage();
				break;
			case "cmd_sortByThread":
				MsgSortByThread();
				break;
			case "cmd_viewAllMsgs":
      case "cmd_viewThreadsWithUnread":
      case "cmd_viewWatchedThreadsWithUnread":
			case "cmd_viewUnreadMsgs":
      case "cmd_viewIgnoredThreads":
				SwitchView(command);
				break;
			case "cmd_undo":
				messenger.Undo(msgWindow);
				break;
			case "cmd_redo":
				messenger.Redo(msgWindow);
				break;
			case "cmd_expandAllThreads":
                gDBView.doCommand(nsMsgViewCommandType.expandAll);
				break;
			case "cmd_collapseAllThreads":
                gDBView.doCommand(nsMsgViewCommandType.collapseAll);
				break;
			case "cmd_renameFolder":
				MsgRenameFolder();
				return;
			case "cmd_sendUnsentMsgs":
				MsgSendUnsentMsgs();
				return;
			case "cmd_openMessage":
                MsgOpenSelectedMessages();
				return;
			case "cmd_printSetup":
			  goPageSetup();
			  return;
			case "cmd_print":
				PrintEnginePrint();
				return;
			case "cmd_saveAsFile":
				MsgSaveAsFile();
				return;
			case "cmd_saveAsTemplate":
				MsgSaveAsTemplate();
				return;
			case "cmd_viewPageSource":
				MsgViewPageSource();
				return;
			case "cmd_setFolderCharset":
				MsgSetFolderCharset();
				return;
			case "cmd_reload":
				MsgReload();
				return;
			case "cmd_find":
				MsgFind();
				return;
			case "cmd_findAgain":
				MsgFindAgain();
				return;
            case "cmd_properties":
                MsgFolderProperties();
                return;
      case "cmd_search":
        MsgSearchMessages();
      case "button_mark":
			case "cmd_markAsRead":
				MsgMarkMsgAsRead(null);
				return;
			case "cmd_markThreadAsRead":
				MsgMarkThreadAsRead();
				return;
			case "cmd_markAllRead":
                gDBView.doCommand(nsMsgViewCommandType.markAllRead);
				return;
      case "cmd_stop":
        MsgStop();
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
			case "cmd_emptyTrash":
				MsgEmptyTrash();
				return;
			case "cmd_compactFolder":
				MsgCompactFolder(true);
				return;
            case "cmd_downloadFlagged":
                MsgDownloadFlagged();
                break;
            case "cmd_downloadSelected":
                MsgDownloadSelected();
                break;
            case "cmd_synchronizeOffline":
                MsgSynchronizeOffline();
                break;
            case "cmd_settingsOffline":
                MsgSettingsOffline();
                break;
            case "cmd_selectThread":
                gDBView.doCommand(nsMsgViewCommandType.selectThread);
                break;
		}
	},
	
	onEvent: function(event)
	{
		// on blur events set the menu item texts back to the normal values
		if ( event == 'blur' )
        {
			goSetMenuValue('cmd_delete', 'valueDefault');
            goSetMenuValue('cmd_undo', 'valueDefault');
            goSetMenuValue('cmd_redo', 'valueDefault');
        }
	}
};

function MailAreaHasFocus()
{
  //Input and TextAreas should get access to the keys that cause these commands.
  //Currently if we don't do this then we will steal the key away and you can't type them
  //in these controls. This is a bug that should be fixed and when it is we can get rid of
  //this.
  var focusedElement = top.document.commandDispatcher.focusedElement;
  if (focusedElement) 
  {
    var name = focusedElement.localName.toLowerCase();
    return ((name != "input") && (name != "textarea"));
  }

  // check if the message pane has focus 
  // see bug #129988
  if (GetMessagePane() == WhichPaneHasFocus())
    return true;

  // if there is no focusedElement,
  // and the message pane doesn't have focus
  // then a mail area can't be focused
  // see bug #128101
  return false;
}

function GetNumSelectedMessages()
{
    try {
        return gDBView.numSelected;
    }
    catch (ex) {
        return 0;
    }
}

var gLastFocusedElement=null;

function FocusRingUpdate_Mail()
{
  // WhichPaneHasFocus() uses on top.document.commandDispatcher.focusedElement
  // to determine which pane has focus
  // if the focusedElement is null, we're here on a blur.
  // nsFocusController::Blur() calls nsFocusController::SetFocusedElement(null), 
  // which will update any commands listening for "focus".
  // we really only care about nsFocusController::Focus() happens, 
  // which calls nsFocusController::SetFocusedElement(element)
  var currentFocusedElement = WhichPaneHasFocus();
  if (!currentFocusedElement)
    return;
      
	if (currentFocusedElement != gLastFocusedElement) {
    currentFocusedElement.setAttribute("focusring", "true");
    
    if (gLastFocusedElement)
      gLastFocusedElement.removeAttribute("focusring");

    gLastFocusedElement = currentFocusedElement;

    // since we just changed the pane with focus we need to update the toolbar to reflect this
    UpdateMailToolbar("focus");
  }
}

function WhichPaneHasFocus()
{
  var threadTree = GetThreadTree();
  var searchInput = GetSearchInput();
  var folderTree = GetFolderTree();
  var messagePane = GetMessagePane();
    
  if (top.document.commandDispatcher.focusedWindow == GetMessagePaneFrame())
    return messagePane;

	var currentNode = top.document.commandDispatcher.focusedElement;	
	while (currentNode) {
    if (currentNode === threadTree ||
        currentNode === searchInput || 
        currentNode === folderTree ||
        currentNode === messagePane)
      return currentNode;
    			
		currentNode = currentNode.parentNode;
  }
	
	return null;
}

function SetupCommandUpdateHandlers()
{
	var widget;
	
	// folder pane
	widget = GetFolderTree();
	if ( widget )
		widget.controllers.appendController(FolderPaneController);
	
	// thread pane
	widget = GetThreadTree();
	if ( widget )
        widget.controllers.appendController(ThreadPaneController);
		
	top.controllers.insertControllerAt(0, DefaultController);
}

function IsSendUnsentMsgsEnabled(folderResource)
{
  var identity;

  if (messenger.sendingUnsentMsgs) // if we're currently sending unsent msgs, disable this cmd.
    return false;
  try {
    if (folderResource) {
      // if folderResource is non-null, it is
      // resource for the "Unsent Messages" folder
      // we're here because we've done a right click on the "Unsent Messages"
      // folder (context menu)
      var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
      return (msgFolder.getTotalMessages(false) > 0);
    }
    else {
      var folders = GetSelectedMsgFolders();
      if (folders.length > 0) {
        identity = getIdentityForServer(folders[0].server);
      }
    }
  }
  catch (ex) {
    dump("ex = " + ex + "\n");
    identity = null;
  }

  try {
    if (!identity) {
      var am = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
      identity = am.defaultAccount.defaultIdentity;
    }

    var msgSendlater = Components.classes["@mozilla.org/messengercompose/sendlater;1"].getService(Components.interfaces.nsIMsgSendLater);
    var unsentMsgsFolder = msgSendlater.getUnsentMessagesFolder(identity);
    return (unsentMsgsFolder.getTotalMessages(false) > 0);
  }
  catch (ex) {
    dump("ex = " + ex + "\n");
  }
  return false;
}

function IsRenameFolderEnabled()
{
    var folderTree = GetFolderTree();
    var selection = folderTree.treeBoxObject.selection;
    if (selection.count == 1)
    {
        var startIndex = {};
        var endIndex = {};
        selection.getRangeAt(0, startIndex, endIndex);
        var folderResource = GetFolderResource(folderTree, startIndex.value);
        var canRename = GetFolderAttribute(folderTree, folderResource, "CanRename") == "true";
        return canRename && isCommandEnabled("cmd_renameFolder");
    }
    else
        return false;
}

function IsCanSearchMessagesEnabled()
{
  var folderURI = GetSelectedFolderURI();
  var server = GetServer(folderURI);
  return server.canSearchMessages;
}
function IsFolderCharsetEnabled()
{
  return IsFolderSelected();
}

function IsPropertiesEnabled(command)
{
   try 
   {
      var serverType;
      var folderTree = GetFolderTree();
      var folderResource = GetSelectedFolderResource();
      
      serverType = GetFolderAttribute(folderTree, folderResource, "ServerType");
   
      switch (serverType)
      {
        case "none":
        case "imap":
        case "pop3":
          goSetMenuValue(command, 'valueFolder');
          break;
        case "nntp":
          goSetMenuValue(command, 'valueNewsgroup');
          break
        default:
          goSetMenuValue(command, 'valueGeneric');
      }
   }
   catch (ex) 
   {
      //properties menu failure
   }
  return IsFolderSelected();
  
}

function IsViewNavigationItemEnabled()
{
	return IsFolderSelected();
}

function IsFolderSelected()
{
    var folderTree = GetFolderTree();
    var selection = folderTree.treeBoxObject.selection;
    if (selection.count == 1)
    {
        var startIndex = {};
        var endIndex = {};
        selection.getRangeAt(0, startIndex, endIndex);
        var folderResource = GetFolderResource(folderTree, startIndex.value);
        return GetFolderAttribute(folderTree, folderResource, "IsServer") != "true";
    }
    else
        return false;
}

function IsMessageDisplayedInMessagePane()
{
	return (!IsThreadAndMessagePaneSplitterCollapsed() && (GetNumSelectedMessages() > 0));
}

function MsgDeleteFolder()
{
    var folderTree = GetFolderTree();
    var selectedFolders = GetSelectedMsgFolders();
    for (var i = 0; i < selectedFolders.length; i++)
    {
        var selectedFolder = selectedFolders[i];
        var folderResource = selectedFolder.QueryInterface(Components.interfaces.nsIRDFResource);
        var specialFolder = GetFolderAttribute(folderTree, folderResource, "SpecialFolder");
        if (specialFolder != "Inbox" && specialFolder != "Trash")
        {
            var protocolInfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + selectedFolder.server.type].getService(Components.interfaces.nsIMsgProtocolInfo);

            // do not allow deletion of special folders on imap accounts
            if ((specialFolder == "Sent" || 
                specialFolder == "Drafts" || 
                specialFolder == "Templates") &&
                !protocolInfo.specialFoldersDeletionAllowed)
            {
                var messengerBundle = document.getElementById("bundle_messenger");
                var errorMessage = messengerBundle.getFormattedString("specialFolderDeletionErr",
                                                    [specialFolder]);
                var specialFolderDeletionErrTitle = gMessengerBundle.getString("specialFolderDeletionErrTitle");
                var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
                promptService.alert(window, specialFolderDeletionErrTitle, errorMessage);
                continue;
            }   
            else if (isNewsURI(folderResource.Value))
            {
                var unsubscribe = ConfirmUnsubscribe(selectedFolder);
                if (unsubscribe)
                    UnSubscribe(selectedFolder);
            }
            else
            {
                var parentResource = selectedFolder.parent.QueryInterface(Components.interfaces.nsIRDFResource);
                messenger.DeleteFolders(GetFolderDatasource(), parentResource, folderResource);
            }
        }
    }
}

function SetFocusThreadPaneIfNotOnMessagePane()
{
  var focusedElement = WhichPaneHasFocus();

  if((focusedElement != GetThreadTree()) &&
     (focusedElement != GetMessagePane()))
     SetFocusThreadPane();
}

// 3pane related commands.  Need to go in own file.  Putting here for the moment.
function MsgNextMessage()
{
	GoNextMessage(nsMsgNavigationType.nextMessage, false );
}

function MsgNextUnreadMessage()
{
	GoNextMessage(nsMsgNavigationType.nextUnreadMessage, true);
}
function MsgNextFlaggedMessage()
{
	GoNextMessage(nsMsgNavigationType.nextFlagged, true);
}

function MsgNextUnreadThread()
{
	//First mark the current thread as read.  Then go to the next one.
	MsgMarkThreadAsRead();
	GoNextMessage(nsMsgNavigationType.nextUnreadThread, true);
}

function MsgPreviousMessage()
{
    GoNextMessage(nsMsgNavigationType.previousMessage, false);
}

function MsgPreviousUnreadMessage()
{
	GoNextMessage(nsMsgNavigationType.previousUnreadMessage, true);
}

function MsgPreviousFlaggedMessage()
{
	GoNextMessage(nsMsgNavigationType.previousFlagged, true);
}

function GetFolderNameFromUri(uri, tree)
{
	var folderResource = RDF.GetResource(uri);

	var db = tree.database;

	var nameProperty = RDF.GetResource('http://home.netscape.com/NC-rdf#Name');

	var nameResult;
	try {
		nameResult = db.GetTarget(folderResource, nameProperty , true);
	}
	catch (ex) {
		return "";
	}

	nameResult = nameResult.QueryInterface(Components.interfaces.nsIRDFLiteral);
	return nameResult.Value;
}

/* XXX hiding the search bar while it is focus kills the keyboard so we focus the thread pane */
function SearchBarToggled()
{
  var searchBox = document.getElementById('searchBox');
  if (searchBox)
  {
    var attribValue = searchBox.getAttribute("hidden") ;
    if (attribValue == "true")
    {
      /*come out of quick search view */
      if (gDBView && gDBView.isSearchView)
        onClearSearch();
    }
    else
    {
      /*we have to initialize searchInput because we cannot do it when searchBox is hidden */
      var searchInput = GetSearchInput();
      searchInput.value="";
    }
  }

  for (var currentNode = top.document.commandDispatcher.focusedElement; currentNode; currentNode = currentNode.parentNode) {
    if (currentNode.getAttribute("hidden") == "true") {
      SetFocusThreadPane();
      return;
    }
  }
}

function SwitchPaneFocus(event)
{
  var focusedElement = WhichPaneHasFocus();
  var folderTree = GetFolderTree();
  var threadTree = GetThreadTree();
  var searchInput = GetSearchInput();
  var messagePane = GetMessagePane();

  if (event && event.shiftKey)
  {
    if (focusedElement == threadTree && searchInput.parentNode.getAttribute('hidden') != 'true')
      searchInput.focus();
    else if ((focusedElement == threadTree || focusedElement == searchInput) && !IsFolderPaneCollapsed())
      folderTree.focus();
    else if (focusedElement != messagePane && !IsThreadAndMessagePaneSplitterCollapsed())
      SetFocusMessagePane();
    else 
      threadTree.focus();
  }
  else
  {
    if (focusedElement == searchInput)
      threadTree.focus();
    else if (focusedElement == threadTree && !IsThreadAndMessagePaneSplitterCollapsed())
      SetFocusMessagePane();
    else if (focusedElement != folderTree && !IsFolderPaneCollapsed())
      folderTree.focus();
    else if (searchInput.parentNode.getAttribute('hidden') != 'true')
      searchInput.focus();
    else
      threadTree.focus();
  }
}

function SetFocusFolderPane()
{
    var folderTree = GetFolderTree();
    folderTree.focus();
}

function SetFocusThreadPane()
{
    var threadTree = GetThreadTree();
    threadTree.focus();
}

function SetFocusMessagePane()
{
    var messagePaneFrame = GetMessagePaneFrame();
    messagePaneFrame.focus();
}

function is_collapsed(element) 
{
  return (element.getAttribute('state') == 'collapsed');
}

function isCommandEnabled(cmd)
{
  var selectedFolders = GetSelectedMsgFolders();
  var numFolders = selectedFolders.length;
  if(numFolders !=1)
    return false;

  var folder = selectedFolders[0];
  if (!folder)
    return false;
  else
    return folder.isCommandEnabled(cmd);

}

function IsFakeAccount() {
  try {
    var folderResource = GetSelectedFolderResource();
    return (folderResource.Value == "http://home.netscape.com/NC-rdf#PageTitleFakeAccount");
  }
  catch(ex) {
  }
  return false;
}

function SendCommandToThreadPane(command)
{
  ThreadPaneController.doCommand(command);

  // if we are sending the command so the thread pane
  // we should focus the thread pane
  SetFocusThreadPane();
}

function debugDump(msg)
{
  // uncomment for noise
  //dump(msg+"\n");
}

function CanDropOnFolderTree(index)
{
    if (!gDragService)
      gDragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService().QueryInterface(Components.interfaces.nsIDragService)
    var dragSession = null;
    var dragFolder = false;

    dragSession = gDragService.getCurrentSession();
    if (! dragSession)
        return false;

    var flavorSupported = dragSession.isDataFlavorSupported("text/x-moz-message") || dragSession.isDataFlavorSupported("text/x-moz-folder");

    var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
    if (! trans)
        return false;

    trans.addDataFlavor("text/x-moz-message");
    trans.addDataFlavor("text/x-moz-folder");
 
    var folderTree = GetFolderTree();
    var targetResource = GetFolderResource(folderTree, index);
    var targetUri = targetResource.Value;
    var targetFolder = targetResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    var targetServer = targetFolder.server;
    var sourceServer;
    var sourceResource;
   
    for (var i = 0; i < dragSession.numDropItems; i++)
    {
        dragSession.getData (trans, i);
        var dataObj = new Object();
        var dataFlavor = new Object();
        var len = new Object();
        try
        {
            trans.getAnyTransferData (dataFlavor, dataObj, len );
        }
        catch (ex)
        {
            continue;   //no data so continue;
        }
        if (dataObj)
            dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
        if (! dataObj)
            continue;

        // pull the URL out of the data object
        var sourceUri = dataObj.data.substring(0, len.value);
        if (! sourceUri)
            continue;
        if (dataFlavor.value == "text/x-moz-message")
        {
            sourceResource = null;
            var isServer = GetFolderAttribute(folderTree, targetResource, "IsServer");
            if (isServer == "true")
            {
                debugDump("***isServer == true\n");
                return false;
            }
            // canFileMessages checks no select, and acl, for imap.
            var canFileMessages = GetFolderAttribute(folderTree, targetResource, "CanFileMessages");
            if (canFileMessages != "true")
            {
                debugDump("***canFileMessages == false\n");
                return false;
            }
            var hdr = messenger.messageServiceFromURI(sourceUri).messageURIToMsgHdr(sourceUri);
            if (hdr.folder == targetFolder)
                return false;
            break;
        }

        // we should only get here if we are dragging and dropping folders
        dragFolder = true;
        sourceResource = RDF.GetResource(sourceUri);
        var sourceFolder = sourceResource.QueryInterface(Components.interfaces.nsIMsgFolder);
        sourceServer = sourceFolder.server;

        if (targetUri == sourceUri)	
            return false;

        //don't allow drop on different imap servers.
        if (sourceServer != targetServer && targetServer.type == "imap")
            return false;

        //don't allow immediate child to be dropped to it's parent
        if (targetFolder.URI == sourceFolder.parent.URI)
        {
            debugDump(targetFolder.URI + "\n");
            debugDump(sourceFolder.parent.URI + "\n");     
            return false;
        }

        var isAncestor = sourceFolder.isAncestorOf(targetFolder);
        // don't allow parent to be dropped on its ancestors
        if (isAncestor)
            return false;
    }

    if (dragFolder)
    {
        //first check these conditions then proceed further
        debugDump("***isFolderFlavor == true \n");

        // no copy for folder drag
        if (dragSession.dragAction == nsIDragService.DRAGDROP_ACTION_COPY)
            return false;

        var canCreateSubfolders = GetFolderAttribute(folderTree, targetResource, "CanCreateSubfolders");
        // if cannot create subfolders then a folder cannot be dropped here     
        if (canCreateSubfolders == "false")
        {
            debugDump("***canCreateSubfolders == false \n");
            return false;
        }
        var serverType = GetFolderAttribute(folderTree, targetResource, "ServerType");

        // if we've got a folder that can't be renamed
        // allow us to drop it if we plan on dropping it on "Local Folders"
        // (but not within the same server, to prevent renaming folders on "Local Folders" that
        // should not be renamed)
        var srcCanRename = GetFolderAttribute(folderTree, sourceResource, "CanRename");
        if (srcCanRename == "false") {
            if (sourceServer == targetServer)
                return false;
            if (serverType != "none")
                return false;
        }
    }

    //message or folder
    if (flavorSupported)
    {
        dragSession.canDrop = true;
        return true;
    }
	
    return false;
}

function CanDropBeforeAfterFolderTree(index, before)
{
    return false;
}

function DropOnFolderTree(row, orientation)
{
    if (orientation != Components.interfaces.nsITreeView.inDropOn)
        return false;

    var folderTree = GetFolderTree();
    var targetResource = GetFolderResource(folderTree, row);

    var targetUri = targetResource.Value;
    debugDump("***targetUri = " + targetUri + "\n");

    if (!gDragService)
      gDragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService().QueryInterface(Components.interfaces.nsIDragService);
    var dragSession = gDragService.getCurrentSession();
    if (! dragSession )
        return false;

    var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
    trans.addDataFlavor("text/x-moz-message");
    trans.addDataFlavor("text/x-moz-folder");

    var list = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);

    var dropMessage;
    var sourceUri;
    var sourceResource;
    var sourceFolder;
    var sourceServer;
	
    for (var i = 0; i < dragSession.numDropItems; i++)
    {
        dragSession.getData (trans, i);
        var dataObj = new Object();
        var flavor = new Object();
        var len = new Object();
        trans.getAnyTransferData(flavor, dataObj, len);
        if (dataObj)
            dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
        if (! dataObj)
            continue;

        // pull the URL out of the data object
        sourceUri = dataObj.data.substring(0, len.value);
        if (! sourceUri)
            continue;

        debugDump("    Node #" + i + ": drop " + sourceUri + " to " + targetUri + "\n");
        
        // only do this for the first object, either they are all messages or they are all folders
        if (i == 0) 
        {
          if (flavor.value == "text/x-moz-folder") 
          {
            sourceResource = RDF.GetResource(sourceUri);
            sourceFolder = sourceResource.QueryInterface(Components.interfaces.nsIMsgFolder);
            dropMessage = false;  // we are dropping a folder
          }
          else if (flavor.value == "text/x-moz-message")
            dropMessage = true;
        }
        else {
           if (!dropMessage)
             dump("drag and drop of multiple folders isn't supported\n");
        }

        if (dropMessage) {
            // from the message uri, get the appropriate messenger service
            // and then from that service, get the msgDbHdr
            list.AppendElement(messenger.messageServiceFromURI(sourceUri).messageURIToMsgHdr(sourceUri));
        }
        else {
            // Prevent dropping of a node before, after, or on itself
            if (sourceResource == targetResource)	
                continue;

            list.AppendElement(sourceResource);
        }
    }

    if (list.Count() < 1)
       return false;

    var isSourceNews = false;
    isSourceNews = isNewsURI(sourceUri);
    
    var targetFolder = targetResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    var targetServer = targetFolder.server;

    if (dropMessage) {
        var sourceMsgHdr = list.GetElementAt(0).QueryInterface(Components.interfaces.nsIMsgDBHdr);
        sourceFolder = sourceMsgHdr.folder;
        sourceServer = sourceFolder.server;

        try {
            if (isSourceNews) {
                // news to pop or imap is always a copy
                messenger.CopyMessages(sourceFolder, targetFolder, list, false);
            }
            else {
                var dragAction = dragSession.dragAction;
                if (dragAction == nsIDragService.DRAGDROP_ACTION_COPY)
                    messenger.CopyMessages(sourceFolder, targetFolder, list, false);
                else if (dragAction == nsIDragService.DRAGDROP_ACTION_MOVE)
                    messenger.CopyMessages(sourceFolder, targetFolder, list, true);
            }
        }
        catch (ex) {
            dump("failed to copy messages: " + ex + "\n");
        }
    }
    else {
        sourceServer = sourceFolder.server;
        try 
        {
            messenger.CopyFolders(GetFolderDatasource(), targetResource, list, (sourceServer == targetServer));
        }
        catch(ex)
        {
            dump ("Exception : CopyFolders " + ex + "\n");
        }
    }
    return true;
}

function BeginDragFolderTree(event)
{
    debugDump("BeginDragFolderTree\n");

    if (event.originalTarget.localName != "treechildren")
      return false;

    var folderTree = GetFolderTree();
    var row = {};
    var col = {};
    var elt = {};
    folderTree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, col, elt);
    if (row.value == -1)
      return false;

    var folderResource = GetFolderResource(folderTree, row.value);

    if (GetFolderAttribute(folderTree, folderResource, "IsServer") == "true")
    {
      debugDump("***IsServer == true\n");
      return false;
    }

    // do not allow the drag when news is the source
    if (GetFolderAttribute(folderTree, folderResource, "ServerType") == "nntp") 
    {
      debugDump("***ServerType == nntp\n");
      return false;
    }

    var selectedFolders = GetSelectedFolders();
    return BeginDragTree(event, folderTree, selectedFolders, "text/x-moz-folder");
}

function BeginDragThreadPane(event)
{
    debugDump("BeginDragThreadPane\n");

    var threadTree = GetThreadTree();
    var selectedMessages = GetSelectedMessages();

    //A message can be dragged from one window and dropped on another window
    //therefore setNextMessageAfterDelete() here 
    //no major disadvantage even if it is a copy operation

    SetNextMessageAfterDelete();
    return BeginDragTree(event, threadTree, selectedMessages, "text/x-moz-message");
}

function BeginDragTree(event, tree, selArray, flavor)
{
    var dragStarted = false;

    var transArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
    if ( !transArray ) 
      return false;

    // let's build the drag region
    var region = null;
    try {
      region = Components.classes["@mozilla.org/gfx/region;1"].createInstance(Components.interfaces.nsIScriptableRegion);
      region.init();
      var obo = tree.treeBoxObject;
      var bo = obo.treeBody.boxObject;
      var obosel= obo.selection;

      var rowX = bo.x;
      var rowY = bo.y;
      var rowHeight = obo.rowHeight;
      var rowWidth = bo.width;

      //add a rectangle for each visible selected row
      for (var i = obo.getFirstVisibleRow(); i <= obo.getLastVisibleRow(); i ++)
      {
        if (obosel.isSelected(i))
          region.unionRect(rowX, rowY, rowWidth, rowHeight);
        rowY = rowY + rowHeight;
      }
      
      //and finally, clip the result to be sure we don't spill over...
      if(!region.isEmpty())
        region.intersectRect(bo.x, bo.y, bo.width, bo.height);
    } catch(ex) {
      dump("Error while building selection region: " + ex + "\n");
      region = null;
    }
    
    var count = selArray.length;
    debugDump("selArray.length = " + count + "\n");
    for (i = 0; i < count; ++i ) {
        var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
        if (!trans) 
          return false;

        var genTextData = Components.classes["@mozilla.org/supports-string;1"].createInstance(Components.interfaces.nsISupportsString);
        if (!genTextData) 
          return false;

        trans.addDataFlavor(flavor);

        // get id (url)
        var id = selArray[i];
        genTextData.data = id;
        debugDump("    ID #" + i + " = " + id + "\n");

        trans.setTransferData ( flavor, genTextData, id.length * 2 );  // doublebyte byte data

        // put it into the transferable as an |nsISupports|
        var genTrans = trans.QueryInterface(Components.interfaces.nsISupports);
        transArray.AppendElement(genTrans);
    }

    if (!gDragService)
      gDragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService().QueryInterface(Components.interfaces.nsIDragService);
    gDragService.invokeDragSession ( event.target, transArray, region, nsIDragService.DRAGDROP_ACTION_COPY +
    nsIDragService.DRAGDROP_ACTION_MOVE );

    dragStarted = true;

    return(!dragStarted);  // don't propagate the event if a drag has begun
}

