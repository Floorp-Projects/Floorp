/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Håkan Waara <hwaara@chello.se>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
var gStatusFeedback = new nsMsgStatusFeedback();
var gTimelineEnabled = false;
var gMessengerBundle = null;
var RDF;
var gSearchBundle;
var gNextMessageViewIndexAfterDelete = -2;

// Datasource search listener -- made global as it has to be registered
// and unregistered in different functions.
var gDataSourceSearchListener;
var gViewSearchListener;

var gSearchStopButton;
var gMailSession;

var MSG_FOLDER_FLAG_VIRTUAL = 0x0020;

// Controller object for search results thread pane
var nsSearchResultsController =
{
    supportsCommand: function(command)
    {
        switch(command) {
        case "cmd_delete":
        case "cmd_shiftDelete":
        case "button_delete":
        case "cmd_open":
        case "file_message_button":
        case "goto_folder_button":
        case "saveas_vf_button":
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
          case "cmd_delete":
          case "cmd_shiftDelete":
          case "button_delete":
            // this assumes that advanced searches don't cross accounts
            if (GetNumSelectedMessages() <= 0 || isNewsURI(gSearchView.getURIForViewIndex(0)))
              enabled = false;
            break;
          case "saveas_vf_button":
              // need someway to see if there are any search criteria...
              return true;
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
            MsgOpenSelectedMessages();
            return true;

        case "cmd_delete":
        case "button_delete":
            MsgDeleteSelectedMessages(nsMsgViewCommandType.deleteMsg);
            return true;
        case "cmd_shiftDelete":
            MsgDeleteSelectedMessages(nsMsgViewCommandType.deleteNoTrash);
            return true;

        case "goto_folder_button":
            GoToFolder();
            return true;

        case "saveas_vf_button":
            saveAsVirtualFolder();
            return true;
        default:
            return false;
        }

    },

    onEvent: function(event)
    {
    }
}

function UpdateMailSearch(caller)
{
  //dump("XXX update mail-search " + caller + "\n");
  document.commandDispatcher.updateCommands('mail-search');
}

function SetAdvancedSearchStatusText(aNumHits)
{
  var statusMsg;
  // if there are no hits, it means no matches were found in the search.
  if (aNumHits == 0)
    statusMsg = gSearchBundle.getString("searchFailureMessage");
  else 
  {
    if (aNumHits == 1) 
      statusMsg = gSearchBundle.getString("searchSuccessMessage");
    else
      statusMsg = gSearchBundle.getFormattedString("searchSuccessMessages", [aNumHits]);
  }

  gStatusFeedback.showStatusString(statusMsg);
}

// nsIMsgSearchNotify object
var gSearchNotificationListener =
{
    onSearchHit: function(header, folder)
    {
        // XXX TODO
        // update status text?
    },

    onSearchDone: function(status)
    {
        gSearchStopButton.setAttribute("label", gSearchBundle.getString("labelForSearchButton"));
        gSearchStopButton.setAttribute("accesskey", gSearchBundle.getString("accesskeyForSearchButton"));
        gStatusFeedback._stopMeteors();
        SetAdvancedSearchStatusText(gSearchView.QueryInterface(Components.interfaces.nsITreeView).rowCount);
    },

    onNewSearch: function()
    {
      gSearchStopButton.setAttribute("label", gSearchBundle.getString("labelForStopButton"));
      gSearchStopButton.setAttribute("accesskey", gSearchBundle.getString("accesskeyForStopButton"));
      UpdateMailSearch("new-search");	
      gStatusFeedback._startMeteors();
      gStatusFeedback.showStatusString(gSearchBundle.getString("searchingMessage"));
    }
}

// the folderListener object
var gFolderListener = {
    OnItemAdded: function(parentItem, item) {},

    OnItemRemoved: function(parentItem, item){},

    OnItemPropertyChanged: function(item, property, oldValue, newValue) {},

    OnItemIntPropertyChanged: function(item, property, oldValue, newValue) {},

    OnItemBoolPropertyChanged: function(item, property, oldValue, newValue) {},

    OnItemUnicharPropertyChanged: function(item, property, oldValue, newValue){},
    OnItemPropertyFlagChanged: function(item, property, oldFlag, newFlag) {},

    OnItemEvent: function(folder, event) {
        var eventType = event.toString();
        
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
  CreateMessenger();

  gSearchBundle = document.getElementById("bundle_search");
  gMessengerBundle = document.getElementById("bundle_messenger");
  setupDatasource();
  setupSearchListener();

  if (window.arguments && window.arguments[0])
      selectFolder(window.arguments[0].folder);

  onMore(null);
  UpdateMailSearch("onload");
  
  // hide and remove these columns from the column picker.  you can't thread search results
  HideSearchColumn("threadCol"); // since you can't thread search results
  HideSearchColumn("totalCol"); // since you can't thread search results
  HideSearchColumn("unreadCol"); // since you can't thread search results
  HideSearchColumn("unreadButtonColHeader");
  HideSearchColumn("statusCol");
  HideSearchColumn("sizeCol");
  HideSearchColumn("flaggedCol");
  HideSearchColumn("idCol");
  HideSearchColumn("junkStatusCol");
  
  // we want to show the location column for search
  ShowSearchColumn("locationCol");
}

function searchOnUnload()
{
    // unregister listeners
    gSearchSession.unregisterListener(gViewSearchListener);
    gSearchSession.unregisterListener(gSearchNotificationListener);

    gMailSession.RemoveFolderListener(gFolderListener);
	
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

function onResetSearch(event) {
    onReset(event);
    
    var tree = GetThreadTree();
    tree.treeBoxObject.view = null;
    gStatusFeedback.showStatusString("");
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

function selectFolder(folder) 
{
    var folderURI;

    // if we can't search messages on this folder, just select the first one
    if (!folder || !folder.server.canSearchMessages) {
        // walk folders to find first item
        var firstItem = getFirstItemByTag(gFolderPicker, "menu");
        folderURI = firstItem.id;
    } else {
        folderURI = folder.URI;
    }
    updateSearchFolderPicker(folderURI);
}

function updateSearchFolderPicker(folderURI) 
{ 
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
    // allow the tree to reset itself
    var treeView = gSearchView.QueryInterface(Components.interfaces.nsITreeView);
    if (treeView)
    {
      var tree = GetThreadTree();
      tree.treeBoxObject.view = treeView;
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
        if (nextFolder && ! (nextFolder.flags & MSG_FOLDER_FLAG_VIRTUAL))
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

function AddSubFoldersToURI(folder) 
{
  var returnString = "";
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
        if (nextFolder && ! (nextFolder.flags & MSG_FOLDER_FLAG_VIRTUAL))
        {
          if (!nextFolder.noSelect && !nextFolder.isServer)
          {
            if (returnString.length > 0)
              returnString += '|';
            returnString += nextFolder.URI;
          }
          var subFoldersString = AddSubFoldersToURI(nextFolder);
          if (subFoldersString.length > 0)
          {
            if (returnString.length > 0)
              returnString += '|';
            returnString += subFoldersString;
          }

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
  return returnString;
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
  displayMessageChanged : function(aFolder, aSubject, aKeywords)
  {
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

    gMailSession = Components.classes[mailSessionContractID].getService(Components.interfaces.nsIMsgMailSession);
    var nsIFolderListener = Components.interfaces.nsIFolderListener;
    var notifyFlags = nsIFolderListener.event;
    gMailSession.AddFolderListener(gFolderListener, notifyFlags);

    // the datasource is a listener on the search results
    gViewSearchListener = gSearchView.QueryInterface(Components.interfaces.nsIMsgSearchNotify);
    gSearchSession.registerListener(gViewSearchListener);
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
        gFolderDatasource = Components.classes[folderDSContractID].getService(Components.interfaces.nsIRDFDataSource);
    return gFolderDatasource;
}

// used to determine if we should try to load a message
function IsThreadAndMessagePaneSplitterCollapsed()
{
    return true;
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

function MsgDeleteSelectedMessages(aCommandType)
{
    // we don't delete news messages, we just return in that case
    if (isNewsURI(gSearchView.getURIForViewIndex(0))) 
        return;

    // if mail messages delete
    SetNextMessageAfterDelete();
    gSearchView.doCommand(aCommandType);
}

function SetNextMessageAfterDelete()
{
  gNextMessageViewIndexAfterDelete = gSearchView.msgToSelectAfterDelete;
}

function HandleDeleteOrMoveMessageFailed(folder)
{
  gNextMessageViewIndexAfterDelete = -2;
}

function HandleDeleteOrMoveMessageCompleted(folder)
{
  var treeView = gSearchView.QueryInterface(Components.interfaces.nsITreeView);
  var treeSelection = treeView.selection;
  var viewSize = treeView.rowCount;

  if (gNextMessageViewIndexAfterDelete == -2) {
    // a move or delete can cause our selection can change underneath us.
    // this can happen when the user
    // deletes message from the stand alone msg window
    // or the three pane
    if (!treeSelection) {
      // this can happen if you open the search window
      // and before you do any searches
      // and you do delete from another mail window
      return;
    }
    else if (treeSelection.count == 0) {
      // this can happen if you double clicked a message
      // in the thread pane, and deleted it from the stand alone msg window
      // see bug #185147
      treeSelection.clearSelection();

      UpdateMailSearch("delete from another view, 0 rows now selected");
    }
    else if (treeSelection.count == 1) {
      // this can happen if you had two messages selected
      // in the search results pane, and you deleted one of them from another view
      // (like the view in the stand alone msg window or the three pane)
      // since one item is selected, we should load it.
      var startIndex = {};
      var endIndex = {};
      treeSelection.getRangeAt(0, startIndex, endIndex);
        
      // select the selected item, so we'll load it
      treeSelection.select(startIndex.value); 
      treeView.selectionChanged();

      EnsureRowInThreadTreeIsVisible(startIndex.value); 
      UpdateMailSearch("delete from another view, 1 row now selected");
    }
    else {
      // this can happen if you have more than 2 messages selected
      // in the search results pane, and you deleted one of them from another view
      // (like the view in the stand alone msg window or the three pane)
      // since multiple messages are still selected, do nothing.
    }
  }
  else {
    if (gNextMessageViewIndexAfterDelete != nsMsgViewIndex_None && gNextMessageViewIndexAfterDelete >= viewSize) 
    {
      if (viewSize > 0)
        gNextMessageViewIndexAfterDelete = viewSize - 1;
      else
      {           
        gNextMessageViewIndexAfterDelete = nsMsgViewIndex_None;

        // there is nothing to select since viewSize is 0
        treeSelection.clearSelection();

        UpdateMailSearch("delete from current view, 0 rows left");
      }
    }

    // if we are about to set the selection with a new element then DON'T clear
    // the selection then add the next message to select. This just generates
    // an extra round of command updating notifications that we are trying to
    // optimize away.
    if (gNextMessageViewIndexAfterDelete != nsMsgViewIndex_None) 
    {
      treeSelection.select(gNextMessageViewIndexAfterDelete);
      // since gNextMessageViewIndexAfterDelete probably has the same value
      // as the last index we had selected, the tree isn't generating a new
      // selectionChanged notification for the tree view. So we aren't loading the 
      // next message. to fix this, force the selection changed update.
      if (treeView)
        treeView.selectionChanged();

      EnsureRowInThreadTreeIsVisible(gNextMessageViewIndexAfterDelete); 

      // XXX TODO
      // I think there is a bug in the suppression code above.
      // what if I have two rows selected, and I hit delete, 
      // and so we load the next row.
      // what if I have commands that only enable where 
      // exactly one row is selected?
      UpdateMailSearch("delete from current view, at least one row selected");
    }
  }

  // default value after delete/move/copy is over
  gNextMessageViewIndexAfterDelete = -2;

  // something might have been deleted, so update the status text
  SetAdvancedSearchStatusText(viewSize);
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

function saveAsVirtualFolder()
{
  var preselectedURI = gCurrentFolder.URI;
  searchFolderURIs = preselectedURI;

  var searchSubfolders = document.getElementById("checkSearchSubFolders").checked;
  if (gCurrentFolder && (searchSubfolders || gCurrentFolder.isServer || gCurrentFolder.noSelect))
  {
    var subFolderURIs = AddSubFoldersToURI(gCurrentFolder);
    if (subFolderURIs.length > 0)
      searchFolderURIs += '|' + subFolderURIs;
  }

  var dialog = window.openDialog("chrome://messenger/content/virtualFolderProperties.xul", "",
                                 "chrome,titlebar,modal,centerscreen",
                                 {preselectedURI:preselectedURI,
                                  searchTerms:gSearchSession.searchTerms,
                                  searchFolderURIs: searchFolderURIs});
}

