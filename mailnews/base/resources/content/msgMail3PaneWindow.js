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

/* This is where functions related to the 3 pane window are kept */

/* globals for a particular window */
var messenger = Components.classes['component://netscape/messenger'].createInstance();
messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);

//Create datasources
var accountManagerDataSource = Components.classes["component://netscape/rdf/datasource?name=msgaccountmanager"].createInstance();
var folderDataSource = Components.classes["component://netscape/rdf/datasource?name=mailnewsfolders"].createInstance();
var messageDataSource = Components.classes["component://netscape/rdf/datasource?name=mailnewsmessages"].createInstance();
var messageViewDataSource = Components.classes["component://netscape/rdf/datasource?name=mail-messageview"].createInstance();

//Create windows status feedback
var statusFeedback = Components.classes["component://netscape/messenger/statusfeedback"].createInstance();
statusFeedback = statusFeedback.QueryInterface(Components.interfaces.nsIMsgStatusFeedback);

/* Functions related to startup */
function OnLoadMessenger()
{
	var pref = Components.classes['component://netscape/preferences'];
	var startpage = "about:blank";

	if (pref) {
          pref = pref.getService();
        }
        if (pref) {
          pref = pref.QueryInterface(Components.interfaces.nsIPref);
        }
	if (pref) {
		startpageenabled= pref.GetBoolPref("mailnews.start_page.enabled");
		if (startpageenabled) {
			startpage = pref.CopyCharPref("mailnews.start_page.url");
		}
	}
	messenger.SetWindow(window, statusFeedback);
	dump("start message pane with: " + startpage + "\n");
	window.frames["messagepane"].location = startpage;

	AddDataSources();
	InitPanes();

	//Load StartFolder
	if(pref)
	{
		try
		{
			var startFolder = pref.CopyCharPref("mailnews.start_folder");
			ChangeFolderByURI(startFolder);
		//	var folder = OpenFolderTreeToFolder(startFolder);
		}
		catch(ex)
		{
		}
	}
}

function OnUnloadMessenger()
{
	dump("\nOnUnload from XUL\nClean up ...\n");
	messenger.OnUnload();
}

function AddDataSources()
{

	//to move menu item
	accountManagerDataSource = accountManagerDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	var moveMenu = document.getElementById('moveMenu');
	if(moveMenu)
	{
		moveMenu.database.AddDataSource(accountManagerDataSource);
		moveMenu.database.AddDataSource(folderDataSource);
		moveMenu.setAttribute('ref', 'msgaccounts:/');
	}

	//to copy menu item
	var copyMenu = document.getElementById('copyMenu');
	if(copyMenu)
	{
		copyMenu.database.AddDataSource(accountManagerDataSource);
		copyMenu.database.AddDataSource(folderDataSource);
		copyMenu.setAttribute('ref', 'msgaccounts:/');
	}
	//Add statusFeedback
	var windowData = folderDataSource.QueryInterface(Components.interfaces.nsIMsgWindowData);
	windowData.statusFeedback = statusFeedback;

	windowData = messageDataSource.QueryInterface(Components.interfaces.nsIMsgWindowData);
	windowData.statusFeedback = statusFeedback;

	windowData = accountManagerDataSource.QueryInterface(Components.interfaces.nsIMsgWindowData);
	windowData.statusFeedback = statusFeedback;

	windowData = messageViewDataSource.QueryInterface(Components.interfaces.nsIMsgWindowData);
	windowData.statusFeedback = statusFeedback;

}	

function InitPanes()
{
	init_sidebar('messenger', 'chrome://messenger/content/sidebar-messenger.xul',  275);
	var tree = GetThreadTree();
	if(tree);
		OnLoadThreadPane(tree);
}

function OnLoadFolderPane(folderTree)
{
	SortFolderPane('FolderColumn', 'http://home.netscape.com/NC-rdf#Name');
	//Add folderDataSource and accountManagerDataSource to folderPane
	accountManagerDataSource = accountManagerDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	folderTree.database.AddDataSource(accountManagerDataSource);
    folderTree.database.AddDataSource(folderDataSource);
	folderTree.setAttribute('ref', 'msgaccounts:/');
}

function OnLoadThreadPane(threadTree)
{
	//Add FolderDataSource
	//to messageview in thread pane.
	messageViewDataSource = messageViewDataSource.QueryInterface(Components.interfaces.nsIRDFCompositeDataSource);
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	messageViewDataSource.AddDataSource(folderDataSource);

	// add messageViewDataSource to thread pane
	messageViewDataSource = messageViewDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	threadTree.database.AddDataSource(messageViewDataSource);

	//Add message data source
	messageDataSource = messageDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	threadTree.database.AddDataSource(messageDataSource);

	ShowThreads(false);
}

/* Functions for accessing particular parts of the window*/
function GetFolderTree()
{
	var folderTree = FindInSidebar(frames[0], 'folderTree'); 
	return folderTree;
}

function FindInSidebar(currentWindow, id)
{
	var item = currentWindow.document.getElementById(id);
	if(item)
		return item;

	for(var i = 0; i < frames.length; i++)
	{
		var frameItem = FindInSidebar(currentWindow.frames[i], id);
		if(frameItem)
			return frameItem;
	}
}

function GetThreadTree()
{
	var threadTree = document.getElementById('threadTree');
	if(!threadTree)
		dump('thread tree is null\n');
	return threadTree;
}

function GetThreadTreeFolder()
{
  var tree = GetThreadTree();
  return tree;
}

function FindMessenger()
{
  return messenger;
}

function RefreshThreadTreeView()
{
	var currentFolder = GetThreadTreeFolder();  
	var currentFolderID = currentFolder.getAttribute('ref');
	currentFolder.setAttribute('ref', currentFolderID);
}

function ClearMessagePane()
{
	messenger.OpenURL("about:blank");	

}