# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#
# Contributor(s):
#   Jan Varga (varga@nixcorp.com)
#   Håkan Waara (hwaara@chello.se)
#   Neil Rashbrook (neil@parkwaycc.co.uk)
#   Seth Spitzer <sspitzer@netscape.com>

/* This is where functions related to the 3 pane window are kept */

// from MailNewsTypes.h
const nsMsgKey_None = 0xFFFFFFFF;
const nsMsgViewIndex_None = 0xFFFFFFFF;
const kMailCheckOncePrefName = "mail.startup.enabledMailCheckOnce";

const kStandardPaneConfig = 0;
const kWidePaneConfig = 1;
const kVerticalPaneConfig = 2;

// from nsMsgFolderFlags.h
const MSG_FOLDER_FLAG_ELIDED = 0x10;

var gFolderTree; 
var gMessagePane;
var gThreadTree;
var gSearchInput;

var gThreadAndMessagePaneSplitter = null;
var gUnreadCount = null;
var gTotalCount = null;
var gCurrentPaneConfig = 0;

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
var gStartMsgKey = nsMsgKey_None;
var gSearchEmailAddress = null;
var gRightMouseButtonDown = false;
// Global var to keep track of which row in the thread pane has been selected
// This is used to make sure that the row with the currentIndex has the selection
// after a Delete or Move of a message that has a row index less than currentIndex.
var gThreadPaneCurrentSelectedIndex = -1;
var gLoadStartFolder = true;
var gNewAccountToLoad = null;

// Global var to keep track of if the 'Delete Message' or 'Move To' thread pane
// context menu item was triggered.  This helps prevent the tree view from
// not updating on one of those menu item commands.
var gThreadPaneDeleteOrMoveOccurred = false;

//If we've loaded a message, set to true.  Helps us keep the start page around.
var gHaveLoadedMessage;

var gDisplayStartupPage = false;

var gNotifyDefaultInboxLoadedOnStartup = false;

function SelectAndScrollToKey(aMsgKey)
{
  // select the desired message
  // if the key isn't found, we won't select anything
  gDBView.selectMsgByKey(aMsgKey);
  
  // is there a selection?
  // if not, bail out.
  var indicies = GetSelectedIndices(gDBView);
  if (!indicies || !indicies.length)
    return false;

  // now scroll to it
  EnsureRowInThreadTreeIsVisible(indicies[0]);
  return true;
}

// the folderListener object
var folderListener = {
    OnItemAdded: function(parentItem, item) { },

    OnItemRemoved: function(parentItem, item) { },

    OnItemPropertyChanged: function(item, property, oldValue, newValue) { },

    OnItemIntPropertyChanged: function(item, property, oldValue, newValue) {
      if (item == msgWindow.openFolder) {
        if(property.toString() == "TotalMessages" || property.toString() == "TotalUnreadMessages") {
          UpdateStatusMessageCounts(msgWindow.openFolder);
        }      
      }
    },

    OnItemBoolPropertyChanged: function(item, property, oldValue, newValue) { },

    OnItemUnicharPropertyChanged: function(item, property, oldValue, newValue) { },
    OnItemPropertyFlagChanged: function(item, property, oldFlag, newFlag) { },

    OnItemEvent: function(folder, event) {
       var eventType = event.toString();
       if (eventType == "FolderLoaded") {
         if (folder) {
           var uri = folder.URI;
           if (uri == gCurrentFolderToReroot) {
             gQSViewIsDirty = true;
             gCurrentFolderToReroot = null;
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

               if (gStartMsgKey != nsMsgKey_None) {
                 scrolled = SelectAndScrollToKey(gStartMsgKey);
                 gStartMsgKey = nsMsgKey_None;
               }

               if (gNextMessageAfterLoad) {
                 var type = gNextMessageAfterLoad;
                 gNextMessageAfterLoad = null;

                 // scroll to and select the proper message
                 scrolled = ScrollToMessage(type, true, true /* selectMessage */);
               }
             }
           }
           if (uri == gCurrentLoadingFolderURI) {
             // NOTE,
             // if you change the scrolling code below,
             // double check the scrolling logic in
             // searchBar.js, restorePreSearchView()

             gCurrentLoadingFolderURI = "";

             // if we didn't just scroll, 
             // scroll to the first new message
             // but don't select it
             if (!scrolled && pref.getBoolPref("mailnews.scroll_to_new_message"))
               scrolled = ScrollToMessage(nsMsgNavigationType.firstNew, true, false /* selectMessage */);

             if (!scrolled && pref.getBoolPref("mailnews.remember_selected_message")) {
               // if we failed to scroll to a new message,
               // reselect the last selected message
               var lastMessageLoaded = msgFolder.lastMessageLoaded;
                 
               if (lastMessageLoaded != nsMsgKey_None) {
                 scrolled = SelectAndScrollToKey(lastMessageLoaded);
               }
             }

             if (!scrolled) {
               // if we still haven't scrolled,
               // scroll to the newest, which might be the top or the bottom
               // depending on our sort order and sort type
               if (gDBView.sortOrder == nsMsgViewSortOrder.ascending) {
                 switch (gDBView.sortType) {
                   case nsMsgViewSortType.byDate: 
                   case nsMsgViewSortType.byId: 
                   case nsMsgViewSortType.byThread: 
                     scrolled = ScrollToMessage(nsMsgNavigationType.lastMessage, true, false /* selectMessage */);
                     break;
                 }
               }

               // if still we haven't scrolled,
               // scroll to the top.
               if (!scrolled)
                 EnsureRowInThreadTreeIsVisible(0);
             }            

             // NOTE,
             // if you change the scrolling code above,
             // double check the scrolling logic in
             // searchBar.js, restorePreSearchView()

             SetBusyCursor(window, false);
           }
           if (gNotifyDefaultInboxLoadedOnStartup && (folder.flags & 0x1000))
           {
              var defaultAccount = accountManager.defaultAccount;
              defaultServer = defaultAccount.incomingServer;
              var inboxFolder = GetInboxFolder(defaultServer);
              if (inboxFolder && inboxFolder.URI == folder.URI)
              {
                NotifyObservers(null,"defaultInboxLoadedOnStartup",null);
                gNotifyDefaultInboxLoadedOnStartup = false;
              }
           }
           //folder loading is over, now issue quick search if there is an email address
           if (gSearchEmailAddress)
           {
             Search(gSearchEmailAddress);
             gSearchEmailAddress = null;
           } 
           else if (gDefaultSearchViewTerms)
           {
             Search("");
           }
           else {
             ViewChangeByValue(pref.getIntPref("mailnews.view.last"));
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
       else if (eventType == "RenameCompleted") {
         SelectFolder(folder.URI);
       }
       else if (eventType == "JunkStatusChanged") {
         HandleJunkStatusChanged(folder);
       }
    }
}

var folderObserver = {
    canDrop: function(index, orientation)
    {
        return CanDropOnFolderTree(index, orientation);
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

    onPerformAction: function(action)
    {
    },

    onPerformActionOnRow: function(action, row)
    {
    },

    onPerformActionOnCell: function(action, row, col)
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

// WARNING
// this is a fragile and complicated function.
// be careful when hacking on it.
// don't forget about things like different imap 
// delete models, multiple views (from multiple thread panes, 
// search windows, stand alone message windows)
function HandleDeleteOrMoveMsgCompleted(folder)
{
  // you might not have a db view.  this can happen if
  // biff fires when the 3 pane is set to account central.
  if (!gDBView)
    return;

  gDBView.onDeleteCompleted(true);

  if (!IsCurrentLoadedFolder(folder)) {
    // default value after delete/move/copy is over
    gNextMessageViewIndexAfterDelete = -2;
    return;
  }

  var treeView = gDBView.QueryInterface(Components.interfaces.nsITreeView);
  var treeSelection = treeView.selection;
          
  if (gNextMessageViewIndexAfterDelete == -2) {
    // a move or delete can cause our selection can change underneath us.
    // this can happen when the user
    // deletes message from the stand alone msg window
    // or the search view, or another 3 pane
    if (treeSelection.count == 0) {
      // this can happen if you double clicked a message
      // in the thread pane, and deleted it from the stand alone msg window
      // see bug #172392
      treeSelection.clearSelection();
      setTitleFromFolder(folder, null);
      ClearMessagePane();
      UpdateMailToolbar("delete from another view, 0 rows now selected");
    }
    else if (treeSelection.count == 1) {
      // this can happen if you had two messages selected
      // in the thread pane, and you deleted one of them from another view
      // (like the view in the stand alone msg window)
      // since one item is selected, we should load it.
      var startIndex = {};
      var endIndex = {};
      treeSelection.getRangeAt(0, startIndex, endIndex);
        
      // select the selected item, so we'll load it
      treeSelection.select(startIndex.value); 
      treeView.selectionChanged();

      EnsureRowInThreadTreeIsVisible(startIndex.value); 

      UpdateMailToolbar("delete from another view, 1 row now selected");
    }
    else {
      // this can happen if you have more than 2 messages selected
      // in the thread pane, and you deleted one of them from another view
      // (like the view in the stand alone msg window)
      // since multiple messages are still selected, do nothing.
    }
  }
  else {
    if (gNextMessageViewIndexAfterDelete != nsMsgViewIndex_None) 
    {
      var viewSize = treeView.rowCount;
      if (gNextMessageViewIndexAfterDelete >= viewSize) 
      {
        if (viewSize > 0)
          gNextMessageViewIndexAfterDelete = viewSize - 1;
        else
        {           
          gNextMessageViewIndexAfterDelete = nsMsgViewIndex_None;

          // there is nothing to select since viewSize is 0
          treeSelection.clearSelection();
          setTitleFromFolder(folder, null);
          ClearMessagePane();
          UpdateMailToolbar("delete from current view, 0 rows left");
        }
      }
    }

    // if we are about to set the selection with a new element then DON'T clear
    // the selection then add the next message to select. This just generates
    // an extra round of command updating notifications that we are trying to
    // optimize away.
    if (gNextMessageViewIndexAfterDelete != nsMsgViewIndex_None) 
    {
      // when deleting a message we don't update the commands 
      // when the selection goes to 0
      // (we have a hack in nsMsgDBView which prevents that update) 
      // so there is no need to
      // update commands when we select the next message after the delete; 
      // the commands already
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
      // selection changed call. 
      // (don't worry it's cheap if we end up calling it twice).
      if (treeView)
        treeView.selectionChanged();

      EnsureRowInThreadTreeIsVisible(gNextMessageViewIndexAfterDelete); 
      gDBView.suppressCommandUpdating = false;

      // hook for extra toolbar items
      // XXX TODO
      // I think there is a bug in the suppression code above.
      // what if I have two rows selected, and I hit delete, 
      // and so we load the next row.
      // what if I have commands that only enable where 
      // exactly one row is selected?
      UpdateMailToolbar("delete from current view, at least one row selected");
    }
  }

  // default value after delete/move/copy is over
  gNextMessageViewIndexAfterDelete = -2;
}

function HandleCompactCompleted(folder)
{
  if (folder)
  {
    var resource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
    if (resource)
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

// aMsgWindowInitialized: false if we are calling from the onload handler, otherwise true

function UpdateMailPaneConfig(aMsgWindowInitialized) {
  var paneConfig = pref.getIntPref("mail.pane_config.dynamic");
  
  if (paneConfig == kStandardPaneConfig)
    document.getElementById('messagepanebox').setAttribute('flex', 1);
  else 
    document.getElementById('messagepanebox').removeAttribute('flex');
    
  
  // don't do anything if we are already in the correct configuration
  if (paneConfig == gCurrentPaneConfig)
    return;

  var mailContentWrapper = document.getElementById("mailContentWrapper");
  var messagesBox = document.getElementById("messagesBox");
  var messagePaneBox = GetMessagePane();
  var msgPaneReRooted = false;

  var threadPaneSplitter = GetThreadAndMessagePaneSplitter();

  // the only element we need to re-root is the message pane.
  var desiredMsgPaneParentId = (paneConfig == "0" || paneConfig == "2") ? "messagesBox" : "mailContentWrapper";

  if (messagePaneBox.parentNode.id != desiredMsgPaneParentId)
  {
     var messagePaneParent = document.getElementById(messagePaneBox.parentNode.id);
     messagePaneParent.removeChild(threadPaneSplitter);
     messagePaneParent.removeChild(messagePaneBox);

     var messagePaneNewParent = document.getElementById(desiredMsgPaneParentId);
     messagePaneNewParent.appendChild(threadPaneSplitter); 
     messagePaneNewParent.appendChild(messagePaneBox); 
     msgPaneReRooted = true;
  }

  // now for each config, handle any extra clean up to create that view (such as changing a box orientation)
  if (paneConfig == kStandardPaneConfig) // standard 3-Pane Layout
  {     
    threadPaneSplitter.setAttribute("orient", "vertical");

    // finally, make sure mailContentWrapper has the correct orientation
    mailContentWrapper.setAttribute("orient", "horizontal");
    messagesBox.setAttribute("orient", "vertical");
  }

  else if (paneConfig == kWidePaneConfig)  // "Wide" Window Pane Layout
  {     
    threadPaneSplitter.setAttribute("orient", "vertical");

    // finally, make sure mailContentWrapper has the correct orientation
    mailContentWrapper.setAttribute("orient", "vertical");
    messagesBox.setAttribute("orient", "vertical");
  }
  
  else if (paneConfig == kVerticalPaneConfig) // Vertical Pane Layout
  {
    messagesBox.setAttribute("orient", "horizontal");
    threadPaneSplitter.removeAttribute("orient");
    // finally, make sure mailContentWrapper has the correct orientation
    mailContentWrapper.setAttribute("orient", "horizontal");
  }

  // re-rooting the message pane causes the docshell to get destroyed 
  // and replaced with another one. As such, we need to re-set the window (and thus the 
  // internal references nsMessenger keeps for the message pane docshell)
  // XXX: is it safe to call this multiple times? May need to add a setMessagePaneDocShell
  //      routine to nsIMessenger.

  if (aMsgWindowInitialized && msgPaneReRooted)
  {
    messenger.SetWindow(null, null);
    messenger.SetWindow(window, msgWindow);
    MsgReload(); 
  }
  
  // record the new configuration
  gCurrentPaneConfig = paneConfig; 
}

const MailPrefObserver = {
  observe: function(subject, topic, prefName) {
    // verify that we're changing the mail pane config pref
    if (topic == "nsPref:changed")
    {
      if (prefName == "mail.pane_config.dynamic")
        UpdateMailPaneConfig(true);
      else if (prefName == "mail.showFolderPaneColumns")
        UpdateFolderColumnVisibility();
    }
  }
};

function OnLoadMessenger()
{
  // update the pane config before we exit onload otherwise the user may see a flicker if we poke the document
  // in delayedOnLoadMessenger...
  UpdateMailPaneConfig(false);
  document.loadBindingDocument('chrome://global/content/bindings/textbox.xml');

  // Set a sane starting width/height for all resolutions on new profiles. Do this before the window loads
  if (!document.documentElement.hasAttribute("width")) 
  {
    var defaultWidth, defaultHeight;
    if (screen.availHeight <= 600) 
    { 
      document.documentElement.setAttribute("sizemode", "maximized");
      defaultWidth = 800;
      defaultHeight = 565;
    }
    else // for higher resolution displays, use larger values for height and width
    {
      defaultWidth = screen.availWidth * .8;
      defaultHeight = screen.availHeight * .8; 
    }

    document.documentElement.setAttribute("width", defaultWidth);
    document.documentElement.setAttribute("height", defaultHeight);
  }


  setTimeout(delayedOnLoadMessenger, 0); // when debugging, set this to 5000, so you can see what happens after the window comes up.
}

/* Functions related to startup */
function delayedOnLoadMessenger()
{
  pref.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
  pref.addObserver("mail.pane_config.dynamic", MailPrefObserver, false);
  pref.addObserver("mail.showFolderPaneColumns", MailPrefObserver, false);

  AddMailOfflineObserver();
  CreateMailWindowGlobals();
  verifyAccounts(null);
    
  HideAccountCentral();
  InitMsgWindow();
  messenger.SetWindow(window, msgWindow);

  InitializeDataSources();
  InitPanes();

  accountManager.setSpecialFolders();
  accountManager.addIncomingServerListener(gThreePaneIncomingServerListener);

  AddToSession();
  //need to add to session before trying to load start folder otherwise listeners aren't
  //set up correctly.
  // argument[0] --> folder uri
  // argument[1] --> optional message key
  // argument[2] --> optional email address; //will come from aim; needs to show msgs from buddy's email address  
  if ("arguments" in window)
  {
    gStartFolderUri = (window.arguments.length > 0) ? window.arguments[0] : null;
    gStartMsgKey = (window.arguments.length > 1) ? window.arguments[1]: nsMsgKey_None;
    gSearchEmailAddress = (window.arguments.length > 2) ? window.arguments[2] : null;
  }

  setTimeout("loadStartFolder(gStartFolderUri);", 0);

  // FIX ME - later we will be able to use onload from the overlay
  OnLoadMsgHeaderPane();

  gHaveLoadedMessage = false;

  gNotifyDefaultInboxLoadedOnStartup = true;

  //Set focus to the Thread Pane the first time the window is opened.
  SetFocusThreadPane();

  // initialize the customizeDone method on the customizeable toolbar
  var toolbox = document.getElementById("mail-toolbox");
  toolbox.customizeDone = MailToolboxCustomizeDone;

  var toolbarset = document.getElementById('customToolbars');
  toolbox.toolbarset = toolbarset;
}

function OnUnloadMessenger()
{
  accountManager.removeIncomingServerListener(gThreePaneIncomingServerListener);
  pref.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
  pref.removeObserver("mail.pane_config.dynamic", MailPrefObserver);
  pref.removeObserver("mail.showFolderPaneColumns", MailPrefObserver);

  // FIX ME - later we will be able to use onload from the overlay
  OnUnloadMsgHeaderPane();

  OnMailWindowUnload();
}

function NotifyObservers(aSubject, aTopic, aData)
{
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(aSubject, aTopic, aData);
}

// because the "open" state persists, we'll call
// PerformExpand() for all servers that are open at startup.            
function PerformExpandForAllOpenServers()
{
    var servers = accountManager.allServers;
    for (var i = 0; i < servers.Count(); i++)
    {
        var server = servers.QueryElementAt(i, Components.interfaces.nsIMsgIncomingServer);
        if (server.type != "imap" && !server.rootMsgFolder.getFlag(MSG_FOLDER_FLAG_ELIDED))
            server.performExpand(msgWindow);
    }
}

function loadStartFolder(initialUri)
{
    var folderTree = GetFolderTree();
    var defaultServer = null;
    var startFolderResource = null;
    var isLoginAtStartUpEnabled = false;

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

            // Enable checknew mail once by turning checkmail pref 'on' to bring 
            // all users to one plane. This allows all users to go to Inbox. User can 
            // always go to server settings panel and turn off "Check for new mail at startup"
            if (!pref.getBoolPref(kMailCheckOncePrefName))
            {
                pref.setBoolPref(kMailCheckOncePrefName, true);
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

        var startFolder = startFolderResource.QueryInterface(Components.interfaces.nsIMsgFolder);

        // it is possible we were given an initial uri and we need to subscribe or try to add
        // the folder. i.e. the user just clicked on a news folder they aren't subscribed to from a browser
        // the news url comes in here.   

        SelectFolder(startFolder.URI);        

        // only do this on startup, when we pass in null
        if (!initialUri && isLoginAtStartUpEnabled && gLoadStartFolder)
        {
            // Perform biff on the server to check for new mail
            defaultServer.PerformBiff(msgWindow);        
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

      if (initialUri)
      {
        messenger.loadURL(window, initialUri);
        return;
      }

      dump(ex);
      dump('Exception in LoadStartFolder caused by no default account.  We know about this\n');
    }

    MsgGetMessagesForAllServers(defaultServer);

    if (this.CheckForUnsentMessages != undefined && CheckForUnsentMessages())
    {
      var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
      if (!ioService.offline) 
      {
        InitPrompts();
        InitServices();

        if (gPromptService) 
        {
          var buttonPressed = gPromptService.confirmEx(window, 
                            gOfflinePromptsBundle.getString('sendMessagesOfflineWindowTitle'), 
                            gOfflinePromptsBundle.getString('sendMessagesLabel'),
                            gPromptService.BUTTON_TITLE_IS_STRING * (gPromptService.BUTTON_POS_0 + 
                            gPromptService.BUTTON_POS_1),
                            gOfflinePromptsBundle.getString('sendMessagesSendButtonLabel'),
                            gOfflinePromptsBundle.getString('sendMessagesNoSendButtonLabel'),
                            null, null, {value:0});
          if (buttonPressed == 0) 
            SendUnsentMessages();
        }
      }
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

function UpdateFolderColumnVisibility()
{
    var folderNameCol = document.getElementById("folderNameCol");
    var showColumns = pref.getBoolPref("mail.showFolderPaneColumns");
    var folderUnreadCol = document.getElementById("folderUnreadCol"); 
    var folderColumnLabel = document.getElementById("folderColumnLabel");
    if (!showColumns)
    {
      var folderTotalCol = document.getElementById("folderTotalCol");
      var folderSizeCol = document.getElementById("folderSizeCol");
      folderUnreadCol.setAttribute("hidden", "true");
      folderTotalCol.setAttribute("hidden", "true");
      folderSizeCol.setAttribute("hidden", "true");
      folderNameCol.removeAttribute("label");
    }
    else
    {
      folderNameCol.setAttribute("label", folderColumnLabel.value);
    }

    folderNameCol.setAttribute("hideheader", showColumns ? "false" : "true");
    var folderPaneHeader = document.getElementById("folderPaneHeader");
    folderPaneHeader.setAttribute("hidden", showColumns ? "true" : "false");
    var folderTree = document.getElementById("folderTree");
    folderTree.setAttribute("hidecolumnpicker", showColumns ? "false" : "true");
    var hidden = folderUnreadCol.getAttribute("hidden");
    if (hidden != "true")
    {
        var folderNameCell = document.getElementById("folderNameCell");
        folderNameCell.setAttribute("label", "?folderTreeSimpleName");
    }
} 

function OnLoadFolderPane()
{
    UpdateFolderColumnVisibility();
    var folderUnreadCol = document.getElementById("folderUnreadCol");
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

    if (threadPaneUIVersion < 5) {
      var threadTree = document.getElementById("threadTree");        
      var junkCol = document.getElementById("junkStatusCol");
      var beforeCol;

      if (threadPaneUIVersion < 4) {

        if (threadPaneUIVersion < 3) {
          
          // in thunderbird, we are inserting the junk column just before the 
          // date column. 
          var dateCol = document.getElementById("dateCol");
          threadTree._reorderColumn(junkCol, dateCol, true);

          // hide labels column by default
          if (threadPaneUIVersion == 1) {
            labelCol = document.getElementById("labelCol");
            labelCol.setAttribute("hidden", "true");
          }
        }

        var senderCol = document.getElementById("senderCol");
        var recipientCol = document.getElementById("recipientCol");    
        threadTree._reorderColumn(recipientCol, junkCol, true);        
        threadTree._reorderColumn(senderCol, recipientCol, true);

      } // version 4 upgrades

      // version 5 adds a new column called attachments
      var attachmentCol = document.getElementById("attachmentCol");
      var subjectCol = document.getElementById("subjectCol");
      
      threadTree._reorderColumn(attachmentCol, subjectCol, true);

      pref.setIntPref("mailnews.ui.threadpane.version", 5);

    } // version 5 upgrades
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

function IsMessagePaneCollapsed()
{
  return GetMessagePane().collapsed;
}

function IsFolderPaneCollapsed()
{
  return GetFolderTree().parentNode.collapsed;
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

    // hide the junk bar
    SetUpJunkBar(null);
  }
}

function GetSelectedFolderIndex()
{
    var folderTree = GetFolderTree();
    var startIndex = {};
    var endIndex = {};
    folderTree.view.selection.getRangeAt(0, startIndex, endIndex);
    return startIndex.value;
}

// Function to change the highlighted row to where the mouse was clicked
// without loading the contents of the selected row.
// It will also keep the outline/dotted line in the original row.
function ChangeSelectionWithoutContentLoad(event, tree)
{
    var treeBoxObj = tree.treeBoxObject;
    var treeSelection = treeBoxObj.view.selection;

    var row = treeBoxObj.getRowAt(event.clientX, event.clientY);
    // make sure that row.value is valid so that it doesn't mess up
    // the call to ensureRowIsVisible().
    if((row >= 0) && !treeSelection.isSelected(row))
    {
        var saveCurrentIndex = treeSelection.currentIndex;
        treeSelection.selectEventsSuppressed = true;
        treeSelection.select(row);
        treeSelection.currentIndex = saveCurrentIndex;
        treeBoxObj.ensureRowIsVisible(row);
        treeSelection.selectEventsSuppressed = false;

        // Keep track of which row in the thread pane is currently selected.
        if(tree.id == "threadTree")
          gThreadPaneCurrentSelectedIndex = row;
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
    if (row.value == -1) {
      if (event.originalTarget.localName == "treecol")
        // clicking on the name column in the folder pane should not sort
        event.preventBubble();
      return;
    }

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
    else if ((event.originalTarget.localName == "slider") ||
             (event.originalTarget.localName == "scrollbarbutton")) {
      event.preventBubble();
    }
    else if (event.detail == 2) {
      FolderPaneDoubleClick(row.value, event);
    }
    else if (gDBView && gDBView.viewType == nsMsgViewType.eShowQuickSearchResults)
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
        tree.view.selection.select(newIndex);
        tree.treeBoxObject.ensureRowIsVisible(newIndex);
    }
}

function GetSelectedFolders()
{
    var folderArray = [];
    var k = 0;
    var folderTree = GetFolderTree();
    var rangeCount = folderTree.view.selection.getRangeCount();

    for(var i = 0; i < rangeCount; i++)
    {
        var startIndex = {};
        var endIndex = {};
        folderTree.view.selection.getRangeAt(i, startIndex, endIndex);
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
    var rangeCount = folderTree.view.selection.getRangeCount();

    for(var i = 0; i < rangeCount; i++)
    {
        var startIndex = {};
        var endIndex = {};
        folderTree.view.selection.getRangeAt(i, startIndex, endIndex);
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
    var view = GetDBView();
    view.getURIsForSelection(messageArray,length);
    if (length.value)
      return messageArray.value;
    else 
      return null;
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

// Figures out how many messages are selected (hilighted - does not necessarily
// have the dotted outline) above a given index row value in the thread pane.
function NumberOfSelectedMessagesAboveCurrentIndex(index)
{
  var numberOfMessages = 0;
  var indicies = GetSelectedIndices(gDBView);

  if (indicies && indicies.length)
  {
    for (var i = 0; i < indicies.length; i++)
    {
      if (indicies[i] < index)
        ++numberOfMessages;
      else
        break;
    }
  }
  return numberOfMessages;
}

function SetNextMessageAfterDelete()
{
  var treeSelection = GetThreadTree().view.selection;

  if (treeSelection.isSelected(treeSelection.currentIndex))
    gNextMessageViewIndexAfterDelete = gDBView.msgToSelectAfterDelete;
  else if(gDBView.removeRowOnMoveOrDelete)
  {
    // Only set gThreadPaneDeleteOrMoveOccurred to true if the message was
    // truly moved to the trash or deleted, as opposed to an IMAP delete
    // (where it is only "marked as deleted".  This will prevent bug 142065.
    //
    // If it's an IMAP delete, then just set gNextMessageViewIndexAfterDelete
    // to treeSelection.currentIndex (where the outline is at) because nothing
    // was moved or deleted from the folder.
    gThreadPaneDeleteOrMoveOccurred = true;
    gNextMessageViewIndexAfterDelete = treeSelection.currentIndex - NumberOfSelectedMessagesAboveCurrentIndex(treeSelection.currentIndex);
  }
  else
    gNextMessageViewIndexAfterDelete = treeSelection.currentIndex;
}

function EnsureFolderIndex(builder, msgFolder)
{
  // try to get the index of the folder in the tree
  var index = builder.getIndexOfResource(msgFolder);
  if (index == -1) {
    // if we couldn't find the folder, open the parent
    builder.toggleOpenState(EnsureFolderIndex(builder, msgFolder.parent));
    index = builder.getIndexOfResource(msgFolder);
  }
  return index;
}

function SelectFolder(folderUri)
{
    var folderTree = GetFolderTree();
    var folderResource = RDF.GetResource(folderUri);
    var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);

    // before we can select a folder, we need to make sure it is "visible"
    // in the tree.  to do that, we need to ensure that all its
    // ancestors are expanded
    var folderIndex = EnsureFolderIndex(folderTree.builderView, msgFolder);
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

