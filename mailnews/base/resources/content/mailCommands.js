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


function DoRDFCommand(dataSource, command, srcArray, argumentArray)
{

	var commandResource = RDF.GetResource(command);
	if(commandResource)
		dataSource.DoCommand(srcArray, commandResource, argumentArray);
}

//Converts an array of messages into an nsISupportsArray of resources. 
//messages:  array of messages that needs to be converted
//resourceArray: nsISupportsArray in which the resources should be put.  If it's null a new one will be created.
//returns the resource array
function ConvertMessagesToResourceArray(messages,  resourceArray)
{
	if(!resourceArray)
	    resourceArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);

    for (var i=0; i<messages.length; i++) {
		var messageResource = messages[i].QueryInterface(Components.interfaces.nsIRDFResource);
        resourceArray.AppendElement(messageResource);
    }

    return resourceArray;
}

function GetNewMessages(selectedFolders, compositeDataSource)
{
	var numFolders = selectedFolders.length;
	if(numFolders > 0)
	{
		var msgFolder = selectedFolders[0];

		//Whenever we do get new messages, clear the old new messages.
		if(msgFolder && msgFolder.hasNewMessages())
			msgFolder.clearNewMessages();
		
		if(compositeDataSource)
		{
			var folderResource = msgFolder.QueryInterface(Components.interfaces.nsIRDFResource);
		    var folderArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
			folderArray.AppendElement(folderResource);

			DoRDFCommand(compositeDataSource, "http://home.netscape.com/NC-rdf#GetNewMessages", folderArray, null);
		}
	}
	else {
		dump("Nothing was selected\n");
	}
}

function CopyMessages(compositeDataSource, srcFolder, destFolder, messages, isMove)
{

	if(compositeDataSource)
	{
		var destFolderResource = destFolder.QueryInterface(Components.interfaces.nsIRDFResource);
		var folderArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
		folderArray.AppendElement(destFolderResource);

		var argumentArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
		var srcFolderResource = destFolder.QueryInterface(Components.interfaces.nsIRDFResource);
		argumentArray.AppendElement(srcFolderResource);
		ConvertMessagesToResourceArray(messages, argumentArray);
		
		DoRDFCommand(compositeDataSource, "http://home.netscape.com/NC-rdf#Copy", folderArray, argumentArray);

	}
}

function getIdentityForServer(server)
{
    var identity = null;
    if(server) {
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
 function ComposeMessage(type, format, folder, messageArray) //type is a nsIMsgCompType and format is a nsIMsgCompFormat
{
	var msgComposeType = Components.interfaces.nsIMsgCompType;
	var identity = null;
	var newsgroup = null;
	var server;

	try 
	{
		server = folder.server;
		if(folder)
		{
			// get the incoming server associated with this uri
			var server = loadedFolder.server;

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

   					if (loadedFolder.isServer) 
    						newsgroup = "";
    					else 
          					newsgroup = folder.name; 
				    }
			}
		}
        
        identity = getIdentityForServer(server);

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
		
	messenger.SetWindow(window, msgWindow);
			
	var object = null;
	
	if (messageArray && messageArray.length > 0)
	{
		uri = "";
		for (var i = 0; i < messageArray.length && i < 8; i ++)
		{	
			var messageResource = messageArray[i].QueryInterface(Components.interfaces.nsIRDFResource);
			var messageUri = messageResource.Value;

			dump('i = '+ i);
			dump('\n');				
			if (type == msgComposeType.Reply || type == msgComposeType.ReplyAll || type == msgComposeType.ForwardInline ||
				type == msgComposeType.ReplyToGroup || type == msgComposeType.ReplyToSenderAndGroup ||
				type == msgComposeType.Template || type == msgComposeType.Draft)
			{
				msgComposeService.OpenComposeWindow(null, messageUri, type, format, identity);
			}
			else
			{
				if (i) 
					uri += ","
				uri += messageUri;
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

function CreateNewSubfolder(chromeWindowURL,windowTitle, preselectedMsgFolder)
{
	var preselectedURI;

	if(preselectedMsgFolder)
	{
		var preselectedFolderResource = preselectedMsgFolder.QueryInterface(Components.interfaces.nsIRDFResource);
		if(preselectedFolderResource)
			preselectedURI = preselectedFolderResource.Value;
		dump("preselectedURI = " + preselectedURI + "\n");
	}

	var dialog = window.openDialog(
				chromeWindowURL,
				"",
				"chrome,modal",
				{preselectedURI:preselectedURI, title:windowTitle,
				okCallback:NewFolder});
}

function NewFolder(name,uri)
{
	//dump("uri,name = " + uri + "," + name + "\n");
	if (uri && (uri != "") && name && (name != "")) {
		var selectedFolderResource = RDF.GetResource(uri);
		//dump("selectedFolder = " + uri + "\n");
		var compositeDataSource = GetCompositeDataSource("NewFolder");

	    var folderArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
	    var nameArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);

		folderArray.AppendElement(selectedFolderResource);

		var nameLiteral = RDF.GetLiteral(name);
		nameArray.AppendElement(nameLiteral);
		DoRDFCommand(compositeDataSource, "http://home.netscape.com/NC-rdf#NewFolder", folderArray, nameArray);

	}
	else {
		dump("no name or nothing selected\n");
	}
}

function Subscribe(windowTitle, preselectedMsgFolder)
{
	var preselectedURI;

	if(preselectedMsgFolder)
	{
		var preselectedFolderResource = preselectedMsgFolder.QueryInterface(Components.interfaces.nsIRDFResource);
		if(preselectedFolderResource)
			preselectedURI = preselectedFolderResource.Value;
		dump("preselectedURI = " + preselectedURI + "\n");
	}

	window.openDialog("chrome://messenger/content/subscribe.xul",
					  "subscribe", "chrome,modal",
				{preselectedURI:preselectedURI, title:windowTitle,
				okCallback:SubscribeOKCallback});
}

function SubscribeOKCallback(serverURI, changeTable)
{
	dump("in SubscribeOKCallback(" + serverURI +")\n");
	dump("change table = " + changeTable + "\n");
	
	for (var name in changeTable) {
		dump(name + " = " + changeTable[name] + "\n");
		if (changeTable[name] == 1) {
			NewFolder(name,serverURI);
		}
		else if (changeTable[name] == -1) {
			dump("unsuscribe\n");
		}
	}
}

function SaveAsFile(message)
{
	var messageResource = message.QueryInterface(Components.interfaces.nsIRDFResource);
	var uri = messageResource.Value;
	//dump (uri);
	if (uri)
		messenger.saveAs(uri, true, null, msgWindow);
}

function SaveAsTemplate(message, folder)
{
	var messageResource = message.QueryInterface(Components.interfaces.nsIRDFResource);
	var uri = messageResource.Value;
	// dump (uri);
	if (uri)
    {
		var identity = getIdentityForServer(folder.server);
		messenger.saveAs(uri, false, identity, msgWindow);
	}
}
