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
var msgWindowProgID		   = "component://netscape/messenger/msgwindow";

var datasourceProgIDPrefix = "component://netscape/rdf/datasource?name=";
var accountManagerDSProgID = datasourceProgIDPrefix + "msgaccountmanager";
var folderDSProgID         = datasourceProgIDPrefix + "mailnewsfolders";
var messageDSProgID        = datasourceProgIDPrefix + "mailnewsmessages";

var gFolderTree;
var gThreadTree;
var gCurrentLoadingFolderURI

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

//Create message window object
var msgWindow = Components.classes[msgWindowProgID].createInstance();
msgWindow = msgWindow.QueryInterface(Components.interfaces.nsIMsgWindow);

// the folderListener object
var folderListener = {
    OnItemAdded: function(parentFolder, item) {},

	OnItemRemoved: function(parentFolder, item){},

	OnItemPropertyChanged: function(item, property, oldValue, newValue) {},

	OnItemPropertyFlagChanged: function(item, property, oldFlag, newFlag) {},

	OnFolderLoaded: function (folder)
	{
		dump('In OnFolderLoader\n');
		if(folder)
		{
			var resource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
			if(resource)
			{
				var uri = resource.Value;
				if(uri == gCurrentLoadingFolderURI)
				{
					gCurrentLoadingFolderURI="";
					var msgFolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
					if(msgFolder)
					{
						msgFolder.endFolderLoading();
						dump("before reroot in OnFolderLoaded\n");
						RerootFolder(uri, msgFolder);
					}
				}
			}
		}
	}
}

/* Functions related to startup */
function OnLoadMessenger()
{
    verifyAccounts();
    
    loadStartPage();
	InitMsgWindow();

	messenger.SetWindow(window, msgWindow);

	AddDataSources();
	InitPanes();

    loadStartFolder();

    AddToSession();
}

function OnUnloadMessenger()
{
	dump("\nOnUnload from XUL\nClean up ...\n");
	var mailSession = Components.classes[mailSessionProgID].getService();
	if(mailSession)
	{
		mailSession = mailSession.QueryInterface(Components.interfaces.nsIMsgMailSession);
		if(mailSession)
		{
			mailSession.RemoveFolderListener(folderListener);
			mailSession.RemoveMsgWindow(msgWindow);
		}
	}

    saveWindowPosition();
	messenger.OnUnload();
}

function saveWindowPosition()
{
    // Get the current window position/size.
    var x = window.screenX;
    var y = window.screenY;
    var h = window.outerHeight;
    var w = window.outerWidth;

    // Store these into the window attributes (for persistence).
    var win = document.getElementById( "messengerWindow" );
    win.setAttribute( "x", x );
    win.setAttribute( "y", y );
    win.setAttribute( "height", h );
    win.setAttribute( "width", w );
    // save x, y, width, height
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

function AddToSession()
{
    try {
        var mailSession = Components.classes[mailSessionProgID].getService(Components.interfaces.nsIMsgMailSession);
        
        mailSession.AddFolderListener(folderListener);
        mailSession.AddMsgWindow(msgWindow);
	} catch (ex) {
        dump("Error adding to session\n");
    }
}

function InitMsgWindow()
{
	msgWindow.statusFeedback = statusFeedback;
	msgWindow.messageView = messageView;
	msgWindow.SetDOMWindow(window);
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

	var msgDS = folderDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = msgWindow;

	msgDS = messageDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = msgWindow;

	msgDS = accountManagerDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = msgWindow;

}	

function InitPanes()
{
	var mailsidebar = new Object
	mailsidebar.db       = 'chrome://messenger/content/sidebar-messenger.rdf'
	mailsidebar.resource = 'NC:MessengerSidebarRoot'

	var threadTree = GetThreadTree();
	if(threadTree);
		OnLoadThreadPane(threadTree);

	var folderTree = GetFolderTree();
	if(folderTree)
		OnLoadFolderPane(folderTree);
		
	top.controllers.appendController(DefaultController);
	SetupCommandUpdateHandlers();
}

function OnLoadFolderPane(folderTree)
{
	dump('In onLoadfolderPane\n');
    gFolderTree = folderTree;
	SortFolderPane('FolderColumn', 'http://home.netscape.com/NC-rdf#FolderTreeName');
	//Add folderDataSource and accountManagerDataSource to folderPane
	accountManagerDataSource = accountManagerDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	var database = folderTree.database;

	database.AddDataSource(accountManagerDataSource);
    database.AddDataSource(folderDataSource);
	folderTree.setAttribute('ref', 'msgaccounts:/');
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
    
	var folderTree = document.getElementById('folderTree');
    gFolderTree = folderTree;
	return folderTree;
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
	//This will make us lose selection when this happens.
	//need to figure out if we have to save off selection or if
	//tree widget is responsible for this.
	ClearThreadTreeSelection();
	currentFolder.setAttribute('ref', currentFolderID);
}

function ClearThreadTreeSelection()
{
	var tree = GetThreadTree();
	if(tree)
	{
		dump('before clearItemSelection\n');
		tree.clearItemSelection();
	}

}

function ClearMessagePane()
{
	messenger.OpenURL("about:blank");	

}

function StopUrls()
{
	msgWindow.StopUrls();
}


