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
        if (msgfolder && (msgfolder.parent || msgfolder.isServer))
		  return msgfolder;
	}
	catch (ex) {
		//dump("failed to get the folder resource\n");
	}
	return null;
}

function GetResourceFromUri(uri)
{
	var RDF = Components.classes['@mozilla.org/rdf/rdf-service;1'].getService();
	RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
        var resource = RDF.GetResource(uri);

	return resource;
}  

function DoRDFCommand(dataSource, command, srcArray, argumentArray)
{
	var commandResource = RDF.GetResource(command);
	if(commandResource) {
      try {
		dataSource.DoCommand(srcArray, commandResource, argumentArray);
      }
      catch(e) { 
        if (command == "http://home.netscape.com/NC-rdf#NewFolder") {
          throw(e); // so that the dialog does not automatically close.
        }
        dump("Exception : In mail commands\n");
      }
    }
}

function ConvertMessagesToResourceArray(messages,  resourceArray)
{
    dump("fix or remove this\n");
    // going away...
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
		    var folderArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
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
    dump("fix or remove this\n");
    // going away...
}

function CopyMessages(compositeDataSource, srcFolder, destFolder, messages, isMove)
{
    dump("fix or remove this\n");
    // going away...
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

function GetNextNMessages(folder)
{
	if (folder) {
		var newsFolder = folder.QueryInterface(Components.interfaces.nsIMsgNewsFolder);
		if (newsFolder) {
			newsFolder.getNextNMessages(msgWindow);
		}
	}
}

// type is a nsIMsgCompType and format is a nsIMsgCompFormat
function ComposeMessage(type, format, folder, messageArray) 
{
	var msgComposeType = Components.interfaces.nsIMsgCompType;
	var identity = null;
	var newsgroup = null;
	var server;

	//dump("ComposeMessage folder="+folder+"\n");
	try 
	{
		if(folder)
		{
			// get the incoming server associated with this uri
			server = folder.server;

			// if they hit new or reply and they are reading a newsgroup
			// turn this into a new post or a reply to group.
      if (!folder.isServer && server.type == "nntp" && type == msgComposeType.New)
			{
        type = msgComposeType.NewsPost;
        newsgroup = server.hostName + "/" + folder.name; 
			}
      identity = getIdentityForServer(server);
      // dump("identity = " + identity + "\n");
		}
	}
	catch (ex) 
	{
        	dump("failed to get an identity to pre-select: " + ex + "\n");
	}

	//dump("\nComposeMessage from XUL: " + identity + "\n");
	var uri = null;

	if (! msgComposeService)
	{
		dump("### msgComposeService is invalid\n");
		return;
	}
	
	if (type == msgComposeType.New) //new message
	{
		//dump("OpenComposeWindow with " + identity + "\n");
		msgComposeService.OpenComposeWindow(null, null, type, format, identity, msgWindow);
		return;
	}
    else if (type == msgComposeType.NewsPost) 
	{
		//dump("OpenComposeWindow with " + identity + " and " + newsgroup + "\n");
		msgComposeService.OpenComposeWindow(null, newsgroup, type, format, identity, msgWindow);
		return;
	}
		
	messenger.SetWindow(window, msgWindow);
			
	var object = null;
	
	if (messageArray && messageArray.length > 0)
	{
		uri = "";
		for (var i = 0; i < messageArray.length && i < 8; i ++)
		{	
			var messageUri = messageArray[i];
      //dump("XXX messageUri in ComposeMessage = " + messageUri + "\n");
			//dump('i = '+ i);
			//dump('\n');				
			if (type == msgComposeType.Reply || type == msgComposeType.ReplyAll || type == msgComposeType.ForwardInline ||
				type == msgComposeType.ReplyToGroup || type == msgComposeType.ReplyToSender || 
				type == msgComposeType.ReplyToSenderAndGroup ||
				type == msgComposeType.Template || type == msgComposeType.Draft)
			{
				msgComposeService.OpenComposeWindow(null, messageUri, type, format, identity, msgWindow);
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
			msgComposeService.OpenComposeWindow(null, uri, type, format, identity, msgWindow);
		}
	}
	else
		dump("### nodeList is invalid\n");
}

function CreateNewSubfolder(chromeWindowURL, preselectedMsgFolder,
                            dualUseFolders, callBackFunctionName)
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
				"chrome,titlebar,modal",
				{preselectedURI:preselectedURI,
	                         dualUseFolders:dualUseFolders,
	                         okCallback:callBackFunctionName});
}

function NewFolder(name,uri)
{
	//dump("uri,name = " + uri + "," + name + "\n");
	if (uri && (uri != "") && name && (name != "")) {
		var selectedFolderResource = RDF.GetResource(uri);
		//dump("selectedFolder = " + uri + "\n");
		var compositeDataSource = GetCompositeDataSource("NewFolder");

	    var folderArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
	    var nameArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);

		folderArray.AppendElement(selectedFolderResource);

		var nameLiteral = RDF.GetLiteral(name);
		nameArray.AppendElement(nameLiteral);
		DoRDFCommand(compositeDataSource, "http://home.netscape.com/NC-rdf#NewFolder", folderArray, nameArray);

	}
	else {
		dump("no name or nothing selected\n");
	}
}

function UnSubscribe(folder)
{
	// Unsubscribe the current folder from the newsserver, this assumes any confirmation has already
	// been made by the user  SPL
    
	var server = folder.server;
	var subscribableServer =  server.QueryInterface(Components.interfaces.nsISubscribableServer);       
	subscribableServer.unsubscribe(folder.name);
	subscribableServer.commitSubscribeChanges();
}   

function Subscribe(preselectedMsgFolder)
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
					  "subscribe", "chrome,modal,titlebar,resizable=yes",
				{preselectedURI:preselectedURI,
				 okCallback:SubscribeOKCallback});
}

function SubscribeOKCallback(changeTable)
{
	for (var serverURI in changeTable) {
	    var folder = GetMsgFolderFromUri(serverURI);
	    var server = folder.server;
	    var subscribableServer =
            server.QueryInterface(Components.interfaces.nsISubscribableServer);

        for (var name in changeTable[serverURI]) {
		    if (changeTable[serverURI][name] == true) {
                try {
			        subscribableServer.subscribe(name);
                }
                catch (ex) {
                    dump("failed to subscribe to " + name + ": " + ex + "\n");
                }
		    }
		    else if (changeTable[serverURI][name] == false) {
                try {
			        subscribableServer.unsubscribe(name);
                }
                catch (ex) {
                    dump("failed to unsubscribe to " + name + ": " + ex + "\n");
                }
            }
		    else {
			    // no change
		    }
        }

        try {
            subscribableServer.commitSubscribeChanges();
        }
        catch (ex) {
            dump("failed to commit the changes: " + ex + "\n");
        }
    }
}

function SaveAsFile(uri)
{
	if (uri) messenger.saveAs(uri, true, null, msgWindow);
}

function SaveAsTemplate(uri, folder)
{
	if (uri) {
		var identity = getIdentityForServer(folder.server);
		messenger.saveAs(uri, false, identity, msgWindow);
	}
}

function MarkSelectedMessagesRead(markRead)
{
    if (markRead) {
        gDBView.doCommand(nsMsgViewCommandType.markMessagesRead);
    }
    else {
        gDBView.doCommand(nsMsgViewCommandType.markMessagesUnread);
    }
}

function MarkSelectedMessagesFlagged(markFlagged)
{
    if (markFlagged) {
        gDBView.doCommand(nsMsgViewCommandType.flagMessages);
    }
    else {
        gDBView.doCommand(nsMsgViewCommandType.unflagMessages);
    }
}

function MarkAllMessagesRead(compositeDataSource, folder)
{
  var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
  var folderArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
  folderArray.AppendElement(folderResource);

  DoRDFCommand(compositeDataSource, "http://home.netscape.com/NC-rdf#MarkAllMessagesRead", folderArray, null);
}

function DownloadFlaggedMessages(compositeDataSource, folder)
{
    dump("fix DownloadFlaggedMessages()\n");
}

function DownloadSelectedMessages(compositeDataSource, messages, markFlagged)
{
    dump("fix DownloadSelectedMessages()\n");
}

function MarkThreadAsRead(message)
{
  gDBView.doCommand(nsMsgViewCommandType.markThreadRead);
}

function ViewPageSource(messages)
{
	var url;
	var uri;
	var mailSessionContractID      = "@mozilla.org/messenger/services/session;1";

	var numMessages = messages.length;

	if (numMessages == 0)
	{
		dump("MsgViewPageSource(): No messages selected.\n");
		return false;
	}

	// First, get the mail session
	var mailSession = Components.classes[mailSessionContractID].getService();
	if (!mailSession)
		return false;

	mailSession = mailSession.QueryInterface(Components.interfaces.nsIMsgMailSession);
	if (!mailSession)
		return false;

	for(var i = 0; i < numMessages; i++)
	{
		uri = messages[i];
  
		// Now, we need to get a URL from a URI
		url = mailSession.ConvertMsgURIToMsgURL(uri, msgWindow);
		if (url) {
            // XXX what if there already is a "?", like "?part=0"
            // XXX shouldn't this be "&header=src" in that case?
			url += "?header=src";
        }
    
		// Use a browser window to view source
		window.openDialog( getBrowserURL(),
						   "_blank",
						   "scrollbars,resizable,chrome,dialog=no",
							url,
							"view-source" );
	}
        return true;
}
