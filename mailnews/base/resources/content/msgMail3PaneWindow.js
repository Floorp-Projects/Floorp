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

var messenger;
var accountManagerDataSource;
var folderDataSource;
var messageDataSource;
var pref;
var statusFeedback;
var messageView;
var msgWindow;

var msgComposeService;
var mailSession;
var accountManager;
var RDF;
var showPerformance;
var msgNavigationService;

var msgComposeType;
var msgComposeFormat;
var Bundle;
var BrandBundle;

var gFolderTree;
var gThreadTree;
var gThreadAndMessagePaneSplitter = null;
var gUnreadCount = null;
var gTotalCount = null;

var gCurrentLoadingFolderURI;
var gCurrentFolderToReroot;
var gCurrentLoadingFolderIsThreaded = false;
var gCurrentLoadingFolderSortID ="";
var gCurrentLoadingFolderSortDirection = null;

var gCurrentDisplayedMessage = null;
var gNextMessageAfterDelete = null;

var gActiveThreadPaneSortColumn = "";

var gStartFolderUri = null;

// the folderListener object
var folderListener = {
    OnItemAdded: function(parentItem, item, view) {},

	OnItemRemoved: function(parentItem, item, view){},

	OnItemPropertyChanged: function(item, property, oldValue, newValue) {},

	OnItemIntPropertyChanged: function(item, property, oldValue, newValue)
	{
		var currentLoadedFolder = GetThreadTreeFolder();
		var currentURI = currentLoadedFolder.getAttribute('ref');

		//if we don't have a folder loaded, don't bother.
		if(currentURI)
		{
			if(property.GetUnicode() == "TotalMessages" || property.GetUnicode() == "TotalUnreadMessages")
			{
				folder = item.QueryInterface(Components.interfaces.nsIMsgFolder);
				if(folder)
				{
					var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
					if(folderResource)
					{
						var folderURI = folderResource.Value;
						if(currentURI == folderURI)
						{
							UpdateStatusMessageCounts(folder);
						}
					}


				}



			}
		}
	},

	OnItemBoolPropertyChanged: function(item, property, oldValue, newValue) {},

    OnItemUnicharPropertyChanged: function(item, property, oldValue, newValue){},
	OnItemPropertyFlagChanged: function(item, property, oldFlag, newFlag) {},

    OnFolderEvent: function(item, event) {},

	OnFolderLoaded: function (folder)
	{
		if(folder)
		{
			var resource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
			if(resource)
			{
				var uri = resource.Value;
				dump("In OnFolderLoaded for " + uri +"\n");
				if(uri == gCurrentFolderToReroot)
				{
					gCurrentFolderToReroot="";
					var msgFolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
					if(msgFolder)
					{
						msgFolder.endFolderLoading();
						RerootFolder(uri, msgFolder, gCurrentLoadingFolderIsThreaded, gCurrentLoadingFolderSortID, gCurrentLoadingFolderSortDirection);
						gCurrentLoadingFolderIsThreaded = false;
						gCurrentLoadingFolderSortID = "";
						gCurrentLoadingFolderSortDirection = null;
					}
				}
				if(uri == gCurrentLoadingFolderURI)
				{
				  gCurrentLoadingFolderURI = "";
				  //Now let's select the first new message if there is one
				  var beforeScrollToNew = new Date();
				  msgNavigationService.EnsureDocumentIsLoaded(document);

				  ScrollToFirstNewMessage();
				  var afterScrollToNew = new Date();
				  var timeToScroll = (afterScrollToNew.getTime() - beforeScrollToNew.getTime())/1000;


				  var afterFolderLoadTime = new Date();
				  var timeToLoad = (afterFolderLoadTime.getTime() - gBeforeFolderLoadTime.getTime())/1000;
				  if(showPerformance)
				  {
					  dump("Time to load " + uri + " is " +  timeToLoad + " seconds\n");
				  	  dump("of which scrolling to new is" + timeToScroll + "seconds\n");
				  }
				}
			}

		}
	},
	OnDeleteOrMoveMessagesCompleted :function(folder)
	{
		if(IsCurrentLoadedFolder(folder))
		{
			msgNavigationService.EnsureDocumentIsLoaded(document);

			if(gNextMessageAfterDelete)
			{
				var nextMessage = document.getElementById(gNextMessageAfterDelete);
				SelectNextMessage(nextMessage);
				var threadTree = GetThreadTree();
				if(threadTree)
					threadTree.ensureElementIsVisible(nextMessage);
				gNextMessageAfterDelete = null;
			}
		}
	}
}

function IsCurrentLoadedFolder(folder)
{
	msgfolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
	if(msgfolder)
	{
		var folderResource = msgfolder.QueryInterface(Components.interfaces.nsIRDFResource);
		if(folderResource)
		{
			var folderURI = folderResource.Value;
			var currentLoadedFolder = GetThreadTreeFolder();
			var currentURI = currentLoadedFolder.getAttribute('ref');
			return(currentURI == folderURI);
		}
	}

	return false;
}

/* Functions related to startup */
function OnLoadMessenger()
{
  var beforeLoadMessenger = new Date();

  CreateGlobals();
  verifyAccounts();
    
  loadStartPage();
	InitMsgWindow();

	messenger.SetWindow(window, msgWindow);

	AddDataSources();
	InitPanes();


  AddToSession();
  //need to add to session before trying to load start folder otherwise listeners aren't
  //set up correctly.
  if(window.arguments && window.arguments[0])
  {
	gStartFolderUri = window.arguments[0];
  }
  else
  {
    gStartFolderUri = null;
  }

  setTimeout("loadStartFolder(gStartFolderUri);", 0);

  // FIX ME - later we will be able to use onload from the overlay
  OnLoadMsgHeaderPane();

	var id = null;
	var headerchoice = null;

	try {
		headerchoice = pref.GetIntPref("mail.show_headers");
	}
	catch (ex) {
		dump("failed to get the header pref\n");
	}

	switch (headerchoice) {
		case 2:	
			id = "viewallheaders";
			break;
		case 0:
			id = "viewbriefheaders";
			break;
		case 1:	
			id = "viewnormalheaders";
			break;
		default:
			id = "viewnormalheaders";
			break;
	}

	var menuitem = document.getElementById(id);

	try {
		// not working right yet.  see bug #??????
		// menuitem.setAttribute("checked", "true"); 
	}
	catch (ex) {
		dump("failed to set the view headers menu item\n");
	}

	var afterLoadMessenger = new Date();

	var timeToLoad = (afterLoadMessenger.getTime() - beforeLoadMessenger.getTime())/1000;
	if(showPerformance)
	{
	  dump("Time in OnLoadMessger is " +  timeToLoad + " seconds\n");
	}

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
			messenger.SetWindow(null, null);
		}
	}
	
	var msgDS = folderDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = null;

	msgDS = messageDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = null;

	msgDS = accountManagerDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = null;


  	msgWindow.closeWindow();
}


function CreateGlobals()
{
	// get the messenger instance
	messenger = Components.classes[messengerProgID].createInstance();
	messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);

	//Create datasources
	accountManagerDataSource = Components.classes[accountManagerDSProgID].createInstance();
	folderDataSource         = Components.classes[folderDSProgID].createInstance();
	messageDataSource        = Components.classes[messageDSProgID].createInstance();

	pref = Components.classes[prefProgID].getService(Components.interfaces.nsIPref);

	//Create windows status feedback
	statusFeedback           = Components.classes[statusFeedbackProgID].createInstance();
	statusFeedback = statusFeedback.QueryInterface(Components.interfaces.nsIMsgStatusFeedback);

	//Create message view object
	messageView = Components.classes[messageViewProgID].createInstance();
	messageView = messageView.QueryInterface(Components.interfaces.nsIMessageView);

	//Create message window object
	msgWindow = Components.classes[msgWindowProgID].createInstance();
	msgWindow = msgWindow.QueryInterface(Components.interfaces.nsIMsgWindow);

	msgComposeService = Components.classes['component://netscape/messengercompose'].getService();
	msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);

	mailSession = Components.classes["component://netscape/messenger/services/session"].getService(Components.interfaces.nsIMsgMailSession); 

	accountManager = Components.classes["component://netscape/messenger/account-manager"].getService(Components.interfaces.nsIMsgAccountManager);

	RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
	RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

	showPerformance = pref.GetBoolPref('mail.showMessengerPerformance');

	msgNavigationService = Components.classes['component://netscape/messenger/msgviewnavigationservice'].getService();
	msgNavigationService= msgNavigationService.QueryInterface(Components.interfaces.nsIMsgViewNavigationService);

	msgComposeType = Components.interfaces.nsIMsgCompType;
	msgComposeFormat = Components.interfaces.nsIMsgCompFormat;
	Bundle = srGetStrBundle("chrome://messenger/locale/messenger.properties");
    BrandBundle = srGetStrBundle("chrome://global/locale/brand.properties");

}


function loadStartPage() {
    try {
		startpageenabled= pref.GetBoolPref("mailnews.start_page.enabled");
        
		if (startpageenabled) {
			startpage = pref.CopyCharPref("mailnews.start_page.url");
            if (startpage != "") {
                window.frames["messagepane"].location = startpage;
                dump("start message pane with: " + startpage + "\n");
            }
        }
	}
    catch (ex) {
        dump("Error loading start page.\n");
        return;
    }
}

function PerformExpandForAllOpenServers(tree)
{
	//dump("PerformExpandForAllOpenServers()\n");

	var uri = null;
	var open = null;
	var treechild = null;
    var server = null;

	if ( tree && tree.childNodes ) {
		for ( var i = tree.childNodes.length - 1; i >= 0; i-- ) {
			treechild = tree.childNodes[i];
			if (treechild.tagName == 'treechildren') {
				var treeitems = treechild.childNodes;
				for ( var j = treeitems.length - 1; j >= 0; j--) {
					open = treeitems[j].getAttribute('open');
					//dump("open="+open+"\n");
					if (open == "true") {
						var isServer = (treeitems[j].getAttribute('IsServer') == "true");
						//dump("isServer="+isServer+"\n");
						if (isServer) {
							uri = treeitems[j].getAttribute('id');
							//dump("uri="+uri+"\n");
							server = GetServer(uri);
							if (server) {
								//dump("PerformExpand on " + uri + "\n");
								server.PerformExpand();
							}
						}
					}
				}
			}
		}
	}
}

function loadStartFolder(startFolderUri)
{
	//First get default account
	try
	{
		if(!startFolderUri)
		{
			var defaultAccount = accountManager.defaultAccount;

			var server = defaultAccount.incomingServer;
			var rootFolder = server.RootFolder;
			var rootMsgFolder = rootFolder.QueryInterface(Components.interfaces.nsIMsgFolder);

			//now find Inbox
			var outNumFolders = new Object();
			var inboxFolder = rootMsgFolder.getFoldersWithFlag(0x1000, 1, outNumFolders); 
			if(!inboxFolder) return;

			var resource = inboxFolder.QueryInterface(Components.interfaces.nsIRDFResource);
			var startFolderUri = resource.Value;

			//first, let's see if it's already in the dom.  This will make life easier.
			//We need to make sure content is built by this time
		}
		msgNavigationService.EnsureDocumentIsLoaded(document);

		var startFolder = document.getElementById(startFolderUri);

		//if it's not here we will have to make sure it's open.
		if(!startFolder)
		{

		}

		var folderTree= GetFolderTree();
		ChangeSelection(folderTree, startFolder);

		// because the "open" state persists, we'll call
		// PerformExpand() for all servers that are open at startup.
		PerformExpandForAllOpenServers(folderTree);
	}
	catch(ex)
	{
		dump(ex);
		dump('Exception in LoadStartFolder caused by no default account.  We know about this\n');
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
	msgWindow.msgHeaderSink = messageHeaderSink;
  msgWindow.SetDOMWindow(window);
}

function AddDataSources()
{

	accountManagerDataSource = accountManagerDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	//to move menu item
	SetupMoveCopyMenus('moveMenu', accountManagerDataSource, folderDataSource);

	//to copy menu item
	SetupMoveCopyMenus('copyMenu', accountManagerDataSource, folderDataSource);

	//To threadpane move context menu
	SetupMoveCopyMenus('threadPaneContext-moveMenu', accountManagerDataSource, folderDataSource);

	//To threadpane copy content menu
	SetupMoveCopyMenus('threadPaneContext-copyMenu', accountManagerDataSource, folderDataSource);

	//To FileButton menu
	SetupMoveCopyMenus('FileButtonMenu', accountManagerDataSource, folderDataSource);
	//Add statusFeedback

	var msgDS = folderDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = msgWindow;

	msgDS = messageDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = msgWindow;

	msgDS = accountManagerDataSource.QueryInterface(Components.interfaces.nsIMsgRDFDataSource);
	msgDS.window = msgWindow;

}	

function SetupMoveCopyMenus(menuid, accountManagerDataSource, folderDataSource)
{
	var menu = document.getElementById(menuid);
	if(menu)
	{
		menu.database.AddDataSource(accountManagerDataSource);
		menu.database.AddDataSource(folderDataSource);
		menu.setAttribute('ref', 'msgaccounts:/');
	}
}

function InitPanes()
{
	var threadTree = GetThreadTree();
	if(threadTree);
		OnLoadThreadPane(threadTree);

	var folderTree = GetFolderTree();
	if(folderTree)
		OnLoadFolderPane(folderTree);
		
	SetupCommandUpdateHandlers();
}

function OnLoadFolderPane(folderTree)
{
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
	//Sort by date by default
	// add folderSource to thread pane
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	threadTree.database.AddDataSource(folderDataSource);

	//Add message data source
	messageDataSource = messageDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	threadTree.database.AddDataSource(messageDataSource);

    //FIX ME: Tempory patch for bug 24182
    //ShowThreads(false);
	setTimeout("ShowThreads(false);", 0);
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
    gThreadTree = threadTree;
	return threadTree;
}

function GetThreadTreeFolder()
{
  var tree = GetThreadTree();
  return tree;
}

function GetThreadAndMessagePaneSplitter()
{
	if(gThreadAndMessagePaneSplitter) return gThreadAndMessagePaneSplitter;
	var splitter = document.getElementById('gray_horizontal_splitter');
	gThreadAndMessagePaneSplitter = splitter;
	return splitter;
}

function GetUnreadCountElement()
{
	if(gUnreadCount) return gUnreadCount;
	var unreadCount = document.getElementById('unreadMessageCount');
	gUnreadCount = unreadCount;
	return unreadCount;
}
function GetTotalCountElement()
{
	if(gTotalCount) return gTotalCount;
	var totalCount = document.getElementById('totalMessageCount');
	gTotalCount = totalCount;
	return totalCount;
}
function IsThreadAndMessagePaneSplitterCollapsed()
{
	var splitter = GetThreadAndMessagePaneSplitter();
	if(splitter)
	{
		var state  = splitter.getAttribute('state');
		return (state == "collapsed");
	}
	else
		return false;
}

function FindMessenger()
{
  return messenger;
}

function RefreshThreadTreeView()
{
	var selection = SaveThreadPaneSelection();

	var currentFolder = GetThreadTreeFolder();  
	var currentFolderID = currentFolder.getAttribute('ref');
	ClearThreadTreeSelection();
	currentFolder.setAttribute('ref', currentFolderID);

	RestoreThreadPaneSelection(selection);
}

function ClearThreadTreeSelection()
{
	var tree = GetThreadTree();
	if(tree)
	{
		tree.clearItemSelection();
	}

}

function ClearMessagePane()
{
	gCurrentDisplayedMessage = null;
    if (window.frames["messagepane"].location != "about:blank")
        window.frames["messagepane"].location = "about:blank";
    // hide the message header view AND the message pane...
    HideMessageHeaderPane();
}

function StopUrls()
{
	msgWindow.StopUrls();
}

function GetSelectedFolder()
{
	var tree = GetFolderTree();
	var selection = tree.selectedItems;
	if(selection.length > 0)
		return selection[0];
	else
		return null;

}

function ThreadPaneOnClick(event)
{
    var targetclass = event.target.getAttribute('class');
    debug('targetclass = ' + targetclass + '\n');

    if (targetclass == 'twisty') {
        // The twisty is nested three below the treeitem:
        // <treeitem>
        //   <treerow>
        //     <treecell>
        //         <titledbutton class="twisty"> <!-- anonymous -->
        var treeitem = event.target.parentNode.parentNode.parentNode;
		var open = treeitem.getAttribute('open');
		if(open == "true")
		{
			//open all of the children of the treeitem
			msgNavigationService.OpenTreeitemAndDescendants(treeitem);
		}
    }
	else if(event.clickCount == 2)
	{
		ThreadPaneDoubleClick(event.target.parentNode.parentNode);
	}
}

function ThreadPaneDoubleClick(treeitem)
{
	if(IsSpecialFolderSelected("Drafts"))
	{
		ComposeMessage(msgComposeType.Draft, msgComposeFormat.Default);
	}
	else if(IsSpecialFolderSelected("Templates"))
	{
		ComposeMessage(msgComposeType.Template, msgComposeFormat.Default);
	}
}

function GetServer(uri)
{
	try {
		var folder = GetMsgFolderFromUri(uri);
		return folder.server;
	}
	catch (ex) {
		dump("GetServer("+uri+") failed, ex="+ex+"\n");
	}

	return null;
}

function FolderPaneOnClick(event)
{
    var targetclass = event.target.getAttribute('class');
    debug('targetclass = ' + targetclass + '\n');

    if (targetclass == 'twisty') {
        // The twisty is nested three below the treeitem:
        // <treeitem>
        //   <treerow>
        //     <treecell>
        //         <titledbutton class="twisty"> <!-- anonymous -->
        var treeitem = event.target.parentNode.parentNode.parentNode;
		var open = treeitem.getAttribute('open');
		if(open == "true") {
			//dump("twisty open\n");

			var item = event.target.parentNode.parentNode.parentNode;
			if (item.nodeName == "treeitem") {
				var isServer = (treeitem.getAttribute('IsServer') == "true");
				if (isServer) {
	    			var uri = treeitem.getAttribute("id");
					var server = GetServer(uri);
					if (server) {
						server.PerformExpand();
					}
				}
			}
		}
    }
	else if(event.clickCount == 2)
	{
		var item = event.target.parentNode.parentNode;
		if (item.nodeName == "treeitem")
			FolderPaneDoubleClick(item);
	}
}

function FolderPaneDoubleClick(treeitem)
{
	var isServer = false;

	if (treeitem) {
		isServer = (treeitem.getAttribute('IsServer') == "true");
		if (isServer) {
			var open = treeitem.getAttribute('open');
			if (open == "true") {
				var uri = treeitem.getAttribute("id");
				server = GetServer(uri);
				if (server) {
					//dump("double clicking open, PerformExpand()\n");
					server.PerformExpand();
				}
			}
			else {
				//dump("double clicking close, don't PerformExpand()\n");
			}
		}
	}

	// don't open a new msg window if we are double clicking on a server.
	// only do it for folders or newsgroups
	if (!isServer) {
		MsgOpenNewWindowForFolder(treeitem);
	}
}

function ChangeSelection(tree, newSelection)
{
	if(newSelection)
	{
		tree.clearItemSelection();
		tree.clearCellSelection();
		tree.selectItem(newSelection);
		tree.ensureElementIsVisible(newSelection);
	}
}

function SetActiveThreadPaneSortColumn(column)
{
	gActiveThreadPaneSortColumn = column;
}

function GetActiveThreadPaneSortColumn()
{
	return gActiveThreadPaneSortColumn;
}

function ClearActiveThreadPaneSortColumn()
{
	var activeColumn = document.getElementById(gActiveThreadPaneSortColumn);
	if(activeColumn)
	{
		activeColumn.removeAttribute("sortActive");
		activeColumn = "";
	}

}
