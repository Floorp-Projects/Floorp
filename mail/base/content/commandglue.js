# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributors(s):
#   Jan Varga <varga@nixcorp.com>
#   Håkan Waara (hwaara@chello.se)
#   David Bienvenu (bienvenu@nventure.com)
/*
 * Command-specific code. This stuff should be called by the widgets
 */

//NOTE: gMessengerBundle and gBrandBundle must be defined and set
//      for this Overlay to work properly

var gFolderJustSwitched = false;
var gBeforeFolderLoadTime;
var gRDFNamespace = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
var gVirtualFolderTerms;
var gXFVirtualFolderTerms;
var gCurrentVirtualFolderUri;
var gPrevFolderFlags;
var gPrevSelectedFolder;
var gMsgFolderSelected;

/* keep in sync with nsMsgFolderFlags.h */
var MSG_FOLDER_FLAG_VIRTUAL = 0x0020;
var MSG_FOLDER_FLAG_TRASH = 0x0100;
var MSG_FOLDER_FLAG_SENTMAIL = 0x0200;
var MSG_FOLDER_FLAG_DRAFTS = 0x0400;
var MSG_FOLDER_FLAG_QUEUE = 0x0800;
var MSG_FOLDER_FLAG_INBOX = 0x1000;
var MSG_FOLDER_FLAG_TEMPLATES = 0x400000;
var MSG_FOLDER_FLAG_JUNK = 0x40000000;

function OpenURL(url)
{
  messenger.SetWindow(window, msgWindow);
  messenger.OpenURL(url);
}

function GetMsgFolderFromResource(folderResource)
{
  if (!folderResource)
     return null;

  var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
  if (msgFolder && (msgFolder.parent || msgFolder.isServer))
    return msgFolder;
  else
    return null;
}

function GetServer(uri)
{
    if (!uri) return null;
    try {
        var folder = GetMsgFolderFromUri(uri, true);
        return folder.server;
    }
    catch (ex) {
        dump("GetServer("+uri+") failed, ex="+ex+"\n");
    }
    return null;
}

function LoadMessageByUri(uri)
{
  //dump("XXX LoadMessageByUri " + uri + " vs " + gCurrentDisplayedMessage + "\n");
  if(uri != gCurrentDisplayedMessage)
  {
        dump("fix this, get the nsIMsgDBHdr and the nsIMsgFolder from the uri...\n");
/*
    var resource = RDF.GetResource(uri);
    var message = resource.QueryInterface(Components.interfaces.nsIMessage);
    if (message)
      setTitleFromFolder(message.msgFolder, message.mimef2DecodedSubject);

    var nsIMsgFolder = Components.interfaces.nsIMsgFolder;
    if (message.msgFolder.server.downloadOnBiff)
      message.msgFolder.biffState = nsIMsgFolder.nsMsgBiffState_NoMail;
*/

    gCurrentDisplayedMessage = uri;
    gHaveLoadedMessage = true;
    OpenURL(uri);
  }

}

function setTitleFromFolder(msgfolder, subject)
{
    var wintype = document.firstChild.getAttribute('windowtype');
    var title; 

    // If we are showing the mail:3pane. Never include the subject of the selected
    // message in the title. ("Inbox for greenlantern@mozilla.org - Mozilla Thunderbird")
    // If we are a stand alone message window, we should show the Subject
    // and the product but not the account name: "Re: New window Title - Mozilla Thunderbird"

    if (wintype == "mail:messageWindow")  
      title = subject ? subject : "";
    else if (msgfolder)
    {
      title = msgfolder.prettyName;

      if (!msgfolder.isServer)
      {
        var server = msgfolder.server;
        var middle;
        var end;
        if (server.type == "nntp") {
            // <folder> on <hostname>
            middle = gMessengerBundle.getString("titleNewsPreHost");
            end = server.hostName;
        } else {
            var identity;
            try {
                var identities = accountManager.GetIdentitiesForServer(server);

                identity = identities.QueryElementAt(0, Components.interfaces.nsIMsgIdentity);
                // <folder> for <email>
                middle = gMessengerBundle.getString("titleMailPreHost");
                end = identity.email;
          } catch (ex) {}
            }
        if (middle) title += " " + middle;
        if (end) title += " " + end;
    }
    }

#ifndef XP_MACOSX
    title += " - " + gBrandBundle.getString("brandShortName");
#endif
    window.title = title;
}

function UpdateMailToolbar(caller)
{
  //dump("XXX update mail-toolbar " + caller + "\n");
  document.commandDispatcher.updateCommands('mail-toolbar');

  // hook for extra toolbar items
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(window, "mail:updateToolbarItems", null);
}

function ChangeFolderByURI(uri, viewType, viewFlags, sortType, sortOrder)
{
  //dump("In ChangeFolderByURI uri = " + uri + " sortType = " + sortType + "\n");
  if (uri == gCurrentLoadingFolderURI)
    return;

  // hook for extra toolbar items
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(window, "mail:setupToolbarItems", uri);

  var resource = RDF.GetResource(uri);
  var msgfolder =
      resource.QueryInterface(Components.interfaces.nsIMsgFolder);

  try {
      setTitleFromFolder(msgfolder, null);
  } catch (ex) {
      dump("error setting title: " + ex + "\n");
  }

  //if it's a server, clear the threadpane and don't bother trying to load.
  if(msgfolder.isServer) {
    msgWindow.openFolder = null;

    ClearThreadPane();

    // Load AccountCentral page here.
    ShowAccountCentral();

    return;
  }
  else
  {
    if (msgfolder.server.displayStartupPage)
    {
      gDisplayStartupPage = true;
      msgfolder.server.displayStartupPage = false;
    }
  }

  // If the user clicks on msgfolder, time to display thread pane and message pane.
  // Hide AccountCentral page
  if (gAccountCentralLoaded)
  {
      HideAccountCentral();
  }

  if (gFakeAccountPageLoaded)
  {
      HideFakeAccountPage();
  }

  gCurrentLoadingFolderURI = uri;
  gNextMessageAfterDelete = null; // forget what message to select, if any

  gCurrentFolderToReroot = uri;
  gCurrentLoadingFolderViewFlags = viewFlags;
  gCurrentLoadingFolderViewType = viewType;
  gCurrentLoadingFolderSortType = sortType;
  gCurrentLoadingFolderSortOrder = sortOrder;

  var showMessagesAfterLoading;
  try {
    var server = msgfolder.server;
    if (gPrefBranch.getBoolPref("mail.password_protect_local_cache"))
    {
      showMessagesAfterLoading = server.passwordPromptRequired;
      // servers w/o passwords (like local mail) will always be non-authenticated.
      // So we need to use the account manager for that case.
    }
    else if (server.redirectorType) {
      var prefString = server.type + "." + server.redirectorType + ".showMessagesAfterLoading";
      showMessagesAfterLoading = gPrefBranch.getBoolPref(prefString);
    }
    else
      showMessagesAfterLoading = false;
  }
  catch (ex) {
    showMessagesAfterLoading = false;
  }

  if (viewType != nsMsgViewType.eShowVirtualFolderResults && (msgfolder.manyHeadersToDownload || showMessagesAfterLoading))
  {
    gRerootOnFolderLoad = true;
    try
    {
      ClearThreadPane();
      SetBusyCursor(window, true);
      msgfolder.startFolderLoading();
      msgfolder.updateFolder(msgWindow);
    }
    catch(ex)
    {
      SetBusyCursor(window, false);
      dump("Error loading with many headers to download: " + ex + "\n");
    }
  }
  else
  {
    if (viewType != nsMsgViewType.eShowVirtualFolderResults)
      SetBusyCursor(window, true);
    RerootFolder(uri, msgfolder, viewType, viewFlags, sortType, sortOrder);
    gRerootOnFolderLoad = false;
    msgfolder.startFolderLoading();

    //Need to do this after rerooting folder.  Otherwise possibility of receiving folder loaded
    //notification before folder has actually changed.
    if (viewType != nsMsgViewType.eShowVirtualFolderResults)
      msgfolder.updateFolder(msgWindow);
  }
}

function isNewsURI(uri)
{
    if (!uri || uri[0] != 'n') {
        return false;
    }
    else {
        return ((uri.substring(0,6) == "news:/") || (uri.substring(0,14) == "news-message:/"));
    }
}

function RerootFolder(uri, newFolder, viewType, viewFlags, sortType, sortOrder)
{
  //dump("In reroot folder, sortType = " +  sortType + "viewType = " + viewType + "\n");

  // workaround for #39655
  gFolderJustSwitched = true;

  ClearThreadPaneSelection();

  //Clear the new messages of the old folder
  var oldFolder = gPrevSelectedFolder;
  if (oldFolder) {
    if (oldFolder.hasNewMessages) {
      oldFolder.clearNewMessages();
    }
    oldFolder.hasNewMessages = false;
  }

  //Set the window's new open folder.
  msgWindow.openFolder = newFolder;

  //the new folder being selected should have its biff state get cleared.
  if(newFolder)
  {
    newFolder.biffState =
          Components.interfaces.nsIMsgFolder.nsMsgBiffState_NoMail;
  }

  //Clear out the thread pane so that we can sort it with the new sort id without taking any time.
  // folder.setAttribute('ref', "");

  // null this out, so we don't try sort.
  if (gDBView) {
    gDBView.close();
    gDBView = null;
  }

  // cancel the pending mark as read timer
  ClearPendingReadTimer();

  // if this is the drafts, sent, or send later folder,
  // we show "Recipient" instead of "Author"
  SetSentFolderColumns(IsSpecialFolder(newFolder, MSG_FOLDER_FLAG_SENTMAIL | MSG_FOLDER_FLAG_DRAFTS | MSG_FOLDER_FLAG_QUEUE, true));
  ShowLocationColumn(viewType == nsMsgViewType.eShowVirtualFolderResults);

  // now create the db view, which will sort it.
  CreateDBView(newFolder, viewType, viewFlags, sortType, sortOrder);
  if (oldFolder)
  {
     /*we don't null out the db reference for inbox because inbox is like the "main" folder
       and performance outweighs footprint */
    if (!IsSpecialFolder(oldFolder, MSG_FOLDER_FLAG_INBOX, false))
    {
      if (oldFolder.URI != newFolder.URI)
        oldFolder.setMsgDatabase(null);
    }
  }
  // that should have initialized gDBView, now re-root the thread pane
  RerootThreadPane();

  SetUpToolbarButtons(uri);

  UpdateStatusMessageCounts(newFolder);
  
  // hook for extra toolbar items
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(window, "mail:updateToolbarItems", null);
  // this is to kick off cross-folder searches for virtual folders.
  if (gSearchSession && !gVirtualFolderTerms) // another var might be better...
  {
    viewDebug("doing a xf folder search in rerootFolder\n");
    gDBView.searchSession = gSearchSession;
    gSearchSession.search(msgWindow);
  }

}

function SwitchView(command)
{
  // when switching thread views, we might be coming out of quick search
  // or a message view.
  // first set view picker to all
  ViewMessagesBy("viewPickerAll");

  // clear the QS text, if we need to
  ClearQSIfNecessary();
  
  // now switch views
  var oldSortType = gDBView ? gDBView.sortType : nsMsgViewSortType.byThread;
  var oldSortOrder = gDBView ? gDBView.sortOrder : nsMsgViewSortOrder.ascending;
  var viewFlags = gDBView ? gDBView.viewFlags : gCurViewFlags;

  // close existing view.
  if (gDBView) {
    gDBView.close();
    gDBView = null; 
  }

  switch(command)
  {
    case "cmd_viewUnreadMsgs":

      viewFlags = viewFlags | nsMsgViewFlagsType.kUnreadOnly;
      CreateDBView(msgWindow.openFolder, nsMsgViewType.eShowAllThreads, viewFlags,
            oldSortType, oldSortOrder );
    break;
    case "cmd_viewAllMsgs":
      viewFlags = viewFlags & ~nsMsgViewFlagsType.kUnreadOnly;
      CreateDBView(msgWindow.openFolder, nsMsgViewType.eShowAllThreads, viewFlags,
            oldSortType, oldSortOrder);
    break;
    case "cmd_viewThreadsWithUnread":
      CreateDBView(msgWindow.openFolder, nsMsgViewType.eShowThreadsWithUnread, nsMsgViewFlagsType.kThreadedDisplay,
            nsMsgViewSortType.byThread, oldSortOrder);

    break;
    case "cmd_viewWatchedThreadsWithUnread":
      CreateDBView(msgWindow.openFolder, nsMsgViewType.eShowWatchedThreadsWithUnread, nsMsgViewFlagsType.kThreadedDisplay,
            nsMsgViewSortType.byThread, oldSortOrder);
   break;
    case "cmd_viewIgnoredThreads":
      if (viewFlags & nsMsgViewFlagsType.kShowIgnored)
        viewFlags = viewFlags & ~nsMsgViewFlagsType.kShowIgnored;
      else
        viewFlags = viewFlags | nsMsgViewFlagsType.kShowIgnored;
      CreateDBView(msgWindow.openFolder, nsMsgViewType.eShowAllThreads, viewFlags,
            oldSortType, oldSortOrder);
    break;
  }

  RerootThreadPane();
}

function SetSentFolderColumns(isSentFolder)
{
  var tree = GetThreadTree();

  var lastFolderSent = tree.getAttribute("lastfoldersent") == "true";
  if (isSentFolder != lastFolderSent)
  {
    var senderColumn = document.getElementById("senderCol");
    var recipientColumn = document.getElementById("recipientCol");
    
    var saveHidden = senderColumn.getAttribute("hidden");
    senderColumn.setAttribute("hidden", senderColumn.getAttribute("swappedhidden"));
    senderColumn.setAttribute("swappedhidden", saveHidden);

    saveHidden = recipientColumn.getAttribute("hidden");
    recipientColumn.setAttribute("hidden", recipientColumn.getAttribute("swappedhidden"));
    recipientColumn.setAttribute("swappedhidden", saveHidden);
  }

  if(isSentFolder)
    tree.setAttribute("lastfoldersent", "true");
  else
    tree.setAttribute("lastfoldersent", "false");
}

function ShowLocationColumn(show)
{
  var col = document.getElementById("locationCol");
  if (col) {
    if (show) {
      col.removeAttribute("hidden");
      col.removeAttribute("ignoreincolumnpicker");
    }
    else {
      col.setAttribute("hidden","true");
      col.setAttribute("ignoreincolumnpicker","true");
    }
  }
}

function SetNewsFolderColumns()
{
  var sizeColumn = document.getElementById("sizeCol");

  if (gDBView.usingLines) {
     sizeColumn.setAttribute("label",gMessengerBundle.getString("linesColumnHeader"));
  }
  else {
     sizeColumn.setAttribute("label", gMessengerBundle.getString("sizeColumnHeader"));
  }
}

function UpdateStatusMessageCounts(folder)
{
  var unreadElement = GetUnreadCountElement();
  var totalElement = GetTotalCountElement();
  if(folder && unreadElement && totalElement)
  {
    var numUnread =
            gMessengerBundle.getFormattedString("unreadMsgStatus",
                                                [ folder.getNumUnread(false)]);
    var numTotal =
            gMessengerBundle.getFormattedString("totalMsgStatus",
                                                [folder.getTotalMessages(false)]);

    unreadElement.setAttribute("label", numUnread);
    totalElement.setAttribute("label", numTotal);

  }

}

function ConvertColumnIDToSortType(columnID)
{
  var sortKey;

  switch (columnID) {
    case "dateCol":
      sortKey = nsMsgViewSortType.byDate;
      break;
    case "senderCol":
    	sortKey = nsMsgViewSortType.byAuthor;
      break;
    case "recipientCol":
    	sortKey = nsMsgViewSortType.byRecipient;
      break;
    case "subjectCol":
      sortKey = nsMsgViewSortType.bySubject;
      break;
    case "locationCol":
      sortKey = nsMsgViewSortType.byLocation;
      break;
    case "accountCol":
      sortKey = nsMsgViewSortType.byAccount;
      break;
    case "unreadButtonColHeader":
      sortKey = nsMsgViewSortType.byUnread;
      break;
    case "statusCol":
      sortKey = nsMsgViewSortType.byStatus;
      break;
    case "sizeCol":
      sortKey = nsMsgViewSortType.bySize;
      break;
    case "priorityCol":
      sortKey = nsMsgViewSortType.byPriority;
      break;
    case "flaggedCol":
      sortKey = nsMsgViewSortType.byFlagged;
      break;
    case "threadCol":
      sortKey = nsMsgViewSortType.byThread;
      break;
    case "labelCol":
      sortKey = nsMsgViewSortType.byLabel;
      break;
    case "junkStatusCol":
      sortKey = nsMsgViewSortType.byJunkStatus;
      break;
    case "idCol":
      sortKey = nsMsgViewSortType.byId;
      break;
    case "attachmentCol":
	    sortKey = nsMsgViewSortType.byAttachments;
	    break;
    default:
      dump("unsupported sort column: " + columnID + "\n");
      sortKey = 0;
      break;
  }
  return sortKey;
}

function ConvertSortTypeToColumnID(sortKey)
{
  var columnID;

  // hack to turn this into an integer, if it was a string
  // it would be a string if it came from localStore.rdf
  sortKey = sortKey - 0;

  switch (sortKey) {
    case nsMsgViewSortType.byDate:
      columnID = "dateCol";
      break;
    case nsMsgViewSortType.byAuthor:
      columnID = "senderCol";
      break;
    case nsMsgViewSortType.byRecipient:
      columnID = "recipientCol";
      break;
    case nsMsgViewSortType.bySubject:
      columnID = "subjectCol";
      break;
    case nsMsgViewSortType.byLocation:
      columnID = "locationCol";
      break;
    case nsMsgViewSortType.byAccount:
      columnID = "accountCol";
      break;
    case nsMsgViewSortType.byUnread:
      columnID = "unreadButtonColHeader";
      break;
    case nsMsgViewSortType.byStatus:
      columnID = "statusCol";
      break;
    case nsMsgViewSortType.byLabel:
      columnID = "labelCol";
      break;
    case nsMsgViewSortType.bySize:
      columnID = "sizeCol";
      break;
    case nsMsgViewSortType.byPriority:
      columnID = "priorityCol";
      break;
    case nsMsgViewSortType.byFlagged:
      columnID = "flaggedCol";
      break;
    case nsMsgViewSortType.byThread:
      columnID = "threadCol";
      break;
    case nsMsgViewSortType.byId:
      // there is no orderReceivedCol, so return null
      columnID = null;
      break;
    case nsMsgViewSortType.byJunkStatus:
      columnID = "junkStatusCol";
      break;
	  case nsMsgViewSortType.byAttachments:
	    columnID = "attachmentCol";
	    break;
    default:
      dump("unsupported sort key: " + sortKey + "\n");
      columnID = null;
      break;
  }
  return columnID;
}

var nsMsgViewSortType = Components.interfaces.nsMsgViewSortType;
var nsMsgViewSortOrder = Components.interfaces.nsMsgViewSortOrder;
var nsMsgViewFlagsType = Components.interfaces.nsMsgViewFlagsType;
var nsMsgViewCommandType = Components.interfaces.nsMsgViewCommandType;
var nsMsgViewType = Components.interfaces.nsMsgViewType;
var nsMsgNavigationType = Components.interfaces.nsMsgNavigationType;

var gDBView = null;
var gCurViewFlags;
var gCurSortType;

// CreateDBView is called when we have a thread pane. CreateBareDBView is called when there is no
// tree associated with the view. CreateDBView will call into CreateBareDBView...

function CreateBareDBView(originalView, msgFolder, viewType, viewFlags, sortType, sortOrder)
{
  var dbviewContractId = "@mozilla.org/messenger/msgdbview;1?type=";
  // hack to turn this into an integer, if it was a string
  // it would be a string if it came from localStore.rdf
  viewType = viewType - 0;

  switch (viewType) {
      case nsMsgViewType.eShowQuickSearchResults:
          dbviewContractId += "quicksearch";
          break;
      case nsMsgViewType.eShowThreadsWithUnread:
          dbviewContractId += "threadswithunread";
          break;
      case nsMsgViewType.eShowWatchedThreadsWithUnread:
          dbviewContractId += "watchedthreadswithunread";
          break;
      case nsMsgViewType.eShowVirtualFolderResults:
          dbviewContractId += "xfvf";
          break;
      case nsMsgViewType.eShowAllThreads:
      default:
          if (viewFlags & nsMsgViewFlagsType.kGroupBySort)
            dbviewContractId += "group";
          else
            dbviewContractId += "threaded";
          break;
  }

//  dump ("contract id = " + dbviewContractId + "original view = " + originalView + "\n");
  if (!originalView)
    gDBView = Components.classes[dbviewContractId].createInstance(Components.interfaces.nsIMsgDBView);

  gCurViewFlags = viewFlags;
  var count = new Object;
  if (!gThreadPaneCommandUpdater)
    gThreadPaneCommandUpdater = new nsMsgDBViewCommandUpdater();

  gCurSortType = sortType;

  if (!originalView) {
    gDBView.init(messenger, msgWindow, gThreadPaneCommandUpdater);
    gDBView.open(msgFolder, gCurSortType, sortOrder, viewFlags, count);
    if (viewType == nsMsgViewType.eShowVirtualFolderResults)
    {
      // the view is a listener on the search results
      gViewSearchListener = gDBView.QueryInterface(Components.interfaces.nsIMsgSearchNotify);
      gSearchSession.registerListener(gViewSearchListener);
    }
  } 
  else {
    gDBView = originalView.cloneDBView(messenger, msgWindow, gThreadPaneCommandUpdater);
  }
}

function CreateDBView(msgFolder, viewType, viewFlags, sortType, sortOrder)
{
  // call the inner create method
  CreateBareDBView(null, msgFolder, viewType, viewFlags, sortType, sortOrder);

  // now do tree specific work

  // based on the collapsed state of the thread pane/message pane splitter,
  // suppress message display if appropriate.
  gDBView.suppressMsgDisplay = IsMessagePaneCollapsed();

  UpdateSortIndicators(gCurSortType, sortOrder);
}

//------------------------------------------------------------
// Sets the column header sort icon based on the requested
// column and direction.
//
// Notes:
// (1) This function relies on the first part of the
//     <treecell id> matching the <treecol id>.  The treecell
//     id must have a "Header" suffix.
// (2) By changing the "sortDirection" attribute, a different
//     CSS style will be used, thus changing the icon based on
//     the "sortDirection" parameter.
//------------------------------------------------------------
function UpdateSortIndicator(column,sortDirection)
{
  // this is obsolete
}

function GetSelectedFolderResource()
{
    var folderTree = GetFolderTree();
    var startIndex = {};
    var endIndex = {};
    folderTree.view.selection.getRangeAt(0, startIndex, endIndex);
    return GetFolderResource(folderTree, startIndex.value);
}

function ChangeMessagePaneVisibility(now_hidden)
{
  // we also have to hide the File/Attachments menuitem
  node = document.getElementById("fileAttachmentMenu");
  if (node)
    node.hidden = now_hidden;

  if (gDBView) {
    // the collapsed state is the state after we released the mouse 
    // so we take it as it is
    gDBView.suppressMsgDisplay = now_hidden;
  }
  var event = document.createEvent('Events');
  if (now_hidden) {
    event.initEvent('messagepane-hide', false, true);
  }
  else {
    event.initEvent('messagepane-unhide', false, true);
  }
  document.getElementById("messengerWindow").dispatchEvent(event);
}

function OnMouseUpThreadAndMessagePaneSplitter()
{
  // the collapsed state is the state after we released the mouse 
  // so we take it as it is
  ChangeMessagePaneVisibility(IsMessagePaneCollapsed());
}

function FolderPaneSelectionChange()
{
    var folderTree = GetFolderTree();
    var folderSelection = folderTree.view.selection;

    // This prevents a folder from being loaded in the case that the user
    // has right-clicked on a folder different from the one that was
    // originally highlighted.  On a right-click, the highlight (selection)
    // of a row will be different from the value of currentIndex, thus if
    // the currentIndex is not selected, it means the user right-clicked
    // and we don't want to load the contents of the folder.
    if (!folderSelection.isSelected(folderSelection.currentIndex))
      return;

    if(gTimelineEnabled) {
      gTimelineService.startTimer("FolderLoading");
      gTimelineService.enter("FolderLoading has Started");
    }

    gVirtualFolderTerms = null;
    gXFVirtualFolderTerms = null;

    if (folderSelection.count == 1)
    {
        var startIndex = {};
        var endIndex = {};


        folderSelection.getRangeAt(0, startIndex, endIndex);
        var folderResource = GetFolderResource(folderTree, startIndex.value);
        var uriToLoad = folderResource.Value;
        var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
        gPrevSelectedFolder = gMsgFolderSelected;
        gMsgFolderSelected = msgFolder;
        var folderFlags = msgFolder.flags;
        // if this is same folder, and we're not showing a virtual folder
        // then do nothing.
        if (msgFolder == msgWindow.openFolder && 
          !(folderFlags & MSG_FOLDER_FLAG_VIRTUAL) && ! (gPrevFolderFlags & MSG_FOLDER_FLAG_VIRTUAL))
        {
            dump("msgFolder already open" + folderResource.URI + "\n");
            return;
        }
        else
        {
            var sortType = 0;
            var sortOrder = 0;
            var viewFlags = 0;
            var viewType = 0;
            gVirtualFolderTerms = null;
            gXFVirtualFolderTerms = null;
            gPrevFolderFlags = folderFlags;
//            gSearchInput.showingSearchCriteria = false;
            gCurrentVirtualFolderUri = null;
            // don't get the db if this folder is a server
            // we're going to be display account central
            if (!(msgFolder.isServer)) 
            {
              try 
              {
                var msgDatabase = msgFolder.getMsgDatabase(msgWindow);
                if (msgDatabase)
                {
                  var dbFolderInfo = msgDatabase.dBFolderInfo;
                  sortType = dbFolderInfo.sortType;
                  sortOrder = dbFolderInfo.sortOrder;
                  if (folderFlags & MSG_FOLDER_FLAG_VIRTUAL)
                  {
                    viewType = nsMsgViewType.eShowQuickSearchResults;
                    var searchTermString = dbFolderInfo.getCharPtrProperty("searchStr");
                    // trick the view code into updating the real folder...
                    gCurrentVirtualFolderUri = uriToLoad;
                    var srchFolderUri = dbFolderInfo.getCharPtrProperty("searchFolderUri");
                    var srchFolderUriArray = srchFolderUri.split('|');
                    // cross folder search
                    var filterService = Components.classes["@mozilla.org/messenger/services/filters;1"].getService(Components.interfaces.nsIMsgFilterService);
                    var filterList = filterService.getTempFilterList(msgFolder);
                    var tempFilter = filterList.createFilter("temp");
                    filterList.parseCondition(tempFilter, searchTermString);
                    if (srchFolderUriArray.length > 1)
                    {
                      viewType = nsMsgViewType.eShowVirtualFolderResults;
                      gXFVirtualFolderTerms = CreateGroupedSearchTerms(tempFilter.searchTerms);
                      setupXFVirtualFolderSearch(srchFolderUriArray, gXFVirtualFolderTerms);
                      gSearchInput.showingSearchCriteria = false;
                      // need to set things up so that reroot folder issues the search
                    }
                    else
                    {
                      gSearchSession = null;
                      uriToLoad = srchFolderUri;
                      // we need to load the db for the actual folder so that many hdrs to download
                      // will return false...
                      var realFolderRes = GetResourceFromUri(uriToLoad);
                      var realFolder = realFolderRes.QueryInterface(Components.interfaces.nsIMsgFolder);
                      msgDatabase = realFolder.getMsgDatabase(msgWindow);
//                      dump("search term string = " + searchTermString + "\n");
                    
                      gVirtualFolderTerms = CreateGroupedSearchTerms(tempFilter.searchTerms);
                      gSearchInput.showingSearchCriteria = false;
                    }
                  }
                  else
                  {
                    gSearchSession = null;
                    viewFlags = dbFolderInfo.viewFlags;
                    viewType = dbFolderInfo.viewType;
                  }
                  msgDatabase = null;
                  dbFolderInfo = null;
                }
              }
              catch (ex)
              {
                dump("failed to get view & sort values.  ex = " + ex +"\n");
              }
            }
            if (gDBView && gDBView.viewType == nsMsgViewType.eShowQuickSearchResults)
            {
              if (gPreQuickSearchView) //close cached view before quick search
              {
                gPreQuickSearchView.close();
                gPreQuickSearchView = null;  
              }
              var searchInput = document.getElementById("searchInput");  //reset the search input on folder switch
              if (searchInput) 
                searchInput.value = "";
            }
            ClearMessagePane();

            if (gXFVirtualFolderTerms)
              viewType = nsMsgViewType.eShowVirtualFolderResults;
            else if (gSearchEmailAddress || gDefaultSearchViewTerms || gVirtualFolderTerms) 
              viewType = nsMsgViewType.eShowQuickSearchResults;
            else if (viewType == nsMsgViewType.eShowQuickSearchResults)
              viewType = nsMsgViewType.eShowAllThreads;  //override viewType - we don't want to start w/ quick search
            ChangeFolderByURI(uriToLoad, viewType, viewFlags, sortType, sortOrder);
            if (gVirtualFolderTerms)
              gDBView.viewFolder = msgFolder;
        }
    }
    else
    {
        msgWindow.openFolder = null;
        ClearThreadPane();
    }

    if (gAccountCentralLoaded)
      UpdateMailToolbar("gAccountCentralLoaded");
    else if (gFakeAccountPageLoaded)
      UpdateMailToolbar("gFakeAccountPageLoaded");

    if (gDisplayStartupPage)
    {
        loadStartPage();
        gDisplayStartupPage = false;
        UpdateMailToolbar("gDisplayStartupPage");
    }    
}

function ClearThreadPane()
{
  if (gDBView) {
    gDBView.close();
    gDBView = null; 
  }
}

function IsSpecialFolder(msgFolder, flags, checkAncestors)
{
    if (!msgFolder) 
        return false;
    else if ((msgFolder.flags & flags) == 0)
    {
      var parentMsgFolder = msgFolder.parentMsgFolder;

      return (parentMsgFolder && checkAncestors) ? IsSpecialFolder(parentMsgFolder, flags, true) : false;
    }
    else {
        // the user can set their INBOX to be their SENT folder.
        // in that case, we want this folder to act like an INBOX, 
        // and not a SENT folder
        return !((flags & MSG_FOLDER_FLAG_SENTMAIL) && (msgFolder.flags & MSG_FOLDER_FLAG_INBOX));
    }
}

function SelectNextMessage(nextMessage)
{
    dump("XXX implement SelectNextMessage()\n");
}

function GetSelectTrashUri(folder)
{
    if (!folder) return null;
    var uri = folder.getAttribute('id');
    var resource = RDF.GetResource(uri);
    var msgFolder =
        resource.QueryInterface(Components.interfaces.nsIMsgFolder);
    if (msgFolder)
    {
        var rootFolder = msgFolder.rootFolder;
        var numFolder;
        var out = new Object();
        var trashFolder = rootFolder.getFoldersWithFlag(MSG_FOLDER_FLAG_TRASH, 1, out);
        numFolder = out.value;
        if (trashFolder) {
            return trashFolder.URI;
        }
    }
    return null;
}

function Undo()
{
    messenger.Undo(msgWindow);
}

function Redo()
{
    messenger.Redo(msgWindow);
}
var mailOfflineObserver = {
  observe: function(subject, topic, state) {
    // sanity checks
    if (topic != "network:offline-status-changed") return;
    MailOfflineStateChanged(state == "offline");
  }
}

function AddMailOfflineObserver() 
{
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService); 
  observerService.addObserver(mailOfflineObserver, "network:offline-status-changed", false);
}

function RemoveMailOfflineObserver()
{
  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService); 
  observerService.removeObserver(mailOfflineObserver,"network:offline-status-changed");
}

function getSearchTermString(searchTerms)
{
  var searchIndex;
  var condition = "";
  var count = searchTerms.Count();
  for (searchIndex = 0; searchIndex < count; )
  {
    var term = searchTerms.QueryElementAt(searchIndex++, Components.interfaces.nsIMsgSearchTerm);
    
    if (condition.length > 1)
      condition += ' ';
    
    condition += (term.booleanAnd) ? "AND (" : "OR (";
    condition += term.termAsString + ')';
  }
  return condition;
}

function  CreateVirtualFolder(newName, parentFolder, searchFolderURIs, searchTerms)
{
  // ### need to make sure view/folder doesn't exist.
  if (searchFolderURIs && (searchFolderURIs != "") && newName && (newName != "")) 
  {
    try
    {
      var newFolder = parentFolder.addSubfolder(newName);
      newFolder.setFlag(MSG_FOLDER_FLAG_VIRTUAL);
      var vfdb = newFolder.getMsgDatabase(msgWindow);
      var searchTermString = getSearchTermString(searchTerms);
      var dbFolderInfo = vfdb.dBFolderInfo;
      // set the view string as a property of the db folder info
      // set the original folder name as well.
      dbFolderInfo.setCharPtrProperty("searchStr", searchTermString);
      dbFolderInfo.setCharPtrProperty("searchFolderUri", searchFolderURIs);
      vfdb.summaryValid = true;
      vfdb.Close(true);
      parentFolder.NotifyItemAdded(newFolder);
      var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
      accountManager.saveVirtualFolders();
    }
    catch(e)
    {
      throw(e); // so that the dialog does not automatically close
      dump ("Exception : creating virtual folder \n");
    }
  }
  else 
  {
    dump("no name or nothing selected\n");
  }   
}

var searchSessionContractID = "@mozilla.org/messenger/searchSession;1";
var gSearchView;
var gSearchSession;

var nsIMsgFolder = Components.interfaces.nsIMsgFolder;
var nsIMsgWindow = Components.interfaces.nsIMsgWindow;
var nsIMsgRDFDataSource = Components.interfaces.nsIMsgRDFDataSource;
var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;

var gFolderDatasource;
var gFolderPicker;
var gStatusBar = null;
var gTimelineEnabled = false;
var gMessengerBundle = null;

// Datasource search listener -- made global as it has to be registered
// and unregistered in different functions.
var gDataSourceSearchListener;
var gViewSearchListener;

var gMailSession;

function GetScopeForFolder(folder) 
{
  return folder.server.searchScope;
}

function setupXFVirtualFolderSearch(folderUrisToSearch, searchTerms)
{

    var count = new Object;

    gSearchSession = Components.classes[searchSessionContractID].createInstance(Components.interfaces.nsIMsgSearchSession);

    gMailSession = Components.classes[mailSessionContractID].getService(Components.interfaces.nsIMsgMailSession);

    for (var i in folderUrisToSearch) 
    {
      var realFolderRes = GetResourceFromUri(folderUrisToSearch[i]);
      var realFolder = realFolderRes.QueryInterface(Components.interfaces.nsIMsgFolder);
      if (!realFolder.isServer)
        gSearchSession.addScopeTerm(GetScopeForFolder(realFolder), realFolder);
    }

    var termsArray = searchTerms.QueryInterface(Components.interfaces.nsISupportsArray);
    for (var i = 0; i < termsArray.Count(); i++)
      gSearchSession.appendTerm(termsArray.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgSearchTerm));
}

function CreateGroupedSearchTerms(searchTermsArray)
{

  var searchSession = gSearchSession || 
    Components.classes[searchSessionContractID].createInstance(Components.interfaces.nsIMsgSearchSession);

  // create a temporary isupports array to store our search terms
  // since we will be modifying the terms so they work with quick search
  var searchTermsArrayForQS = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
  
  var numEntries = searchTermsArray.Count();
  for (var i = 0; i < numEntries; i++) {
    var searchTerm = searchTermsArray.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgSearchTerm); 

    // clone the term, since we might be modifying it
    var searchTermForQS = searchSession.createTerm();
    searchTermForQS.value = searchTerm.value;
    searchTermForQS.attrib = searchTerm.attrib;
    searchTermForQS.op = searchTerm.op;

    // mark the first node as a group
    if (i == 0)
      searchTermForQS.beginsGrouping = true;
    else if (i == numEntries - 1)
      searchTermForQS.endsGrouping = true;

    // turn the first term to true to work with quick search...
    searchTermForQS.booleanAnd = i ? searchTerm.booleanAnd : true; 
    
    searchTermsArrayForQS.AppendElement(searchTermForQS);
  }
  return searchTermsArrayForQS;
}

var gViewDebug = false;

function viewDebug(str)
{
  if (gViewDebug)
    dump(str);
}

