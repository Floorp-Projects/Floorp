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
 *   Håkan Waara <hwaara@chello.se>
 */

var rdfDatasourcePrefix = "@mozilla.org/rdf/datasource;1?name=";
var rdfServiceContractID    = "@mozilla.org/rdf/rdf-service;1";
var searchSessionContractID = "@mozilla.org/messenger/searchSession;1";
var folderDSContractID      = rdfDatasourcePrefix + "mailnewsfolders";
var gSearchView;
var gSearchSession;
var gCurrentFolder;

var nsIMsgFolder = Components.interfaces.nsIMsgFolder;
var nsIMsgWindow = Components.interfaces.nsIMsgWindow;
var nsIMsgRDFDataSource = Components.interfaces.nsIMsgRDFDataSource;
var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;

var gFolderDatasource;
var gFolderPicker;
var gStatusBar = null;
var gStatusFeedback = new nsMsgStatusFeedback;
var gNumOfSearchHits = 0;
var RDF;
var gSearchBundle;
var gNextMessageViewIndexAfterDelete = -1;

// Datasource search listener -- made global as it has to be registered
// and unregistered in different functions.
var gDataSourceSearchListener;
var gViewSearchListener;

var gSearchStopButton;
var gSearchSessionFolderListener;
var gMailSession;

// Controller object for search results thread pane
var nsSearchResultsController =
{
    supportsCommand: function(command)
    {
        switch(command) {
        case "button_delete":
        case "cmd_open":
        case "file_message_button":
        case "goto_folder_button":
            return true;
        default:
            return false;
        }
    },

    // this controller only handles commands
    // that rely on items being selected in
    // the search results pane.
    isCommandEnabled: function(command)
    {
        var enabled = true;
        
        switch (command) { 
          case "goto_folder_button":
            if (GetNumSelectedMessages() != 1)
              enabled = false;
            break;
          default:
            if (GetNumSelectedMessages() <= 0)
              enabled = false;
            break;
        }

        return enabled;
    },

    doCommand: function(command)
    {
        switch(command) {
        case "cmd_open":
            MsgOpenSelectedMessages(gSearchView);
            return true;

        case "button_delete":
            MsgDeleteSelectedMessages();
            return true;

        case "goto_folder_button":
            GoToFolder();
            return true;

        default:
            return false;
        }

    },

    onEvent: function(event)
    {
    }
}

// nsIMsgSearchNotify object
var gSearchNotificationListener =
{
    onSearchHit: function(header, folder)
    {
        gNumOfSearchHits++;
    },

    onSearchDone: function(status)
    {
        gSearchStopButton.setAttribute("label", gSearchBundle.getString("labelForSearchButton"));

        var statusMsg;
        // if there are no hits, it means no matches were found in the search.
        if (gNumOfSearchHits == 0) {
            statusMsg = gSearchBundle.getString("searchFailureMessage");
        }
        else 
        {
            if (gNumOfSearchHits == 1) 
                statusMsg = gSearchBundle.getString("searchSuccessMessage");
            else
                statusMsg = gSearchBundle.getFormattedString("searchSuccessMessages", [gNumOfSearchHits]);

            gNumOfSearchHits = 0;
        }

        gStatusFeedback.showProgress(100);
        gStatusFeedback.showStatusString(statusMsg);
        gStatusBar.setAttribute("mode","normal");
    },

    onNewSearch: function()
    {
      gSearchStopButton.setAttribute("label", gSearchBundle.getString("labelForStopButton"));
      document.commandDispatcher.updateCommands('mail-search');
      gStatusFeedback.showProgress(0);
      gStatusFeedback.showStatusString(gSearchBundle.getString("searchingMessage"));
      gStatusBar.setAttribute("mode","undetermined");
    }
}

// the folderListener object
var gFolderListener = {
    OnItemAdded: function(parentItem, item, view) {},

    OnItemRemoved: function(parentItem, item, view){},

    OnItemPropertyChanged: function(item, property, oldValue, newValue) {},

    OnItemIntPropertyChanged: function(item, property, oldValue, newValue) {},

    OnItemBoolPropertyChanged: function(item, property, oldValue, newValue) {},

    OnItemUnicharPropertyChanged: function(item, property, oldValue, newValue){},
    OnItemPropertyFlagChanged: function(item, property, oldFlag, newFlag) {},

    OnItemEvent: function(folder, event) {
        var eventType = event.GetUnicode();

        if (eventType == "DeleteOrMoveMsgCompleted") {
            HandleDeleteOrMoveMessageCompleted(folder);
        }     
        else if (eventType == "DeleteOrMoveMsgFailed") {
            HandleDeleteOrMoveMessageFailed(folder);
        }

    }
}

function HideSearchColumn(id)
{
  var col = document.getElementById(id);
  if (col) {
    col.setAttribute("hidden","true");
    col.setAttribute("ignoreincolumnpicker","true");
  }
}

function ShowSearchColumn(id)
{
  var col = document.getElementById(id);
  if (col) {
    col.removeAttribute("hidden");
    col.removeAttribute("ignoreincolumnpicker");
  }
}

function searchOnLoad()
{
  initializeSearchWidgets();
  initializeSearchWindowWidgets();

  gSearchBundle = document.getElementById("bundle_search");
  setupDatasource();
  setupSearchListener();

  if (window.arguments && window.arguments[0])
      selectFolder(window.arguments[0].folder);

  onMore(null);
  document.commandDispatcher.updateCommands('mail-search');

  // hide and remove these columns from the column picker.  you can't thread search results
  HideSearchColumn("threadCol"); // since you can't thread search results
  HideSearchColumn("totalCol"); // since you can't thread search results
  HideSearchColumn("unreadCol"); // since you can't thread search results
  HideSearchColumn("unreadButtonColHeader");
  HideSearchColumn("statusCol");
  HideSearchColumn("sizeCol");
  HideSearchColumn("flaggedCol");
  
  // we want to show the location column for search
  ShowSearchColumn("locationCol");
}

function searchOnUnload()
{
    // unregister listeners
    gSearchSession.unregisterListener(gViewSearchListener);
    gSearchSession.unregisterListener(gSearchNotificationListener);

    gMailSession.RemoveFolderListener(gSearchSessionFolderListener);
	gSearchSession.removeFolderListener(gFolderListener);
	
    if (gSearchView) {
	gSearchView.close();
	gSearchView = null;
    }

    // release this early because msgWindow holds a weak reference
    msgWindow.rootDocShell = null;
}

function initializeSearchWindowWidgets()
{
    gFolderPicker = document.getElementById("searchableFolders");
    gSearchStopButton = document.getElementById("search-button");
    gStatusBar = document.getElementById('statusbar-icon');

    msgWindow = Components.classes[msgWindowContractID].createInstance(nsIMsgWindow);
    msgWindow.statusFeedback = gStatusFeedback;
    msgWindow.SetDOMWindow(window);

    // functionality to enable/disable buttons using nsSearchResultsController
    // depending of whether items are selected in the search results thread pane.
    top.controllers.insertControllerAt(0, nsSearchResultsController);
}


function onSearchStop() {
    gSearchSession.interruptSearch();
}

function onReset() {
}

function getFirstItemByTag(root, tag)
{
    var node;
    if (root.localName == tag)
        return root;

    if (root.childNodes) {
        for (node = root.firstChild; node; node=node.nextSibling) {
            if (node.localName != "template") {
                var result = getFirstItemByTag(node, tag);
                if (result) return result;
            }
        }
    }
    return null;
}

function selectFolder(folder) {
    var folderURI;
    if (!folder) {
        // walk folders to find first item
        var firstItem = getFirstItemByTag(gFolderPicker, "menu");
        folderURI = firstItem.id;
    } else {
        folderURI = folder.URI;
    }
    updateSearchFolderPicker(folderURI);
}

function updateSearchFolderPicker(folderURI) {
 
    SetFolderPicker(folderURI, gFolderPicker.id);

    // use the URI to get the real folder
    gCurrentFolder =
        RDF.GetResource(folderURI).QueryInterface(nsIMsgFolder);

    setSearchScope(GetScopeForFolder(gCurrentFolder));
}

function UpdateAfterCustomHeaderChange()
{
  updateSearchAttributes();
}

function onChooseFolder(event) {
    var folderURI = event.id;
    if (folderURI) {
        updateSearchFolderPicker(folderURI);
    }
}

function onEnterInSearchTerm()
{
  // on enter
  // if not searching, start the search
  // if searching, stop and then start again
  if (gSearchStopButton.getAttribute("label") == gSearchBundle.getString("labelForSearchButton")) { 
     onSearch(); 
  }
  else {
     onSearchStop();
     onSearch();
  }
}

function onSearch()
{
    // set the view.  do this on every search, to
    // allow the outliner to reset itself
    var outlinerView = gSearchView.QueryInterface(Components.interfaces.nsIOutlinerView);
    if (outlinerView)
    {
      var outliner = GetThreadOutliner();
      outliner.boxObject.QueryInterface(Components.interfaces.nsIOutlinerBoxObject).view = outlinerView;
    }

    gSearchSession.clearScopes();
    // tell the search session what the new scope is
    if (!gCurrentFolder.isServer && !gCurrentFolder.noSelect)
        gSearchSession.addScopeTerm(GetScopeForFolder(gCurrentFolder),
                                    gCurrentFolder);

    var searchSubfolders = document.getElementById("checkSearchSubFolders").checked;
    if (gCurrentFolder && (searchSubfolders || gCurrentFolder.isServer || gCurrentFolder.noSelect))
    {
        AddSubFolders(gCurrentFolder);
    }
    // reflect the search widgets back into the search session
    saveSearchTerms(gSearchSession.searchTerms, gSearchSession);

    try
    {
      gSearchSession.search(msgWindow);
    }
    catch(ex)
    {
       dump("Search Exception\n");
    }
    // refresh the tree after the search starts, because initiating the
    // search will cause the datasource to clear itself
}

function AddSubFolders(folder) {
  if (folder.hasSubFolders)
  {
    var subFolderEnumerator = folder.GetSubFolders();
    var done = false;
    while (!done)
    {
      var next = subFolderEnumerator.currentItem();
      if (next)
      {
        var nextFolder = next.QueryInterface(Components.interfaces.nsIMsgFolder);
        if (nextFolder)
        {
          if (!nextFolder.noSelect)
            gSearchSession.addScopeTerm(GetScopeForFolder(nextFolder), nextFolder);
          AddSubFolders(nextFolder);
        }
      }
      try
      {
        subFolderEnumerator.next();
       }
       catch (ex)
       {
          done = true;
       }
    }
  }
}


function GetScopeForFolder(folder) 
{
  return folder.server.searchScope;
}

var nsMsgViewSortType = Components.interfaces.nsMsgViewSortType;
var nsMsgViewSortOrder = Components.interfaces.nsMsgViewSortOrder;
var nsMsgViewFlagsType = Components.interfaces.nsMsgViewFlagsType;
var nsMsgViewCommandType = Components.interfaces.nsMsgViewCommandType;

function goUpdateSearchItems(commandset)
{
  for (var i = 0; i < commandset.childNodes.length; i++)
  {
    var commandID = commandset.childNodes[i].getAttribute("id");
    if (commandID)
    {
      goUpdateCommand(commandID);
    }
  }
}

function nsMsgSearchCommandUpdater()
{}

nsMsgSearchCommandUpdater.prototype =
{
  updateCommandStatus : function()
    {
      // the back end is smart and is only telling us to update command status
      // when the # of items in the selection has actually changed.
      document.commandDispatcher.updateCommands('mail-search');
    },
  displayMessageChanged : function(aFolder, aSubject)
  {
  },

  QueryInterface : function(iid)
   {
     if(iid.equals(Components.interfaces.nsIMsgDBViewCommandUpdater))
      return this;

     throw Components.results.NS_NOINTERFACE;
     return null;
   }
}

function setupDatasource() {

    RDF = Components.classes[rdfServiceContractID].getService(Components.interfaces.nsIRDFService);
    gSearchView = Components.classes["@mozilla.org/messenger/msgdbview;1?type=search"].createInstance(Components.interfaces.nsIMsgDBView);
    var count = new Object;
    var cmdupdator = new nsMsgSearchCommandUpdater();

    gSearchView.init(messenger, msgWindow, cmdupdator);
    gSearchView.open(null, nsMsgViewSortType.byId, nsMsgViewSortOrder.ascending, nsMsgViewFlagsType.kNone, count);

    // the thread pane needs to use the search datasource (to get the
    // actual list of messages) and the message datasource (to get any
    // attributes about each message)
    gSearchSession = Components.classes[searchSessionContractID].createInstance(Components.interfaces.nsIMsgSearchSession);

    gSearchSessionFolderListener = gSearchSession.QueryInterface(Components.interfaces.nsIFolderListener);
    gMailSession = Components.classes[mailSessionContractID].getService(Components.interfaces.nsIMsgMailSession);
    var nsIFolderListener = Components.interfaces.nsIFolderListener;
    var notifyFlags = nsIFolderListener.event;
    gMailSession.AddFolderListener(gSearchSessionFolderListener, notifyFlags);

    // the datasource is a listener on the search results
    gViewSearchListener = gSearchView.QueryInterface(Components.interfaces.nsIMsgSearchNotify);
    gSearchSession.registerListener(gViewSearchListener);
    gSearchSession.addFolderListener(gFolderListener);
}


function setupSearchListener()
{
    // Setup the javascript object as a listener on the search results
    gSearchSession.registerListener(gSearchNotificationListener);
}

// stuff after this is implemented to make the thread pane work
function GetFolderDatasource()
{
    if (!gFolderDatasource)
        gFolderDatasource = Components.classes[folderDSContractID].createInstance(Components.interfaces.nsIRDFDataSource);
    return gFolderDatasource;
}

// used to determine if we should try to load a message
function IsThreadAndMessagePaneSplitterCollapsed()
{
    return true;
}

function setMsgDatasourceWindow(ds, msgwindow)
{
    try {
        var msgDatasource = ds.QueryInterface(nsIMsgRDFDataSource);
        msgDatasource.window = msgwindow;
    } catch (ex) {
        dump("error setting DS on " + ds + ": " + ex + "\n");
    }
}

// used to toggle functionality for Search/Stop button.
function onSearchButton(event)
{
    if (event.target.label == gSearchBundle.getString("labelForSearchButton"))
        onSearch();
    else
        onSearchStop();
}

// threadPane.js will be needing this, too
function GetNumSelectedMessages()
{
   try {
       return gSearchView.numSelected;
   }
   catch (ex) {
       return 0;
   }
}

function GetDBView()
{
    return gSearchView;
}

function MsgDeleteSelectedMessages()
{
    // we don't delete news messages, we just return in that case
    if (isNewsURI(gSearchView.getURIForViewIndex(0))) 
        return;

    // if mail messages delete
    SetNextMessageAfterDelete();
    gSearchView.doCommand(nsMsgViewCommandType.deleteMsg);
}

function SetNextMessageAfterDelete()
{
    dump("setting next msg view index after delete to " + gSearchView.msgToSelectAfterDelete + "\n");
    gNextMessageViewIndexAfterDelete = gSearchView.msgToSelectAfterDelete;
}

function HandleDeleteOrMoveMessageFailed(folder)
{
    gNextMessageViewIndexAfterDelete = -1;
}


function HandleDeleteOrMoveMessageCompleted(folder)
{
        var outlinerView = gSearchView.QueryInterface(Components.interfaces.nsIOutlinerView);
        var outlinerSelection = outlinerView.selection;
        viewSize = outlinerView.rowCount;

        if (gNextMessageViewIndexAfterDelete >= viewSize)
        {
            if (viewSize > 0)
                gNextMessageViewIndexAfterDelete = viewSize - 1;
            else
                gNextMessageViewIndexAfterDelete = -1;
        }

        // if we are about to set the selection with a new element then DON'T clear
        // the selection then add the next message to select. This just generates
        // an extra round of command updating notifications that we are trying to
        // optimize away.
        if (gNextMessageViewIndexAfterDelete != -1) {
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
            outlinerSelection.clearSelection(); /* clear selection in either case  */

}

function MoveMessageInSearch(destFolder)
{
    try {
        // get the msg folder we're moving messages into
        // if the id (uri) is not set, use file-uri which is set for
        // "File Here"
        var destUri = destFolder.getAttribute('id');
        if (destUri.length == 0) { 
          destUri = destFolder.getAttribute('file-uri')
        }
        
        var destResource = RDF.GetResource(destUri);

        var destMsgFolder = destResource.QueryInterface(Components.interfaces.nsIMsgFolder);

        // we don't move news messages, we copy them
        if (isNewsURI(gSearchView.getURIForViewIndex(0))) {
          gSearchView.doCommandWithFolder(nsMsgViewCommandType.copyMessages, destMsgFolder);
        }
        else {
            SetNextMessageAfterDelete();
            gSearchView.doCommandWithFolder(nsMsgViewCommandType.moveMessages, destMsgFolder);
        } 
    }
    catch (ex) {
        dump("MsgMoveMessage failed: " + ex + "\n");
    }   
}

function GoToFolder()
{
  MsgOpenNewWindowForMsgHdr(gSearchView.hdrForFirstSelectedMessage);
}

function BeginDragThreadPane(event)
{
    // no search pane dnd yet
    return false;
}
