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

var rdfDatasourcePrefix = "component://netscape/rdf/datasource?name=";
var rdfServiceProgID    = "component://netscape/rdf/rdf-service";
var searchSessionProgID = "component://netscape/messenger/searchSession";
var folderDSProgID         = rdfDatasourcePrefix + "mailnewsfolders";
var gSearchDatasource;

var nsIMsgFolder = Components.interfaces.nsIMsgFolder;
var nsIMsgWindow = Components.interfaces.nsIMsgWindow;
var nsIMsgRDFDataSource = Components.interfaces.nsIMsgRDFDataSource;
var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;

var gFolderDatasource;
var gFolderPicker;
var gThreadTree;
var gStatusFeedback = new nsMsgStatusFeedback;
var RDF;

function searchOnLoad()
{
    initializeSearchWidgets();
    initializeSearchWindowWidgets();

    setupDatasource();

    if (window.arguments && window.arguments[0])
        selectFolder(window.arguments[0].folder);
    
    onMore(null);

}

function searchOnUnload()
{
    // release this early because msgWindow holds a weak reference
    msgWindow.rootDocShell = null;
}

function initializeSearchWindowWidgets()
{
    gFolderPicker = document.getElementById("searchableFolders");
    gThreadTree = document.getElementById("threadTree");

    msgWindow = Components.classes[msgWindowProgID].createInstance(nsIMsgWindow);
    msgWindow.statusFeedback = gStatusFeedback;
    msgWindow.SetDOMWindow(window);
}


function onSearchStop() {

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
        folderResource =
            folder.QueryInterface(Components.interfaces.nsIRDFResource);
        dump("Selecting " + folderResource.Value + "\n");

        
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
    dump("id = " + selectedItem.id + "\n");
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
    dump("setting up search..\n");
    gSearchSession.clearScopes();
    // tell the search session what the new scope is
    gSearchSession.addScopeTerm(GetScopeForFolder(gCurrentFolder),
                                gCurrentFolder);

    var searchSubfolders = document.getElementById("checkSearchSubFolders").checked;
	if (gCurrentFolder && (searchSubfolders || gCurrentFolder.isServer) && gCurrentFolder.hasSubFolders)
	{
		var subFolderEnumerator = gCurrentFolder.GetSubFolders();
		var done = false;
		while (!done)
		{
			var next = subFolderEnumerator.currentItem();
			if (next)
			{
				var nextFolder = next.QueryInterface(Components.interfaces.nsIMsgFolder);
				if (nextFolder)
					gSearchSession.addScopeTerm(GetScopeForFolder(nextFolder), nextFolder);
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
    // reflect the search widgets back into the search session
    saveSearchTerms(gSearchSession.searchTerms, gSearchSession);

    gSearchSession.search(msgWindow);
    // refresh the tree after the search starts, because initiating the
    // search will cause the datasource to clear itself
    gThreadTree.setAttribute("ref", gThreadTree.getAttribute("ref"));
    dump("Kicking it off with " + gThreadTree.getAttribute("ref") + "\n");
}


function GetScopeForFolder(folder) {
    if (folder.server.type == "nntp")
        return nsMsgSearchScope.Newsgroup;
    else
        return nsMsgSearchScope.MailFolder;
}
    
function setupDatasource() {

    RDF = Components.classes[rdfServiceProgID].getService(Components.interfaces.nsIRDFService);
    
    gSearchDatasource = Components.classes[rdfDatasourcePrefix + "msgsearch"].createInstance(Components.interfaces.nsIRDFDataSource);

    dump("The root is " + gSearchDatasource.URI + "\n");
    gThreadTree.setAttribute("ref", gSearchDatasource.URI);
    
    // the thread pane needs to use the search datasource (to get the
    // actual list of messages) and the message datasource (to get any
    // attributes about each message)
    gSearchSession = Components.classes[searchSessionProgID].createInstance(Components.interfaces.nsIMsgSearchSession);
    
    setMsgDatasourceWindow(gSearchDatasource, msgWindow);
    gThreadTree.database.AddDataSource(gSearchDatasource);

    var messageDatasource = Components.classes[rdfDatasourcePrefix + "mailnewsmessages"].createInstance(Components.interfaces.nsIRDFDataSource);
    setMsgDatasourceWindow(messageDatasource, msgWindow);
    
    gThreadTree.database.AddDataSource(messageDatasource);
    
    // the datasource is a listener on the search results
    searchListener = gSearchDatasource.QueryInterface(Components.interfaces.nsIMsgSearchNotify);
    gSearchSession.registerListener(searchListener);

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
        gFolderDatasource = Components.classes[folderDSProgID].createInstance(Components.interfaces.nsIRDFDataSource);
    return gFolderDatasource;
}

// used to determine if we should try to load a message
function IsThreadAndMessagePaneSplitterCollapsed()
{
    return true;
}

function setMsgDatasourceWindow(ds, msgwindow)
{
    dump("setMsgDatasourceWindow(" + ds + ")\n");
    try {
        var msgDatasource = ds.QueryInterface(nsIMsgRDFDataSource);
        msgDatasource.window = msgwindow;
    } catch (ex) {
        dump("error setting DS on " + ds + ": " + ex + "\n");
    }
}
