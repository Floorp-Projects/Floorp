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

var messenger = Components.classes['component://netscape/messenger'].createInstance();
messenger = messenger.QueryInterface(Components.interfaces.nsIMessenger);

var msgComposeService = Components.classes['component://netscape/messengercompose'].getService();
msgComposeService = msgComposeService.QueryInterface(Components.interfaces.nsIMsgComposeService);		

var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var prefs = Components.classes['component://netscape/preferences'].getService();
prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
var showPerformance = prefs.GetBoolPref('mail.showMessengerPerformance');

//Create datasources
var accountManagerDataSource = Components.classes["component://netscape/rdf/datasource?name=msgaccountmanager"].createInstance();
var folderDataSource = Components.classes["component://netscape/rdf/datasource?name=mailnewsfolders"].createInstance();
var messageDataSource = Components.classes["component://netscape/rdf/datasource?name=mailnewsmessages"].createInstance();
var messageViewDataSource = Components.classes["component://netscape/rdf/datasource?name=mail-messageview"].createInstance();

//Create windows status feedback
var statusFeedback = Components.classes["component://netscape/messenger/statusfeedback"].createInstance();
statusFeedback = statusFeedback.QueryInterface(Components.interfaces.nsIMsgStatusFeedback);

//put this in a function so we can change the position in hierarchy if we have to.
function GetFolderTree()
{
	var folderTree = FindInSidebar(frames[0].frames[0], 'folderTree'); 
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

function GetThreadPane()
{
	return frames[0].frames[1].frames[0];
}

function GetThreadTree()
{
	var threadTree = GetThreadPane().document.getElementById('threadTree');
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

function OpenURL(url)
{
  dump("\n\nOpenURL from XUL\n\n\n");
  messenger.SetWindow(window, statusFeedback);
  messenger.OpenURL(url);
}

function ComposeMessage(type, format)
//type: 0=new message, 1=reply, 2=reply all,
//      3=forward inline, 4=forward as attachment
//
//format: 0=default (use preference), 1=HTML, 2=plain text
{
	dump("\nComposeMessage from XUL\n");
	var uri = null;

	if (! msgComposeService)
	{
		dump("### msgComposeService is invalid\n");
		return;
	}
	
	if (type == 0) //new message
	{
		msgComposeService.OpenComposeWindow(null, null, 0, format, null);
		return;
	}
		
	var tree = GetThreadTree();
	if (tree)
	{
		var nodeList = tree.selectedItems;
		var appCore = FindMessenger();
		if (appCore)
			appCore.SetWindow(window, statusFeedback);
			
		var object = null;
	
		if (nodeList && nodeList.length > 0)
		{
			uri = "";
			for (var i = 0; i < nodeList.length && i < 8; i ++)
			{	
				dump('i = '+ i);
				dump('\n');				
				if (type == 1 || type == 2) //reply or reply all
				{
					if (appCore)
						object = appCore.GetRDFResourceForMessage(tree, nodeList); //temporary
					msgComposeService.OpenComposeWindow(null, nodeList[i].getAttribute('id'), type, format, object);
				}
				else
				{
					if (i) 
						uri += " "
					uri += nodeList[i].getAttribute('id');
				}
			}
			
			if (type == 3 || type == 4) //forward
			{
				if (appCore)
					object = appCore.GetRDFResourceForMessage(tree, nodeList); //temporary
				msgComposeService.OpenComposeWindow(null, uri, type, format, object);
			}
		}
		else
			dump("### nodeList is invalid\n");
	}
	else
		dump("### tree is invalid\n");
}

function NewMessage()
{

  dump("\n\nnewMsg from XUL\n\n\n");
  ComposeMessage(0, 0);
}

function GetNewMessages()
{
	var folderTree = GetFolderTree();; 
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
  if(uri)
	  ChangeFolderByURI(uri);
}

function ChangeFolderByURI(uri)
{
  var folder = GetThreadTreeFolder();
  var beforeTime = new Date();
  folder.setAttribute('ref', uri);
  var afterTime = new Date();
  var timeToLoad = (afterTime.getTime() - beforeTime.getTime())/1000;
  if(showPerformance)
	  dump("Time to load " + uri + " is " +  timeToLoad + " seconds\n");
}

function SortThreadPane(column, sortKey)
{
    var threadPane = GetThreadPane();

	var node = threadPane.document.getElementById(column);
	if(!node)
		return false;

	var rdfCore = XPAppCoresManager.Find("RDFCore");
	if (!rdfCore)
	{
		rdfCore = new RDFCore();
		if (!rdfCore)
		{
			return(false);
		}

		rdfCore.Init("RDFCore");

	}

	// sort!!!
	sortDirection = "ascending";
    var currentDirection = node.getAttribute('sortDirection');
    if (currentDirection == "ascending")
            sortDirection = "descending";
    else if (currentDirection == "descending")
            sortDirection = "ascending";
    else    sortDirection = "ascending";

    rdfCore.doSort(node, sortKey, sortDirection);

    return(true);


}

var prefwindow = null;

function MsgPreferences()
{
    if (!prefwindow) {
        prefwindow = Components.classes['component://netscape/prefwindow'].createInstance(Components.interfaces.nsIPrefWindow);
    }

    prefwindow.showWindow("navigator.js", window, "chrome://pref/content/pref-mailnews.xul");
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

function RefreshThreadTreeView()
{
	var currentFolder = GetThreadTreeFolder();  
	var currentFolderID = currentFolder.getAttribute('ref');
	currentFolder.setAttribute('ref', currentFolderID);
}

function ToggleTwisty(treeItem)
{

	var openState = treeItem.getAttribute('open');
	if(openState == 'true')
	{
		treeItem.removeAttribute('open');
	}
	else
	{
		treeItem.setAttribute('open', 'true');
	}
}

function ToggleMessageRead(treeItem)
{

	var tree = GetThreadTree();
	var status = treeItem.getAttribute('Status');
	var unread = (status == "") || (status == "new");
	messenger.MarkMessageRead(tree.database, treeItem, unread);
}

function ThreadPaneSelectionChange(selectedElement)
{
	var tree = GetThreadTree();
	
	var selArray = tree.selectedItems;
	dump('In ThreadPaneSelectionChange().  Num Selected Items = ' + selArray.length);
	dump('\n');
	if ( selArray && (selArray.length == 1) )
		LoadMessage(selArray[0]);
	else
		ClearMessagePane();

}

function ClearMessagePane()
{
	messenger.OpenURL("about:blank");	

}

function GoNextMessage()
{
	var tree = GetThreadTree();
	
	var selArray = tree.selectedItems;
	if ( selArray && (selArray.length == 1) )
	{
		var nextMessage = GetNextMessage(selArray[0]);
		if(nextMessage)
		{
			tree.clearItemSelection();
			tree.selectItem(nextMessage);
		}
	}
}

function GoNextUnreadMessage()
{
	var tree = GetThreadTree();
	
	var selArray = tree.selectedItems;
	if ( selArray && (selArray.length == 1) )
	{
		var nextMessage = GetNextUnreadMessage(selArray[0]);
		if(nextMessage)
		{
			tree.clearItemSelection();
			tree.selectItem(nextMessage);
		}
	}
}
function GetNextMessage(currentMessage)
{
	var nextMessage = currentMessage.nextSibling;
	if(!nextMessage)
	{
		dump('We need to start from the top\n');
		var parent = currentMessage.parentNode;
		nextMessage = parent.firstChild;
		if(nextMessage == currentMessage)
			nextMessage = null;
	}

	if(nextMessage)
	{
		var id = nextMessage.getAttribute('id');
		dump(id + '\n');
	}
	else
		dump('No next message\n');
	return nextMessage;

}

function GetNextUnreadMessage(currentMessage)
{
	var foundMessage = false;

	
	var nextMessage = currentMessage.nextSibling;
	while(nextMessage)
	{
		var status = nextMessage.getAttribute('Status');
		dump('status = ' + status);
		dump('\n');
		if(status == '' || status == 'New')
			break;
		nextMessage = nextMessage.nextSibling;
	}

	if(!nextMessage)
	{
		dump('We need to start from the top\n');
		var parent = currentMessage.parentNode;
		nextMessage = parent.firstChild;
		while(nextMessage)
		{
			if(nextMessage == currentMessage)
			{
				nextMessage = null;
				break;
			}
			var status = nextMessage.getAttribute('Status');
			dump('status = ' + status);
			dump('\n');
			if(status == '' || status == 'New')
				break;
			nextMessage = nextMessage.nextSibling;
		}

	}


	if(nextMessage)
	{
		var id = nextMessage.getAttribute('id');
		dump(id + '\n');
	}
	else
		dump('No next message\n');
	return nextMessage;

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

function AddDataSources()
{

	//to move menu item
	accountManagerDataSource = accountManagerDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	folderDataSource = folderDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
	var moveMenu = document.getElementById('moveMenu');
	moveMenu.database.AddDataSource(accountManagerDataSource);
	moveMenu.database.AddDataSource(folderDataSource);
	moveMenu.setAttribute('ref', 'msgaccounts:/');

	//to copy menu item
	var copyMenu = document.getElementById('copyMenu');
	copyMenu.database.AddDataSource(accountManagerDataSource);
	copyMenu.database.AddDataSource(folderDataSource);
	copyMenu.setAttribute('ref', 'msgaccounts:/');

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

function OnLoadFolderPane(folderTree)
{
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


}

