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
 */

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
				var folderTree = GetFolderTree();
				if ( folderTree && folderTree.selectedItems &&
                     folderTree.selectedItems.length > 0)
                {
					var specialFolder = null;
					var isServer = null;
					var serverType = null;
					try {
						var selectedFolder = folderTree.selectedItems[0];
                    	specialFolder = selectedFolder.getAttribute('SpecialFolder');
                    	isServer = selectedFolder.getAttribute('IsServer');
						serverType = selectedFolder.getAttribute('ServerType');
            
                        if (serverType == "nntp") {
				            if ( command == "cmd_delete" )
					            goSetMenuValue(command, 'valueNewsgroup');
                        }
					}
					catch (ex) {
						//dump("specialFolder failure: " + ex + "\n");
					}
                    if (specialFolder == "Inbox" || specialFolder == "Trash" || isServer == "true")
                       return false;
                    else
					   return true;
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
      case "cmd_undo":
      case "cmd_redo":
			case "cmd_expandAllThreads":
			case "cmd_collapseAllThreads":
			case "cmd_renameFolder":
			case "cmd_openMessage":
      case "button_print":
			case "cmd_print":
			case "cmd_saveAsFile":
			case "cmd_saveAsTemplate":
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
      case "cmd_watchThread":
      case "cmd_killThread":
      case "cmd_toggleWorkOffline":
      case "cmd_close":
      case "cmd_selectThread":
      case "cmd_selectFlagged":
				return true;
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
            goSetMenuValue(command, 'valueNewsMessage');
          else
            goSetMenuValue(command, 'valueMessage');
        }
        else 
        {
          if (IsNewsMessage(uri)) 
            goSetMenuValue(command, 'valueNewsMessage');
          else 
            goSetMenuValue(command, 'valueMessages');
        }
        if (gDBView)
          gDBView.getCommandStatus(nsMsgViewCommandType.deleteMsg, enabled, checkStatus);
        return enabled.value;
 			case "cmd_shiftDelete":
        if (gDBView)
         gDBView.getCommandStatus(nsMsgViewCommandType.deleteNoTrash, enabled, checkStatus);
       return enabled.value;
			case "cmd_killThread":
				return MailAreaHasFocus() && IsViewNavigationItemEnabled();
			case "cmd_watchThread":
        if (gDBView)
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
      case "cmd_downloadSelected":
      case "button_mark":
			case "cmd_markAsRead":
				if(!MailAreaHasFocus())
					return false;
				else
					return(GetNumSelectedMessages() > 0);
			case "cmd_markAllRead":
      case "cmd_downloadFlagged":
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
        return true;
      case "cmd_undo":
      case "cmd_redo":
          return SetupUndoRedoCommand(command);
			case "cmd_renameFolder":
				return IsRenameFolderEnabled();
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
      case "cmd_toggleWorkOffline":
        return true;
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
      case "cmd_downloadFlagged":
        MsgDownloadFlagged();
        return;
      case "cmd_downloadSelected":
        MsgDownloadSelected();
        return;
			case "cmd_emptyTrash":
				MsgEmptyTrash();
				return;
			case "cmd_compactFolder":
				MsgCompactFolder(true);
				return;
      case "cmd_toggleWorkOffline":
        MsgToggleWorkOffline();
        return;
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

        else if(currentFocusedElement==GetFolderTree()) {
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

function SetupUndoRedoCommand(command)
{
    // dump ("--- SetupUndoRedoCommand: " + command + "\n");
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


function CommandUpdate_UndoRedo()
{
    ShowMenuItem("menu_undo", true);
    EnableMenuItem("menu_undo", SetupUndoRedoCommand("cmd_undo"));
    ShowMenuItem("menu_redo", true);
    EnableMenuItem("menu_redo", SetupUndoRedoCommand("cmd_redo"));
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

  var threadTree = GetThreadOutliner();
  var folderTree = GetFolderTree();
  var messagePane = GetMessagePane();
    
	while (currentNode) {
        if (currentNode === threadTree ||
            currentNode === folderTree ||
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
	widget = GetFolderTree();
	if ( widget )
		widget.controllers.appendController(FolderPaneController);
	
	// thread pane
	widget = GetThreadOutliner();
	if ( widget )
        widget.controllers.appendController(ThreadPaneController);
		
	top.controllers.insertControllerAt(0, DefaultController);
}

function IsRenameFolderEnabled()
{
	var tree = GetFolderTree();
	var folderList = tree.selectedItems;

	if(folderList.length == 1)
	{
		var folderNode = folderList[0];
		return(folderNode.getAttribute("CanRename") == "true");
	}
	else
		return false;

}

function IsFolderCharsetEnabled()
{
  return IsFolderSelected();
}

function IsViewNavigationItemEnabled()
{
	return IsFolderSelected();
}

function IsFolderSelected()
{
	var tree = GetFolderTree();
	var folderList = tree.selectedItems;

	if(folderList.length == 1)
	{
		var folderNode = folderList[0];
		return(folderNode.getAttribute("IsServer") != "true");
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
	//get the selected elements
	var tree = GetFolderTree();
	var folderList = tree.selectedItems;
	var i;
	var folder, parent;
    var specialFolder;
	for(i = 0; i < folderList.length; i++)
	{
		folder = folderList[i];
	    folderuri = folder.getAttribute('id');
        specialFolder = folder.getAttribute('SpecialFolder');
        if (specialFolder != "Inbox" && specialFolder != "Trash")
        {
            if (isNewsURI(folderuri)) {
              var msgfolder = GetMsgFolderFromURI(folderuri);
              var unsubscribe = ConfirmUnsubscribe(msgfolder);
              if (unsubscribe) {
                UnSubscribe(msgfolder);
              }
            }
            else {
              parent = folder.parentNode.parentNode;	
              var parenturi = parent.getAttribute('id');
              messenger.DeleteFolders(tree.database,
                                    parent.resource, folder.resource);
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


function FillInFolderTooltip(cellNode)
{
	var folderNode = cellNode.parentNode.parentNode;
	var uri = folderNode.getAttribute('id');
	var folderTree = GetFolderTree();

	var name = GetFolderNameFromUri(uri, folderTree);

	var folderResource = RDF.GetResource(uri);
	var msgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
	var unreadCount = msgFolder.getNumUnread(false);
	if(unreadCount < 0)
		unreadCount = 0;

	var totalCount = msgFolder.getTotalMessages(false);
	if(totalCount < 0)
		totalCount = 0;

	var textNode = document.getElementById("foldertooltipText");
	var folderTooltip = name;
	if(!msgFolder.isServer)
		folderTooltip += " ("  + unreadCount + "/" + totalCount +")";
	textNode.setAttribute('value', folderTooltip);
	return true;
	

}

function GetFolderNameFromUri(uri, tree)
{
	var folderResource = RDF.GetResource(uri);

	var db = tree.database;

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
				else if(focusedElementId == "folderTree")
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
				else if(focusedElementId == "folderTree")
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
  var folderTree = GetFolderTree();
  folderTree.focus();
	return;
}

function SetFocusThreadPane()
{
  var threadTree = GetThreadOutliner();
  threadTree.focus();
	return;
}

function SetFocusMessagePane()
{
	var messagePaneFrame = GetMessagePane();
  messagePaneFrame.focus();
	return;
}

function is_collapsed(element) 
{
  return (element.getAttribute('state') == 'collapsed');
}

