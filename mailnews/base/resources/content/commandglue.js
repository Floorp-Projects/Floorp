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

//The eventual goal is for this file to go away and for the functions to either be brought into
//mailCommands.js or into 3pane specific code.

var gBeforeFolderLoadTime;
var gRDFNamespace = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
function OpenURL(url)
{
  dump("\n\nOpenURL from XUL\n\n\n");
  messenger.SetWindow(window, msgWindow);
  messenger.OpenURL(url);
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

	LoadMessageByUri(uri);

}

function LoadMessageByUri(uri)
{  
	if(uri != gCurrentDisplayedMessage)
	{
		var resource = RDF.GetResource(uri);
		var message = resource.QueryInterface(Components.interfaces.nsIMessage); 
		if (message)
			setTitleFromFolder(message.msgFolder, message.mime2DecodedSubject);

		var nsIMsgFolder = Components.interfaces.nsIMsgFolder;
		if (message.msgFolder.server.downloadOnBiff)
			message.msgFolder.biffState = nsIMsgFolder.nsMsgBiffState_NoMail;

		gCurrentDisplayedMessage = uri;
		gHaveLoadedMessage = true;
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
  dump('In ChangeFolderByURI uri = ' + uri + "\n");
  if (uri == gCurrentLoadingFolderURI)
    return;
  var resource = RDF.GetResource(uri);
  var msgfolder =
      resource.QueryInterface(Components.interfaces.nsIMsgFolder);

  try {
      setTitleFromFolder(msgfolder, null);
  } catch (ex) {
      dump("error setting title: " + ex + "\n");
  }

  
  //if it's a server, clear the threadpane and don't bother trying to load.
  if(msgfolder.isServer)
  {
	ClearThreadPane();
	return;
  }
    
  gBeforeFolderLoadTime = new Date();
  gCurrentLoadingFolderURI = uri;

  if(msgfolder.manyHeadersToDownload())
  {
	try
	{
		SetBusyCursor(window, true);
		gCurrentFolderToReroot = uri;
		gCurrentLoadingFolderIsThreaded = isThreaded;
		gCurrentLoadingFolderSortID = sortID;
		gCurrentLoadingFolderSortDirection = sortDirection;
		msgfolder.startFolderLoading();
		msgfolder.updateFolder(msgWindow);
	}
	catch(ex)
	{
        dump("Error loading with many headers to download: " + ex + "\n");
	}
  }
  else
  {
    SetBusyCursor(window, true);
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
  if(oldFolderURI && (oldFolderURI != "null") && (oldFolderURI !=""))
  {
   var oldFolder = GetMsgFolderFromURI(oldFolderURI);
   if(oldFolder)
   {
       if (oldFolder.hasNewMessages)
           oldFolder.clearNewMessages();
   }
  }

  //the new folder being selected should have its biff state get cleared.   
  if(newFolder)
  {
    newFolder.biffState = 
          Components.interfaces.nsIMsgFolder.nsMsgBiffState_NoMail;
  }

  //Clear out the thread pane so that we can sort it with the new sort id without taking any time.
  folder.setAttribute('ref', "");

  var column = FindThreadPaneColumnBySortResource(sortID);

  if(column)
	SortThreadPane(column, sortID, "http://home.netscape.com/NC-rdf#Date", false, sortDirection, false);
  else
	SortThreadPane("DateColumn", "http://home.netscape.com/NC-rdf#Date", "", false, null, false);

  SetSentFolderColumns(IsSpecialFolder(newFolder, "Sent"));

  // Since SetSentFolderColumns() may alter the template's structure,
  // we need to explicitly force the builder to recompile its rules.
  //when switching folders, switch back to closing threads
  SetTemplateTreeItemOpen(false);
  folder.builder.rebuild();

  folder.setAttribute('ref', uri);
    msgNavigationService.EnsureDocumentIsLoaded(document);

  UpdateStatusMessageCounts(newFolder);

}

function SetSentFolderColumns(isSentFolder)
{
	var senderColumn = document.getElementById("SenderColumnHeader");
	var senderColumnTemplate = document.getElementById("SenderColumnTemplate");
	var authorColumnHeader = document.getElementById("AuthorColumn");

	if(isSentFolder)
	{
		senderColumn.setAttribute("value", Bundle.GetStringFromName("recipientColumnHeader"));
		senderColumn.setAttribute("onclick", "return top.MsgSortByRecipient();");
		senderColumnTemplate.setAttribute("value", "rdf:http://home.netscape.com/NC-rdf#Recipient");
		authorColumnHeader.setAttributeNS(gRDFNamespace, "resource", "http://home.netscape.com/NC-rdf#Recipient");
	}
	else
	{
		senderColumn.setAttribute("value", Bundle.GetStringFromName("senderColumnHeader"));
		senderColumn.setAttribute("onclick", "return top.MsgSortBySender();");
		senderColumnTemplate.setAttribute("value", "rdf:http://home.netscape.com/NC-rdf#Sender");
		authorColumnHeader.setAttributeNS(gRDFNamespace, "resource", "http://home.netscape.com/NC-rdf#Sender");
	}


}

function UpdateStatusMessageCounts(folder)
{
	var unreadElement = GetUnreadCountElement();
	var totalElement = GetTotalCountElement();
	if(folder && unreadElement && totalElement)
	{
		var numUnread =
            Bundle.formatStringFromName("unreadMsgStatus",
                                        [ folder.getNumUnread(false)], 1);
		var numTotal =
            Bundle.formatStringFromName("totalMsgStatus",
                                        [folder.getTotalMessages(false)], 1);

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
		if(messageElement)
		{
			dump("We have a messageElement\n");
			tree.addItemToSelection(messageElement);
			if(i==0)
				tree.ensureElementIsVisible(messageElement);
		}
	}

}

function FindThreadPaneColumnBySortResource(sortID)
{

	if(sortID == "http://home.netscape.com/NC-rdf#Date")
		return "DateColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#Sender")
		return "AuthorColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#Recipient")
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
	else if(sortID == "http://home.netscape.com/NC-rdf#IsUnread")
		return "UnreadButtonColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#TotalUnreadMessages")
		return "UnreadColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#TotalMessages")
		return "TotalColumn";
	else if(sortID == "http://home.netscape.com/NC-rdf#OrderReceived")
		return "OrderReceivedColumn";

	return null;


}

//If toggleCurrentDirection is true, then get current direction and flip to opposite.
//If it's not true then use the direction passed in.
function SortThreadPane(column, sortKey, secondarySortKey, toggleCurrentDirection, direction, changeCursor)
{
	//dump("In SortThreadPane\n");
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

	UpdateSortIndicator(column, direction);
	UpdateSortMenu(column);

   var folder = GetSelectedFolder();
	if(folder)
	{
		folder.setAttribute("sortResource", sortKey);
		folder.setAttribute("sortDirection", direction);
	}

	SetActiveThreadPaneSortColumn(column);

	var selection = SaveThreadPaneSelection();
	var beforeSortTime = new Date();

	if(changeCursor)
		SetBusyCursor(window, true);
	var result = SortColumn(node, sortKey, secondarySortKey, direction);
	if(changeCursor)
		SetBusyCursor(window, false);
	var afterSortTime = new Date();
	var timeToSort = (afterSortTime.getTime() - beforeSortTime.getTime())/1000;

	if(showPerformance)
		dump("timeToSort is " + timeToSort + "seconds\n");
	RestoreThreadPaneSelection(selection);
	return result;
}

//------------------------------------------------------------
// Sets the column header sort icon based on the requested 
// column and direction.
// 
// Notes:
// (1) This function relies on the first part of the 
//     <treecell id> matching the <treecol id>.  The treecell
//     id must have a "Header" suffix.
// (2) By changing the "sortDirection" attribute, a different
//     CSS style will be used, thus changing the icon based on
//     the "sortDirection" parameter.
//------------------------------------------------------------
function UpdateSortIndicator(column,sortDirection)
{
	// Find the <treerow> element
	var treerow = document.getElementById("headRow");

	//The SortThreadPane function calls the Sender/Recipient column 'AuthorColumn' 
	//but it's treecell header id is actually 'SenderColumnHeader', so we need to flip
	//it here so that the css can handle changing it's style correctly.
	if(column == "AuthorColumn"){
		column = "SenderColumn";
	}

	var id = column + "Header";
	
	if (treerow)
	{
		// Grab all of the <treecell> elements
		var treecell = treerow.getElementsByTagName("treecell");
		if (treecell)
		{
			// Loop through each treecell...
			var node_count = treecell.length;
			for (var i=0; i < node_count; i++)
			{
				// Is this the requested column ?
				if (id == treecell[i].getAttribute("id"))
				{
					// Set the sortDirection so the class (CSS) will add the
					// appropriate icon to the header cell
					treecell[i].setAttribute('sortDirection',sortDirection);
				}
				else
				{
					// This is not the sorted row
					treecell[i].removeAttribute('sortDirection');
				}
			}
		}
	}
}

function UpdateSortMenu(currentSortColumn)
{
	UpdateSortMenuitem(currentSortColumn, "sortByDateMenuitem", "DateColumn");
	UpdateSortMenuitem(currentSortColumn, "sortByFlagMenuitem", "FlaggedButtonColumn");
	UpdateSortMenuitem(currentSortColumn, "sortByOrderReceivedMenuitem", "OrderReceivedColumn");
	UpdateSortMenuitem(currentSortColumn, "sortByPriorityMenuitem", "PriorityColumn");
	UpdateSortMenuitem(currentSortColumn, "sortBySenderMenuitem", "AuthorColumn");
	UpdateSortMenuitem(currentSortColumn, "sortBySizeMenuitem", "SizeColumn");
	UpdateSortMenuitem(currentSortColumn, "sortByStatusMenuitem", "StatusColumn");
	UpdateSortMenuitem(currentSortColumn, "sortBySubjectMenuitem", "SubjectColumn");
	UpdateSortMenuitem(currentSortColumn, "sortByUnreadMenuitem", "UnreadButtonColumn");

}

function UpdateSortMenuitem(currentSortColumnID, menuItemID, columnID)
{
	var menuItem = document.getElementById(menuItemID);
	if(menuItem)
	{
		menuItem.setAttribute("checked", currentSortColumnID == columnID);
	}
}

function SortFolderPane(column, sortKey)
{
	var node = FindInSidebar(window, column);
	if(!node)
	{
		dump('Couldnt find sort column\n');
		return false;
	}
	SortColumn(node, sortKey, null, null);
	//Remove the sortActive attribute because we don't want this pane to have any
	//sort styles.
	node.setAttribute("sortActive", "false");
	return true;
}

function SortColumn(node, sortKey, secondarySortKey, direction)
{
	dump('In SortColumn\n');
	var xulSortService = Components.classes["@mozilla.org/rdf/xul-sort-service;1"].getService();

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
					node.setAttributeNS(gRDFNamespace, 'resource2', secondarySortKey);
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
	var db = GetFolderDatasource();

	var charsetResource = RDF.GetLiteral(aCharset);
	var charsetProperty = RDF.GetResource("http://home.netscape.com/NC-rdf#Charset");

	db.Assert(folderResource, charsetProperty, charsetResource, true);
}



function ToggleMessageRead(treeItem)
{

	var tree = GetThreadTree();

	var messageResource = RDF.GetResource(treeItem.getAttribute('id'));

	var property = RDF.GetResource('http://home.netscape.com/NC-rdf#IsUnread');
	var result = tree.database.GetTarget(messageResource, property , true);
	result = result.QueryInterface(Components.interfaces.nsIRDFLiteral);
	var isUnread = (result.Value == "true")

	var message = messageResource.QueryInterface(Components.interfaces.nsIMessage);
	var messageArray = new Array(1);
	messageArray[0] = message;

	MarkMessagesRead(tree.database, messageArray, isUnread);
}

function ToggleMessageFlagged(treeItem)
{

	var tree = GetThreadTree();

	var messageResource = RDF.GetResource(treeItem.getAttribute('id'));

	var property = RDF.GetResource('http://home.netscape.com/NC-rdf#Flagged');
	var result = tree.database.GetTarget(messageResource, property , true);
	result = result.QueryInterface(Components.interfaces.nsIRDFLiteral);
	var flagged = (result.Value == "flagged")

	var message = messageResource.QueryInterface(Components.interfaces.nsIMessage);
	var messageArray = new Array(1);
	messageArray[0] = message;

	MarkMessagesFlagged(tree.database, messageArray, !flagged);

}

//Called when the splitter in between the thread and message panes is clicked.
function OnClickThreadAndMessagePaneSplitter()
{
	dump("We are in OnClickThreadAndMessagePaneSplitter()\n");
	var collapsed = IsThreadAndMessagePaneSplitterCollapsed();
	//collapsed is the previous state so we know we are opening.
	if(collapsed)
		LoadSelectionIntoMessagePane();	
}


//takes the selection from the thread pane and loads it into the message pane
function LoadSelectionIntoMessagePane()
{
	var tree = GetThreadTree();
	
	var selArray = tree.selectedItems;
	if (!gNextMessageAfterDelete && selArray && (selArray.length == 1) )
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
        }
		else
		{
			ClearThreadPane();
		}
	}
	ClearMessagePane();

}

function ClearThreadPane()
{
	var threadTree = GetThreadTree();
	ClearThreadTreeSelection();
	threadTree.setAttribute('ref', null);
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


function IsSpecialFolder(msgFolder, specialFolderName)
{
	var db = GetFolderDatasource();
	var folderResource = msgFolder.QueryInterface(Components.interfaces.nsIRDFResource);
	if(folderResource)
	{
		var property =
			RDF.GetResource('http://home.netscape.com/NC-rdf#SpecialFolder');
		if (!property) return false;
		var result = db.GetTarget(folderResource, property , true);
		if (!result) return false;
		result = result.QueryInterface(Components.interfaces.nsIRDFLiteral);
		if (!result) return false;
		dump("We are looking for " + specialFolderName + "\n");
		dump("special folder name = " + result.Value + "\n");
		if(result.Value == specialFolderName)
			return true;
	}

	return false;
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

		curMessage = messages[0];
		nextMessage = null;
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

