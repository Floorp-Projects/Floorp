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

var nsMsgFilterMotion = Components.interfaces.nsMsgFilterMotion;

var gBundle;

function getBundle()
{
    if (!gBundle) gBundle = srGetStrBundle("chrome://messenger/locale/search.properties");
    return gBundle;
}

function onLoad()
{
    rdf = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

    editButton = document.getElementById("editButton");
    deleteButton = document.getElementById("deleteButton");

    doSetOKCancel(onOk, null);
    
    updateButtons();
    
    var firstItem;
    var args = window.arguments;
    if (args && args[0])
        firstItem = args[0].initialServerUri;
    
    else {
        var serverMenu = document.getElementById("serverMenu");
        var menuitems = serverMenu.getElementsByTagName("menuitem");
        firstItem = menuitems[1].id;
    }
    
    selectServer(firstItem);
	moveToAlertPosition();
}

function onOk()
{
    // make sure to save the filter to disk
    filterList =  currentFilterList();
    if (filterList) filterList.saveToDefaultFile();
    window.close();
}

function onServerClick(event)
{
    var item = event.target;

    // don't check this in.
    dump("setServer(\"" + item.id + "\");\n");
    setTimeout("setServer(\"" + item.id + "\");", 0);
}

// roots the tree at the specified server
function setServer(uri)
{
    var tree = document.getElementById("filterTree");
    tree.setAttribute("ref", uri);
    updateButtons();
}

function onToggleEnabled(event)
{
    var item = event.target;
    while (item && item.localName != "treeitem") {
        lastItem = item;
        item = item.parentNode;
    }
    
    var filterResource = rdf.GetUnicodeResource(item.id);
    filter = filterResource.GetDelegate("filter",
                                        Components.interfaces.nsIMsgFilter);
    filter.enabled = !filter.enabled;
    refreshFilterList();
}

// sets up the menulist and the tree
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
    var selection = document.getElementById("filterTree").selectedItems;
    if (!selection || selection.length <=0)
        return null;

    var filter;
    try {
        var filterResource = rdf.GetUnicodeResource(selection[0].id);
        filter = filterResource.GetDelegate("filter",
                                            Components.interfaces.nsIMsgFilter);
    } catch (ex) {
        dump(ex);
        dump("no filter selected!\n");
    }
    return filter;
}

function currentFilterList()
{
    var serverMenu = document.getElementById("serverMenu");
    var serverUri = serverMenu.data;

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
    
    window.openDialog("chrome://messenger/content/FilterEditor.xul", "FilterEditor", "chrome,modal,titlebar,resizable", args);

    if (args.refresh)
        refreshFilterList();
}

function onNewFilter()
{
    var curFilterList = currentFilterList();
    var args = {filterList: curFilterList };
    
  window.openDialog("chrome://messenger/content/FilterEditor.xul", "FilterEditor", "chrome,modal,titlebar,resizable", args);

  if (args.refresh) refreshFilterList();
  
}

function onDeleteFilter()
{
    var filter = currentFilter();
    if (!filter) return;
    var filterList = currentFilterList();
    if (!filterList) return;

    var confirmStr = getBundle().GetStringFromName("deleteFilterConfirmation");
    
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

function refreshFilterList() {
    var tree = document.getElementById("filterTree");
    if (!tree) return;

    var selection;
    
    var selectedItems = tree.selectedItems;
    if (selectedItems && selectedItems.length >0)
        selection = tree.selectedItems[0].id;

    tree.clearItemSelection();
    tree.setAttribute("ref", tree.getAttribute("ref"));

    if (selection) {
        var newItem = document.getElementById(selection);

        // sometimes the selected element is gone.
        if (newItem) {
            tree.selectItem(newItem);
            tree.ensureElementIsVisible(newItem);
        }
    }
}

function updateButtons()
{
    var filter = currentFilter();
    if (filter) {
        editButton.removeAttribute("disabled");
        deleteButton.removeAttribute("disabled");
    } else {
        editButton.setAttribute("disabled", "true");
        deleteButton.setAttribute("disabled", "true");
    }                      
}

