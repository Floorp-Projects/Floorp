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


var msgComposeService = Components.classes['component://netscape/messengercompose'].getService();
msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);
var mailSession = Components.classes["component://netscape/messenger/services/session"].getService(Components.interfaces.nsIMsgMailSession); 
var accountManager = Components.classes["component://netscape/messenger/account-manager"].getService(Components.interfaces.nsIMsgAccountManager);

var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var prefs = Components.classes['component://netscape/preferences'].getService();
prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
var showPerformance = prefs.GetBoolPref('mail.showMessengerPerformance');

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

function ComposeMessage(type, format) //type is a nsIMsgCompType and format is a nsIMsgCompFormat
{
		var msgComposeType = Components.interfaces.nsIMsgCompType;
        var identity = null;

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
			// dump("server = " + server + "\n");
			// get the identity associated with this server
			var identities = accountManager.GetIdentitiesForServer(server);
			// dump("identities = " + identities + "\n");
			// just get the first one
			if (identities.Count() > 0 ) {
				identity = identities.GetElementAt(0).QueryInterface(Components.interfaces.nsIMsgIdentity);  
			}
		}
		// dump("identity = " + identity + "\n");
	}
	catch (ex) 
	{
		// dump("failed to get an identity to pre-select\n");
	}

	dump("\nComposeMessage from XUL\n");
	var uri = null;

	if (! msgComposeService)
	{
		dump("### msgComposeService is invalid\n");
		return;
	}
	
	if (type == msgComposeType.New) //new message
	{
		//dump("OpenComposeWindow with " + identity + "\n");
		msgComposeService.OpenComposeWindow(null, null, 0, format, identity);
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
				if (type == msgComposeType.Reply || type == msgComposeType.ReplyAll || type == msgComposeType.ForwardInline)
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
		messenger.GetNewMessages(folderTree.database, selectedFolder);
	}
	else {
		dump("Nothing was selected\n");
	}
}


function LoadMessage(messageNode)
{
  var uri = messageNode.getAttribute('id');
  dump(uri);
  if(uri)
	  OpenURL(uri);
}

function ChangeFolderByDOMNode(folderNode)
{
  var uri = folderNode.getAttribute('id');
  dump(uri + "\n");
  
  var isThreaded = folderNode.getAttribute('threaded');

  if (uri)
	  ChangeFolderByURI(uri, isThreaded == "true", "");
}

function ChangeFolderByURI(uri, isThreaded, sortID)
{
  dump('In ChangeFolderByURI\n');
  var resource = RDF.GetResource(uri);
  var msgfolder =
      resource.QueryInterface(Components.interfaces.nsIMsgFolder);
  if (msgfolder.isServer)
      window.title = msgfolder.name;
  else if (msgfolder.server)
      window.title = msgfolder.name + " on " +
          msgfolder.server.prettyName;
  else
      window.title = msgfolder.name;

  gBeforeFolderLoadTime = new Date();

  if(msgfolder.manyHeadersToDownload())
  {
	try
	{
		gCurrentLoadingFolderURI = uri;
		gCurrentLoadingFolderIsThreaded = isThreaded;
		gCurrentLoadingFolderSortID = sortID;
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
	gCurrentLoadingFolderURI = "";
	gCurrentLoadingFolderIsThreaded = false;
	gCurrentLoadingFolderSortID = "";
	msgfolder.updateFolder(msgWindow);
	RerootFolder(uri, msgfolder, isThreaded, sortID);
  }
}

function RerootFolder(uri, newFolder, isThreaded, sortID)
{
  dump('In reroot folder\n');
  var folder = GetThreadTreeFolder();
  ClearThreadTreeSelection();

  //Set the window's new open folder.
  msgWindow.openFolder = newFolder;

  //Set threaded state
  ShowThreads(isThreaded);

  folder.setAttribute('ref', uri);
  
  UpdateStatusMessageCounts(newFolder);

  var afterFolderLoadTime = new Date();
  var timeToLoad = (afterFolderLoadTime.getTime() - gBeforeFolderLoadTime.getTime())/1000;
  if(showPerformance)
	  dump("Time to load " + uri + " is " +  timeToLoad + " seconds\n");
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

function SortThreadPane(column, sortKey)
{
	var node = document.getElementById(column);
	if(!node)
		return false;

	return SortColumn(node, sortKey);


}

function SortFolderPane(column, sortKey)
{
	var node = FindInSidebar(window, column);
	if(!node)
	{
		dump('Couldnt find sort column\n');
		return false;
	}
	return SortColumn(node, sortKey);
}

function SortColumn(node, sortKey)
{
	dump('In sortColumn\n');
	var xulSortService = Components.classes["component://netscape/rdf/xul-sort-service"].getService();

	if (xulSortService)
	{
		xulSortService = xulSortService.QueryInterface(Components.interfaces.nsIXULSortService);
		if (xulSortService)
		{
			// sort!!!
			sortDirection = "ascending";
			var currentDirection = node.getAttribute('sortDirection');
			if (currentDirection == "ascending")
					sortDirection = "descending";
			else if (currentDirection == "descending")
					sortDirection = "ascending";
			else    sortDirection = "ascending";

			try
			{
				xulSortService.Sort(node, sortKey, sortDirection);
			}
			catch(e)
			{
                dump("Sort failed: " + e + "\n");
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
	var status = treeItem.getAttribute('Status');
	var unread = (status == " ") || (status == "new");
	messenger.MarkMessageRead(tree.database, treeItem, unread);
}

function ToggleMessageFlagged(treeItem)
{

	var tree = GetThreadTree();
	var flaggedValue = treeItem.getAttribute('Flagged');
	dump('flaggedValue is ' + flaggedValue);
	dump('\n');
	var flagged = (flaggedValue =="flagged");
	messenger.MarkMessageFlagged(tree.database, treeItem, !flagged);
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
		ClearMessagePane();
}

function FolderPaneSelectionChange()
{
	var tree = GetFolderTree();
	
	if(tree)
	{
		var selArray = tree.selectedItems;
		if ( selArray && (selArray.length == 1) )
			ChangeFolderByDOMNode(selArray[0]);
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
	var db = db.QueryInterface(Components.interfaces.nsIRDFDataSource);

	var property = RDF.GetResource('http://home.netscape.com/NC-rdf#SpecialFolder');
	var result = db.GetTarget(folderResource, property , true);
	result = result.QueryInterface(Components.interfaces.nsIRDFLiteral);
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

function FolderTest5000()
{

	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);

	var childProperty = RDF.GetResource("http://home.netscape.com/NC-rdf#MessageChild");

	var folderResource = RDF.GetResource("mailbox://scottip@nsmail-2.mcom.com/test5000");

	var beforeTime = new Date();

	var messageChildren = folderDataSource.GetTargets(folderResource, childProperty, true);

	var afterGetTargetsTime = new Date();
	var timeToLoad = (afterGetTargetsTime.getTime() - beforeTime.getTime())/1000;
	dump("Time to load is " +  timeToLoad + " seconds\n");

	messageChildren = messageChildren.QueryInterface(Components.interfaces.nsISimpleEnumerator);

	while(messageChildren.HasMoreElements())
	{
		messageChildren.GetNext();
	}

	var afterTime = new Date();
	timeToLoad = (afterTime.getTime() - beforeTime.getTime())/1000;
	dump("Time to load is " +  timeToLoad + " seconds\n");
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
		nextMessage = GetNextMessage(tree, curMessage, GoMessage, false);
		if(nextMessage)
		{
			if(!MessageInSelection(nextMessage, messages))
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
			nextMessage = GetPreviousMessage(curMessage, GoMessage, false);
			if(nextMessage)
			{
				if(!MessageInSelection(nextMessage, messages))
				{
					break;
				}
			}
			curMessage = nextMessage;
		}



	}
	return nextMessage;
}

function MessageInSelection(message, messages)
{
	var count = messages.length;

	for(var i = 0; i < count; i++)
	{
		if(message == messages[i])
			return true;

	}
	return false;
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
        var out0 = new Object();
        var trashFolder; 
        var out1 = new Object();
        
        rootFolder.getFoldersWithFlag(0x0100, out0, 1, out1); 
        trashFolder = out0.value;
        numFolder = out1.value;
        dump (numFolder + "\n");
        if (trashFolder)
        {
            dump (trashFolder.URI + "\n");
            return trashFolder.URI;
        }
    }
    return null;
}
