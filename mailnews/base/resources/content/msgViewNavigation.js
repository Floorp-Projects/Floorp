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

/*  This file contains the js functions necessary to implement view navigation within the 3 pane. */

//NOTE: gMessengerBundle must be defined and set or this Overlay won't work

var commonDialogs = Components.classes["@mozilla.org/appshell/commonDialogs;1"].getService();
commonDialogs = commonDialogs.QueryInterface(Components.interfaces.nsICommonDialogs);
var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);


function FindNextFolder(originalFolderURI)
{
    if (!originalFolderURI) return null;

    var originalFolderResource = RDF.GetResource(originalFolderURI);
    var folder = originalFolderResource.QueryInterface(Components.interfaces.nsIFolder);
    if (!folder) return null;
    dump("folder = " + folder.URI + "\n");

    try {
       var subFolderEnumerator = folder.GetSubFolders();
       var done = false;
       while (!done) {
         var element = subFolderEnumerator.currentItem();
         var currentSubFolder = element.QueryInterface(Components.interfaces.nsIMsgFolder);
         dump("current folder = " + currentSubFolder.URI + "\n");
         if (currentSubFolder.getNumUnread(false /* don't descend */) > 0) {
           dump("if the child has unread, use it.\n");
           return currentSubFolder.URI;
         }
         else if (currentSubFolder.getNumUnread(true /* descend */) > 0) {
           dump("if the child doesn't have any unread, but it's children do, recurse\n");
           return FindNextFolder(currentSubFolder.URI);
         }

         try {
           subFolderEnumerator.next();
         } 
         catch (ex) {
           done=true;
         }
      } // while
    }
    catch (ex) {
        // one way to get here is if the folder has no sub folders
    }
 
    if (folder.parent && folder.parent.URI) {
      dump("parent = " + folder.parent.URI + "\n");
      return FindNextFolder(folder.parent.URI);
    }
    else {
      dump("no parent\n");
    }
    return null;
}

function ScrollToFirstNewMessage()
{
  dump("XXX ScrollToFirstNewMessage needs to be rewritten.\n");
/*
	var tree = GetThreadTree();
	var treeFolder = GetThreadTreeFolder();

	var folderURI = treeFolder.getAttribute('ref');
	var folderResource = RDF.GetResource(folderURI);
	var folder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
	var hasNew = folder.hasNewMessages;
	if(hasNew)
	{
		var newMessage = folder.firstNewMessage;

		if(messageView.showThreads)
		{
			//if we're in thread mode, then we need to actually make sure the message is showing.
			var topLevelMessage = GetTopLevelMessageForMessage(newMessage, folder);
			var topLevelResource = topLevelMessage.QueryInterface(Components.interfaces.nsIRDFResource);
			var topLevelURI = topLevelResource.Value;
			var topElement = document.getElementById(topLevelURI);
			if(topElement)
			{
//				msgNavigationService.OpenTreeitemAndDescendants(topElement);
			}

		}
		
		var messageResource = newMessage.QueryInterface(Components.interfaces.nsIRDFResource);
		var messageURI = messageResource.Value;
		var messageElement = document.getElementById(messageURI);

		if(messageElement)
		{
			tree.ensureElementIsVisible(messageElement); 
		}
	}
  */
}

function GetTopLevelMessageForMessage(message, folder)
{
	if(!folder)
		folder = message.msgFolder;

	var thread = folder.getThreadForMessage(message);
    var outIndex = new Object();
	var rootHdr = thread.GetRootHdr(outIndex);

	var topMessage = folder.createMessageFromMsgDBHdr(rootHdr);

	return topMessage;

}


function CrossFolderNavigation (type, supportsFolderPane )
{
  if (type != nsMsgNavigationType.nextUnreadMessage) 
  {
      // only do cross folder navigation for "next unread message"
      return nsnull;
  }

  var nextMode = pref.GetIntPref("mailnews.nav_crosses_folders");
  // 0: "next" goes to the next folder, without prompting
  // 1: "next" goes to the next folder, and prompts (the default)
  // 2: "next" does nothing when there are no unread messages

  // not crossing folders, don't find next
  if (nextMode == 2) return;

  var originalFolderURI = gDBView.msgFolder.URI;
  var nextFolderURI = null;
  var done = false;
  var startAtURI = originalFolderURI;
  var i = 0;
  var allServers = accountManager.allServers;
  var numServers = allServers.Count();

  // XXX fix this
  // this will search the originalFolderURI server twice
  while (!done) 
  {
     dump("start looking at " + startAtURI + "\n");
     nextFolderURI = FindNextFolder(startAtURI);
     if (!nextFolderURI) 
     {
       if (i == numServers) 
       {
          // no more servers, we're done
          done = true;
       }
       else 
       {
         // get the uri for the next server and start there
         startAtURI = allServers.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgIncomingServer).serverURI;
         i++;
        }
      }
      else 
      {
         // got a folder with unread messages, start with it
         done = true;    
      }
    }

    if (nextFolderURI && (originalFolderURI != nextFolderURI)) 
    {
      var nextFolderResource = RDF.GetResource(nextFolderURI);
      var nextFolder = nextFolderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
      switch (nextMode) 
      {
        case 0:
            // do this unconditionally
            gNextMessageAfterLoad = true;
            if (supportsFolderPane)
              SelectFolder(nextFolderURI);
            dump("XXX we need code to select the correct type of message, after we load the folder\n");
            break;
        case 1:
            var promptText = gMessengerBundle.getFormattedString("advanceNextPrompt", [ nextFolder.name ], 1); 
            if (commonDialogs.Confirm(window, promptText, promptText)) {
                gNextMessageAfterLoad = true;
                if (supportsFolderPane)
                  SelectFolder(nextFolderURI);
                dump("XXX we need code to select the correct type of message, after we load the folder\n");
            }
            break;
        default:
            dump("huh?");
            break;
        }
    }

    return nextFolderURI;
}

function GoNextMessage(type, startFromBeginning)
{
  try {
    dump("XXX GoNextMessage(" + type + "," + startFromBeginning + ")\n");

    var outlinerView = gDBView.QueryInterface(Components.interfaces.nsIOutlinerView);
    var outlinerSelection = outlinerView.selection;
    var currentIndex = outlinerSelection.currentIndex;

    dump("XXX outliner selection = " + outlinerSelection + "\n");
    dump("XXX current Index = " + currentIndex + "\n");

    var status = gDBView.navigateStatus(type);

    dump("XXX status = " + status + "\n");

    var resultId = new Object;
    var resultIndex = new Object;
    var threadIndex = new Object;

    gDBView.viewNavigate(type, resultId, resultIndex, threadIndex, true /* wrap */);

    dump("XXX resultId = " + resultId.value + "\n");
    dump("XXX resultIndex = " + resultIndex.value + "\n");

    // only scroll and select if we found something
    if ((resultId.value != -1) && (resultIndex.value != -1)) {
        outlinerSelection.select(resultIndex.value);
        EnsureRowInThreadOutlinerIsVisible(resultIndex.value); 
        return;
    }
    
    CrossFolderNavigation(type, true);
  }
  catch (ex) {
    dump("XXX ex = " + ex + "\n");
  }
}
