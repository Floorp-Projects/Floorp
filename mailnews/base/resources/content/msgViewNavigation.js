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

// we need the folder datasource and account manager datasource
// for when trying to figure out which folder (or account) is next
// in the folder pane.
var gFolderDataSource = Components.classes["@mozilla.org/rdf/datasource;1?name=mailnewsfolders"].createInstance().QueryInterface(Components.interfaces.nsIRDFDataSource);

var gAccountManagerDataSource = Components.classes["@mozilla.org/rdf/datasource;1?name=msgaccountmanager"].createInstance().QueryInterface(Components.interfaces.nsIRDFDataSource);

// we can't compare the name to determine the order in the folder pane
// we need to compare the value of the sort resource, 
// as that's what we use to sort on in the folder pane
var gNameProperty = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService).GetResource("http://home.netscape.com/NC-rdf#Name?sort=true");

function compareServerSortOrder(a,b)
{
  return compareSortOrder(a,b, gAccountManagerDataSource);
}

function compareFolderSortOrder(a,b)
{
  return compareSortOrder(a,b, gFolderDataSource);
}

function compareSortOrder(folder1, folder2, datasource)
{
  var sortValue1, sortValue2;

  try {
    var res1 = RDF.GetResource(folder1.URI);
    sortValue1 = datasource.GetTarget(res1, gNameProperty, true).QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  }
  catch (ex) {
    dump("XXX ex " + folder1.URI + "," + ex + "\n");
    sortValue1 = "";
  }

  try {
    var res2 = RDF.GetResource(folder2.URI);
    sortValue2 = datasource.GetTarget(res2, gNameProperty, true).QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  }
  catch (ex) {
    dump("XXX ex " + folder2.URI + "," + ex + "\n");
    sortValue2 = "";
  }

  if (sortValue1 < sortValue2)
    return -1;
  else if (sortValue1 > sortValue2)
    return 1;
  else 
    return 0;
}

function GetSubFoldersInFolderPaneOrder(folder)
{
  var subFolderEnumerator = folder.GetSubFolders();
  var done = false;
  var msgFolders = Array();

  // get all the subfolders
  while (!done) {
    try {
      var element = subFolderEnumerator.currentItem();
      msgFolders[msgFolders.length] = element.QueryInterface(Components.interfaces.nsIMsgFolder);

      subFolderEnumerator.next();
    }
    catch (ex) {
      done = true;
    }
  }

  // sort the subfolders
  msgFolders.sort(compareFolderSortOrder);
  return msgFolders;
}

function IgnoreFolderForNextNavigation(folder)
{
   // if there is unread mail in the trash, sent, drafts, unsent messages
   // or templates folders, we ignore it
   // when doing cross folder "next" navigation
   return IsSpecialFolder(folder, MSG_FOLDER_FLAG_TRASH | MSG_FOLDER_FLAG_SENTMAIL | MSG_FOLDER_FLAG_DRAFTS | MSG_FOLDER_FLAG_QUEUE | MSG_FOLDER_FLAG_TEMPLATES)
}

function FindNextFolder(originalFolderURI)
{
    if (!originalFolderURI) return null;

    var originalFolderResource = RDF.GetResource(originalFolderURI);
    var folder = originalFolderResource.QueryInterface(Components.interfaces.nsIFolder);
    if (!folder) return null;

    var originalMsgFolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);

    // don't land on folders or subfolder of folders
    // that we don't care about
    if(!IgnoreFolderForNextNavigation(originalMsgFolder)) {
      if (originalMsgFolder.getNumUnread(false /* don't descend */) > 0) {
        return originalMsgFolder.URI;
      }
    }

    // first check the children
    var msgFolders = GetSubFoldersInFolderPaneOrder(folder);
    var i;
    for (i=0;i<msgFolders.length;i++) {
      var currentSubFolder = msgFolders[i];
      // don't land on folders we don't care about
      if(!IgnoreFolderForNextNavigation(currentSubFolder)) {
        if (currentSubFolder.getNumUnread(false /* don't descend */) > 0) {
           // if the child has unread, use it.
           return currentSubFolder.URI;
        }
        else if (currentSubFolder.getNumUnread(true /* descend */) > 0) {
           // if the child doesn't have any unread, but it's children do, recurse
           return FindNextFolder(currentSubFolder.URI);
        }
      }
    } 
 
    // didn't find folder in children
    // go up to the parent, and start at the folder after the current one
    // unless we are at a server, in which case bail out.
    if (originalMsgFolder.isServer) {
      return null;
    }

    msgFolders = GetSubFoldersInFolderPaneOrder(folder.parent);
    for (i=0;i<msgFolders.length;i++) {
      if (msgFolders[i].URI == folder.URI) 
        break;
    }
    
    // the current folder is at index i
    // start at the next folder after that, if there is one
    if (i+1 < msgFolders.length) {
      return FindNextFolder(msgFolders[i+1].URI);
    }
 
    try {
      // none at this level after the current folder.  go up.
      if (folder.parent && folder.parent.URI) { 
        var parentMsgFolder = folder.parent.QueryInterface(Components.interfaces.nsIMsgFolder);
        if (parentMsgFolder.isServer) {
          // we've already search the parent's children below us.
          // don't search the server again, return null and let
          // the caller search the next server
          return null;
        }
        else {
          FindNextFolder(folder.parent.URI);
        }
      }
    }
    catch (ex) {
      dump("XXX ex " + ex + "\n");
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

function GetRootFoldersInFolderPaneOrder()
{
  var allServers = accountManager.allServers;
  var numServers = allServers.Count();
  var i;

  var serversMsgFolders = Array(numServers);
  for (i=0;i<numServers;i++) {
    serversMsgFolders[i] = allServers.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgIncomingServer).RootFolder.QueryInterface(Components.interfaces.nsIMsgFolder);
  }

  // sort accounts, so they are in the same order as folder pane
  serversMsgFolders.sort(compareServerSortOrder);
 
  return serversMsgFolders;
}

function CrossFolderNavigation(type, supportsFolderPane )
{
  if (type != nsMsgNavigationType.nextUnreadMessage) {
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
  var i,j;
  var rootFolders;

  // look for the next folder, this will only look on the current account
  // and below us, in the folder pane
  nextFolderURI = FindNextFolder(startAtURI);

  // if nothing in the current account, start with the next account (below)
  // and try until we hit the bottom of the folder pane
  if (!nextFolderURI) {
    // start at the account after the current account
    rootFolders = GetRootFoldersInFolderPaneOrder();
    for (i=0;i<rootFolders.length;i++) {
      if (rootFolders[i].URI == gDBView.msgFolder.server.serverURI)
        break;
    }

    for (j=i+1; j<rootFolders.length; j++) {
      nextFolderURI = FindNextFolder(rootFolders[j].URI);
      if (nextFolderURI)
        break;
    }
 
    // if nothing from the current account down to the bottom
    // (of the folder pane), start again at the top.
    if (!nextFolderURI) {
      for (j=0; j<rootFolders.length; j++) {
        nextFolderURI = FindNextFolder(rootFolders[j].URI);
        if (nextFolderURI)
          break;
      }
    }
  }

  if (nextFolderURI && (originalFolderURI != nextFolderURI)) {
    var nextFolderResource = RDF.GetResource(nextFolderURI);
    var nextFolder = nextFolderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    switch (nextMode) {
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

// from MailNewsTypes.h
const nsMsgViewIndex_None = 0xFFFFFFFF;

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
    if ((resultId.value != nsMsgViewIndex_None) && (resultIndex.value != nsMsgViewIndex_None)) {
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

