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

//put this in a function so we can change the position in hierarchy if we have to.
function GetFolderTree()
{
	var folderTree = frames[0].frames[0].document.getElementById('folderTree'); 
	return folderTree;
}

function FindMessenger()
{
  return messenger;
}

function OpenURL(url)
{
  dump("\n\nOpenURL from XUL\n\n\n");
  messenger.SetWindow(window);
  messenger.OpenURL(url);
}

function ComposeMessage(type, format)
//type: 0=new message, 1=reply, 2=reply all,
//      3=forward inline, 4=forward quoted, 5=forward as attachment
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
		
	var tree = frames[0].frames[1].document.getElementById('threadTree');
	if (tree)
	{
		var nodeList = tree.getElementsByAttribute("selected", "true");
		var appCore = FindMessenger();
		if (appCore)
			appCore.SetWindow(window);
			
		var object = null;
	
		if (nodeList && nodeList.length > 0)
		{
			uri = "";
			for (var i = 0; i < nodeList.length && i < 8; i ++)
			{					
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
			
			if (type >= 3 && type <= 5) //forward
			{
				if (appCore)
					object = appCore.GetRDFResourceForMessage(tree, nodeList); //temporary
				msgComposeService.OpenComposeWindow(null, uri, type, format, object);
			}
		}
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
	var folderTree = frames[0].frames[0].document.getElementById('folderTree'); 
	var selectedFolderList = folderTree.getElementsByAttribute("selected", "true");
	if(selectedFolderList.length > 0)
	{
		var selectedFolder = selectedFolderList[0];
		messenger.GetNewMessages(folderTree.database, selectedFolder);
	}
	else {
		dump("Nothing was selected\n");
	}
}

function MsgAccountManager()
{
    dump('Opening account manager..\n');
    window.openDialog("chrome://messenger/content/AccountManager.xul", "AccountManager", "chrome");
}

function MsgSubscribe()
{
    dump('open subscribe window.\n');
}

function LoadMessage(messageNode)
{
  var uri = messageNode.getAttribute('id');
  dump(uri);
  OpenURL(uri);
}

function ChangeFolderByDOMNode(folderNode)
{
  var uri = folderNode.getAttribute('id');
  dump(uri + "\n");
  ChangeFolderByURI(uri);
}

function ChangeFolderByURI(uri)
{
  var tree = frames[0].frames[1].document.getElementById('threadTree');
  tree.childNodes[5].setAttribute('id', uri);
}

function SortThreadPane(column, sortKey)
{
	var node = frames[0].frames[1].document.getElementById(column);
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

function MsgPreferences()
{
  var prefsCore = XPAppCoresManager.Find("PrefsCore");
  if (!prefsCore) {
    prefsCore = new PrefsCore();
    if (prefsCore) {
      prefsCore.Init("PrefsCore");
    }
  }
  if (prefsCore) {
    prefsCore.ShowWindow(window);
  }
}


function GetSelectedFolderResource()
{
	var folderTree = GetFolderTree();
	var selectedFolderList = folderTree.getElementsByAttribute("selected", "true");
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
