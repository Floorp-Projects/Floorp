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


/*
 * Command-specific code. This stuff should be called by the widgets
 */


var gBeforeFolderLoadTime;

function OpenURL(url)
{
  dump("\n\nOpenURL from XUL\n\n\n");
  messenger.SetWindow(window, msgWindow);
  messenger.OpenURL(url);
}

function FindIncomingServer(uri)
{
	//dump("FindIncomingServer("+uri+")\n");
	var server=null;

	if (!uri) return server;
	
	try {
		var resource = RDF.GetResource(uri);
  		var msgfolder = resource.QueryInterface(Components.interfaces.nsIMsgFolder);
		server = msgfolder.server;

		//dump("server = " + server + "\n");
		return server;
	}
	catch (ex) {
		return null;
	}
}


function getIdentityForSelectedServer()
{
    var folderTree = GetFolderTree();
    var identity = null;
    var selectedFolderList = folderTree.selectedItems;
    if(selectedFolderList.length > 0) {

        var selectedFolder = selectedFolderList[0]; 
        var folderUri = selectedFolder.getAttribute('id');
        // dump("selectedFolder uri = " + uri + "\n");
        
        // get the incoming server associated with this folder uri
        var server = FindIncomingServer(folderUri);
        // dump("server = " + server + "\n");
        // get the identity associated with this server
        var identities = accountManager.GetIdentitiesForServer(server);
        // dump("identities = " + identities + "\n");
        // just get the first one
        if (identities.Count() > 0 ) {
            identity = identities.GetElementAt(0).QueryInterface(Components.interfaces.nsIMsgIdentity);  
        }
    }

    return identity;
}


function ComposeMessage(type, format) //type is a nsIMsgCompType and format is a nsIMsgCompFormat
{
		var msgComposeType = Components.interfaces.nsIMsgCompType;
        var identity = null;
	var newsgroup = null;

	try 
	{
		var folderTree = GetFolderTree();
		var selectedFolderList = folderTree.selectedItems;
		if(selectedFolderList.length > 0)
		{
			var selectedFolder = selectedFolderList[0]; 
			var uri = selectedFolder.getAttribute('id');
			// dump("selectedFolder uri = " + uri + "\n");

			// get the incoming server associated with this uri
			var server = FindIncomingServer(uri);

			// if they hit new or reply and they are reading a newsgroup
			// turn this into a new post or a reply to group.
			if (server.type == "nntp")
			{
			    if (type == msgComposeType.Reply)
			        type = msgComposeType.ReplyToGroup;
			    else
			        if (type == msgComposeType.New)
			        {
					    type = msgComposeType.NewsPost;

    					// from the uri, get the newsgroup name
    					var resource = RDF.GetResource(uri);
    					var msgfolder = resource.QueryInterface(Components.interfaces.nsIMsgFolder); 
    					if (msgfolder.isServer) 
    						newsgroup = "";
    					else 
          					newsgroup = msgfolder.name; 
				    }
			}
		}
        
        identity = getIdentityForSelectedServer();

		// dump("identity = " + identity + "\n");
	}
	catch (ex) 
	{
        // dump("failed to get an identity to pre-select: " + ex + "\n");
	}

	dump("\nComposeMessage from XUL: " + identity + "\n");
	var uri = null;

	if (! msgComposeService)
	{
		dump("### msgComposeService is invalid\n");
		return;
	}
	
	if (type == msgComposeType.New) //new message
	{
		//dump("OpenComposeWindow with " + identity + "\n");
		msgComposeService.OpenComposeWindow(null, null, type, format, identity);
		return;
	}
        else if (type == msgComposeType.NewsPost) 
	{
		dump("OpenComposeWindow with " + identity + " and " + newsgroup + "\n");
		msgComposeService.OpenComposeWindow(null, newsgroup, type, format, identity);
		return;
	}
		
	var tree = GetThreadTree();
	if (tree)
	{
		var nodeList = tree.selectedItems;
		var appCore = FindMessenger();
		if (appCore)
			appCore.SetWindow(window, msgWindow);
			
		var object = null;
	
		if (nodeList && nodeList.length > 0)
		{
			uri = "";
			for (var i = 0; i < nodeList.length && i < 8; i ++)
			{	
				dump('i = '+ i);
				dump('\n');				
				if (type == msgComposeType.Reply || type == msgComposeType.ReplyAll || type == msgComposeType.ForwardInline ||
					type == msgComposeType.ReplyToGroup || type == msgComposeType.ReplyToSenderAndGroup ||
					type == msgComposeType.Template || type == msgComposeType.Draft)
				{
					msgComposeService.OpenComposeWindow(null, nodeList[i].getAttribute('id'), type, format, identity);
				}
				else
				{
					if (i) 
						uri += ","
					uri += nodeList[i].getAttribute('id');
				}
			}
			if (type == msgComposeType.ForwardAsAttachment)
			{
				msgComposeService.OpenComposeWindow(null, uri, type, format, identity);
  			}
		}
		else
			dump("### nodeList is invalid\n");
	}
	else
		dump("### tree is invalid\n");
}

function GetNewMessages()
{
	var folderTree = GetFolderTree();
	var selectedFolderList = folderTree.selectedItems;
	if(selectedFolderList.length > 0)
	{
		var selectedFolder = selectedFolderList[0];

		//Whenever we do get new messages, clear the old new messages.
		var msgFolder = GetMsgFolderFromNode(selectedFolder);
		if(msgFolder && msgFolder.hasNewMessages())
			msgFolder.clearNewMessages();

		messenger.GetNewMessages(folderTree.database, selectedFolder.resource);
	}
	else {
		dump("Nothing was selected\n");
	}
}

function GetMsgFolderFromNode(folderNode)
{
	var folderURI = folderNode.getAttribute("id");
	return GetMsgFolderFromURI(folderURI);
}

function GetMsgFolderFromURI(folderURI)
{
	var folderResource = RDF.GetResource(folderURI);
	if(folderResource)
	{
		var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
		return msgFolder;
	}

	return null;


}

function LoadMessage(messageNode)
{
  var uri = messageNode.getAttribute('id');
  dump(uri + "\n");

  if(uri != gCurrentDisplayedMessage)
  {
    var resource = RDF.GetResource(uri);
    var message = resource.QueryInterface(Components.interfaces.nsIMessage); 
    if (message)
      setTitleFromFolder(message.GetMsgFolder(), message.mime2DecodedSubject);

	  gCurrentDisplayedMessage = uri;
	  OpenURL(uri);
  }
}

function ChangeFolderByDOMNode(folderNode)
{
  var uri = folderNode.getAttribute('id');
  dump(uri + "\n");
  
  var isThreaded = folderNode.getAttribute('threaded');
  var sortResource = folderNode.getAttribute('sortResource');
  if(!sortResource)
	sortResource = "";

  var sortDirection = folderNode.getAttribute('sortDirection');

  if (uri)
	  ChangeFolderByURI(uri, isThreaded == "true", sortResource, sortDirection);
}

function setTitleFromFolder(msgfolder, subject)
{
    if (!msgfolder) return;

    var title;
    var server = msgfolder.server;

    if (null != subject)
      title = subject+" - ";
    else
      title = "";

    if (msgfolder.isServer) {
            // <hostname>
            title += server.hostName;
    }
    else {
        var middle;
        var end;
        if (server.type == "nntp") {
            // <folder> on <hostname>
            middle = Bundle.GetStringFromName("titleNewsPreHost");
            end = server.hostName;
        } else {
            var identity;
            try {
                var identities = accountManager.GetIdentitiesForServer(server);

                identity = identities.QueryElementAt(0, Components.interfaces.nsIMsgIdentity);
                // <folder> for <email>
                middle = Bundle.GetStringFromName("titleMailPreHost");
                end = identity.email;
            } catch (ex) {
            }
            
        }

        title += msgfolder.prettyName;
        if (middle) title += " " + middle;
        if (end) title += " " + end;
    }

    title += " - " + BrandBundle.GetStringFromName("brandShortName");
    window.title = title;
}

function ChangeFolderByURI(uri, isThreaded, sortID, sortDirection)
{
  dump('In ChangeFolderByURI\n');
  var resource = RDF.GetResource(uri);
  var msgfolder =
      resource.QueryInterface(Components.interfaces.nsIMsgFolder);

  try {
      setTitleFromFolder(msgfolder, null);
  } catch (ex) {
      dump("error setting title: " + ex + "\n");
  }
  
  gBeforeFolderLoadTime = new Date();
  gCurrentLoadingFolderURI = uri;

  if(msgfolder.manyHeadersToDownload())
  {
	try
	{
		gCurrentFolderToReroot = uri;
		gCurrentLoadingFolderIsThreaded = isThreaded;
		gCurrentLoadingFolderSortID = sortID;
		gCurrentLoadingFolderSortDirection = sortDirection;
		msgfolder.startFolderLoading();
		msgfolder.updateFolder(msgWindow);
	}
	catch(ex)
	{
        dump("Error loading with many headers to download\n");
	}
  }
  else
  {
	gCurrentFolderToReroot = "";
	gCurrentLoadingFolderIsThreaded = false;
	gCurrentLoadingFolderSortID = "";
	RerootFolder(uri, msgfolder, isThreaded, sortID, sortDirection);

	//Need to do this after rerooting folder.  Otherwise possibility of receiving folder loaded
	//notification before folder has actually changed.
	msgfolder.updateFolder(msgWindow);
  }
}

function RerootFolder(uri, newFolder, isThreaded, sortID, sortDirection)
{
  dump('In reroot folder\n');
  var folder = GetThreadTreeFolder();
  ClearThreadTreeSelection();

  //Set the window's new open folder.
  msgWindow.openFolder = newFolder;

  //Set threaded state
  ShowThreads(isThreaded);
  
  //Clear the new messages of the old folder
  var oldFolderURI = folder.getAttribute("ref");
  if(oldFolderURI != "null")
  {
	var oldFolder = GetMsgFolderFromURI(oldFolderURI);
	if(oldFolder && oldFolder.hasNewMessages())
		oldFolder.clearNewMessages();
  }
  
  //Clear out the thread pane so that we can sort it with the new sort id without taking any time.
  folder.setAttribute('ref', "");

  var column = FindThreadPaneColumnBySortResource(sortID);

  if(column)
	SortThreadPane(column, sortID, "http://home.netscape.com/NC-rdf#Date", false, sortDirection);
  else
	SortThreadPane("DateColumn", "http://home.netscape.com/NC-rdf#Date", "", false, null);

  folder.setAttribute('ref', uri);
    msgNavigationService.EnsureDocumentIsLoaded(document);

  UpdateStatusMessageCounts(newFolder);
}


function UpdateStatusMessageCounts(folder)
{
	var unreadElement = GetUnreadCountElement();
	var totalElement = GetTotalCountElement();
	if(folder && unreadElement && totalElement)
	{
		var numUnread = folder.getNumUnread(false);
		var numTotal = folder.getTotalMessages(false);

		unreadElement.setAttribute("value", numUnread);
		totalElement.setAttribute("value", numTotal);

	}

}

function SaveThreadPaneSelection()
{
	var tree = GetThreadTree();
	var selectedItems = tree.selectedItems;
	var numSelected = selectedItems.length;

	var selectionArray = new Array(numSelected);

	for(var i = 0; i < numSelected; i++)
	{
		selectionArray[i] = selectedItems[i].getAttribute("id");
	}

	return selectionArray;
}

function RestoreThreadPaneSelection(selectionArray)
{
	var tree = GetThreadTree();
	var numSelected = selectionArray.length;

	msgNavigationService.EnsureDocumentIsLoaded(document);

	var messageElement;
	for(var i = 0 ; i < numSelected; i++)
	{
		messageElement = document.getElementById(selectionArray[i]);

		if(!messageElement && messageView.showThreads)
		{
			var treeFolder = GetThreadTreeFolder();
			var folderURI = treeFolder.getAttribute('ref');
			var folderResource = RDF.GetResource(folderURI);
			var folder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);

			var messageResource = RDF.GetResource(selectionArray[i]);
			var message = messageResource.QueryInterface(Components.interfaces.nsIMessage);

			var topLevelMessage = GetTopLevelMessageForMessage(message, folder);
			var topLevelResource = topLevelMessage.QueryInterface(Components.interfaces.nsIRDFResource);
			var topLevelURI = topLevelResource.Value;
			var topElement = document.getElementById(topLevelURI);
			if(topElement)
			{
				msgNavigationService.OpenTreeitemAndDescendants(topElement);
			}

			messageElement = document.getElementById(selectionArray[i]);

		}
		tree.addItemToSelection(messageElement);
		if(messageElement && (i==0))
			tree.ensureElementIsVisible(messageElement);
	}

}

function FindThreadPaneColumnBySortResource(sortID)
{

	if(sortID == "http://home.netscape.com/NC-rdf#Date")
		return "DateColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#Sender")
		return "AuthorColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#Status")
		return "StatusColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#Subject")
		return "SubjectColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#Flagged")
		return "FlaggedButtonColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#Priority")
		return "PriorityColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#Size")
		return "SizeColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#HasUnreadMessages")
		return "UnreadButtonColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#TotalUnreadMessages")
		return "UnreadColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#TotalMessages")
		return "TotalColumn";

	return null;


}

//If toggleCurrentDirection is true, then get current direction and flip to opposite.
//If it's not true then use the direction passed in.
function SortThreadPane(column, sortKey, secondarySortKey, toggleCurrentDirection, direction)
{
	var node = document.getElementById(column);
	if(!node)
		return false;

	if(!direction)
	{
		direction = "ascending";
		//If we just clicked on the same column, then change the direction
		if(toggleCurrentDirection)
		{
			var currentDirection = node.getAttribute('sortDirection');
			if (currentDirection == "ascending")
					direction = "descending";
			else if (currentDirection == "descending")
					direction = "ascending";
		}
	}

   var folder = GetSelectedFolder();
	if(folder)
	{
		folder.setAttribute("sortResource", sortKey);
		folder.setAttribute("sortDirection", direction);
	}

	SetActiveThreadPaneSortColumn(column);

	var selection = SaveThreadPaneSelection();
	var beforeSortTime = new Date();

	var result = SortColumn(node, sortKey, secondarySortKey, direction);
	var afterSortTime = new Date();
	var timeToSort = (afterSortTime.getTime() - beforeSortTime.getTime())/1000;

	if(showPerformance)
		dump("timeToSort is " + timeToSort + "seconds\n");
	RestoreThreadPaneSelection(selection);
	return result;
}

function SortFolderPane(column, sortKey)
{
	var node = FindInSidebar(window, column);
	if(!node)
	{
		dump('Couldnt find sort column\n');
		return false;
	}
	return SortColumn(node, sortKey, null, null);
}

function SortColumn(node, sortKey, secondarySortKey, direction)
{
	dump('In sortColumn\n');
	var xulSortService = Components.classes["component://netscape/rdf/xul-sort-service"].getService();

	if (xulSortService)
	{
		xulSortService = xulSortService.QueryInterface(Components.interfaces.nsIXULSortService);
		if (xulSortService)
		{
			// sort!!!
			var sortDirection;
			if(direction)
				sortDirection = direction;
			else
			{
				var currentDirection = node.getAttribute('sortDirection');
				if (currentDirection == "ascending")
						sortDirection = "descending";
				else if (currentDirection == "descending")
						sortDirection = "ascending";
				else    sortDirection = "ascending";
			}

			try
			{
				if(secondarySortKey)
					node.setAttribute('rdf:resource2', secondarySortKey);
				xulSortService.Sort(node, sortKey, sortDirection);
			}
			catch(e)
			{
                		//dump("Sort failed: " + e + "\n");
			}
		}
	}

}

function GetSelectedFolderResource()
{
	var folderTree = GetFolderTree();
	var selectedFolderList = folderTree.selectedItems;
	var selectedFolder = selectedFolderList[0];
	var uri = selectedFolder.getAttribute('id');


	var folderResource = RDF.GetResource(uri);
	return folderResource;

}

function SetFolderCharset(folderResource, aCharset)
{
	var folderTree = GetFolderTree();

	var db = folderTree.database;
	var db2 = db.QueryInterface(Components.interfaces.nsIRDFDataSource);

	var charsetResource = RDF.GetLiteral(aCharset);
	var charsetProperty = RDF.GetResource("http://home.netscape.com/NC-rdf#Charset");

	db2.Assert(folderResource, charsetProperty, charsetResource, true);
}



function ToggleMessageRead(treeItem)
{

	var tree = GetThreadTree();
	var isUnread = treeItem.getAttribute('IsUnread');
	var unread = (isUnread == "true");
	messenger.MarkMessageRead(tree.database, treeItem.resource, unread);
}

function ToggleMessageFlagged(treeItem)
{

	var tree = GetThreadTree();
	var flaggedValue = treeItem.getAttribute('Flagged');
	dump('flaggedValue is ' + flaggedValue);
	dump('\n');
	var flagged = (flaggedValue =="flagged");
	messenger.MarkMessageFlagged(tree.database, treeItem.resource, !flagged);
}

//Called when the splitter in between the thread and message panes is clicked.
function OnClickThreadAndMessagePaneSplitter()
{
	var collapsed = IsThreadAndMessagePaneSplitterCollapsed();
	//collapsed is the previous state so we know we are opening.
	if(collapsed)
		LoadSelectionIntoMessagePane();	
}

//Called when selection changes in the thread pane.
function ThreadPaneSelectionChange()
{
	var collapsed = IsThreadAndMessagePaneSplitterCollapsed();

	if(!collapsed)
	{
		LoadSelectionIntoMessagePane();
	}
}

//takes the selection from the thread pane and loads it into the message pane
function LoadSelectionIntoMessagePane()
{
	var tree = GetThreadTree();
	
	var selArray = tree.selectedItems;
	if ( selArray && (selArray.length == 1) )
		LoadMessage(selArray[0]);
	else
  {
    // don't necessarily clear the message pane...if you uncomment this,
    // you'll be introducing a large inefficiency when deleting messages...as deleting
    // a msg brings us here twice...so we end up clearing the message pane twice for no
    // good reason...
		// ClearMessagePane();
  }
}

function FolderPaneSelectionChange()
{
	var tree = GetFolderTree();
	if(tree)
	{
		var selArray = tree.selectedItems;
		if ( selArray && (selArray.length == 1) )
    {
			ChangeFolderByDOMNode(selArray[0]);
      // explicitly force the message pane to get cleared when we switch folders
      ClearMessagePane(); 
    }
		else
		{
			var threadTree = GetThreadTree();
			ClearThreadTreeSelection();
			threadTree.setAttribute('ref', null);
			ClearMessagePane();
		}
	}
}

function OpenFolderTreeToFolder(folderURI)
{
	var tree = GetFolderTree();
	return OpenToFolder(tree, folderURI);

}

function OpenToFolder(item, folderURI)
{

	if(item.nodeType != Node.ELEMENT_NODE)
		return null;

	var uri = item.getAttribute('id');
	dump(uri);
	dump('\n');
	if(uri == folderURI)
	{
		dump('found folder: ' + uri);
		dump('\n');
		return item;
	}

	var children = item.childNodes;
	var length = children.length;
	var i;
	dump('folder ' + uri);
	dump('has ' + length);
	dump('children\n');
	for(i = 0; i < length; i++)
	{
		var child = children[i];
		var folder = OpenToFolder(child, folderURI);
		if(folder)
		{
			child.setAttribute('open', 'true');
			return folder;
		}
	}
	return null;
}

function IsSpecialFolderSelected(folderName)
{
	var selectedFolder = GetThreadTreeFolder();
	var id = selectedFolder.getAttribute('ref');
	var folderResource = RDF.GetResource(id);
	if(!folderResource)
		return false;

	var folderTree = GetFolderTree();
	var db = folderTree.database;
    if (!db) return false;
	db = db.QueryInterface(Components.interfaces.nsIRDFDataSource);
    if (!db) return false;

	var property =
        RDF.GetResource('http://home.netscape.com/NC-rdf#SpecialFolder');
    if (!property) return false;
	var result = db.GetTarget(folderResource, property , true);
    if (!result) return false;
	result = result.QueryInterface(Components.interfaces.nsIRDFLiteral);
    if (!result) return false;
	if(result.Value == folderName)
		return true;

	return false;
}



function ChangeThreadView()
{
   var folder = GetSelectedFolder();

	var threadColumn = document.getElementById('ThreadColumnHeader');
	if(threadColumn)
	{
		var currentView = threadColumn.getAttribute('currentView');
		if(currentView== 'threaded')
		{
			ShowThreads(false);
			if(folder)
				folder.setAttribute('threaded', "");
		}
		else if(currentView == 'unthreaded')
		{
			ShowThreads(true);
			if(folder)
				folder.setAttribute('threaded', "true");
		}
		RefreshThreadTreeView();
	}
}

function ShowThreads(showThreads)
{
	dump('in showthreads\n');
	if(messageView)
	{
		messageView.showThreads = showThreads;
		var threadColumn = document.getElementById('ThreadColumnHeader');
		if(threadColumn)
		{
			if(showThreads)
			{
				threadColumn.setAttribute('currentView', 'threaded');
			}
			else
			{
				threadColumn.setAttribute('currentView', 'unthreaded');
			}
		}
	}
}


function GetNextMessageAfterDelete(messages)
{
	var count = messages.length;

	var curMessage = messages[0];
	var nextMessage = null;
	var tree = GetThreadTree();

	//search forward
	while(curMessage)
	{
		nextMessage = msgNavigationService.FindNextMessage(navigateAny, tree, curMessage, RDF, document, false, messageView.showThreads);
		if(nextMessage)
		{
			if(nextMessage.getAttribute("selected") != "true")
			{
				break;
			}
		}
		curMessage = nextMessage;
	}
	//if no nextmessage then search backwards
	if(!nextMessage)
	{

		var curMessage = messages[0];
		var nextMessage = null;
		//search forward
		while(curMessage)
		{
			nextMessage = msgNavigationService.FindPreviousMessage(navigateAny, tree, curMessage, RDF, document, false, messageView.showThreads);
			if(nextMessage)
			{
				if(nextMessage.getAttribute("selected") != "true")
				{
					break;
				}
			}
			curMessage = nextMessage;
		}



	}
	return nextMessage;
}


function SelectNextMessage(nextMessage)
{
	var tree = GetThreadTree();
	ChangeSelection(tree, nextMessage);

}

function GetSelectTrashUri(folder)
{
    if (!folder) return null;
    var uri = folder.getAttribute('id');
    dump (uri + "\n");
    var resource = RDF.GetResource(uri);
    var msgFolder =
        resource.QueryInterface(Components.interfaces.nsIMsgFolder);
    if (msgFolder)
    {
        dump("GetSelectTrashUri" + "\n");
        var rootFolder = msgFolder.rootFolder;
        var numFolder;
        var out = new Object();
        var trashFolder = rootFolder.getFoldersWithFlag(0x0100, 1, out); 
        numFolder = out.value;
        dump (numFolder + "\n");
        if (trashFolder)
        {
            dump (trashFolder.URI + "\n");
            return trashFolder.URI;
        }
    }
    return null;
}

function Undo()
{
    messenger.Undo(msgWindow);
}

function Redo()
{
    messenger.Redo(msgWindow);
}

function PrintEnginePrint()
{
	var tree = GetThreadTree();
	var selectedItems = tree.selectedItems;
	var numSelected = selectedItems.length;

  if (numSelected == 0)
  {
    dump("PrintEnginePrint(): No messages selected.\n");
    return false;
  }  

	var selectionArray = new Array(numSelected);

	for(var i = 0; i < numSelected; i++)
	{
		selectionArray[i] = selectedItems[i].getAttribute("id");
	}

  printEngineWindow = window.openDialog("chrome://messenger/content/msgPrintEngine.xul",
								                        "",
								                        "chrome,dialog=no,all",
								                        numSelected, selectionArray, statusFeedback);
  return true;
}
