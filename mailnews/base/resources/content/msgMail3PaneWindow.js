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

/* This is where functions related to the 3 pane window are kept */


var showPerformance;
var msgNavigationService;

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

//If we've loaded a message, set to true.  Helps us keep the start page around.
var gHaveLoadedMessage;

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

    OnItemEvent: function(folder, event) {
        if (event.GetUnicode() == "FolderLoaded") {
            
		if(folder)
		{
			var resource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
			if(resource)
			{
				var uri = resource.Value;
				//dump("In OnFolderLoaded for " + uri +"\n");
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
        } else if (event.GetUnicode() == "DeleteOrMoveMsgCompleted") {
			HandleDeleteOrMoveMsgCompleted(folder);
		}     
    }
}

function HandleDeleteOrMoveMsgCompleted(folder)
{

	if(IsCurrentLoadedFolder(folder))
	{
		msgNavigationService.EnsureDocumentIsLoaded(document);
		if(gNextMessageAfterDelete)
		{
			var nextMessage = document.getElementById(gNextMessageAfterDelete);
			gNextMessageAfterDelete = null;
			SelectNextMessage(nextMessage);
			var threadTree = GetThreadTree();
			if(threadTree)
				threadTree.ensureElementIsVisible(nextMessage);
		}
		//if there's nothing to select then see if the tree has any messages.
		//if not, then clear the message pane.
		else
		{
			var tree = GetThreadTree();
			var topmost = msgNavigationService.FindFirstMessage(tree);
			if(!topmost)
				ClearMessagePane()
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

  CreateMailWindowGlobals();
  Create3PaneGlobals();
  verifyAccounts();
    
  loadStartPage();
	InitMsgWindow();

	messenger.SetWindow(window, msgWindow);

	InitializeDataSources();
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

	gHaveLoadedMessage = false;

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
		}
	}
	
	OnMailWindowUnload();
}


function Create3PaneGlobals()
{
	showPerformance = pref.GetBoolPref('mail.showMessengerPerformance');

	msgNavigationService = Components.classes['component://netscape/messenger/msgviewnavigationservice'].getService();
	msgNavigationService= msgNavigationService.QueryInterface(Components.interfaces.nsIMsgViewNavigationService);

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
			if (treechild.localName == 'treechildren') {
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
								// don't do this for imap servers.
								// see bug #41943
								if (server.type != "imap") {
									//dump("PerformExpand on " + uri + "\n");
									server.PerformExpand(msgWindow);
								}
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
	} catch (ex) {
        dump("Error adding to session\n");
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

function InitializeDataSources()
{
	//Setup common mailwindow stuff.
	AddDataSources();

	//To threadpane move context menu
	SetupMoveCopyMenus('threadPaneContext-moveMenu', accountManagerDataSource, folderDataSource);

	//To threadpane copy content menu
	SetupMoveCopyMenus('threadPaneContext-copyMenu', accountManagerDataSource, folderDataSource);
}

function OnLoadFolderPane(folderTree)
{
    gFolderTree = folderTree;
	SortFolderPane('FolderColumn', 'http://home.netscape.com/NC-rdf#FolderTreeName');

	//Add folderDataSource and accountManagerDataSource to folderPane
	accountManagerDataSource = accountManagerDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	var database = GetFolderDatasource();

	database.AddDataSource(accountManagerDataSource);
    database.AddDataSource(folderDataSource);
	folderTree.setAttribute('ref', 'msgaccounts:/');
}

function GetFolderDatasource()
{
    var folderTree = GetFolderTree();
    var db = folderTree.database;
    if (!db) return false;
    return db;
}

function OnLoadThreadPane(threadTree)
{
    gThreadTree = threadTree;
	//Sort by date by default

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
	if(gHaveLoadedMessage)
	{	
		gCurrentDisplayedMessage = null;
		if (window.frames["messagepane"].location != "about:blank")
			window.frames["messagepane"].location = "about:blank";
		// hide the message header view AND the message pane...
		HideMessageHeaderPane();
	}
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

function GetSelectedMessage()
{
	var tree = GetThreadTree();
	var selection = tree.selectedItems;
	if(selection.length > 0)
		return selection[0];
	else
		return null;

}



function GetServer(uri)
{
    if (!uri) return null;
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
	debug("in FolderPaneClick()\n");

    var targetclass = event.target.getAttribute('class');
    debug('targetclass = ' + targetclass + '\n');

    if (targetclass == 'tree-cell-twisty') {
        // The twisty is nested three below the treeitem:
        // <treeitem>
        //   <treerow>
        //     <treecell>
        //         <titledbutton class="tree-cell-twisty"> <!-- anonymous -->
        var treeitem = event.target.parentNode.parentNode.parentNode;
		var open = treeitem.getAttribute('open');
		if(open == "true") {
			//dump("twisty open\n");

			var item = event.target.parentNode.parentNode.parentNode;
			if (item.localName == "treeitem") {
				var isServer = (treeitem.getAttribute('IsServer') == "true");
				if (isServer) {
	    			var uri = treeitem.getAttribute("id");
					var server = GetServer(uri);
					if (server) {
						server.PerformExpand(msgWindow);
					}
				}
				else {
					var isImap = (treeitem.getAttribute('ServerType') == "imap");
					if (isImap) {
		    			var uri = treeitem.getAttribute("id");
						var folder = GetMsgFolderFromUri(uri);
						if (folder) {
							var imapFolder = folder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
							if (imapFolder) {
								imapFolder.PerformExpand(msgWindow);
							}
						}
					}
				}
			}
		}
    }
	else if(event.detail == 2)
	{
		var item = event.target.parentNode.parentNode;
		if (item.localName == "treeitem")
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
					server.PerformExpand(msgWindow);
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
		MsgOpenNewWindowForFolder(treeitem.getAttribute('id'));
	}
}

function ChangeSelection(tree, newSelection)
{
	if(newSelection)
	{
		tree.clearItemSelection();
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


function GetSelectedMsgFolders()
{
	var folderTree = GetFolderTree();
	var selectedFolders = folderTree.selectedItems;
	var numFolders = selectedFolders.length;

	var folderArray = new Array(numFolders);

	for(var i = 0; i < numFolders; i++)
	{
		var folder = selectedFolders[i];
		var folderUri = folder.getAttribute("id");
		var folderResource = RDF.GetResource(folderUri);
		var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
		if(msgFolder)
		{
			folderArray[i] = msgFolder;	
		}
	}
	return folderArray;
}

function GetSelectedMessages()
{
	var threadTree = GetThreadTree();
	var selectedMessages = threadTree.selectedItems;
	var numMessages = selectedMessages.length;

	var messageArray = new Array(numMessages);

	for(var i = 0; i < numMessages; i++)
	{
		var messageNode = selectedMessages[i];
		var messageUri = messageNode.getAttribute("id");
		var messageResource = RDF.GetResource(messageUri);
		var message = messageResource.QueryInterface(Components.interfaces.nsIMessage);
		if(message)
		{
			messageArray[i] = message;	
		}
	}
	return messageArray;
}

function GetLoadedMsgFolder()
{
	var loadedFolder = GetThreadTreeFolder();
	var folderUri = loadedFolder.getAttribute("ref");
	if(folderUri && folderUri != "" && folderUri !="null")
	{
		var folderResource = RDF.GetResource(folderUri);
		if(folderResource)
		{
			try {
				var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
				return msgFolder;
			}
			catch (ex) {
				dump(ex + "\n");
				dump("we know about this.  see bug #35591\n");
			}
		}
	}
	return null;
}

function GetLoadedMessage()
{
	if(gCurrentDisplayedMessage)
	{
		var messageResource = RDF.GetResource(gCurrentDisplayedMessage);
		if(messageResource)
		{
			var message = messageResource.QueryInterface(Components.interfaces.nsIMessage);
			return message;
		}
	}
	return null;

}

function GetCompositeDataSource(command)
{
	if(command == "GetNewMessages" || command == "DeleteMessages" || command == "Copy"  || 
	   command == "Move" ||  command == "NewFolder" ||  command == "MarkAllMessagesRead")
	{
        return GetFolderDatasource();
	}
	else if(command == "MarkMessageRead" || 
			command == "MarkMessageFlagged" || command == "MarkThreadAsRead" ||
			command == "MessageProperty")
	{
		var threadTree = GetThreadTree();
		return threadTree.database;
	}

	return null;

}

//Sets the next message after a delete.  If useSelection is true then use the
//current selection to determine this.  Otherwise use messagesToCheck which will
//be an array of nsIMessage's.
function SetNextMessageAfterDelete(messagesToCheck, useSelection)
{
	if(useSelection)
	{
		var tree = GetThreadTree();
		var nextMessage = GetNextMessageAfterDelete(tree.selectedItems);
		if(nextMessage)
			gNextMessageAfterDelete = nextMessage.getAttribute('id');
		else
			gNextMessageAfterDelete = null;
	}
}

function SelectFolder(folderUri)
{
	var tree = GetFolderTree();
	var treeitem = document.getElementById(folderUri);
	if(tree && treeitem)
		ChangeSelection(tree, treeitem);
}

function SelectMessage(messageUri)
{
	var tree = GetThreadTree();
	var treeitem = document.getElementById(messageUri);
	if(tree && treeitem)
		ChangeSelection(tree, treeitem);

}

function ReloadMessage()
{
	var msgToLoad = gCurrentDisplayedMessage;
	//null it out so it will work.
	gCurrentDisplayedMessage = null;
	LoadMessageByUri(msgToLoad);
}



