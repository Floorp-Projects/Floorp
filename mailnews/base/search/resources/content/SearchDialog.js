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
 */

var rdfDatasourcePrefix = "@mozilla.org/rdf/datasource;1?name=";
var rdfServiceContractID    = "@mozilla.org/rdf/rdf-service;1";
var searchSessionContractID = "@mozilla.org/messenger/searchSession;1";
var folderDSContractID         = rdfDatasourcePrefix + "mailnewsfolders";
var gSearchView;
var gSearchSession;
var gCurrentFolder;

var nsIMsgFolder = Components.interfaces.nsIMsgFolder;
var nsIMsgWindow = Components.interfaces.nsIMsgWindow;
var nsIMsgRDFDataSource = Components.interfaces.nsIMsgRDFDataSource;
var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;

var gFolderDatasource;
var gFolderPicker;
var gThreadTree;
var gStatusBar = null;
var gStatusFeedback = new nsMsgStatusFeedback;
var gNumOfSearchHits = 0;
var RDF;
var gSearchBundle;

// Datasource search listener -- made global as it has to be registered
// and unregistered in different functions.
var gDataSourceSearchListener;
var gViewSearchListener;
var gIsSearchHit = false;

var gButton;

// Controller object for search results thread pane
var nsSearchResultsController =
{
    supportsCommand: function(command)
    {
        switch(command) {
        case "cmd_open":
            return true;
        default:
            return false;
        }
    },

    // this controller only handles commands
    // that rely on items being selected in
    // the threadpane.
    isCommandEnabled: function(command)
    {
        var enabled = true;
        if (GetNumSelectedMessages() <= 0)
          enabled = false;

        return enabled;
    },

    doCommand: function(command)
    {
        switch(command) {
        case "cmd_open":
            MsgOpenSelectedMessages(gSearchView);
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
        gButton.setAttribute("value", gSearchBundle.getString("labelForSearchButton"));

        var statusMsg;
        // if there are no hits, it means no matches were found in the search.
        if (!gIsSearchHit) {
            gStatusFeedback.showStatusString(gSearchBundle.getString("searchFailureMessage"));
        }
        else
        {
            gStatusFeedback.showStatusString(gSearchBundle.getString("searchSuccessMessage"));
            gIsSearchHit = false;
        }

        gStatusFeedback.showProgress(100);
        gStatusFeedback.showStatusString(statusMsg);
        gStatusBar.setAttribute("mode","normal");
    },
	
    onNewSearch: function() 
    {
      gButton.setAttribute("value", gSearchBundle.getString("labelForStopButton"));
//        if (gThreadTree)
//            gThreadTree.clearItemSelection();

      document.commandDispatcher.updateCommands('mail-search');
      gStatusFeedback.showProgress(0);
      gStatusFeedback.showStatusString(gSearchBundle.getString("searchingMessage"));
      gStatusBar.setAttribute("mode","undetermined");
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
	moveToAlertPosition();
}


function searchOnUnload()
{
    // unregister listeners
    gSearchSession.unregisterListener(gViewSearchListener);
    gSearchSession.unregisterListener(gSearchNotificationListener);

    // release this early because msgWindow holds a weak reference
    msgWindow.rootDocShell = null;
}

function initializeSearchWindowWidgets()
{
    gFolderPicker = document.getElementById("searchableFolders");
//    gThreadTree = document.getElementById("threadTree");
    gButton = document.getElementById("search-button");
    gStatusBar = document.getElementById('statusbar-icon');

    msgWindow = Components.classes[msgWindowContractID].createInstance(nsIMsgWindow);
    msgWindow.statusFeedback = gStatusFeedback;
    msgWindow.SetDOMWindow(window);

    // functionality to enable/disable buttons using nsSearchResultsController
    // depending of whether items are selected in the search results thread pane.
//    gThreadTree.controllers.appendController(nsSearchResultsController);
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
                result = getFirstItemByTag(node, tag);
                if (result) return result;
            }
        }
    }
    return null;
}

function selectFolder(folder) {
    var items;
    if (!folder) {
        // walk folders to find first menuitem
        var firstMenuitem = getFirstItemByTag(gFolderPicker, "menuitem");
        gFolderPicker.selectedItem = firstMenuitem;
            
    } else {
        // the URI of the folder is in the data attribute of the menuitem
        var folderResource =
            folder.QueryInterface(Components.interfaces.nsIRDFResource);
        
        var elements =
            gFolderPicker.getElementsByAttribute("data", folderResource.Value);
        if (elements && elements.length)
            gFolderPicker.selectedItem = elements[0];
    }
    updateSearchFolderPicker()
}

function updateSearchFolderPicker() {

    var selectedItem = gFolderPicker.selectedItem;
    if (selectedItem.localName != "menuitem") return;
    SetFolderPicker(selectedItem.id, gFolderPicker.id);

    // use the URI to get the real folder
    gCurrentFolder =
        RDF.GetResource(selectedItem.id).QueryInterface(nsIMsgFolder);

    
    setSearchScope(GetScopeForFolder(gCurrentFolder));

}

function onChooseFolder(event) {
    updateSearchFolderPicker();
}

function onSearch(event)
{
    gSearchSession.clearScopes();
    // tell the search session what the new scope is
    if (!gCurrentFolder.isServer)
        gSearchSession.addScopeTerm(GetScopeForFolder(gCurrentFolder),
                                    gCurrentFolder);

    var searchSubfolders = document.getElementById("checkSearchSubFolders").checked;
    if (gCurrentFolder && (searchSubfolders || gCurrentFolder.isServer))
    {
        AddSubFolders(gCurrentFolder);
    }
    // reflect the search widgets back into the search session
    saveSearchTerms(gSearchSession.searchTerms, gSearchSession);

    gSearchSession.search(msgWindow);
    // refresh the tree after the search starts, because initiating the
    // search will cause the datasource to clear itself
//    gThreadTree.setAttribute("ref", gThreadTree.getAttribute("ref"));
//    dump("Kicking it off with " + gThreadTree.getAttribute("ref") + "\n");
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

function GetScopeForFolder(folder) {
    if (folder.server.type == "nntp")
        return nsMsgSearchScope.Newsgroup;
    else
        return nsMsgSearchScope.MailFolder;
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

    var outlinerView = gSearchView.QueryInterface(Components.interfaces.nsIOutlinerView);
    if (outlinerView)
    {     
      var outliner = GetThreadOutliner();
      outliner.boxObject.QueryInterface(Components.interfaces.nsIOutlinerBoxObject).view = outlinerView; 
    }

    // the thread pane needs to use the search datasource (to get the
    // actual list of messages) and the message datasource (to get any
    // attributes about each message)
    gSearchSession = Components.classes[searchSessionContractID].createInstance(Components.interfaces.nsIMsgSearchSession);
    
    
    // the datasource is a listener on the search results
    gViewSearchListener = gSearchView.QueryInterface(Components.interfaces.nsIMsgSearchNotify);
    gSearchSession.registerListener(gViewSearchListener);
}


function setupSearchListener()
{
    // Setup the javascript object as a listener on the search results
    gSearchSession.registerListener(gSearchNotificationListener);
}


// this is test stuff only, ignore for now
function onTesting(event)
{
    var testattr;
    
    DumpDOM(document.getElementById("searchTermTree"));
    testattr = document.getElementById("searchAttr");
    testelement(testattr);

    testattr = document.getElementById("searchAttr0");
    testelement(testattr);
    
    testattr = document.getElementById("searchAttr99");
    testelement(testattr);

}

function testelement(element)
{
    dump(element.id + " = " + element + "\n");
    dump(element.id + ".searchScope = " + element.searchScope + "\n");
    element.searchScope = 0;
    dump(element.id + ".searchScope = " + element.searchScope + "\n");

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
    if (event.target.value == gSearchBundle.getString("labelForSearchButton"))
        onSearch(event);
    else
        onSearchStop(event);
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
