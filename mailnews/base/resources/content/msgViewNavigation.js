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

var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);
var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);


function FindNextFolder(originalFolderURI)
{
    if (!originalFolderURI) return null;

    var originalFolderResource = RDF.GetResource(originalFolderURI);
    var folder = originalFolderResource.QueryInterface(Components.interfaces.nsIFolder);
    if (!folder) return null;

    try {
       var subFolderEnumerator = folder.GetSubFolders();
       var done = false;
       while (!done) {
         var element = subFolderEnumerator.currentItem();
         var currentSubFolder = element.QueryInterface(Components.interfaces.nsIMsgFolder);

         // don't land in the Trash folder.
         if(!IsSpecialFolder(currentSubFolder, MSG_FOLDER_FLAG_TRASH)) {
           if (currentSubFolder.getNumUnread(false /* don't descend */) > 0) {
             // if the child has unread, use it.
             return currentSubFolder.URI;
           }
           else if (currentSubFolder.getNumUnread(true /* descend */) > 0) {
             // if the child doesn't have any unread, but it's children do, recurse
             return FindNextFolder(currentSubFolder.URI);
           }
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
      return FindNextFolder(folder.parent.URI);
    }
    return null;
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

function CrossFolderNavigation(type, supportsFolderPane )
{
  if (type != nsMsgNavigationType.nextUnreadMessage) 
  {
      // only do cross folder navigation for "next unread message"
      return null;
  }

  var nextMode = pref.GetIntPref("mailnews.nav_crosses_folders");
  // 0: "next" goes to the next folder, without prompting
  // 1: "next" goes to the next folder, and prompts (the default)
  // 2: "next" does nothing when there are no unread messages

  // not crossing folders, don't find next
  if (nextMode == 2) return null;

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
            gNextMessageAfterLoad = type;
            if (supportsFolderPane)
              SelectFolder(nextFolderURI);
            break;
        case 1:
        default:
            var promptText = gMessengerBundle.getFormattedString("advanceNextPrompt", [ nextFolder.name ], 1); 
            if (promptService.confirm(window, promptText, promptText)) {
                gNextMessageAfterLoad = type;
                if (supportsFolderPane)
                  SelectFolder(nextFolderURI);
            }
            break;
        }
    }

    return nextFolderURI;
}

function ScrollToMessage(type, wrap, selectMessage)
{
  try {
    var outlinerView = gDBView.QueryInterface(Components.interfaces.nsIOutlinerView);
    var outlinerSelection = outlinerView.selection;
    var currentIndex = outlinerSelection.currentIndex;

    var resultId = new Object;
    var resultIndex = new Object;
    var threadIndex = new Object;

    gDBView.viewNavigate(type, resultId, resultIndex, threadIndex, true /* wrap */);

    // only scroll and select if we found something
    if ((resultId.value != -1) && (resultIndex.value != -1)) {
        if (selectMessage) {
            outlinerSelection.select(resultIndex.value);
        }
        EnsureRowInThreadOutlinerIsVisible(resultIndex.value);
        return true;
    }
    else {
        return false;
    }
  }
  catch (ex) {
    return false;
  }
}

function GoNextMessage(type, startFromBeginning)
{
  try {
    var succeeded = ScrollToMessage(type, startFromBeginning, true);
    if (!succeeded) {
      CrossFolderNavigation(type, true);
    }
  }
  catch (ex) {
    dump("GoNextMessage ex = " + ex + "\n");
  }
}

