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
	       gNextMessageAfterDelete = null;
				SelectNextMessage(nextMessage);
				var threadTree = GetThreadTree();
				if(threadTree)
					threadTree.ensureElementIsVisible(nextMessage);
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
								server.PerformExpand(msgWindow);
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
	if(gHaveLoadedMessage)
	{	
		gCurrentDisplayedMessage = null;
		if (window.frames["messagepane"].location != "about:blank")
			window.frames["messagepane"].location = "about:blank";
		// hide the message header view AND the message pane...
		HideMessageHeaderPane();
	}
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

function GetSelectedMessage()
{
	var tree = GetThreadTree();
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

    if (targetclass == 'tree-cell-twisty') {
        // The twisty is nested three below the treeitem:
        // <treeitem>
        //   <treerow>
        //     <treecell>
        //         <titledbutton class="tree-cell-twisty"> <!-- anonymous -->
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
		var loadedFolder = GetLoadedMsgFolder();
		var messageArray = GetSelectedMessages();

		ComposeMessage(msgComposeType.Draft, msgComposeFormat.Default, loadedFolder, messageArray);
	}
	else if(IsSpecialFolderSelected("Templates"))
	{
		var loadedFolder = GetLoadedMsgFolder();
		var messageArray = GetSelectedMessages();
		ComposeMessage(msgComposeType.Template, msgComposeFormat.Default, loadedFolder, messageArray);
	}
	else
	{
		var messageUri = treeitem.getAttribute("id");
		MsgOpenNewWindowForMessage(messageUri, null);
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
			if (item.nodeName == "treeitem") {
				var isServer = (treeitem.getAttribute('IsServer') == "true");
				if (isServer) {
	    			var uri = treeitem.getAttribute("id");
					var server = GetServer(uri);
					if (server) {
						server.PerformExpand(msgWindow);
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

// Controller object for folder pane
var FolderPaneController =
{
   supportsCommand: function(command)
	{
		switch ( command )
		{
			case "cmd_delete":
			case "button_delete":
				return true;
			
			case "cmd_selectAll":
			case "cmd_undo":
			case "cmd_redo":
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return true;
				
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
        //		dump("FolderPaneController.IsCommandEnabled(" + command + ")\n");
		switch ( command )
		{
			case "cmd_selectAll":
			case "cmd_undo":
			case "cmd_redo":
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return false;
				
			case "cmd_delete":
			case "button_delete":
				if ( command == "cmd_delete" )
					goSetMenuValue(command, 'valueFolder');
				var folderTree = GetFolderTree();
				if ( folderTree && folderTree.selectedItems &&
                     folderTree.selectedItems.length > 0)
                {
					var specialFolder = null;
					try {
                    	specialFolder = folderTree.selectedItems[0].getAttribute('SpecialFolder');
					}
					catch (ex) {
						//dump("specialFolder failure: " + ex + "\n");
					}
                    if (specialFolder == "Inbox" || specialFolder == "Trash")
                       return false;
                    else
					   return true;
                }
				else
					return false;
			
			default:
				return false;
		}
	},

	doCommand: function(command)
	{
		switch ( command )
		{
			case "cmd_delete":
			case "button_delete":
				MsgDeleteFolder();
				break;
		}
	},
	
	onEvent: function(event)
	{
		// on blur events set the menu item texts back to the normal values
		if ( event == 'blur' )
        {
			goSetMenuValue('cmd_delete', 'valueDefault');
            goSetMenuValue('cmd_undo', 'valueDefault');
            goSetMenuValue('cmd_redo', 'valueDefault');
        }
	}
};


// Controller object for thread pane
var ThreadPaneController =
{
   supportsCommand: function(command)
	{
		switch ( command )
		{
			case "cmd_undo":
			case "cmd_redo":
			case "cmd_selectAll":
				return true;

			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return true;
				
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
		switch ( command )
		{
			case "cmd_selectAll":
				return true;
			
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return false;
				
			case "cmd_undo":
			case "cmd_redo":
               return SetupUndoRedoCommand(command);

			default:
				return false;
		}
	},

	doCommand: function(command)
	{
		switch ( command )
		{
			case "cmd_selectAll":
				var threadTree = GetThreadTree();
				if ( threadTree )
				{
					threadTree.selectAll();
					if ( threadTree.selectedItems && threadTree.selectedItems.length != 1 )
						ClearMessagePane();
				}
				break;
			
			case "cmd_undo":
				messenger.Undo(msgWindow);
				break;
			
			case "cmd_redo":
				messenger.Redo(msgWindow);
				break;
		}
	},
	
	onEvent: function(event)
	{
		// on blur events set the menu item texts back to the normal values
		if ( event == 'blur' )
        {
			goSetMenuValue('cmd_undo', 'valueDefault');
			goSetMenuValue('cmd_redo', 'valueDefault');
		}
	}
};

// DefaultController object (handles commands when one of the trees does not have focus)
var DefaultController =
{
   supportsCommand: function(command)
	{

		switch ( command )
		{
			case "cmd_delete":
			case "button_delete":
			case "cmd_shiftDelete":
			case "cmd_nextUnreadMsg":
			case "cmd_nextUnreadThread":
			case "cmd_sortBySubject":
			case "cmd_sortByDate":
			case "cmd_sortByFlag":
			case "cmd_sortByPriority":
			case "cmd_sortBySender":
			case "cmd_sortBySize":
			case "cmd_sortByStatus":
			case "cmd_sortByUnread":
			case "cmd_sortByOrderReceived":
				return true;
            
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
		switch ( command )
		{
			case "cmd_delete":
			case "button_delete":
			case "cmd_shiftDelete":
				var threadTree = GetThreadTree();
				var numSelected = 0;
				if ( threadTree && threadTree.selectedItems )
					numSelected = threadTree.selectedItems.length;
				if ( command == "cmd_delete")
				{
					if ( numSelected < 2 )
						goSetMenuValue(command, 'valueMessage');
					else
						goSetMenuValue(command, 'valueMessages');
				}
				return ( numSelected > 0 );
			case "cmd_nextUnreadMsg":
			case "cmd_nextUnreadThread":
			    //Input and TextAreas should get access to the keys that cause these commands.
				//Currently if we don't do this then we will steal the key away and you can't type them
				//in these controls. This is a bug that should be fixed and when it is we can get rid of
				//this.
				var focusedElement = top.document.commandDispatcher.focusedElement;
				if(focusedElement)
				{
					var name = focusedElement.nodeName;
					return ((name != "INPUT") && (name != "TEXTAREA"));
				}
				else
				{
					return true;
				}
			case "cmd_sortBySubject":
			case "cmd_sortByDate":
			case "cmd_sortByFlag":
			case "cmd_sortByPriority":
			case "cmd_sortBySender":
			case "cmd_sortBySize":
			case "cmd_sortByStatus":
			case "cmd_sortByUnread":
			case "cmd_sortByOrderReceived":
				return true;
			default:
				return false;
		}
	},

	doCommand: function(command)
	{
   		//dump("ThreadPaneController.doCommand(" + command + ")\n");

		switch ( command )
		{
			case "cmd_delete":
				MsgDeleteMessage(false, false);
				break;
			case "cmd_shiftDelete":
				MsgDeleteMessage(true, false);
				break;
			case "button_delete":
				MsgDeleteMessage(false, true);
				break;
			case "cmd_nextUnreadMsg":
				MsgNextUnreadMessage();
				break;
			case "cmd_nextUnreadThread":
				MsgNextUnreadThread();
				break;
			case "cmd_sortBySubject":
				MsgSortBySubject();
				break;
			case "cmd_sortByDate":
				MsgSortByDate();
				break;
			case "cmd_sortByFlag":
				MsgSortByFlag();
				break;
			case "cmd_sortByPriority":
				MsgSortByPriority();
				break;
			case "cmd_sortBySender":
				MsgSortBySender();
				break;
			case "cmd_sortBySize":
				MsgSortBySize();
				break;
			case "cmd_sortByStatus":
				MsgSortByStatus();
				break;
			case "cmd_sortByUnread":
				MsgSortByUnread();
				break;
			case "cmd_sortByOrderReceived":
				MsgSortByOrderReceived();
				break;
		}
	},
	
	onEvent: function(event)
	{
		// on blur events set the menu item texts back to the normal values
		if ( event == 'blur' )
        {
			goSetMenuValue('cmd_delete', 'valueDefault');
        }
	}
};


function CommandUpdate_Mail()
{
	/*var messagePane = top.document.getElementById('messagePane');
	var drawFocusBorder = messagePane.getAttribute('draw-focus-border');
	
	if ( MessagePaneHasFocus() )
	{
		if ( !drawFocusBorder )
			messagePane.setAttribute('draw-focus-border', 'true');
	}
	else
	{
		if ( drawFocusBorder )
			messagePane.removeAttribute('draw-focus-border');
	}*/
		
	goUpdateCommand('cmd_delete');
	goUpdateCommand('button_delete');
	goUpdateCommand('cmd_shiftDelete');
	goUpdateCommand('cmd_nextUnreadMsg');
	goUpdateCommand('cmd_nextUnreadThread');
	goUpdateCommand('cmd_sortBySubject');
	goUpdateCommand('cmd_sortByDate');
	goUpdateCommand('cmd_sortByFlag');
	goUpdateCommand('cmd_sortByPriority');
	goUpdateCommand('cmd_sortBySender');
	goUpdateCommand('cmd_sortBySize');
	goUpdateCommand('cmd_sortByStatus');
	goUpdateCommand('cmd_sortByUnread');
	goUpdateCommand('cmd_sortByOrderReceived');
}

function SetupUndoRedoCommand(command)
{
    // dump ("--- SetupUndoRedoCommand: " + command + "\n");
    var canUndoOrRedo = false;
    var txnType = 0;

    if (command == "cmd_undo")
    {
        canUndoOrRedo = messenger.CanUndo();
        txnType = messenger.GetUndoTransactionType();
    }
    else
    {
        canUndoOrRedo = messenger.CanRedo();
        txnType = messenger.GetRedoTransactionType();
    }

    if (canUndoOrRedo)
    {
        switch (txnType)
        {
        default:
        case 0:
            goSetMenuValue(command, 'valueDefault');
            break;
        case 1:
            goSetMenuValue(command, 'valueDeleteMsg');
            break;
        case 2:
            goSetMenuValue(command, 'valueMoveMsg');
            break;
        case 3:
            goSetMenuValue(command, 'valueCopyMsg');
            break;
        }
    }
    return canUndoOrRedo;
}


function CommandUpdate_UndoRedo()
{
    ShowMenuItem("menu_undo", true);
    EnableMenuItem("menu_undo", SetupUndoRedoCommand("cmd_undo"));
    ShowMenuItem("menu_redo", true);
    EnableMenuItem("menu_redo", SetupUndoRedoCommand("cmd_redo"));
}

/*function MessagePaneHasFocus()
{
	var focusedWindow = top.document.commandDispatcher.focusedWindow;
	var messagePaneWindow = top.frames['messagepane'];
	
	if ( focusedWindow && messagePaneWindow && (focusedWindow != top) )
	{
		var hasFocus = IsSubWindowOf(focusedWindow, messagePaneWindow, false);
		dump("...........Focus on MessagePane = " + hasFocus + "\n");
		return hasFocus;
	}
	
	return false;
}

function IsSubWindowOf(search, wind, found)
{
	//dump("IsSubWindowOf(" + search + ", " + wind + ", " + found + ")\n");
	if ( found || (search == wind) )
		return true;
	
	for ( index = 0; index < wind.frames.length; index++ )
	{
		if ( IsSubWindowOf(search, wind.frames[index], false) )
			return true;
	}
	return false;
}*/


function SetupCommandUpdateHandlers()
{
	dump("SetupCommandUpdateHandlers\n");

	var widget;
	
	// folder pane
	widget = GetFolderTree();
	if ( widget )
		widget.controllers.appendController(FolderPaneController);
	
	// thread pane
	widget = GetThreadTree();
	if ( widget )
		widget.controllers.appendController(ThreadPaneController);
		
	top.controllers.insertControllerAt(0, DefaultController);
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
	var messageResource = RDF.GetResource(gCurrentDisplayedMessage);
	if(messageResource)
	{
		var message = messageResource.QueryInterface(Components.interfaces.nsIMessage);
		return message;
	}
	return null;

}

function GetCompositeDataSource(command)
{
	if(command == "GetNewMessages" || command == "Copy"  || command == "Move" || 
	   command == "NewFolder" ||  command == "MarkAllMessagesRead")
	{
		var folderTree = GetFolderTree();
		return folderTree.database;
	}
	else if(command == "DeleteMessages" || command == "MarkMessageRead" || 
			command == "MarkMessageFlagged" || command == "MarkThreadAsRead")
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


//3pane related commands.  Need to go in own file.  Putting here for the moment.
function MsgSortByDate()
{
	SortThreadPane('DateColumn', 'http://home.netscape.com/NC-rdf#Date', null, true, null);
}

function MsgSortBySender()
{
	SortThreadPane('AuthorColumn', 'http://home.netscape.com/NC-rdf#Sender', 'http://home.netscape.com/NC-rdf#Date', true, null);
}

function MsgSortByRecipient()
{
	SortThreadPane('AuthorColumn', 'http://home.netscape.com/NC-rdf#Recipient', 'http://home.netscape.com/NC-rdf#Date', true, null);
}

function MsgSortByStatus()
{
	SortThreadPane('StatusColumn', 'http://home.netscape.com/NC-rdf#Status', 'http://home.netscape.com/NC-rdf#Date', true, null);
}

function MsgSortBySubject()
{
	SortThreadPane('SubjectColumn', 'http://home.netscape.com/NC-rdf#Subject', 'http://home.netscape.com/NC-rdf#Date', true, null);
}

function MsgSortByFlagged() 
{
	SortThreadPane('FlaggedButtonColumn', 'http://home.netscape.com/NC-rdf#Flagged', 'http://home.netscape.com/NC-rdf#Date', true, null);
}
function MsgSortByPriority()
{
	SortThreadPane('PriorityColumn', 'http://home.netscape.com/NC-rdf#Priority', 'http://home.netscape.com/NC-rdf#Date',true, null);
}
function MsgSortBySize() 
{
	SortThreadPane('SizeColumn', 'http://home.netscape.com/NC-rdf#Size', 'http://home.netscape.com/NC-rdf#Date', true, null);
}
function MsgSortByUnread()
{
	SortThreadPane('UnreadColumn', 'http://home.netscape.com/NC-rdf#TotalUnreadMessages','http://home.netscape.com/NC-rdf#Date', true, null);
}
function MsgSortByOrderReceived()
{
	SortThreadPane('OrderReceivedColumn', 'http://home.netscape.com/NC-rdf#OrderReceived','http://home.netscape.com/NC-rdf#Date', true, null);
}

