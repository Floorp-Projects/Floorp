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


var showPerformance = false;

var gFolderTree; 
var gThreadOutliner;
var gMessagePane;
var gMessagePaneFrame;

var gThreadAndMessagePaneSplitter = null;
var gUnreadCount = null;
var gTotalCount = null;

var gCurrentLoadingFolderURI;
var gCurrentFolderToReroot;
var gCurrentLoadingFolderSortType = 0;
var gCurrentLoadingFolderSortOrder = 0;
var gCurrentLoadingFolderViewType = 0;
var gCurrentLoadingFolderViewFlags = 0;

var gCurrentDisplayedMessage = null;
var gNextMessageAfterDelete = null;
var gNextMessageAfterLoad = false;
var gNextMessageViewIndexAfterDelete = -1;

var gActiveThreadPaneSortColumn = "";

var gStartFolderUri = null;

//If we've loaded a message, set to true.  Helps us keep the start page around.
var gHaveLoadedMessage;

var gDisplayStartupPage = false;

// the folderListener object
var folderListener = {
    OnItemAdded: function(parentItem, item, view) {},

	OnItemRemoved: function(parentItem, item, view){},

	OnItemPropertyChanged: function(item, property, oldValue, newValue) {},

	OnItemIntPropertyChanged: function(item, property, oldValue, newValue)
	{
		var currentLoadedFolder = GetThreadPaneFolder();
        if (!currentLoadedFolder) return;

		var currentURI = currentLoadedFolder.URI;

		//if we don't have a folder loaded, don't bother.
		if(currentURI)
		{
			if(property.GetUnicode() == "TotalMessages" || property.GetUnicode() == "TotalUnreadMessages")
			{
				var folder = item.QueryInterface(Components.interfaces.nsIMsgFolder);
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
        var eventType = event.GetUnicode();

        if (eventType == "FolderLoaded") {
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
						RerootFolder(uri, msgFolder, gCurrentLoadingFolderViewType, gCurrentLoadingFolderViewFlags, gCurrentLoadingFolderSortType, gCurrentLoadingFolderSortOrder);
						gIsEditableMsgFolder = IsSpecialFolder(msgFolder, MSG_FOLDER_FLAG_DRAFTS);

						gCurrentLoadingFolderSortType = 0;
						gCurrentLoadingFolderSortOrder = 0;
            gCurrentLoadingFolderViewType = 0;
            gCurrentLoadingFolderViewFlags = 0;

            SetFocusThreadPane();
            if (gNextMessageAfterLoad) 
            {
              gNextMessageAfterLoad = false;

              // should scroll to new here.
              PositionThreadPane();
            }
					}
				}
				if(uri == gCurrentLoadingFolderURI)
				{
				  gCurrentLoadingFolderURI = "";
				  //Now let's select the first new message if there is one
          var beforeScrollToNew;
				  if(showPerformance) {
				    beforeScrollToNew = new Date();
                  }
				  ScrollToFirstNewMessage();

				  if(showPerformance) {
				      var afterScrollToNew = new Date();
				      var timeToScroll = (afterScrollToNew.getTime() - beforeScrollToNew.getTime())/1000;

				      var afterFolderLoadTime = new Date();
				      var timeToLoad = (afterFolderLoadTime.getTime() - gBeforeFolderLoadTime.getTime())/1000;
					  dump("Time to load " + uri + " is " +  timeToLoad + " seconds\n");
				  	  dump("of which scrolling to new is " + timeToScroll + " seconds\n");
				  }
    				SetBusyCursor(window, false);
				}
			}

		}
        } else if (eventType == "DeleteOrMoveMsgCompleted") {
			HandleDeleteOrMoveMsgCompleted(folder);
        }     
          else if (eventType == "DeleteOrMoveMsgFailed") {
                        HandleDeleteOrMoveMsgFailed(folder);
        }

    }
}

function HandleDeleteOrMoveMsgFailed(folder)
{
  if(IsCurrentLoadedFolder(folder)) {
    if(gNextMessageAfterDelete) {
      gNextMessageAfterDelete = null;
      gNextMessageViewIndexAfterDelete = -1;
    }
  }

  // fix me???
  // ThreadPaneSelectionChange(true);
}

function HandleDeleteOrMoveMsgCompleted(folder)
{
  if (IsCurrentLoadedFolder(folder)) {
    if (gNextMessageViewIndexAfterDelete != -1) {
      var outlinerView = gDBView.QueryInterface(Components.interfaces.nsIOutlinerView);
      var outlinerSelection = outlinerView.selection;
      viewSize = outlinerView.rowCount;
      if (gNextMessageViewIndexAfterDelete >= viewSize) {
        if (viewSize > 0)
          gNextMessageViewIndexAfterDelete = viewSize - 1;
        else
          gNextMessageViewIndexAfterDelete = -1;
      }

      // if we are about to set the selection with a new element then DON'T clear
      // the selection then add the next message to select. This just generates
      // an extra round of command updating notifications that we are trying to
      // optimize away.
      if (gNextMessageViewIndexAfterDelete != -1) {
        outlinerSelection.select(gNextMessageViewIndexAfterDelete);
        // since gNextMessageViewIndexAfterDelete probably has the same value
        // as the last index we had selected, the outliner isn't generating a new
        // selectionChanged notification for the outliner view. So we aren't loading the 
        // next message. to fix this, force the selection changed update.
        if (outlinerView)
          outlinerView.selectionChanged();
        EnsureRowInThreadOutlinerIsVisible(gNextMessageViewIndexAfterDelete); 
      }
      else {
        outlinerSelection.clearSelection(); /* clear selection in either case  */
        ClearMessagePane();
      }

      gNextMessageViewIndexAfterDelete = -1;
    }
  }
}

function IsCurrentLoadedFolder(folder)
{
	var msgfolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
	if(msgfolder)
	{
		var folderResource = msgfolder.QueryInterface(Components.interfaces.nsIRDFResource);
		if(folderResource)
		{
			var folderURI = folderResource.Value;
			var currentLoadedFolder = GetThreadPaneFolder();
			var currentURI = currentLoadedFolder.URI;
			return(currentURI == folderURI);
		}
	}

	return false;
}

/* Functions related to startup */
function OnLoadMessenger()
{
  showPerformance = pref.GetBoolPref('mail.showMessengerPerformance');
  var beforeLoadMessenger;
  if(showPerformance) {
      beforeLoadMessenger = new Date();
  }

  CreateMailWindowGlobals();
  Create3PaneGlobals();
  verifyAccounts();
    
  HideAccountCentral();
  loadStartPage();
	InitMsgWindow();

	messenger.SetWindow(window, msgWindow);

	InitializeDataSources();
	InitPanes();

	accountManager.SetSpecialFoldersForIdentities();

  AddToSession();
  //need to add to session before trying to load start folder otherwise listeners aren't
  //set up correctly.
  if ("arguments" in window && window.arguments[0])
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

	//Set focus to the Thread Pane the first time the window is opened.
	SetFocusThreadPane();

	if(showPerformance) {
	  var afterLoadMessenger = new Date();
	  var timeToLoad = (afterLoadMessenger.getTime() - beforeLoadMessenger.getTime())/1000;
	  dump("Time in OnLoadMessger is " +  timeToLoad + " seconds\n");
	}
	
}

function OnUnloadMessenger()
{
	dump("\nOnUnload from XUL\nClean up ...\n");
	
	OnMailWindowUnload();
}


function Create3PaneGlobals()
{
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

function loadStartFolder(initialUri)
{
    var defaultServer = null;
    var startFolderUri = initialUri;
    var isLoginAtStartUpEnabled = false;
    var enabledNewMailCheckOnce = false;
    var mailCheckOncePref = "mail.startup.enabledMailCheckOnce";

    //First get default account
    try
    {
        if(!startFolderUri)
        {
            var defaultAccount = accountManager.defaultAccount;

            defaultServer = defaultAccount.incomingServer;
            var rootFolder = defaultServer.RootFolder;
            var rootMsgFolder = rootFolder.QueryInterface(Components.interfaces.nsIMsgFolder);

            var folderResource = rootMsgFolder.QueryInterface(Components.interfaces.nsIRDFResource);
            startFolderUri = folderResource.Value;

            enabledNewMailCheckOnce = pref.GetBoolPref(mailCheckOncePref);

            // Enable checknew mail once by turning checkmail pref 'on' to bring 
            // all users to one plane. This allows all users to go to Inbox. User can 
            // always go to server settings panel and turn off "Check for new mail at startup"
            if (!enabledNewMailCheckOnce)
            {
                pref.SetBoolPref(mailCheckOncePref, true);
                defaultServer.loginAtStartUp = true;
            }

            // Get the user pref to see if the login at startup is enabled for default account
            isLoginAtStartUpEnabled = defaultServer.loginAtStartUp;

            // Get Inbox only if when we have to login 
            if (isLoginAtStartUpEnabled) 
            {
                //now find Inbox
                var outNumFolders = new Object();
                var inboxFolder = rootMsgFolder.getFoldersWithFlag(0x1000, 1, outNumFolders); 
                if(!inboxFolder) return;

                var resource = inboxFolder.QueryInterface(Components.interfaces.nsIRDFResource);
                startFolderUri = resource.Value;
            }
        }

        var startFolder = document.getElementById(startFolderUri);

        //if it's not here we will have to make sure it's open.
        if(!startFolder && startFolderUri && isLoginAtStartUpEnabled)
        {
            // Opens the twisty for the default account 
            OpenTwistyForServer(defaultServer);
            startFolder = document.getElementById(startFolderUri);
        }

        var folderTree= GetFolderTree();
        ChangeSelection(folderTree, startFolder);
                
        // only do this on startup, when we pass in null
        if (!initialUri && isLoginAtStartUpEnabled)
        {
            // Perform biff on the server to check for new mail, except for imap
            if (defaultServer.type != "imap") 
              defaultServer.PerformBiff();
        } 

        // because the "open" state persists, we'll call
        // PerformExpand() for all servers that are open at startup.
        PerformExpandForAllOpenServers(folderTree);
    }
    catch(ex)
    {
        dump(ex);
        dump('Exception in LoadStartFolder caused by no default account.  We know about this\n');
    }

    if (!initialUri) 
    {
        MsgGetMessagesForAllServers(defaultServer);
    }

}

function OpenTwistyForServer(server)
{
    var treeNode = GetTreeNodeForServerURI(server.serverURI); 

    if (treeNode)
        treeNode.setAttribute('open', 'true');
}


function GetTreeNodeForServerURI(serverURI)
{
    var treeNode = null;

    var tree = GetFolderTree();     

    // Iterate through folder tree to find the node associated with given serverURI
    if ( tree && tree.childNodes ) {
        for ( var i = tree.childNodes.length - 1; i >= 0; i-- ) {
            var treechild = tree.childNodes[i];
            if (treechild.localName == 'treechildren') {
                var treeitems = treechild.childNodes;
                for ( var j = treeitems.length - 1; j >= 0; j--) {
                    var isServer = treeitems[j].getAttribute('IsServer');
                    if (isServer == "true") {
                        var uri = treeitems[j].getAttribute('id');
                        if (uri == serverURI) {
                            treeNode = treeitems[j];
                            break; 
                        }
                    }
                }
            } 
        }
    }
    return treeNode;
}

function TriggerGetMessages(server)
{
    // downloadMessagesAtStartup for a given server type indicates whether 
    // or not there is a need to Trigger GetMessages action
    if (server.downloadMessagesAtStartup)
        MsgGetMessage();
}

function AddToSession()
{
    try {
        var mailSession = Components.classes[mailSessionContractID].getService(Components.interfaces.nsIMsgMailSession);
        
        mailSession.AddFolderListener(folderListener);
	} catch (ex) {
        dump("Error adding to session\n");
    }
}


function InitPanes()
{
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

function GetMessagePane()
{
    if (gMessagePane) return gMessagePane;
    gMessagePane = document.getElementById("messagepanebox");
    return gMessagePane;
}

function GetMessagePaneFrame()
{
    if (gMessagePaneFrame) return gMessagePaneFrame;
    gMessagePaneFrame = top.frames['messagepane'];
    return gMessagePaneFrame;
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

	return null;
}

function GetThreadAndMessagePaneSplitter()
{
	if(gThreadAndMessagePaneSplitter) return gThreadAndMessagePaneSplitter;
	var splitter = document.getElementById('threadpane-splitter');
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

function ClearThreadPaneSelection()
{
  try {
    if (gDBView) {
      var outlinerView = gDBView.QueryInterface(Components.interfaces.nsIOutlinerView);
      var outlinerSelection = outlinerView.selection;
      outlinerSelection.clearSelection(); 
    }
  }
  catch (ex) {
    dump("ClearThreadPaneSelection: ex = " + ex + "\n");
  }
}

function ClearMessagePane()
{
	if(gHaveLoadedMessage)
	{	
    gHaveLoadedMessage = false;
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

function FolderPaneOnClick(event)
{
	debug("in FolderPaneClick()\n");

  var t = event.originalTarget;
  var item;
  var uri;

    if (t.getAttribute('twisty') == 'true') {
        // The twisty is nested three below the treeitem:
        // <treeitem>
        //   <treerow>
        //     <treecell>
        //         <button class="tree-cell-twisty"> <!-- anonymous -->
        var treeitem = t.parentNode.parentNode.parentNode;
		var open = treeitem.getAttribute('open');
		if(open == "true") {
			//dump("twisty open\n");

			item = t.parentNode.parentNode.parentNode;
			if (item.localName == "treeitem") {
				var isServer = (treeitem.getAttribute('IsServer') == "true");
				if (isServer) {
	    			uri = treeitem.getAttribute("id");
					var server = GetServer(uri);
					if (server) {
						server.PerformExpand(msgWindow);
					}
				}
				else {
					var isImap = (treeitem.getAttribute('ServerType') == "imap");
					if (isImap) {
		    			uri = treeitem.getAttribute("id");
						var folder = GetMsgFolderFromUri(uri);
						if (folder) {
							var imapFolder = folder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
							if (imapFolder) {
								imapFolder.performExpand(msgWindow);
							}
						}
					}
				}
			}
		}
    }
	else if(event.detail == 2)
	{
		item = t.parentNode.parentNode;
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
					// double clicking open, PerformExpand()
					server.PerformExpand(msgWindow);
				}
			}
			else {
				// double clicking close, don't PerformExpand()
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

function GetFirstSelectedMessage()
{
    try {
        return gDBView.URIForFirstSelectedMessage;
    }
    catch (ex) {
        return null;
    }
}

function GetSelectedIndices(dbView)
{
  try {
    var indicesArray = {}; 
    var length = {};
    dbView.getIndicesForSelection(indicesArray,length);
    return indicesArray.value;
  }
  catch (ex) {
    dump("ex = " + ex + "\n");
    return null;
  }
}

function GetSelectedMessages()
{
  try {
    var messageArray = {}; 
    var length = {};
    gDBView.getURIsForSelection(messageArray,length);
    return messageArray.value;
  }
  catch (ex) {
    dump("ex = " + ex + "\n");
    return null;
  }
}

function GetLoadedMsgFolder()
{
    if (!gDBView) return null;
    return gDBView.msgFolder;
}

function GetLoadedMessage()
{
    try {
        return gDBView.URIForFirstSelectedMessage;
    }
    catch (ex) {
        return null;
    }
}

//Clear everything related to the current message. called after load start page.
function ClearMessageSelection()
{
	ClearThreadPaneSelection();
}

function GetCompositeDataSource(command)
{
	if (command == "GetNewMessages" || command == "NewFolder" || command == "MarkAllMessagesRead")
	{
        return GetFolderDatasource();
	}

	return null;
}

function SetNextMessageAfterDelete()
{
    //dump("setting next msg view index after delete to " + gDBView.firstSelected + "\n");
    gNextMessageViewIndexAfterDelete = gDBView.firstSelected;
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
  // this isn't going to work anymore
  dump("XXX fix this or remove SelectMessage()\n");
}

function ReloadMessage()
{
  gDBView.reloadMessage();
}

function SetBusyCursor(window, enable)
{
	if(enable)
		window.setCursor("wait");
	else
		window.setCursor("auto");

	var numFrames = window.frames.length;
	for(var i = 0; i < numFrames; i++)
	{
		SetBusyCursor(window.frames[i], enable);
	}
}

function GetDBView()
{
    return gDBView;
}
