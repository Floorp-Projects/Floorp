/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributors(s):
 *   Jan Varga <varga@utcru.sk>
 *   Håkan Waara (hwaara@chello.se)
 */

var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
var gMessengerBundle = document.getElementById("bundle_messenger");

// Controller object for folder pane
var FolderPaneController =
{
   supportsCommand: function(command)
	{
		switch ( command )
		{
			case "cmd_delete":
			case "button_delete":
			case "cmd_selectAll":
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return true;
				
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
        //		dump("FolderPaneController.IsCommandEnabled(" + command + ")\n");
		switch ( command )
		{
			case "cmd_selectAll":
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return false;
			case "cmd_delete":
			case "button_delete":
				if ( command == "cmd_delete" )
					goSetMenuValue(command, 'valueFolder');
                                var folderOutliner = GetFolderOutliner();
                                var startIndex = {};
                                var endIndex = {};
                                folderOutliner.outlinerBoxObject.selection.getRangeAt(0, startIndex, endIndex);
                                if (startIndex.value >= 0) {
                                        var canDeleteThisFolder;
					var specialFolder = null;
					var isServer = null;
					var serverType = null;
					try {
                                                var folderResource = GetFolderResource(folderOutliner, startIndex.value);
                                                specialFolder = GetFolderAttribute(folderOutliner, folderResource, "SpecialFolder");
                                                isServer = GetFolderAttribute(folderOutliner, folderResource, "IsServer");
                                                serverType = GetFolderAttribute(folderOutliner, folderResource, "ServerType");
                                                if (serverType == "nntp") {
			     	                        if ( command == "cmd_delete" ) {
					                        goSetMenuValue(command, 'valueNewsgroup');
				    	                        goSetAccessKey(command, 'valueNewsgroupAccessKey');
                                                        }
                                                }
					}
					catch (ex) {
						//dump("specialFolder failure: " + ex + "\n");
					}
                                        if (specialFolder == "Inbox" || specialFolder == "Trash" || isServer == "true")
                                                canDeleteThisFolder = false;
                                        else
                                                canDeleteThisFolder = true;
                                        return canDeleteThisFolder && isCommandEnabled(command);
                                }
				else
					return false;

			default:
				return false;
		}
	},

	doCommand: function(command)
	{
    // if the user invoked a key short cut then it is possible that we got here for a command which is
    // really disabled. kick out if the command should be disabled.
    if (!this.isCommandEnabled(command)) return;

		switch ( command )
		{
			case "cmd_delete":
			case "button_delete":
				MsgDeleteFolder();
				break;
		}
	},
	
	onEvent: function(event)
	{
		// on blur events set the menu item texts back to the normal values
		if ( event == 'blur' )
        {
			goSetMenuValue('cmd_delete', 'valueDefault');
        }
	}
};


// Controller object for thread pane
var ThreadPaneController =
{
   supportsCommand: function(command)
	{
		switch ( command )
		{
			case "cmd_selectAll":
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return true;
				
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
		switch ( command )
		{
			case "cmd_selectAll":
				return true;
			
			case "cmd_cut":
			case "cmd_copy":
			case "cmd_paste":
				return false;

			default:
				return false;
		}
	},

	doCommand: function(command)
	{
    // if the user invoked a key short cut then it is possible that we got here for a command which is
    // really disabled. kick out if the command should be disabled.
    if (!this.isCommandEnabled(command)) return;
    if (!gDBView) return;

		switch ( command )
		{
			case "cmd_selectAll":
                // if in threaded mode, the view will expand all before selecting all
                gDBView.doCommand(nsMsgViewCommandType.selectAll)
                if (gDBView.numSelected != 1) {
                    ClearMessagePane();
                }
                break;
		}
	},
	
	onEvent: function(event)
	{
	}
};

// DefaultController object (handles commands when one of the trees does not have focus)
var DefaultController =
{
   supportsCommand: function(command)
	{

		switch ( command )
		{
			case "cmd_close":
			case "cmd_reply":
			case "button_reply":
			case "cmd_replySender":
			case "cmd_replyGroup":
			case "cmd_replyall":
			case "button_replyall":
			case "cmd_forward":
			case "button_forward":
			case "cmd_forwardInline":
			case "cmd_forwardAttachment":
			case "cmd_editAsNew":
			case "cmd_delete":
			case "button_delete":
			case "cmd_shiftDelete":
			case "cmd_editDraft":
			case "cmd_nextMsg":
      case "button_next":
			case "cmd_nextUnreadMsg":
			case "cmd_nextFlaggedMsg":
			case "cmd_nextUnreadThread":
			case "cmd_previousMsg":
			case "cmd_previousUnreadMsg":
			case "cmd_previousFlaggedMsg":
			case "cmd_viewAllMsgs":
			case "cmd_viewUnreadMsgs":
      case "cmd_viewThreadsWithUnread":
      case "cmd_viewWatchedThreadsWithUnread":
      case "cmd_viewIgnoredThreads":
      case "cmd_undo":
      case "cmd_redo":
			case "cmd_expandAllThreads":
			case "cmd_collapseAllThreads":
			case "cmd_renameFolder":
			case "cmd_sendUnsentMsgs":
			case "cmd_openMessage":
      case "button_print":
			case "cmd_print":
			case "cmd_saveAsFile":
			case "cmd_saveAsTemplate":
            case "cmd_properties":
			case "cmd_viewPageSource":
			case "cmd_setFolderCharset":
			case "cmd_reload":
      case "button_getNewMessages":
			case "cmd_getNewMessages":
      case "cmd_getMsgsForAuthAccounts":
			case "cmd_getNextNMessages":
			case "cmd_find":
			case "cmd_findAgain":
      case "button_mark":
			case "cmd_markAsRead":
			case "cmd_markAllRead":
			case "cmd_markThreadAsRead":
			case "cmd_markAsFlagged":
      case "button_file":
			case "cmd_file":
			case "cmd_emptyTrash":
			case "cmd_compactFolder":
			case "cmd_sortByThread":
      case "cmd_downloadFlagged":
      case "cmd_downloadSelected":
	  case "cmd_settingsOffline":
      case "cmd_synchronizeOffline":
      case "cmd_close":
      case "cmd_selectThread":
      case "cmd_selectFlagged":
				return true;

      case "cmd_watchThread":
      case "cmd_killThread":
        return(isNewsURI(GetFirstSelectedMessage()));

			default:
				return false;
		}
	},

  isCommandEnabled: function(command)
  {
    var enabled = new Object();
    enabled.value = false;
    var checkStatus = new Object();
//    dump('entering is command enabled for: ' + command + '\n');
    switch ( command )
    {
      case "button_delete":
      case "cmd_delete":
        var uri = GetFirstSelectedMessage();
        if ( GetNumSelectedMessages() < 2 ) 
        {
          if (IsNewsMessage(uri))
          {
            goSetMenuValue(command, 'valueNewsMessage');
            goSetAccessKey(command, 'valueNewsMessageAccessKey');
          }
          else
          {
            goSetMenuValue(command, 'valueMessage');
            goSetAccessKey(command, 'valueMessageAccessKey');
          }
        }
        else 
        {
          if (IsNewsMessage(uri)) 
          {
            goSetMenuValue(command, 'valueNewsMessage');
            goSetAccessKey(command, 'valueNewsMessageAccessKey');
          }
          else 
          {
            goSetMenuValue(command, 'valueMessages');
            goSetAccessKey(command, 'valueMessagesAccessKey');
          }
        }
        if (gDBView)
          gDBView.getCommandStatus(nsMsgViewCommandType.deleteMsg, enabled, checkStatus);
        return enabled.value;
      case "cmd_shiftDelete":
        if (gDBView)
          gDBView.getCommandStatus(nsMsgViewCommandType.deleteNoTrash, enabled, checkStatus);
        return enabled.value;
      case "cmd_killThread":
        return ((GetNumSelectedMessages() == 1) && MailAreaHasFocus() && IsViewNavigationItemEnabled());
      case "cmd_watchThread":
        if ((GetNumSelectedMessages() == 1) && gDBView)
          gDBView.getCommandStatus(nsMsgViewCommandType.toggleThreadWatched, enabled, checkStatus);
        return enabled.value;
      case "cmd_reply":
      case "button_reply":
      case "cmd_replySender":
      case "cmd_replyGroup":
      case "cmd_replyall":
      case "button_replyall":
      case "cmd_forward":
      case "button_forward":
      case "cmd_forwardInline":
      case "cmd_forwardAttachment":
      case "cmd_editAsNew":
      case "cmd_openMessage":
      case "button_print":
      case "cmd_print":
      case "cmd_saveAsFile":
      case "cmd_saveAsTemplate":
      case "cmd_viewPageSource":
      case "cmd_reload":
	      if ( GetNumSelectedMessages() > 0)
        {
          if (gDBView)
          {
             gDBView.getCommandStatus(nsMsgViewCommandType.cmdRequiringMsgBody, enabled, checkStatus);
              return enabled.value;
          }
        }
        return false;
      case "cmd_markThreadAsRead":
      case "cmd_markAsFlagged":
      case "button_file":
      case "cmd_file":
        return ( GetNumSelectedMessages() > 0 );
      case "cmd_editDraft":
                return (gIsEditableMsgFolder && (GetNumSelectedMessages() > 0));
      case "cmd_nextMsg":
      case "button_next":
      case "cmd_nextUnreadMsg":
      case "cmd_nextUnreadThread":
      case "cmd_previousMsg":
      case "cmd_previousUnreadMsg":
        return MailAreaHasFocus() && IsViewNavigationItemEnabled();
      case "button_mark":
      case "cmd_markAsRead":
        if(!MailAreaHasFocus())
          return false;
        else
          return(GetNumSelectedMessages() > 0);
      case "cmd_markAllRead":
        return(MailAreaHasFocus() && IsFolderSelected());
      case "cmd_find":
      case "cmd_findAgain":
        return IsFindEnabled();
        break;
      // these are enabled on when we are in threaded mode
      case "cmd_selectThread":
        if (GetNumSelectedMessages() <= 0) return false;
      case "cmd_expandAllThreads":
      case "cmd_collapseAllThreads":
        if (!gDBView) return false;
          return (gDBView.sortType == nsMsgViewSortType.byThread);
        break;
      case "cmd_nextFlaggedMsg":
      case "cmd_previousFlaggedMsg":
        return IsViewNavigationItemEnabled();
      case "cmd_viewAllMsgs":
      case "cmd_sortByThread":
      case "cmd_viewUnreadMsgs":
      case "cmd_viewThreadsWithUnread":
      case "cmd_viewWatchedThreadsWithUnread":
      case "cmd_viewIgnoredThreads":
        return true;
      case "cmd_undo":
      case "cmd_redo":
          return SetupUndoRedoCommand(command);
      case "cmd_renameFolder":
        return IsRenameFolderEnabled();
      case "cmd_sendUnsentMsgs":
        return IsSendUnsentMsgsEnabled(null);
      case "cmd_properties":
        return IsPropertiesEnabled();
      case "button_getNewMessages":
      case "cmd_getNewMessages":
      case "cmd_getMsgsForAuthAccounts":
        return IsGetNewMessagesEnabled();
      case "cmd_getNextNMessages":
        return IsGetNextNMessagesEnabled();
      case "cmd_emptyTrash":
        return IsEmptyTrashEnabled();
      case "cmd_compactFolder":
        return IsCompactFolderEnabled();
      case "cmd_setFolderCharset":
        return IsFolderCharsetEnabled();
      case "cmd_close":
        return true;
      case "cmd_downloadFlagged":
        return(CheckOnline());
      case "cmd_downloadSelected":
        return(MailAreaHasFocus() && IsFolderSelected() && CheckOnline() && GetNumSelectedMessages() > 0);
      case "cmd_synchronizeOffline":
        return IsAccountOfflineEnabled();       
      case "cmd_settingsOffline":
        return (MailAreaHasFocus() && IsAccountOfflineEnabled());
      case "cmd_selectFlagged":
        // disable select flagged until I finish the code in nsMsgDBView.cpp
        return false;
      default:
        return false;
    }
    return false;
  },

	doCommand: function(command)
	{
    // if the user invoked a key short cut then it is possible that we got here for a command which is
    // really disabled. kick out if the command should be disabled.
    if (!this.isCommandEnabled(command)) return;

		switch ( command )
		{
			case "cmd_close":
				CloseMailWindow();
				break;
      case "button_getNewMessages":
			case "cmd_getNewMessages":
				MsgGetMessage();
				break;
      case "cmd_getMsgsForAuthAccounts":
        MsgGetMessage();
        MsgGetMessagesForAllAuthenticatedAccounts();
        break;
			case "cmd_getNextNMessages":
				MsgGetNextNMessages();
				break;
			case "cmd_reply":
				MsgReplyMessage(null);
				break;
			case "cmd_replySender":
				MsgReplySender(null);
				break;
			case "cmd_replyGroup":
				MsgReplyGroup(null);
				break;
			case "cmd_replyall":
				MsgReplyToAllMessage(null);
				break;
			case "cmd_forward":
				MsgForwardMessage(null);
				break;
			case "cmd_forwardInline":
				MsgForwardAsInline(null);
				break;
			case "cmd_forwardAttachment":
				MsgForwardAsAttachment(null);
				break;
			case "cmd_editAsNew":
				MsgEditMessageAsNew();
				break;
			case "button_delete":
			case "cmd_delete":
        SetNextMessageAfterDelete();
        gDBView.doCommand(nsMsgViewCommandType.deleteMsg);
				break;
			case "cmd_shiftDelete":
        SetNextMessageAfterDelete();
        gDBView.doCommand(nsMsgViewCommandType.deleteNoTrash);
				break;
      case "cmd_killThread":
        /* kill thread kills the thread and then does a next unread */
      	GoNextMessage(nsMsgNavigationType.toggleThreadKilled, true);
        break;
      case "cmd_watchThread":
        gDBView.doCommand(nsMsgViewCommandType.toggleThreadWatched);
        break;
			case "cmd_editDraft":
                if (gDBView.numSelected >= 0)
                    MsgComposeDraftMessage();
				break;
			case "cmd_nextUnreadMsg":
				MsgNextUnreadMessage();
				break;
			case "cmd_nextUnreadThread":
				MsgNextUnreadThread();
				break;
			case "cmd_nextMsg":
				MsgNextMessage();
				break;
			case "cmd_nextFlaggedMsg":
				MsgNextFlaggedMessage();
				break;
			case "cmd_previousMsg":
				MsgPreviousMessage();
				break;
			case "cmd_previousUnreadMsg":
				MsgPreviousUnreadMessage();
				break;
			case "cmd_previousFlaggedMsg":
				MsgPreviousFlaggedMessage();
				break;
			case "cmd_sortByThread":
				MsgSortByThread();
				break;
			case "cmd_viewAllMsgs":
      case "cmd_viewThreadsWithUnread":
      case "cmd_viewWatchedThreadsWithUnread":
			case "cmd_viewUnreadMsgs":
      case "cmd_viewIgnoredThreads":
				SwitchView(command);
				break;
			case "cmd_undo":
				messenger.Undo(msgWindow);
				break;
			case "cmd_redo":
				messenger.Redo(msgWindow);
				break;
			case "cmd_expandAllThreads":
                gDBView.doCommand(nsMsgViewCommandType.expandAll);
				break;
			case "cmd_collapseAllThreads":
                gDBView.doCommand(nsMsgViewCommandType.collapseAll);
				break;
			case "cmd_renameFolder":
				MsgRenameFolder();
				return;
			case "cmd_sendUnsentMsgs":
				MsgSendUnsentMsgs();
				return;
			case "cmd_openMessage":
                MsgOpenSelectedMessages();
				return;
			case "cmd_print":
				PrintEnginePrint();
				return;
			case "cmd_saveAsFile":
				MsgSaveAsFile();
				return;
			case "cmd_saveAsTemplate":
				MsgSaveAsTemplate();
				return;
			case "cmd_viewPageSource":
				MsgViewPageSource();
				return;
			case "cmd_setFolderCharset":
				MsgSetFolderCharset();
				return;
			case "cmd_reload":
				MsgReload();
				return;
			case "cmd_find":
				MsgFind();
				return;
			case "cmd_findAgain":
				MsgFindAgain();
				return;
            case "cmd_properties":
                MsgFolderProperties();
                return;
      case "button_mark":
			case "cmd_markAsRead":
				MsgMarkMsgAsRead(null);
				return;
			case "cmd_markThreadAsRead":
				MsgMarkThreadAsRead();
				return;
			case "cmd_markAllRead":
                gDBView.doCommand(nsMsgViewCommandType.markAllRead);
				return;
			case "cmd_markAsFlagged":
				MsgMarkAsFlagged(null);
				return;
			case "cmd_emptyTrash":
				MsgEmptyTrash();
				return;
			case "cmd_compactFolder":
				MsgCompactFolder(true);
				return;
            case "cmd_downloadFlagged":
                MsgDownloadFlagged();
                break;
            case "cmd_downloadSelected":
                MsgDownloadSelected();
                break;
            case "cmd_synchronizeOffline":
                MsgSynchronizeOffline();
                break;
            case "cmd_settingsOffline":
                MsgSettingsOffline();
                break;
            case "cmd_selectThread":
                gDBView.doCommand(nsMsgViewCommandType.selectThread);
                break;
            case "cmd_selectFlagged":
                gDBView.doCommand(nsMsgViewCommandType.selectFlagged);
                break;
		}
	},
	
	onEvent: function(event)
	{
		// on blur events set the menu item texts back to the normal values
		if ( event == 'blur' )
        {
			goSetMenuValue('cmd_delete', 'valueDefault');
            goSetMenuValue('cmd_undo', 'valueDefault');
            goSetMenuValue('cmd_redo', 'valueDefault');
        }
	}
};

function MailAreaHasFocus()
{
	//Input and TextAreas should get access to the keys that cause these commands.
	//Currently if we don't do this then we will steal the key away and you can't type them
	//in these controls. This is a bug that should be fixed and when it is we can get rid of
	//this.
	var focusedElement = top.document.commandDispatcher.focusedElement;
	if(focusedElement)
	{
		var name = focusedElement.localName;
		return ((name != "INPUT") && (name != "TEXTAREA"));
	}
	return true;
}

function GetNumSelectedMessages()
{
    try {
        return gDBView.numSelected;
    }
    catch (ex) {
        return 0;
    }
}

var lastFocusedElement=null;

function FocusRingUpdate_Mail()
{
  //dump ('entering focus ring update\n');
  var currentFocusedElement = WhichPaneHasFocus();
  if (!currentFocusedElement)
  {
     // dump ('setting default focus to message pane');
     currentFocusedElement = GetMessagePane();
  }
    
	if(currentFocusedElement != lastFocusedElement) {
        if( currentFocusedElement == GetThreadOutliner()) {
            // XXX fix me
            GetThreadOutliner().setAttribute("focusring","true");
            GetMessagePane().setAttribute("focusring","false");
        }

        else if(currentFocusedElement==GetFolderOutliner()) {
            // XXX fix me
            GetThreadOutliner().setAttribute("focusring","false");
            GetMessagePane().setAttribute("focusring","false");
        }
        else if(currentFocusedElement==GetMessagePane()){
            // mscott --> fix me!!
            GetThreadOutliner().setAttribute("focusring","false");
            GetMessagePane().setAttribute("focusring","true");
        }
        else {
            // XXX fix me
            GetThreadOutliner().setAttribute("focusring","false");
            GetMessagePane().setAttribute("focusring","false");
        }
        
        lastFocusedElement=currentFocusedElement;

        // since we just changed the pane with focus we need to update the toolbar to reflect this
        document.commandDispatcher.updateCommands('mail-toolbar');
    }
//    else
//      dump('current focused element matched last focused element\n');
}

function MessagePaneHasFocus()
{
  // message pane has focus if the iframe has focus OR if the message pane box
  // has focus....
  // first, check to see if the message pane box has focus...if it does, return true
  var messagePane = GetMessagePane();
  if (WhichPaneHasFocus() == messagePane)
    return true;

  // second, check to see if the iframe has focus...if it does, return true....

  // check to see if the iframe has focus...
	var focusedWindow = top.document.commandDispatcher.focusedWindow;
	var messagePaneWindow = top.frames['messagepane'];
	
	if ( focusedWindow && messagePaneWindow && (focusedWindow != top) )
	{        
		var hasFocus = IsSubWindowOf(focusedWindow, messagePaneWindow);
		return hasFocus;
	}
	
	return false;
}

function IsSubWindowOf(search, wind)
{
	//dump("IsSubWindowOf(" + search + ", " + wind + ", " + found + ")\n");
	if (search == wind)
		return true;
	
	for ( index = 0; index < wind.frames.length; index++ )
	{
		if ( IsSubWindowOf(search, wind.frames[index]) )
			return true;
	}
	return false;
}


function WhichPaneHasFocus(){
	var whichPane= null;
	var currentNode = top.document.commandDispatcher.focusedElement;	

  var threadOutliner = GetThreadOutliner();
  var folderOutliner = GetFolderOutliner();
  var messagePane = GetMessagePane();
    
	while (currentNode) {
        if (currentNode === threadOutliner ||
            currentNode === folderOutliner ||
            currentNode === messagePane)
            return currentNode;
					
			currentNode = currentNode.parentNode;
    }
	
	return null;
}

function SetupCommandUpdateHandlers()
{
	//dump("SetupCommandUpdateHandlers\n");

	var widget;
	
	// folder pane
	widget = GetFolderOutliner();
	if ( widget )
		widget.controllers.appendController(FolderPaneController);
	
	// thread pane
	widget = GetThreadOutliner();
	if ( widget )
        widget.controllers.appendController(ThreadPaneController);
		
	top.controllers.insertControllerAt(0, DefaultController);
}

function IsSendUnsentMsgsEnabled(folderResource)
{
  var identity;
  try {
    if (folderResource) {
      // if folderResource is non-null, it is
      // resource for the "Unsent Messages" folder
      // we're here because we've done a right click on the "Unsent Messages"
      // folder (context menu)
      var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
      return (msgFolder.getTotalMessages(false) > 0);
    }
    else {
      var folders = GetSelectedMsgFolders();
      if (folders.length > 0) {
        identity = getIdentityForServer(folders[0].server);
      }
    }
  }
  catch (ex) {
    dump("ex = " + ex + "\n");
    identity = null;
  }

  try {
    if (!identity) {
      var am = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
      identity = am.defaultAccount.defaultIdentity;
    }

    var msgSendlater = Components.classes["@mozilla.org/messengercompose/sendlater;1"].getService(Components.interfaces.nsIMsgSendLater);
    var unsentMsgsFolder = msgSendlater.getUnsentMessagesFolder(identity);
    return (unsentMsgsFolder.getTotalMessages(false) > 0);
  }
  catch (ex) {
    dump("ex = " + ex + "\n");
  }
  return false;
}

function IsRenameFolderEnabled()
{
    var folderOutliner = GetFolderOutliner();
    var selection = folderOutliner.outlinerBoxObject.selection;
    if (selection.count == 1)
    {
        var startIndex = {};
        var endIndex = {};
        selection.getRangeAt(0, startIndex, endIndex);
        var folderResource = GetFolderResource(folderOutliner, startIndex.value);
        var canRename = GetFolderAttribute(folderOutliner, folderResource, "CanRename") == "true";
        return canRename && isCommandEnabled("cmd_renameFolder");
    }
    else
        return false;
}

function IsFolderCharsetEnabled()
{
  return IsFolderSelected();
}

function IsPropertiesEnabled()
{
  return IsFolderSelected();
}

function IsViewNavigationItemEnabled()
{
	return IsFolderSelected();
}

function IsFolderSelected()
{
    var folderOutliner = GetFolderOutliner();
    var selection = folderOutliner.outlinerBoxObject.selection;
    if (selection.count == 1)
    {
        var startIndex = {};
        var endIndex = {};
        selection.getRangeAt(0, startIndex, endIndex);
        var folderResource = GetFolderResource(folderOutliner, startIndex.value);
        return GetFolderAttribute(folderOutliner, folderResource, "IsServer") != "true";
    }
    else
        return false;
}

function IsFindEnabled()
{
	return (!IsThreadAndMessagePaneSplitterCollapsed() && (GetNumSelectedMessages() > 0));
}

function MsgDeleteFolder()
{
    var folderOutliner = GetFolderOutliner();
    var selectedFolders = GetSelectedMsgFolders();
    for (var i = 0; i < selectedFolders.length; i++)
    {
        var selectedFolder = selectedFolders[i];
        var folderResource = selectedFolder.QueryInterface(Components.interfaces.nsIRDFResource);
        var specialFolder = GetFolderAttribute(folderOutliner, folderResource, "SpecialFolder");
        if (specialFolder != "Inbox" && specialFolder != "Trash")
        {
            var protocolInfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + selectedFolder.server.type].getService(Components.interfaces.nsIMsgProtocolInfo);

            // do not allow deletion of special folders on imap accounts
            if ((specialFolder == "Sent" || 
                specialFolder == "Drafts" || 
                specialFolder == "Templates") &&
                !protocolInfo.specialFoldersDeletionAllowed)
            {
                var errorMessage = gMessengerBundle.getFormattedString("specialFolderDeletionErr",
                                                    [specialFolder]);
                var specialFolderDeletionErrTitle = gMessengerBundle.getString("specialFolderDeletionErrTitle");
                promptService.alert(window, specialFolderDeletionErrTitle, errorMessage);
                continue;
            }   
            else if (isNewsURI(folderResource.Value))
            {
                var unsubscribe = ConfirmUnsubscribe(selectedFolder);
                if (unsubscribe)
                    UnSubscribe(selectedFolder);
            }
            else
            {
                var parentResource = selectedFolder.parent.QueryInterface(Components.interfaces.nsIRDFResource);
                messenger.DeleteFolders(GetFolderDatasource(), parentResource, folderResource);
            }
        }
    }
}

// 3pane related commands.  Need to go in own file.  Putting here for the moment.
function MsgNextMessage()
{
	GoNextMessage(nsMsgNavigationType.nextMessage, false );
}

function MsgNextUnreadMessage()
{
	GoNextMessage(nsMsgNavigationType.nextUnreadMessage, true);
}
function MsgNextFlaggedMessage()
{
	GoNextMessage(nsMsgNavigationType.nextFlagged, true);
}

function MsgNextUnreadThread()
{
	//First mark the current thread as read.  Then go to the next one.
	MsgMarkThreadAsRead();
	GoNextMessage(nsMsgNavigationType.nextUnreadThread, true);
}

function MsgPreviousMessage()
{
    GoNextMessage(nsMsgNavigationType.previousMessage, false);
}

function MsgPreviousUnreadMessage()
{
	GoNextMessage(nsMsgNavigationType.previousUnreadMessage, true);
}

function MsgPreviousFlaggedMessage()
{
	GoNextMessage(nsMsgNavigationType.previousFlagged, true);
}

function MsgViewAllMsgs() 
{
	//dump("MsgViewAllMsgs\n");
	if(gDBView)
	{
		gDBView.viewType = nsMsgViewType.eShowAllThreads;

        var folder = GetSelectedFolder();
        if(folder) {
            folder.setAttribute("viewType", nsMsgViewType.eShowAllThreads);
        }
	}
}

function GetFolderNameFromUri(uri, outliner)
{
	var folderResource = RDF.GetResource(uri);

	var db = outliner.outlinerBoxObject.outlinerBody.database;

	var nameProperty = RDF.GetResource('http://home.netscape.com/NC-rdf#Name');

	var nameResult;
	try {
		nameResult = db.GetTarget(folderResource, nameProperty , true);
	}
	catch (ex) {
		return "";
	}

	nameResult = nameResult.QueryInterface(Components.interfaces.nsIRDFLiteral);
	return nameResult.Value;
}

function SwitchPaneFocus(direction)
{
  var gray_vertical_splitter = document.getElementById("gray_vertical_splitter"); 
  var focusedElement = document.commandDispatcher.focusedElement;
  var focusedElementId;
  if (direction == "counter-clockwise")
	{
		if ( MessagePaneHasFocus() )
			SetFocusThreadPane();
		else 
		{
			try 
			{ 
				focusedElementId = focusedElement.getAttribute('id');
				if(focusedElementId == "threadOutliner")
				{
					if (gray_vertical_splitter)
					{
						if (!(is_collapsed(gray_vertical_splitter)))
						  SetFocusFolderPane();
						else if(!(IsThreadAndMessagePaneSplitterCollapsed()))
						  SetFocusMessagePane();
					}
					else 
					{
						if (!(sidebar_is_collapsed()))
						  SetFocusFolderPane();
						else if(!(IsThreadAndMessagePaneSplitterCollapsed()))
						  SetFocusMessagePane();
					}
				}
				else if(focusedElementId == "folderOutliner")
				{
					if (!(IsThreadAndMessagePaneSplitterCollapsed()))
						SetFocusMessagePane();
					else
						SetFocusThreadPane();
				}
			}
			catch(e) 
			{
				SetFocusMessagePane();
			}
		}
	}
	else
	{

		if ( MessagePaneHasFocus() )
		{
			if (gray_vertical_splitter)
			{
				if (!(is_collapsed(gray_vertical_splitter)))
					SetFocusFolderPane();
				else
					SetFocusThreadPane();
			}
			else 
			{
				if (!(sidebar_is_collapsed()))
				  SetFocusFolderPane();
				else
				  SetFocusThreadPane();
			}
		}
		else 
		{
			try 
			{ 
				focusedElementId = focusedElement.getAttribute('id');
				if(focusedElementId == "threadOutliner")
				{
					if (!(IsThreadAndMessagePaneSplitterCollapsed()))
						SetFocusMessagePane();
					else if (gray_vertical_splitter)
					{
						if (!(is_collapsed(gray_vertical_splitter)))
						SetFocusFolderPane();
					}
					else if (!(sidebar_is_collapsed()))
						SetFocusFolderPane();

				}
				else if(focusedElementId == "folderOutliner")
					SetFocusThreadPane();
			}
			catch(e) 
			{
				SetFocusMessagePane();
			}
		}
	}

}

function SetFocusFolderPane()
{
    var folderOutliner = GetFolderOutliner();
    folderOutliner.focus();
}

function SetFocusThreadPane()
{
    var threadOutliner = GetThreadOutliner();
    threadOutliner.focus();
}

function SetFocusMessagePane()
{
    var messagePaneFrame = GetMessagePaneFrame();
    messagePaneFrame.focus();
}

function is_collapsed(element) 
{
  return (element.getAttribute('state') == 'collapsed');
}

function isCommandEnabled(cmd)
{
  var selectedFolders = GetSelectedMsgFolders();
  var numFolders = selectedFolders.length;
  if(numFolders !=1)
    return false;

  var folder = selectedFolders[0];
  if (!folder)
    return false;
  else
    return folder.isCommandEnabled(cmd);

}

