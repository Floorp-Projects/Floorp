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
 *
 * Contributors: timeless
 *               slucy@objectivesw.co.uk
 *               Håkan Waara <hwaara@chello.se>
 *               Jan Varga <varga@utcru.sk>
 *               Seth Spitzer <sspitzer@netscape.com>
 */

var gMessengerBundle;
var gPromptService;
var gOfflinePromptsBundle;
var nsPrefBranch = null;
var gOfflineManager;
var gWindowManagerInterface;

var gPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);

// Disable the new account menu item if the account preference is locked.
// Two other affected areas are the account central and the account manager
// dialog.
function menu_new_init()
{
  if (!nsPrefBranch) {
    var prefService = Components.classes["@mozilla.org/preferences-service;1"];
    prefService = prefService.getService();
    prefService = prefService.QueryInterface(Components.interfaces.nsIPrefService);
    nsPrefBranch = prefService.getBranch(null);
  }
  var newAccountItem = document.getElementById('newAccountMenuItem');
  if (nsPrefBranch.prefIsLocked("mail.accountmanager.accounts"))
    newAccountItem.setAttribute("disabled","true");
}

function goUpdateMailMenuItems(commandset)
{
//  dump("Updating commands for " + commandset.id + "\n");

  for (var i = 0; i < commandset.childNodes.length; i++)
  {
    var commandID = commandset.childNodes[i].getAttribute("id");
    if (commandID)
    {
      goUpdateCommand(commandID);
    }
  }
}

function file_init()
{
    if (!gMessengerBundle)
        gMessengerBundle = document.getElementById("bundle_messenger");
    file_attachments();
/* file_attachments() can return false to indicate a load failure,
   but if you return false to oncreate then
   the popup menu will not display which is not a good thing.
 */

  document.commandDispatcher.updateCommands('create-menu-file');
}

function file_attachments()
{
  var numAttachments = GetNumberOfAttachmentsForDisplayedMessage();
  var amParent=document.getElementById('fileAttachmentMenu');
  if (!amParent)
    return false;

  // hide the attachment menu item if the message does not have any messages..
  if (numAttachments > 0)
  {
    amParent.removeAttribute('hidden');
  }
  else
    amParent.setAttribute('hidden',true);
  return true;
}

function InitEditMessagesMenu()
{
  document.commandDispatcher.updateCommands('create-menu-edit');
}

function InitSearchMessagesMenu()
{
  document.commandDispatcher.updateCommands('create-menu-search');
}

function InitGoMessagesMenu()
{
  document.commandDispatcher.updateCommands('create-menu-go');
}

function view_init()
{
  if (!gMessengerBundle)
      gMessengerBundle = document.getElementById("bundle_messenger");
  var message_menuitem=document.getElementById('menu_showMessage');

  if (message_menuitem)
  {
      var message_menuitem_hidden = message_menuitem.getAttribute("hidden");
      if(message_menuitem_hidden != "true"){
          message_menuitem.setAttribute('checked',!IsThreadAndMessagePaneSplitterCollapsed());
      }
  }
  var threadColumn = document.getElementById('ThreadColumnHeader');
  var thread_menuitem=document.getElementById('menu_showThreads');
  if (threadColumn && thread_menuitem){
      thread_menuitem.setAttribute('checked',threadColumn.getAttribute('currentView')=='threaded');
  }

  document.commandDispatcher.updateCommands('create-menu-view');
}

function setSortByMenuItemCheckState(id, value)
{
    var menuitem = document.getElementById(id);
    if (menuitem) {
      menuitem.setAttribute("checked", value);
    }
}

function InitViewSortByMenu()
{
    var sortType = gDBView.sortType;

    setSortByMenuItemCheckState("sortByDateMenuitem", (sortType == nsMsgViewSortType.byDate));
    setSortByMenuItemCheckState("sortByFlagMenuitem", (sortType == nsMsgViewSortType.byFlagged));
    setSortByMenuItemCheckState("sortByOrderReceivedMenuitem", (sortType == nsMsgViewSortType.byId));
    setSortByMenuItemCheckState("sortByPriorityMenuitem", (sortType == nsMsgViewSortType.byPriority));
    setSortByMenuItemCheckState("sortBySizeMenuitem", (sortType == nsMsgViewSortType.bySize));
    setSortByMenuItemCheckState("sortByStatusMenuitem", (sortType == nsMsgViewSortType.byStatus));
    setSortByMenuItemCheckState("sortBySubjectMenuitem", (sortType == nsMsgViewSortType.bySubject));
    setSortByMenuItemCheckState("sortByThreadMenuitem", (sortType == nsMsgViewSortType.byThread));
    setSortByMenuItemCheckState("sortByUnreadMenuitem", (sortType == nsMsgViewSortType.byUnread));
 
    // the Sender / Recipient menu is dynamic
    setSortByMenuItemCheckState("sortBySenderOrRecipientMenuitem", (sortType == nsMsgViewSortType.byAuthor) || (sortType == nsMsgViewSortType.byRecipient));
    var senderOrRecipientMenuitem = document.getElementById("sortBySenderOrRecipientMenuitem");
    if (senderOrRecipientMenuitem) {
    	var currentFolder = gDBView.msgFolder;
	if (IsSpecialFolder(currentFolder, MSG_FOLDER_FLAG_SENTMAIL | MSG_FOLDER_FLAG_DRAFTS | MSG_FOLDER_FLAG_QUEUE)) {
	  senderOrRecipientMenuitem.setAttribute('label',gMessengerBundle.getString('recipientColumnHeader'));
	  senderOrRecipientMenuitem.setAttribute('accesskey',gMessengerBundle.getString('recipientAccessKey'));
        }
        else {
	  senderOrRecipientMenuitem.setAttribute('label',gMessengerBundle.getString('senderColumnHeader'));
	  senderOrRecipientMenuitem.setAttribute('accesskey',gMessengerBundle.getString('senderAccessKey'));
	}
    }
    var sortOrder = gDBView.sortOrder;

    setSortByMenuItemCheckState("sortAscending", (sortOrder == nsMsgViewSortOrder.ascending));
    setSortByMenuItemCheckState("sortDescending", (sortOrder == nsMsgViewSortOrder.descending));
}

function InitViewMessagesMenu()
{
  var allMenuItem = document.getElementById("viewAllMessagesMenuItem");
  var viewFlags = gDBView.viewFlags;
  var viewType = gDBView.viewType;

  if(allMenuItem)
      allMenuItem.setAttribute("checked",  (viewFlags & nsMsgViewFlagsType.kUnreadOnly) == 0 && (viewType == nsMsgViewType.eShowAllThreads));

  var unreadMenuItem = document.getElementById("viewUnreadMessagesMenuItem");
  if(unreadMenuItem)
      unreadMenuItem.setAttribute("checked", (viewFlags & nsMsgViewFlagsType.kUnreadOnly) != 0);

  var theadedMenuItem = document.getElementById("menu_showThreads");
  if (theadedMenuItem)
    theadedMenuItem.setAttribute("checked", (viewFlags & nsMsgViewFlagsType.kThreadedDisplay) != 0);
  document.commandDispatcher.updateCommands('create-menu-view');

  var theadsWithUnreadMenuItem = document.getElementById("viewThreadsWithUnreadMenuItem");
  if(theadsWithUnreadMenuItem)
      theadsWithUnreadMenuItem.setAttribute("checked", viewType == nsMsgViewType.eShowThreadsWithUnread);

  var watchedTheadsWithUnreadMenuItem = document.getElementById("viewWatchedThreadsWithUnreadMenuItem");
  if(watchedTheadsWithUnreadMenuItem)
      watchedTheadsWithUnreadMenuItem.setAttribute("checked", viewType == nsMsgViewType.eShowWatchedThreadsWithUnread);
  var ignoredTheadsMenuItem = document.getElementById("viewIgnoredThreadsMenuItem");
  if(ignoredTheadsMenuItem)
      ignoredTheadsMenuItem.setAttribute("checked", (viewFlags & nsMsgViewFlagsType.kShowIgnored) != 0);
}

function InitMessageMenu()
{
  var aMessage = GetFirstSelectedMessage();
  var isNews = false;
  if(aMessage) {
      isNews = IsNewsMessage(aMessage);
  }

  //We show reply to Newsgroups only for news messages.
  var replyNewsgroupMenuItem = document.getElementById("replyNewsgroupMainMenu");
  if(replyNewsgroupMenuItem)
  {
      replyNewsgroupMenuItem.setAttribute("hidden", isNews ? "" : "true");
  }

  //For mail messages we say reply. For news we say ReplyToSender.
  var replyMenuItem = document.getElementById("replyMainMenu");
  if(replyMenuItem)
  {
      replyMenuItem.setAttribute("hidden", !isNews ? "" : "true");
  }

  var replySenderMenuItem = document.getElementById("replySenderMainMenu");
  if(replySenderMenuItem)
  {
      replySenderMenuItem.setAttribute("hidden", isNews ? "" : "true");
  }

  // we only kill and watch threads for news
  var killThreadMenuItem = document.getElementById("killThread");
  if (killThreadMenuItem) {
      killThreadMenuItem.setAttribute("hidden", isNews ? "" : "true");
  }
  var watchThreadMenuItem = document.getElementById("watchThread");
  if (watchThreadMenuItem) {
      watchThreadMenuItem.setAttribute("hidden", isNews ? "" : "true");
  }

  //disable the move and copy menus only if there are no messages selected.
  var moveMenu = document.getElementById("moveMenu");
  if(moveMenu)
      moveMenu.setAttribute("disabled", !aMessage);

  var copyMenu = document.getElementById("copyMenu");
  if(copyMenu)
      copyMenu.setAttribute("disabled", !aMessage);

  document.commandDispatcher.updateCommands('create-menu-message');
}

function IsNewsMessage(messageUri)
{
    if (!messageUri) return false;
    return (messageUri.substring(0,14) == "news_message:/")
}

function InitMessageMark()
{
    InitMarkReadItem("markReadMenuItem");
    InitMarkReadItem("markReadToolbarItem");
    InitMarkFlaggedItem("markFlaggedMenuItem");
    InitMarkFlaggedItem("markFlaggedToolbarItem");
    document.commandDispatcher.updateCommands('create-menu-mark');
}

function InitMessageLabel()
{
dump("initing message label\n");
    document.commandDispatcher.updateCommands('create-menu-label');
  /* this code should probably get the label strings and change the menu labels */
}

function InitMarkReadItem(id)
{
    var areMessagesRead = SelectedMessagesAreRead();
    var item = document.getElementById(id);
    if(item)
        item.setAttribute("checked", areMessagesRead);
}

function InitMarkFlaggedItem(id)
{
    var areMessagesFlagged = SelectedMessagesAreFlagged();
    var item = document.getElementById(id);
    if(item)
        item.setAttribute("checked", areMessagesFlagged);
}

function SelectedMessagesAreRead()
{
    var isRead;
    try {
        isRead = gDBView.hdrForFirstSelectedMessage.isRead;
    }
    catch (ex) {
        isRead = false;
    }
    return isRead;
}

function SelectedMessagesAreFlagged()
{
    var isFlagged;
    try {
        isFlagged = gDBView.hdrForFirstSelectedMessage.isFlagged;
    }
    catch (ex) {
        isFlagged = false;
    }
    return isFlagged;
}

function getMsgToolbarMenu_init()
{
    document.commandDispatcher.updateCommands('create-menu-getMsgToolbar');
}

function GetFirstSelectedMsgFolder()
{
    var result = null;
    var selectedFolders = GetSelectedMsgFolders();
    if (selectedFolders.length > 0) {
        result = selectedFolders[0];
    }

    return result;
}

function GetWindowMediator()
{
    if (gWindowManagerInterface)
        return gWindowManagerInterface;

    var windowManager = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService();
    return (gWindowManagerInterface = windowManager.QueryInterface(Components.interfaces.nsIWindowMediator));
}

function GetInboxFolder(server)
{
    try {
        var rootFolder = server.RootFolder;
        var rootMsgFolder = rootFolder.QueryInterface(Components.interfaces.nsIMsgFolder);

        //now find Inbox
        var outNumFolders = new Object();
        var inboxFolder = rootMsgFolder.getFoldersWithFlag(0x1000, 1, outNumFolders);

        return inboxFolder.QueryInterface(Components.interfaces.nsIMsgFolder);
    }
    catch (ex) {
        dump(ex + "\n");
    }
    return null;
}

function GetMessagesForInboxOnServer(server)
{
  var inboxFolder = GetInboxFolder(server);
  if (!inboxFolder) return;

  var folders = new Array(1);
  folders[0] = inboxFolder;

  var compositeDataSource = GetCompositeDataSource("GetNewMessages");
  GetNewMessages(folders, compositeDataSource);
}

function MsgGetMessage()
{
  // if offline, prompt for getting messages
  if(CheckOnline()) {
    GetFolderMessages();
  }
  else {
    var option = PromptGetMessagesOffline();
    if(option == 0) {
      if (!gOfflineManager) 
        GetOfflineMgrService();
      gOfflineManager.goOnline(false /* sendUnsentMessages */, 
                               false /* playbackOfflineImapOperations */, 
                               msgWindow);
      GetFolderMessages();
    }
  }
}

function MsgGetMessagesForAllServers(defaultServer)
{
    // now log into any server
    try
    {
        var allServers = accountManager.allServers;

        for (var i=0;i<allServers.Count();i++)
        {
            var currentServer = allServers.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgIncomingServer);
            var protocolinfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + currentServer.type].getService(Components.interfaces.nsIMsgProtocolInfo);
            if (protocolinfo.canLoginAtStartUp && currentServer.loginAtStartUp)
            {
                if (defaultServer && defaultServer.equals(currentServer))
                {
                    dump(currentServer.serverURI + "...skipping, already opened\n");
                }
                else
                {
                    // Check to see if there are new messages on the server
                    currentServer.PerformBiff();
                }
            }
        }
    }
    catch(ex)
    {
        dump(ex + "\n");
    }
}

/**
  * Get messages for all those accounts which have the capability
  * of getting messages and have session password available i.e.,
  * curretnly logged in accounts.
  * if offline, prompt for getting messages.
  */
function MsgGetMessagesForAllAuthenticatedAccounts()
{
  if(CheckOnline()) {
    GetMessagesForAllAuthenticatedAccounts();
  }
  else {
    var option = PromptGetMessagesOffline();
    if(option == 0) {
      if (!gOfflineManager) 
        GetOfflineMgrService();
      gOfflineManager.goOnline(false /* sendUnsentMessages */, 
                               false /* playbackOfflineImapOperations */, 
                               msgWindow);
      GetMessagesForAllAuthenticatedAccounts();
    }
  }
}

/**
  * Get messages for the account selected from Menu dropdowns.
  * if offline, prompt for getting messages.
  */
function MsgGetMessagesForAccount(aEvent)
{
  if (!aEvent)
    return;

  if(CheckOnline()) {
    GetMessagesForAccount(aEvent);
  }
  else {
    var option = PromptGetMessagesOffline();
    if(option == 0) {
      if (!gOfflineManager) 
        GetOfflineMgrService();
      gOfflineManager.goOnline(false /* sendUnsentMessages */, 
                               false /* playbackOfflineImapOperations */, 
                               msgWindow);
      GetMessagesForAccount(aEvent);
    }
  }
}

// if offline, prompt for getNextNMessages
function MsgGetNextNMessages()
{
  var folder;
  
  if(CheckOnline()) {
    folder = GetFirstSelectedMsgFolder();
    if(folder) 
      GetNextNMessages(folder);
  }
  else {
    var option = PromptGetMessagesOffline();
    if(option == 0) {
      if (!gOfflineManager) 
        GetOfflineMgrService();
      gOfflineManager.goOnline(false /* sendUnsentMessages */, 
                               false /* playbackOfflineImapOperations */, 
                               msgWindow);
      folder = GetFirstSelectedMsgFolder();
      if(folder) {
        GetNextNMessages(folder);
      }
    }
  }   
}

function MsgDeleteMessage(reallyDelete, fromToolbar)
{
    var srcFolder = GetLoadedMsgFolder();
    // if from the toolbar, return right away if this is a news message
    // only allow cancel from the menu:  "Edit | Cancel / Delete Message"
    if (fromToolbar)
    {
        var folderResource = srcFolder.QueryInterface(Components.interfaces.nsIRDFResource);
        var uri = folderResource.Value;
        if (isNewsURI(uri)) {
            // if news, don't delete
            return;
        }
    }

    var compositeDataSource = GetCompositeDataSource("DeleteMessages");
    var messages = GetSelectedMessages();

    SetNextMessageAfterDelete();
    if (reallyDelete) {
        gDBView.doCommand(nsMsgViewCommandType.deleteNoTrash);
    }
    else {
        gDBView.doCommand(nsMsgViewCommandType.deleteMsg);
    }
}

function MsgCopyMessage(destFolder)
{
  try {
    // get the msg folder we're copying messages into
    var destUri = destFolder.getAttribute('id');
    var destResource = RDF.GetResource(destUri);
    var destMsgFolder = destResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    gDBView.doCommandWithFolder(nsMsgViewCommandType.copyMessages, destMsgFolder);
  }
  catch (ex) {
    dump("MsgCopyMessage failed: " + ex + "\n");
  }
}

function MsgMoveMessage(destFolder)
{
  try {
    // get the msg folder we're moving messages into
    var destUri = destFolder.getAttribute('id');
    var destResource = RDF.GetResource(destUri);
    var destMsgFolder = destResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    // we don't move news messages, we copy them
    if (isNewsURI(gDBView.msgFolder.URI)) {
      gDBView.doCommandWithFolder(nsMsgViewCommandType.copyMessages, destMsgFolder);
    }
    else {
      SetNextMessageAfterDelete();
      gDBView.doCommandWithFolder(nsMsgViewCommandType.moveMessages, destMsgFolder);
    }
  }
  catch (ex) {
    dump("MsgMoveMessage failed: " + ex + "\n");
  }
}

function MsgNewMessage(event)
{
  var loadedFolder = GetFirstSelectedMsgFolder();
  var messageArray = GetSelectedMessages();

  if (event && event.shiftKey)
    ComposeMessage(msgComposeType.New, msgComposeFormat.OppositeOfDefault, loadedFolder, messageArray);
  else
    ComposeMessage(msgComposeType.New, msgComposeFormat.Default, loadedFolder, messageArray);
}

function MsgReplyMessage(event)
{
  var loadedFolder = GetLoadedMsgFolder();
  var server = loadedFolder.server;

  if(server && server.type == "nntp")
    MsgReplyGroup(event);
  else
    MsgReplySender(event);
}

function MsgReplySender(event)
{
  var loadedFolder = GetLoadedMsgFolder();
  var messageArray = GetSelectedMessages();

  if (event && event.shiftKey)
    ComposeMessage(msgComposeType.ReplyToSender, msgComposeFormat.OppositeOfDefault, loadedFolder, messageArray);
  else
    ComposeMessage(msgComposeType.ReplyToSender, msgComposeFormat.Default, loadedFolder, messageArray);
}

function MsgReplyGroup(event)
{
  var loadedFolder = GetLoadedMsgFolder();
  var messageArray = GetSelectedMessages();

  if (event && event.shiftKey)
    ComposeMessage(msgComposeType.ReplyToGroup, msgComposeFormat.OppositeOfDefault, loadedFolder, messageArray);
  else
    ComposeMessage(msgComposeType.ReplyToGroup, msgComposeFormat.Default, loadedFolder, messageArray);
}

function MsgReplyToAllMessage(event)
{
  var loadedFolder = GetLoadedMsgFolder();
  var messageArray = GetSelectedMessages();

  if (event && event.shiftKey)
    ComposeMessage(msgComposeType.ReplyAll, msgComposeFormat.OppositeOfDefault, loadedFolder, messageArray);
  else
    ComposeMessage(msgComposeType.ReplyAll, msgComposeFormat.Default, loadedFolder, messageArray);
}

function MsgForwardMessage(event)
{
  var forwardType = 0;
  try {
      forwardType = gPrefs.getIntPref("mail.forward_message_mode");
  } catch (e) {dump ("failed to retrieve pref mail.forward_message_mode");}

  // mail.forward_message_mode could be 1, if the user migrated from 4.x
  // 1 (forward as quoted) is obsolete, so we treat is as forward inline
  // since that is more like forward as quoted then forward as attachment
  if (forwardType == 0)
      MsgForwardAsAttachment(event);
  else
      MsgForwardAsInline(event);
}

function MsgForwardAsAttachment(event)
{
  var loadedFolder = GetLoadedMsgFolder();
  var messageArray = GetSelectedMessages();

  //dump("\nMsgForwardAsAttachment from XUL\n");
  if (event && event.shiftKey)
    ComposeMessage(msgComposeType.ForwardAsAttachment,
                   msgComposeFormat.OppositeOfDefault, loadedFolder, messageArray);
  else
    ComposeMessage(msgComposeType.ForwardAsAttachment, msgComposeFormat.Default, loadedFolder, messageArray);
}

function MsgForwardAsInline(event)
{
  var loadedFolder = GetLoadedMsgFolder();
  var messageArray = GetSelectedMessages();

  //dump("\nMsgForwardAsInline from XUL\n");
  if (event && event.shiftKey)
    ComposeMessage(msgComposeType.ForwardInline,
                   msgComposeFormat.OppositeOfDefault, loadedFolder, messageArray);
  else
    ComposeMessage(msgComposeType.ForwardInline, msgComposeFormat.Default, loadedFolder, messageArray);
}


function MsgEditMessageAsNew()
{
    var loadedFolder = GetLoadedMsgFolder();
    var messageArray = GetSelectedMessages();
    ComposeMessage(msgComposeType.Template, msgComposeFormat.Default, loadedFolder, messageArray);
}

function MsgCreateFilter()
{
  var emailAddressNode;

  if (gCollapsedHeaderViewMode)
    emailAddressNode = document.getElementById("collapsedfromValue");
  else
    emailAddressNode = document.getElementById("expandedfromValue");
  
  if (emailAddressNode)
  {
     var emailAddress = emailAddressNode.getTextAttribute("emailAddress");
     if (emailAddress){
         top.MsgFilters(emailAddress);
     }
  }
}

function MsgHome(url)
{
  window.open(url, "_blank", "chrome,dependent=yes,all");
}

function MsgNewFolder(callBackFunctionName)
{
    var preselectedFolder = GetFirstSelectedMsgFolder();
    var dualUseFolders = true;
    var server = null;
    var destinationFolder = null;

    if (preselectedFolder)
    {
        try {
            server = preselectedFolder.server;
            if (server)
            {
                destinationFolder = getDestinationFolder(preselectedFolder, server);

                var imapServer =
                    server.QueryInterface(Components.interfaces.nsIImapIncomingServer);
                if (imapServer)
                    dualUseFolders = imapServer.dualUseFolders;
            }
        } catch (e) {
            dump ("Exception: dualUseFolders = true\n");
        }
    }

    CreateNewSubfolder("chrome://messenger/content/newFolderDialog.xul", destinationFolder, dualUseFolders, callBackFunctionName);
}


function getDestinationFolder(preselectedFolder, server)
{
    var destinationFolder = null;

    var isCreateSubfolders = preselectedFolder.canCreateSubfolders;
    if (!isCreateSubfolders)
    {
        var tmpDestFolder = server.RootFolder;
        destinationFolder
          = tmpDestFolder.QueryInterface(Components.interfaces.nsIMsgFolder);

        var verifyCreateSubfolders = null;
        if (destinationFolder)
            verifyCreateSubfolders = destinationFolder.canCreateSubfolders;

        // in case the server cannot have subfolders,
        // get default account and set its incoming server as parent folder
        if (!verifyCreateSubfolders)
        {
            try {
                var account = accountManager.defaultAccount;
                var defaultServer = account.incomingServer;
                var tmpDefaultFolder = defaultServer.RootFolder;
                var defaultFolder
                  = tmpDefaultFolder.QueryInterface(Components.interfaces.nsIMsgFolder);

                var checkCreateSubfolders = null;
                if (defaultFolder)
                    checkCreateSubfolders = defaultFolder.canCreateSubfolders;

                if (checkCreateSubfolders)
                    destinationFolder = defaultFolder;

            } catch (e) {
                dump ("Exception: defaultAccount Not Available\n");
            }
        }
    }
    else
        destinationFolder = preselectedFolder;

    return destinationFolder;
}

function MsgSubscribe()
{
    var preselectedFolder = GetFirstSelectedMsgFolder();
    Subscribe(preselectedFolder);
}

function ConfirmUnsubscribe(folder)
{
    if (!gMessengerBundle)
        gMessengerBundle = document.getElementById("bundle_messenger");

    var titleMsg = gMessengerBundle.getString("confirmUnsubscribeTitle");
    var dialogMsg = gMessengerBundle.getFormattedString("confirmUnsubscribeText",
                                        [folder.name], 1);

    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
    return promptService.confirm(window, titleMsg, dialogMsg);
}

function MsgUnsubscribe()
{
    var folder = GetFirstSelectedMsgFolder();
    if (ConfirmUnsubscribe(folder)) {
        UnSubscribe(folder);
    }
}

function MsgSaveAsFile()
{
    if (GetNumSelectedMessages() == 1) {
        SaveAsFile(GetFirstSelectedMessage());
    }
}


function MsgSaveAsTemplate()
{
    var folder = GetLoadedMsgFolder();
    if (GetNumSelectedMessages() == 1) {
        SaveAsTemplate(GetFirstSelectedMessage(), folder);
    }
}

function MsgOpenNewWindowForMsgHdr(hdr)
{
  MsgOpenNewWindowForFolder(hdr.folder.URI, hdr.messageKey);
}

function MsgOpenNewWindowForFolder(uri, key)
{
  var uriToOpen = uri;
  var keyToSelect = key;

  if (!uriToOpen)
  {
    var folder = GetLoadedMsgFolder();
    var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
    uriToOpen = folderResource.Value;
  }

  if (uriToOpen) {
    var layoutType = gPrefs.getIntPref("mail.pane_config");

    if(layoutType == 0)
      window.openDialog("chrome://messenger/content/messenger.xul", "_blank", "chrome,all,dialog=no", {uri: uriToOpen, key: keyToSelect});
    else
      window.openDialog("chrome://messenger/content/mail3PaneWindowVertLayout.xul", "_blank", "chrome,all,dialog=no", {uri: uriToOpen, key: keyToSelect});
  }
}

// passing in the view, so this will work for search and the thread pane
function MsgOpenSelectedMessages()
{
  var dbView = GetDBView();

  var indices = GetSelectedIndices(dbView);
  var numMessages = indices.length;

  for (var i = 0; i < numMessages; i++) {
    MsgOpenNewWindowForMessage(dbView.getURIForViewIndex(indices[i]),dbView.getFolderForViewIndex(indices[i]).URI);
  }
}

function MsgOpenNewWindowForMessage(messageUri, folderUri)
{
    var currentIndex;

    if (!messageUri || !folderUri) {
        var outlinerView = gDBView.QueryInterface(Components.interfaces.nsIOutlinerView);
        var outlinerSelection = outlinerView.selection;
        currentIndex = outlinerSelection.currentIndex;
    }

    if (!messageUri) {
        messageUri = gDBView.getURIForViewIndex(currentIndex);
    }

    if (!folderUri) {
        folderUri = gDBView.getFolderForViewIndex(currentIndex).URI;
    }

    // be sure to pass in the current view....
    if (messageUri && folderUri) {
        window.openDialog( "chrome://messenger/content/messageWindow.xul", "_blank", "chrome,all,dialog=no", messageUri, folderUri, gDBView );
    }
}

function CloseMailWindow()
{
    //dump("\nClose from XUL\nDo something...\n");
    window.close();
}

function MsgMarkMsgAsRead(markRead)
{
    if (!markRead) {
        markRead = !SelectedMessagesAreRead();
    }
    MarkSelectedMessagesRead(markRead);
}

function MsgMarkAsFlagged(markFlagged)
{
    if (!markFlagged) {
        markFlagged = !SelectedMessagesAreFlagged();
    }
    MarkSelectedMessagesFlagged(markFlagged);
}

function MsgMarkAllRead()
{
    var compositeDataSource = GetCompositeDataSource("MarkAllMessagesRead");
    var folder = GetLoadedMsgFolder();

    if(folder)
        MarkAllMessagesRead(compositeDataSource, folder);
}

function MsgDownloadFlagged()
{
  gDBView.doCommand(nsMsgViewCommandType.downloadFlaggedForOffline);
}

function MsgDownloadSelected()
{
  gDBView.doCommand(nsMsgViewCommandType.downloadSelectedForOffline);
}

function MsgMarkThreadAsRead()
{
    var uri = GetFirstSelectedMessage();
    if (uri) {
        MarkThreadAsRead(uri);
    }
}

function MsgViewPageSource()
{
    var messages = GetSelectedMessages();
    ViewPageSource(messages);
}

function MsgFind()
{
  var contentWindow = window.top._content;

  // from utilityOverlay.js
  findInPage(getMessageBrowser(), contentWindow, contentWindow)
}

function MsgFindAgain()
{
  var contentWindow = window.top._content;
  findAgainInPage(getMessageBrowser(), contentWindow, contentWindow)
}

function MsgCanFindAgain()
{
  return canFindAgainInPage();
}

function MsgSearchMessages()
{
    var preselectedFolder = GetFirstSelectedMsgFolder();
    var windowManagerInterface = GetWindowMediator();
    var searchWindow = windowManagerInterface.getMostRecentWindow("mailnews:search");

    if (searchWindow)
        searchWindow.focus();
    else
        window.openDialog("chrome://messenger/content/SearchDialog.xul", "", 
                          "chrome,resizable,centerscreen,dialog=no", { folder: preselectedFolder });
}

function MsgFilters(emailAddress)
{
    var preselectedFolder = GetFirstSelectedMsgFolder();
    var windowManagerInterface = GetWindowMediator();
    var filterList = windowManagerInterface.getMostRecentWindow("mailnews:filterlist");
    var args = { folder: preselectedFolder };
    if (emailAddress)
      args.prefillValue = emailAddress;

    if (filterList)
    {
      var filterEditor = windowManagerInterface.getMostRecentWindow("mailnews:filtereditor");

      // If the filtereditor is open, then we focus that because it is modal and
      // thus blocking the filterlist from focusing.
      if (filterEditor)
      {
        if(emailAddress)
        {
          if(!gPromptService) {
            gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
            gPromptService = gPromptService.QueryInterface(Components.interfaces.nsIPromptService);
          }

          gPromptService.alert(window,
                gBrandBundle.getString("brandShortName"),
                gMessengerBundle.getString('cannotHaveTwoFilterRulesText')
          );
        }
        filterEditor.focus();
      }
      else
      filterList.openPrefillOnExistingFilterList(emailAddress);
    }
    else
      window.openDialog("chrome://messenger/content/FilterListDialog.xul", "", 
                          "chrome,resizable,centerscreen,dialog=yes", args);
}

function MsgViewAllHeaders()
{
    gPrefs.setIntPref("mail.show_headers",2);
    MsgReload();
    return true;
}

function MsgViewNormalHeaders()
{
    gPrefs.setIntPref("mail.show_headers",1);
    MsgReload();
    return true;
}

function MsgViewBriefHeaders()
{
    gPrefs.setIntPref("mail.show_headers",0);
    MsgReload();
    return true;
}

function MsgReload()
{
    ReloadMessage();
}

function MsgStop()
{
    StopUrls();
}

function MsgSendUnsentMsgs()
{
  // if offline, prompt for sendUnsentMessages
  if(CheckOnline()) {
    SendUnsentMessages();    
  }
  else {
    var option = PromptSendMessagesOffline();
    if(option == 0) {
      if (!gOfflineManager) 
        GetOfflineMgrService();
      gOfflineManager.goOnline(false /* sendUnsentMessages */, 
                               false /* playbackOfflineImapOperations */, 
                               msgWindow);
      SendUnsentMessages();
    }
  }
}

function PrintEnginePrint()
{
    var messageList = GetSelectedMessages();
    var numMessages = messageList.length;

    if (numMessages == 0) {
        dump("PrintEnginePrint(): No messages selected.\n");
        return false;
    }

    printEngineWindow = window.openDialog("chrome://messenger/content/msgPrintEngine.xul",
                                          "",
                                          "chrome,dialog=no,all",
                                          numMessages, messageList, statusFeedback);
    return true;
}

function IsMailFolderSelected()
{
    var selectedFolders = GetSelectedMsgFolders();
    var numFolders = selectedFolders.length;
    if(numFolders !=1)
        return false;

    var folder = selectedFolders[0];
    if (!folder)
        return false;

    var server = folder.server;
    var serverType = server.type;

    if((serverType == "nntp"))
        return false;
    else return true;
}

function IsGetNewMessagesEnabled()
{
    var selectedFolders = GetSelectedMsgFolders();
    var numFolders = selectedFolders.length;
    if(numFolders !=1)
        return false;

    var folder = selectedFolders[0];
    if (!folder)
        return false;

    var server = folder.server;
    var isServer = folder.isServer;
    var serverType = server.type;

    if(isServer && (serverType == "nntp"))
        return false;
    else if(serverType == "none")
        return false;
    else
        return true;
}

function IsGetNextNMessagesEnabled()
{
    var selectedFolders = GetSelectedMsgFolders();
    var numFolders = selectedFolders.length;
    if(numFolders !=1)
        return false;

    var folder = selectedFolders[0];
    if (!folder)
        return false;

    var server = folder.server;
    var serverType = server.type;

    var menuItem = document.getElementById("menu_getnextnmsg");
    if((serverType == "nntp")) {
        var newsServer = server.QueryInterface(Components.interfaces.nsINntpIncomingServer);
        var menuLabel = gMessengerBundle.getFormattedString("getNextNMessages",
                                                            [ newsServer.maxArticles ]);
        menuItem.setAttribute("label",menuLabel);
        menuItem.setAttribute("hidden","false");
        return true;
    }
    else {
        menuItem.setAttribute("hidden","true");
        return false;
    }
}

function IsEmptyTrashEnabled()
{
    return IsMailFolderSelected();
}

function IsCompactFolderEnabled()
{
    var folderOutliner = GetFolderOutliner();
    var startIndex = {};
    var endIndex = {};
    folderOutliner.outlinerBoxObject.selection.getRangeAt(0, startIndex, endIndex);
    if (startIndex.value < 0)
         return false;

    var folderResource = GetFolderResource(folderOutliner, startIndex.value);
    if (! folderResource)
         return false;

    return GetFolderAttribute(folderOutliner, folderResource, "CanCompact") == "true" && isCommandEnabled("cmd_compactFolder");
}

var gDeleteButton = null;
var gMarkButton = null;

function SetUpToolbarButtons(uri)
{
    // dump("SetUpToolbarButtons("+uri+")\n");

    // eventually, we might want to set up the toolbar differently for imap,
    // pop, and news.  for now, just tweak it based on if it is news or not.
    var forNews = isNewsURI(uri);

    if(!gMarkButton) gMarkButton = document.getElementById("button-mark");
    if(!gDeleteButton) gDeleteButton = document.getElementById("button-delete");

    var buttonToHide = null;
    var buttonToShow = null;

    if (forNews) {
        buttonToHide = gDeleteButton;
        buttonToShow = gMarkButton;
    }
    else {
        buttonToHide = gMarkButton;
        buttonToShow = gDeleteButton;
    }

    if (buttonToHide) {
        buttonToHide.setAttribute('hidden',true);
    }
    if (buttonToShow) {
        buttonToShow.removeAttribute('hidden');
    }
}

var gMessageBrowser;

function getMessageBrowser()
{
  if (!gMessageBrowser)
    gMessageBrowser = document.getElementById("messagepane");

  return gMessageBrowser;
}

function getMarkupDocumentViewer()
{
  return getMessageBrowser().markupDocumentViewer;
}

function MsgSynchronizeOffline()
{
    dump("in MsgSynchronize() \n"); 
    window.openDialog("chrome://messenger/content/msgSynchronize.xul",
          "", "centerscreen,chrome,modal,titlebar,resizable=yes",{msgWindow:msgWindow}); 		     
}


function MsgMarkByDate() {}
function MsgOpenAttachment() {}
function MsgUpdateMsgCount() {}
function MsgImport() {}
function MsgSynchronize() {}
function MsgGetSelectedMsg() {}
function MsgGetFlaggedMsg() {}
function MsgSelectThread() {}
function MsgSelectFlaggedMsg() {}
function MsgShowFolders(){}
function MsgShowLocationbar() {}
function MsgViewAttachInline() {}
function MsgWrapLongLines() {}
function MsgIncreaseFont() {}
function MsgDecreaseFont() {}
function MsgShowImages() {}
function MsgRefresh() {}
function MsgViewPageInfo() {}
function MsgFirstUnreadMessage() {}
function MsgFirstFlaggedMessage() {}
function MsgGoBack() {}
function MsgGoForward() {}
function MsgAddSenderToAddressBook() {}
function MsgAddAllToAddressBook() {}

function SpaceHit()
{
  var contentWindow = window.top._content;
  var oldScrollY = contentWindow.scrollY;

  contentWindow.scrollByPages(1);

  // if at the end of the message, go to the next one
  if (oldScrollY == contentWindow.scrollY) {
    goDoCommand('cmd_nextUnreadMsg');
  }
}

function IsAccountOfflineEnabled()
{
  var selectedFolders = GetSelectedMsgFolders();

  if (selectedFolders && (selectedFolders.length == 1))
      return selectedFolders[0].supportsOffline;
     
  return false;
}

// init nsIPromptService and strings
function InitPrompts()
{
  if(!gPromptService) {
    gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
    gPromptService = gPromptService.QueryInterface(Components.interfaces.nsIPromptService);
  }
  if (!gOfflinePromptsBundle) 
    gOfflinePromptsBundle = document.getElementById("bundle_offlinePrompts");
}

// prompt for getting messages when offline
function PromptGetMessagesOffline()
{
  InitPrompts();
  if (gPromptService) {
    var buttonPressed = {value:0};
    var checkValue = {value:false};
    gPromptService.confirmEx(window, 
                            gOfflinePromptsBundle.getString('getMessagesOfflineWindowTitle'), 
                            gOfflinePromptsBundle.getString('getMessagesOfflineLabel'),
                            (gPromptService.BUTTON_TITLE_IS_STRING * gPromptService.BUTTON_POS_0) +
                            (gPromptService.BUTTON_TITLE_CANCEL * gPromptService.BUTTON_POS_1),
                            gOfflinePromptsBundle.getString('getMessagesOfflineGoButtonLabel'),
                            null, null, null, checkValue, buttonPressed);
    if(buttonPressed) {
      return buttonPressed.value;
    }
  }
}

// prompt for sending messages when offline
function PromptSendMessagesOffline()
{
  InitPrompts();
  if (gPromptService) {
    var buttonPressed = {value:0};
    var checkValue= {value:false};
    gPromptService.confirmEx(window, 
                            gOfflinePromptsBundle.getString('sendMessagesOfflineWindowTitle'), 
                            gOfflinePromptsBundle.getString('sendMessagesOfflineLabel'),
                            (gPromptService.BUTTON_TITLE_IS_STRING * gPromptService.BUTTON_POS_0) +
                            (gPromptService.BUTTON_TITLE_CANCEL * gPromptService.BUTTON_POS_1),
                            gOfflinePromptsBundle.getString('sendMessagesOfflineGoButtonLabel'),
                            null, null, null, checkValue, buttonPressed);
    if(buttonPressed) {
      return buttonPressed.value;
    }
  }
}

function GetFolderMessages()
{
  var folders = GetSelectedMsgFolders();
  var compositeDataSource = GetCompositeDataSource("GetNewMessages");
  GetNewMessages(folders, compositeDataSource);
}

function SendUnsentMessages()
{
  var am = Components.classes["@mozilla.org/messenger/account-manager;1"]
               .getService(Components.interfaces.nsIMsgAccountManager);
  var msgSendlater = Components.classes["@mozilla.org/messengercompose/sendlater;1"]
               .getService(Components.interfaces.nsIMsgSendLater);
  var identitiesCount, allIdentities, currentIdentity, numMessages, msgFolder;

  if(am) { 
    allIdentities = am.allIdentities;
    identitiesCount = allIdentities.Count();
    for (var i = 0; i < identitiesCount; i++) {
      currentIdentity = allIdentities.QueryElementAt(i, Components.interfaces.nsIMsgIdentity);
      msgFolder = msgSendlater.getUnsentMessagesFolder(currentIdentity);
      if(msgFolder) {
        numMessages = msgFolder.getTotalMessages(false /* include subfolders */);
        if(numMessages > 0) {
          messenger.SendUnsentMessages(currentIdentity);
          // right now, all identities point to the same unsent messages
          // folder, so to avoid sending multiple copies of the
          // unsent messages, we only call messenger.SendUnsentMessages() once
          // see bug #89150 for details
          break;
        }
      }
    } 
  }
}

function GetMessagesForAllAuthenticatedAccounts()
{
   try {
    var allServers = accountManager.allServers;
    for (var i=0;i<allServers.Count();i++) {
      var currentServer = allServers.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgIncomingServer);
      var protocolinfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" +
                           currentServer.type].getService(Components.interfaces.nsIMsgProtocolInfo);
      if (protocolinfo.canGetMessages && currentServer.password) {
        // Get new messages now
        GetMessagesForInboxOnServer(currentServer);
      }
    }
  }
  catch(ex) {
    dump(ex + "\n");
  }
}

function GetMessagesForAccount(aEvent)
{
  var uri = aEvent.target.id;
  var server = GetServer(uri);
  GetMessagesForInboxOnServer(server);
  aEvent.preventBubble();
}


function CommandUpdate_UndoRedo()
{
    ShowMenuItem("menu_undo", true);
    EnableMenuItem("menu_undo", SetupUndoRedoCommand("cmd_undo"));
    ShowMenuItem("menu_redo", true);
    EnableMenuItem("menu_redo", SetupUndoRedoCommand("cmd_redo"));
}

function SetupUndoRedoCommand(command)
{
    var canUndoOrRedo = false;
    var txnType = 0;

    if (command == "cmd_undo")
    {
        canUndoOrRedo = messenger.CanUndo();
        txnType = messenger.GetUndoTransactionType();
    }
    else
    {
        canUndoOrRedo = messenger.CanRedo();
        txnType = messenger.GetRedoTransactionType();
    }

    if (canUndoOrRedo)
    {
        switch (txnType)
        {
        default:
        case 0:
            goSetMenuValue(command, 'valueDefault');
            break;
        case 1:
            goSetMenuValue(command, 'valueDeleteMsg');
            break;
        case 2:
            goSetMenuValue(command, 'valueMoveMsg');
            break;
        case 3:
            goSetMenuValue(command, 'valueCopyMsg');
            break;
        }
    }
    else
    {
        goSetMenuValue(command, 'valueDefault');
    }
    return canUndoOrRedo;
}

