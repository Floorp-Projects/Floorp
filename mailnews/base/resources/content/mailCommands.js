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
        newsgroup = folder.folderURL;
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

    // if the addressbook sidebar panel is open and has focus, get
    // the selected addresses from it
    if (document.commandDispatcher.focusedWindow.document.documentElement.hasAttribute("selectedaddresses"))
      NewMessageToSelectedAddresses(type, format, identity);

    else
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
		for (var i = 0; i < messageArray.length; i ++)
		{	
			var messageUri = messageArray[i];
			if (type == msgComposeType.Reply || type == msgComposeType.ReplyAll || type == msgComposeType.ForwardInline ||
				type == msgComposeType.ReplyToGroup || type == msgComposeType.ReplyToSender || 
				type == msgComposeType.ReplyToSenderAndGroup ||
				type == msgComposeType.Template || type == msgComposeType.Draft)
			{
				msgComposeService.OpenComposeWindow(null, messageUri, type, format, identity, msgWindow);
        //limit the number of new compose windows to 8. Why 8? I like that number :-)
        if (i == 7)
          break;
			}
			else
			{
				if (i) 
					uri += ","
				uri += messageUri;
			}
		}
		if (type == msgComposeType.ForwardAsAttachment)
			msgComposeService.OpenComposeWindow(null, uri, type, format, identity, msgWindow);
	}
	else
		dump("### nodeList is invalid\n");
}

function NewMessageToSelectedAddresses(type, format, identity) {
  var abSidebarPanel = document.commandDispatcher.focusedWindow;
  var abResultsTree = abSidebarPanel.document.getElementById("abResultsTree");
  var abResultsBoxObject = abResultsTree.treeBoxObject;
  var abView = abResultsBoxObject.view;
  abView = abView.QueryInterface(Components.interfaces.nsIAbView);
  var addresses = abView.selectedAddresses;
  var params = Components.classes["@mozilla.org/messengercompose/composeparams;1"].createInstance(Components.interfaces.nsIMsgComposeParams);
  if (params) {
    params.type = type;
    params.format = format;
    params.identity = identity;
    var composeFields = Components.classes["@mozilla.org/messengercompose/composefields;1"].createInstance(Components.interfaces.nsIMsgCompFields);
    if (composeFields) {
      var addressList = "";
      for (var i = 0; i < addresses.Count(); i++) {
        addressList = addressList + (i > 0 ? ",":"") + addresses.QueryElementAt(i,Components.interfaces.nsISupportsString).data;
      }
      composeFields.to = addressList;
      params.composeFields = composeFields;
      msgComposeService.OpenComposeWindowWithParams(null, params);
    }
  }
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
	    var folder = GetMsgFolderFromUri(serverURI, true);
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
  gDBView.doCommand(markRead ? nsMsgViewCommandType.markMessagesRead : nsMsgViewCommandType.markMessagesUnread);
}

function MarkSelectedMessagesFlagged(markFlagged)
{
  gDBView.doCommand(markFlagged ? nsMsgViewCommandType.flagMessages : nsMsgViewCommandType.unflagMessages);
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

function ViewPageSource(messages)
{
	var numMessages = messages.length;

	if (numMessages == 0)
	{
		dump("MsgViewPageSource(): No messages selected.\n");
		return false;
	}

    try {
        // First, get the mail session
        const mailSessionContractID = "@mozilla.org/messenger/services/session;1";
        const nsIMsgMailSession = Components.interfaces.nsIMsgMailSession;
        var mailSession = Components.classes[mailSessionContractID].getService(nsIMsgMailSession);

        var mailCharacterSet = "charset=" + msgWindow.mailCharacterSet;

        for (var i = 0; i < numMessages; i++)
        {
            // Now, we need to get a URL from a URI
            var url = mailSession.ConvertMsgURIToMsgURL(messages[i], msgWindow);

            window.openDialog( "chrome://navigator/content/viewSource.xul",
                               "_blank",
                               "scrollbars,resizable,chrome,dialog=no",
                               url,
                               mailCharacterSet);
        }
        return true;
    } catch (e) {
        // Couldn't get mail session
        return false;
    }
}

function doHelpButton() 
{
    openHelp("mail-offline-items");
}

// XXX The following junkmail code might be going away or changing

const NS_BAYESIANFILTER_CONTRACTID = "@mozilla.org/messenger/filter-plugin;1?name=bayesianfilter";
const nsIJunkMailPlugin = Components.interfaces.nsIJunkMailPlugin;

var gJunkmailComponent;

function getJunkmailComponent()
{
    if (!gJunkmailComponent) {
        gJunkmailComponent = Components.classes[NS_BAYESIANFILTER_CONTRACTID].getService(nsIJunkMailPlugin);
        // gJunkmailComponent.initComponent();
        // who calls init method?
    }
}

function analyze(aMessage, aNextFunction)
{
    var listener = {
        onMessageClassified: function(aMsgURL, aClassification)
        {
            dump(aMsgURL + ' is ' + (aClassification == nsIJunkMailPlugin.JUNK ? 'JUNK' : 'GOOD') + '\n');
            // XXX TODO, make the cut off 50, like in nsMsgSearchTerm.cpp
            var score = (aClassification == nsIJunkMailPlugin.JUNK ? "100" : "0");
            aMessage.setStringProperty("junkscore", score);
            aNextFunction();
        }
    };

    // XXX TODO jumping through hoops here.
    var messageURI = aMessage.folder.generateMessageURI(aMessage.messageKey) + "?fetchCompleteMessage=true";
    gJunkmailComponent.classifyMessage(messageURI, listener);
}

function analyzeFolder()
{
    function processNext()
    {
        if (messages.hasMoreElements()) {
            // XXX TODO jumping through hoops here.
            var message = messages.getNext().QueryInterface(Components.interfaces.nsIMsgDBHdr);
            while (!message.isRead) {
                if (!messages.hasMoreElements()) {
                    gJunkmailComponent.batchUpdate = false;
                    return;
                }
                message = messages.getNext().QueryInterface(Components.interfaces.nsIMsgDBHdr);
            }
            analyze(message, processNext);
        }
        else {
            gJunkmailComponent.batchUpdate = false;
        }
    }

    getJunkmailComponent();
    var folder = GetFirstSelectedMsgFolder();
    var messages = folder.getMessages(msgWindow);
    gJunkmailComponent.batchUpdate = true;
    processNext();
}

function analyzeMessages()
{
    function processNext()
    {
        if (counter < messages.length) {
            // XXX TODO jumping through hoops here.
            var messageUri = messages[counter];
            var message = messenger.messageServiceFromURI(messageUri).messageURIToMsgHdr(messageUri);

            ++counter;
            while (!message.isRead) {
                if (counter == messages.length) {
                    dump('[bayesian filter message analysis complete.]\n');
                    gJunkmailComponent.mBatchUpdate = false;
                    return;
                }
                messageUri = messages[counter];
                message = messenger.messageServiceFromURI(messageUri).messageURIToMsgHdr(messageUri);
                ++counter;
            }
            analyze(message, processNext);
        }
        else {
            dump('[bayesian filter message analysis complete.]\n');
            gJunkmailComponent.batchUpdate = false;
        }
    }

    getJunkmailComponent();
    var messages = GetSelectedMessages();
    var counter = 0;
    gJunkmailComponent.batchUpdate = true;
    dump('[bayesian filter message analysis begins.]\n');
    processNext();
}

function writeHash()
{
    getJunkmailComponent();
    gJunkmailComponent.mTable.writeHash();
}

function mark(aMessage, aSpam, aNextFunction)
{
    // XXX TODO jumping through hoops here.
    var score = aMessage.getStringProperty("junkscore");

    var oldClassification = ((score == "100") ? nsIJunkMailPlugin.JUNK :
                             (score == "0") ? nsIJunkMailPlugin.GOOD : nsIJunkMailPlugin.UNCLASSIFIED);
    
    var newClassification = (aSpam ? nsIJunkMailPlugin.JUNK : nsIJunkMailPlugin.GOOD);

    var messageURI = aMessage.folder.generateMessageURI(aMessage.messageKey) + "?fetchCompleteMessage=true";
    
    var listener = (aNextFunction == null ? null :
                    {
                        onMessageClassified: function(aMsgURL, aClassification)
                        {
                            aNextFunction();
                        }
                    });
    
    gJunkmailComponent.setMessageClassification(messageURI, oldClassification, newClassification, listener);
}

function JunkSelectedMessages(setAsJunk)
{
    getJunkmailComponent();
    var messages = GetSelectedMessages();

    // start the batch of messages
    //
    gJunkmailComponent.batchUpdate = true;

    // mark each one
    //
    for ( var msg in messages ) {
        var message = messenger.messageServiceFromURI(messages[msg])
            .messageURIToMsgHdr(messages[msg]);
        mark(message, setAsJunk, null);
    }

    // end the batch (tell the component to write out its data)
    //
    gJunkmailComponent.batchUpdate = false;

    // this actually sets the score on the selected messages
    // so we need to call it after we call mark()
    gDBView.doCommand(setAsJunk ? nsMsgViewCommandType.junk
                      : nsMsgViewCommandType.unjunk);
}

// temporary
function markFolderAsJunk(aSpam)
{
    function processNext()
    {
        if (messages.hasMoreElements()) {
            // Pffft, jumping through hoops here.
            var message = messages.getNext().QueryInterface(Components.interfaces.nsIMsgDBHdr);
            mark(message, aSpam, processNext);

            // now set the score
            // XXX TODO invalidate the row
            if (aSpam)
              message.setStringProperty("junkscore","100");
            else
              message.setStringProperty("junkscore","0");
        }
        else {
            dump('[folder marking complete.]\n');
            gJunkmailComponent.batchUpdate = false;
        }
    }

    getJunkmailComponent();
    var folder = GetFirstSelectedMsgFolder();
    var messages = folder.getMessages(msgWindow);
    gJunkmailComponent.batchUpdate = true;
    dump('[folder marking starting.]\n');
    processNext();
}
