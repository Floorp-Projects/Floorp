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


function GetMsgFolderFromUri(uri)
{
	//dump("GetMsgFolderFromUri of " + uri + "\n");
	try {
		var resource = GetResourceFromUri(uri);
		var msgfolder = resource.QueryInterface(Components.interfaces.nsIMsgFolder);
		return msgfolder;
	}
	catch (ex) {
		//dump("failed to get the folder resource\n");
	}
	return null;
}

function GetResourceFromUri(uri)
{
	var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
	RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
        var resource = RDF.GetResource(uri);

	return resource;
}  

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
		if(msgFolder) 
		{
			var nsIMsgFolder = Components.interfaces.nsIMsgFolder;
			msgFolder.biffState = nsIMsgFolder.nsMsgBiffState_NoMail;

			if (msgFolder.hasNewMessages)
				msgFolder.clearNewMessages();
		}
		
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

function DeleteMessages(compositeDataSource, srcFolder, messages, reallyDelete)
{

	var srcFolderResource = srcFolder.QueryInterface(Components.interfaces.nsIRDFResource);
	var folderArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
	folderArray.AppendElement(srcFolderResource);

	var argumentArray = ConvertMessagesToResourceArray(messages, null);

	var command;

	if(reallyDelete)
		command = "http://home.netscape.com/NC-rdf#ReallyDelete"
	else
		command = "http://home.netscape.com/NC-rdf#Delete"

	DoRDFCommand(compositeDataSource, command, folderArray, argumentArray);

}

function CopyMessages(compositeDataSource, srcFolder, destFolder, messages, isMove)
{

	if(compositeDataSource)
	{
		var destFolderResource = destFolder.QueryInterface(Components.interfaces.nsIRDFResource);
		var folderArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
		folderArray.AppendElement(destFolderResource);

		var argumentArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
		var srcFolderResource = srcFolder.QueryInterface(Components.interfaces.nsIRDFResource);
		argumentArray.AppendElement(srcFolderResource);
		ConvertMessagesToResourceArray(messages, argumentArray);
		
		var command;
		if(isMove)
			command = "http://home.netscape.com/NC-rdf#Move"
		else
			command = "http://home.netscape.com/NC-rdf#Copy";

		DoRDFCommand(compositeDataSource, command, folderArray, argumentArray);

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

function SendUnsentMessages(folder)
{
	if(folder)
	{	
		var identity = getIdentityForServer(folder.server);
		messenger.SendUnsentMessages(identity);
	}

}

 function ComposeMessage(type, format, folder, messageArray) //type is a nsIMsgCompType and format is a nsIMsgCompFormat
{
	var msgComposeType = Components.interfaces.nsIMsgCompType;
	var identity = null;
	var newsgroup = null;
	var server;

	dump("ComposeMessage folder="+folder+"\n");
	try 
	{
		if(folder)
		{
			// get the incoming server associated with this uri
			server = folder.server;

			// if they hit new or reply and they are reading a newsgroup
			// turn this into a new post or a reply to group.
			if (server.type == "nntp")
			{
		        if (type == msgComposeType.New)
		        {
				    type = msgComposeType.NewsPost;
   					if (folder.isServer) 
   						newsgroup = "";
   					else 
       					newsgroup = server.hostName + "/" + folder.name; 
			    }
			}
	        identity = getIdentityForServer(server);
		    // dump("identity = " + identity + "\n");

		}
        

	}
	catch (ex) 
	{
        	dump("failed to get an identity to pre-select: " + ex + "\n");
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
					  "subscribe", "chrome,modal,resizable=yes",
				{preselectedURI:preselectedURI, title:windowTitle,
				okCallback:SubscribeOKCallback});
}

function SubscribeOKCallback(serverURI, changeTable)
{
	//dump("in SubscribeOKCallback(" + serverURI +")\n");
	//dump("change table = " + changeTable + "\n");
	
	var folder = GetMsgFolderFromUri(serverURI);
	var server = folder.server;
	var subscribableServer =
        server.QueryInterface(Components.interfaces.nsISubscribableServer);

	for (var name in changeTable) {
		//dump(name + " = " + changeTable[name] + "\n");
		if (changeTable[name] == true) {
			//dump("from js, subscribe to " + name +"\n");
			subscribableServer.subscribe(name);
		}
		else if (changeTable[name] == false) {
			//dump("from js, unsubscribe to " + name +"\n");
			subscribableServer.unsubscribe(name);
		}
		else {
			//dump("no change to " + name + "\n");
		}
	}
    try {
        var imapServer =
            server.QueryInterface(Components.interfaces.nsIImapIncomingServer);
        if (imapServer)
            imapServer.reDiscoverAllFolders();
    }
    catch (ex) {
        //dump("*** not an imap server\n");
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

function MarkMessagesRead(compositeDataSource, messages, markRead)
{

	var messageResourceArray = ConvertMessagesToResourceArray(messages, null);
	var command;

	if(markRead)
		command = "http://home.netscape.com/NC-rdf#MarkRead";
	else
		command = "http://home.netscape.com/NC-rdf#MarkUnread";

	DoRDFCommand(compositeDataSource, command, messageResourceArray, null);

}

function MarkMessagesFlagged(compositeDataSource, messages, markFlagged)
{

	var messageResourceArray = ConvertMessagesToResourceArray(messages, null);
	var command;

	if(markFlagged)
		command = "http://home.netscape.com/NC-rdf#MarkFlagged";
	else
		command = "http://home.netscape.com/NC-rdf#MarkUnflagged";

	DoRDFCommand(compositeDataSource, command, messageResourceArray, null);

}

function MarkAllMessagesRead(compositeDataSource, folder)
{

	var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
	var folderResourceArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
	folderResourceArray.AppendElement(folderResource);

	DoRDFCommand(compositeDataSource, "http://home.netscape.com/NC-rdf#MarkAllMessagesRead", folderResourceArray, null);
}

function MarkThreadAsRead(compositeDataSource, message)
{

	if(message)
	{
		var folder = message.msgFolder
		if(folder)
		{
			var thread = folder.getThreadForMessage(message);
			if(thread)
			{
				var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
				var folderResourceArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
				folderResourceArray.AppendElement(folderResource);

				var argumentArray = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);
				argumentArray.AppendElement(thread);

				DoRDFCommand(compositeDataSource, "http://home.netscape.com/NC-rdf#MarkThreadRead", folderResourceArray, argumentArray);
			}
		}
	}
}


function ViewPageSource(messages)
{
	var url;
	var uri;
	var mailSessionProgID      = "component://netscape/messenger/services/session";

	var numMessages = messages.length;

	if (numMessages == 0)
	{
		dump("MsgViewPageSource(): No messages selected.\n");
		return false;
	}

	// First, get the mail session
	var mailSession = Components.classes[mailSessionProgID].getService();
	if (!mailSession)
		return false;

	mailSession = mailSession.QueryInterface(Components.interfaces.nsIMsgMailSession);
	if (!mailSession)
		return false;

	for(var i = 0; i < numMessages; i++)
	{
		var messageResource = messages[i].QueryInterface(Components.interfaces.nsIRDFResource);
		uri = messageResource.Value;
  
		// Now, we need to get a URL from a URI
		url = mailSession.ConvertMsgURIToMsgURL(uri, msgWindow);
		if (url)
			url += "?header=src";

		// Use a browser window to view source
		window.openDialog( getBrowserURL(),
						   "_blank",
						   "chrome,menubar,status,dialog=no,resizable",
							url,
							"view-source" );
	}
}

