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

function onLoad()
{
    dump("Loading..\n");
    
    var firstitem;
    var args = window.arguments;
    if (args && args[0])
        firstItem = args[0].initialServerUri;
    
    else {
        var serverMenu = document.getElementById("serverMenu");
        var menuitems = serverMenu.getElementsByTagName("menuitem");
        firstItem = menuitems[1].id;
    }
    
    dump("selecting item " + firstItem + "\n");
    selectServer(firstItem);
}

function onServerClick(event)
{
    var item = event.target;
    setServer(item.id);
}

// roots the tree at the specified server
function setServer(uri)
{
    var tree = document.getElementById("filterTree");
    tree.setAttribute("ref", uri);
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

function EditFilter() {

    var tree = document.getElementById("filterTree");
    var selectedFilter = tree.selectedItems[0];

    var args = {selectedFilter: selectedFilter};
    
    window.openDialog("chrome://messenger/content/FilterEditor.xul", "FilterEditor", "chrome", args);
}

function NewFilter() {
  // pass the URI too, so that we know what filter to put this before
  window.openDialog("chrome://messenger/content/FilterEditor.xul", "FilterEditor", "chrome");

}
