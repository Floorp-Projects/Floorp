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

var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var prefs = Components.classes['component://netscape/preferences'].getService();
prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
var showPerformance = prefs.GetBoolPref('mail.showMessengerPerformance');



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
  
  if (uri)
	  ChangeFolderByURI(uri);
}

function ChangeFolderByURI(uri)
{
  var resource = RDF.GetResource(uri);
  var msgfolder =
      resource.QueryInterface(Components.interfaces.nsIMsgFolder);
  window.title = "Netscape: " + msgfolder.name + " on " +
      msgfolder.server.prettyName;

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
	var node = document.getElementById(column);
	if(!node)
		return false;

	return SortColumn(node, sortKey);


}

function SortFolderPane(column, sortKey)
{
	var node = FindInSidebar(frames[0].frames[0], column)
	if(!node)
		return false;
	return SortColumn(node, sortKey);
}

function SortColumn(node, sortKey)
{

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
			}
		}
	}

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
	var flagged = (flaggedValue =="flagged");
	messenger.MarkMessageFlagged(tree.database, treeItem, !flagged);
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
	var threadColumn = document.getElementById('ThreadColumnHeader');
	if(threadColumn)
	{
		var currentView = threadColumn.getAttribute('currentView');
		if(currentView== 'threaded')
		{
			ShowThreads(false);
		}
		else if(currentView == 'unthreaded')
		{
			ShowThreads(true);
		}
		RefreshThreadTreeView();
	}
}

function ShowThreads(showThreads)
{
	dump('in showthreads\n');
	var view = messageViewDataSource.QueryInterface(Components.interfaces.nsIMessageView);
	if(view)
	{
		view.SetShowThreads(showThreads);
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

