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
var messengerProgID        = "component://netscape/messenger";
var statusFeedbackProgID   = "component://netscape/messenger/statusfeedback";
var messageViewProgID      = "component://netscape/messenger/messageview";
var mailSessionProgID      = "component://netscape/messenger/services/session";
var prefProgID             = "component://netscape/preferences";

var datasourceProgIDPrefix = "component://netscape/rdf/datasource?name=";
var accountManagerDSProgID = datasourceProgIDPrefix + "msgaccountmanager";
var folderDSProgID         = datasourceProgIDPrefix + "mailnewsfolders";
var messageDSProgID        = datasourceProgIDPrefix + "mailnewsmessages";

var gFolderTree;
var gThreadTree;

// get the messenger instance
var messenger = Components.classes[messengerProgID].createInstance();
messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);

//Create datasources
var accountManagerDataSource = Components.classes[accountManagerDSProgID].createInstance();
var folderDataSource         = Components.classes[folderDSProgID].createInstance();
var messageDataSource        = Components.classes[messageDSProgID].createInstance();

//Create windows status feedback
var statusFeedback           = Components.classes[statusFeedbackProgID].createInstance();
statusFeedback = statusFeedback.QueryInterface(Components.interfaces.nsIMsgStatusFeedback);

//Create message view object
var messageView = Components.classes[messageViewProgID].createInstance();
messageView = messageView.QueryInterface(Components.interfaces.nsIMessageView);

// the folderListener object
var folderListener = {
    OnItemAdded: function(parentFolder, item) {},

	OnItemRemoved: function(parentFolder, item){
		dump('in OnItemRemoved\n');
	},

	OnItemPropertyChanged: function(item, property, oldValue, newValue) {},

	OnItemPropertyFlagChanged: function(item, property, oldFlag, newFlag) {}
}

/* Functions related to startup */
function OnLoadMessenger()
{

    verifyAccounts();
    
    loadStartPage();
	messenger.SetWindow(window, statusFeedback);

	AddDataSources();
	InitPanes();

    loadStartFolder();

    getFolderListener();
	
	// FIX ME! - can remove these as soon as waterson enables auto-registration
	document.commandDispatcher.addCommand(document.getElementById('cmd_selectAll'));
	document.commandDispatcher.addCommand(document.getElementById('cmd_delete'));
}

function OnUnloadMessenger()
{
	dump("\nOnUnload from XUL\nClean up ...\n");
	var mailSession = Components.classes[mailSessionProgID].getService();
	if(mailSession)
	{
		mailSession = mailSession.QueryInterface(Components.interfaces.nsIMsgMailSession);
		if(mailSession)
			mailSession.RemoveFolderListener(folderListener);
	}
	messenger.OnUnload();
}

function verifyAccounts() {
    try {
        var mail = Components.classes[mailSessionProgID].getService(Components.interfaces.nsIMsgMailSession);

        var am = mail.accountManager;
        var accounts = am.accounts;

        // as long as we have some accounts, we're fine.
        if (accounts.Count() > 0) return;

        try {
            dump("attempt to UpgradePrefs.  If that fails, open the account wizard.\n");
            am.UpgradePrefs();
            refreshFolderPane();
        }
        catch (ex) {
            // upgrade prefs failed, so open account wizard
            MsgAccountWizard();
        }
        

    }
    catch (ex) {
        dump("error verifying accounts " + ex + "\n");
        return;
    }
}


function loadStartPage() {

	var startpage = "about:blank";

    if (!pref) return;

    try {
        var pref = Components.classes[prefProgID].getService(Components.interfaces.nsIPref);

		startpageenabled= pref.GetBoolPref("mailnews.start_page.enabled");
        
		if (startpageenabled)
			startpage = pref.CopyCharPref("mailnews.start_page.url");
        window.frames["messagepane"].location = startpage;

        dump("start message pane with: " + startpage + "\n");
	}
    catch (ex) {
        dump("Error loading start page.\n");
        return;
    }
}

function loadStartFolder()
{
	//Load StartFolder
    try {
        var pref = Components.classes[prefProgID].getService(Components.interfaces.nsIPref);
        
        var startFolder = pref.CopyCharPref("mailnews.start_folder");
        ChangeFolderByURI(startFolder);
		//	var folder = OpenFolderTreeToFolder(startFolder);
    }
    catch(ex) {

    }

}

function getFolderListener()
{
    try {
        var mailSession = Components.classes[mailSessionProgID].getService(Components.interfaces.nsIMsgMailSession);
        
        mailSession.AddFolderListener(folderListener);
	} catch (ex) {
        dump("Error adding folder listener\n");
    }
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
	windowData.messageView = messageView;

	windowData = messageDataSource.QueryInterface(Components.interfaces.nsIMsgWindowData);
	windowData.statusFeedback = statusFeedback;
	windowData.messageView = messageView;

	windowData = accountManagerDataSource.QueryInterface(Components.interfaces.nsIMsgWindowData);
	windowData.statusFeedback = statusFeedback;
	windowData.messageView = messageView;

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
    gFolderTree = folderTree;
	SortFolderPane('FolderColumn', 'http://home.netscape.com/NC-rdf#Name');
	//Add folderDataSource and accountManagerDataSource to folderPane
	accountManagerDataSource = accountManagerDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	folderTree.database.AddDataSource(accountManagerDataSource);
    folderTree.database.AddDataSource(folderDataSource);
	folderTree.setAttribute('ref', 'msgaccounts:/');
	
	SetupCommandUpdateHandlers();
}

function OnLoadThreadPane(threadTree)
{
    gThreadTree = threadTree;
	// add folderSource to thread pane
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	threadTree.database.AddDataSource(folderDataSource);

	//Add message data source
	messageDataSource = messageDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	threadTree.database.AddDataSource(messageDataSource);

	ShowThreads(false);
}

/* Functions for accessing particular parts of the window*/
function GetFolderTree()
{
    if (gFolderTree) return gFolderTree;
    
	var folderTree = FindInSidebar(frames[0], 'folderTree');
    gFolderTree = folderTree;
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
    if (gThreadTree) return gThreadTree;
	var threadTree = document.getElementById('threadTree');
	if(!threadTree)
		dump('thread tree is null\n');
    gThreadTree = threadTree;
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

function ClearThreadTreeSelection()
{
	var tree = GetThreadTree();
	if(tree)
		tree.clearItemSelection();

}

function ClearMessagePane()
{
	messenger.OpenURL("about:blank");	

}



