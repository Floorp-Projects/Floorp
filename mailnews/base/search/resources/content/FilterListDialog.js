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

var rdf;

var editButton;
var deleteButton;
var reorderUpButton;
var reorderDownButton;

const nsMsgFilterMotion = Components.interfaces.nsMsgFilterMotion;

var gFilterBundle;
var gPromptService;
var gFilterListMsgWindow = null;
var gFilterTree;

function onLoad()
{
    rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

    var gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
    gPromptService = gPromptService.QueryInterface(Components.interfaces.nsIPromptService);

    var msgWindowContractID = "@mozilla.org/messenger/msgwindow;1";
    var nsIMsgWindow = Components.interfaces.nsIMsgWindow;
    gFilterListMsgWindow = Components.classes[msgWindowContractID].createInstance(nsIMsgWindow);
    gFilterListMsgWindow.SetDOMWindow(window); 

    gFilterBundle = document.getElementById("bundle_filter");
    gFilterTree = document.getElementById("filterTree");

    editButton = document.getElementById("editButton");
    deleteButton = document.getElementById("deleteButton");
    reorderUpButton = document.getElementById("reorderUpButton");
    reorderDownButton = document.getElementById("reorderDownButton");

    updateButtons();

    moveToAlertPosition();

    // get the selected server if it can have filters.
    var firstItem = getSelectedServerForFilters();

    // if the selected server cannot have filters, get the default server
    // if the default server cannot have filters, check all accounts
    // and get a server that can have filters.
    if (!firstItem)
        firstItem = getServerThatCanHaveFilters();

    if (firstItem) {
        selectServer(firstItem);
    }
    
    if (("arguments" in window) && window.arguments[0] && ("prefillValue" in window.arguments[0])) 
        onNewFilter(window.arguments[0].prefillValue);
}

function onOk()
{
  // make sure to save the filter to disk
  var filterList = currentFilterList();
  if (filterList) filterList.saveToDefaultFile();
  window.close();
}

function onServerClick(event)
{
    var item = event.target;

    // don't check this in.
    setTimeout("setServer(\"" + item.id + "\");", 0);
}

// roots the tree at the specified server
function setServer(uri)
{
   var resource = rdf.GetResource(uri);
   var msgFolder =  resource.QueryInterface(Components.interfaces.nsIMsgFolder);

   //Calling getFilterList will detect any errors in rules.dat, backup the file, and alert the user
   //we need to do this because gFilterTree.setAttribute will cause rdf to call getFilterList and there is 
   //no way to pass msgWindow in that case. 

   if (msgFolder)
     msgFolder.getFilterList(gFilterListMsgWindow);

    gFilterTree.setAttribute("ref", uri);
    updateButtons();
}

function toggleFilter(aFilterURI)
{
    var filterResource = rdf.GetResource(aFilterURI);
    var filter = filterResource.GetDelegate("filter",
                                            Components.interfaces.nsIMsgFilter);
    filter.enabled = !filter.enabled;
    refreshFilterList();
}

// sets up the menulist and the gFilterTree
function selectServer(uri)
{
    // update the server menu
    var serverMenu = document.getElementById("serverMenu");
    var menuitems = serverMenu.getElementsByAttribute("id", uri);
    serverMenu.selectedItem = menuitems[0];
    setServer(uri);
}

function currentFilter()
{
    if (gFilterTree.currentIndex == -1)
        return null;
    
    var filter;
    try {
        filter = gFilterTree.builderView.getResourceAtIndex(gFilterTree.currentIndex);
        filter = filter.GetDelegate("filter", Components.interfaces.nsIMsgFilter);
    } catch (ex) {
        dump(ex);
        dump("no filter selected!\n");
    }
    return filter;
}

function currentFilterList()
{
    var serverMenu = document.getElementById("serverMenu");
    var serverUri = serverMenu.value;
    var filterList = rdf.GetResource(serverUri).GetDelegate("filter", Components.interfaces.nsIMsgFilterList);
    return filterList;
}

function onFilterSelect(event)
{
    updateButtons();
}

function onEditFilter() {
    var selectedFilter = currentFilter();
    var args = {filter: selectedFilter};

    window.openDialog("chrome://messenger/content/FilterEditor.xul", "FilterEditor", "chrome,modal,titlebar,resizable,centerscreen", args);

    if ("refresh" in args && args.refresh)
        refreshFilterList();
}

function onNewFilter(emailAddress)
{
  var curFilterList = currentFilterList();
  var args = {filterList: curFilterList};
  if(emailAddress) {
    args.okCallback = function() {};
    args.filterName = emailAddress;
  }
    
  window.openDialog("chrome://messenger/content/FilterEditor.xul", "FilterEditor", "chrome,modal,titlebar,resizable,centerscreen", args);

  if ("refresh" in args && args.refresh)
    refreshFilterList();
}

function onDeleteFilter()
{
    var filter = currentFilter();
    if (!filter) return;
    var filterList = currentFilterList();
    if (!filterList) return;

    var confirmStr = gFilterBundle.getString("deleteFilterConfirmation");

    if (!window.confirm(confirmStr)) return;

    filterList.removeFilter(filter);
    refreshFilterList();
}

function onUp(event)
{
    moveCurrentFilter(nsMsgFilterMotion.up);
}

function onDown(event)
{
    moveCurrentFilter(nsMsgFilterMotion.down);
}

function moveCurrentFilter(motion)
{
    var filterList = currentFilterList();
    var filter = currentFilter();
    if (!filterList) return;
    if (!filter) return;

    filterList.moveFilter(filter, motion);
    refreshFilterList();
}

function refreshFilterList() 
{
    if (!gFilterTree) 
      return;

    // store the selected resource before we rebuild the tree
    var selectedRes = gFilterTree.currentIndex >= 0 ? gFilterTree.builderView.getResourceAtIndex(gFilterTree.currentIndex) : null;

    // rebuild the tree    
    gFilterTree.view.selection.clearSelection();
    gFilterTree.builder.rebuild();

    // restore selection to the previous selected resource
    if (selectedRes) {
        var previousSel = gFilterTree.builderView.getIndexOfResource(selectedRes);
        if (previousSel >= 0) {
            // sometimes the selected element is gone.
            gFilterTree.treeBoxObject.selection.select(previousSel);
            gFilterTree.treeBoxObject.ensureRowIsVisible(previousSel);
        }
    }
}

function updateButtons()
{
    var filterSelected = gFilterTree.view.selection.count > 0;
    editButton.disabled = !filterSelected;
    deleteButton.disabled = !filterSelected;
    reorderUpButton.disabled = !(filterSelected && gFilterTree.currentIndex > 0);
    reorderDownButton.disabled = !(filterSelected && gFilterTree.currentIndex < gFilterTree.view.rowCount-1);
}

/**
  * get the selected server if it can have filters
  */
function getSelectedServerForFilters()
{
    var firstItem = null;
    var args = window.arguments;
    var selectedFolder = args[0].folder;

    if (args && args[0] && selectedFolder)
    {
        var msgFolder = selectedFolder.QueryInterface(Components.interfaces.nsIMsgFolder);
        try
        {
            var rootFolder = msgFolder.rootFolder;
            if (rootFolder.isServer)
            {
                var server = rootFolder.server;

                if (server.canHaveFilters)
                {
                    firstItem = rootFolder.URI;
                }
            }
        }
        catch (ex)
        {
            //dump("**** exception: "+ex+"\n");
            //dump("**** !!!!!!! NOT A VALID msgfolder\n");
        }
    }

    return firstItem;
}

/** if the selected server cannot have filters, get the default server
  * if the default server cannot have filters, check all accounts
  * and get a server that can have filters.
  */
function getServerThatCanHaveFilters()
{
    var firstItem = null;

    var accountManager
        = Components.classes["@mozilla.org/messenger/account-manager;1"].
            getService(Components.interfaces.nsIMsgAccountManager);

    var defaultAccount = accountManager.defaultAccount;
    var defaultIncomingServer = defaultAccount.incomingServer;

    // check to see if default server can have filters
    if (defaultIncomingServer.canHaveFilters) {
        firstItem = defaultIncomingServer.serverURI;
    }
    // if it cannot, check all accounts to find a server
    // that can have filters
    else
    {
        var allServers = accountManager.allServers;
        var numServers = allServers.Count();
        var index = 0;
        for (index = 0; index < numServers; index++)
        {
            var currentServer
            = allServers.GetElementAt(index).QueryInterface(Components.interfaces.nsIMsgIncomingServer);

            if (currentServer.canHaveFilters)
            {
                firstItem = currentServer.serverURI;
                break;
            }
        }
    }

    return firstItem;
}

function onFilterClick(event)
{
    // we only care about button 0 (left click) events
    if (event.button != 0 || event.originalTarget.localName != "treechildren") return;
    var row = {}, colID = {}, childElt = {};
    var filterTree = document.getElementById("filterTree");
    filterTree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, colID, childElt);
    if (row.value == -1 || row.value > filterTree.view.rowCount-1)
        return;
        
    if (colID.value == "activeColumn") {
      var res = filterTree.builderView.getResourceAtIndex(row.value);
      toggleFilter(res.Value);
    }
}

function onFilterDoubleClick(event)
{
    // we only care about button 0 (left click) events
    if (event.button != 0 || event.originalTarget.localName != "treechildren")
      return;

    onEditFilter();
}

function onFilterTreeKeyPress(event)
{
  // for now, only do something on space key
  if (event.keyCode != 0)
    return;

  var rangeCount = gFilterTree.view.selection.getRangeCount();
  for (var i = 0; i < rangeCount; ++i) {
    var start = {}, end = {};
    gFilterTree.view.selection.getRangeAt(i, start, end);
    for (var k = start.value; k <= end.value; ++k) {
      var res = gFilterTree.builderView.getResourceAtIndex(k);
      toggleFilter(res.Value);
    }
  }
}
