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
				return true;
			
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
				return true;

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
		switch ( command )
		{
			case "cmd_selectAll":
				var threadTree = GetThreadTree();
				if ( threadTree )
				{
					//if we're threaded we need to expand everything before selecting all
					if(messageView.showThreads)
						ExpandOrCollapseThreads(true);
					threadTree.selectAll();
					if ( threadTree.selectedItems && threadTree.selectedItems.length != 1 )
						ClearMessagePane();
				}
					//setting threadTree on
        			document.getElementById("threadTree").setAttribute("focusring","true");
				break;
		}
	},
	
	onEvent: function(event)
	{
		// on blur events set the menu item texts back to the normal values
		if ( event == 'blur' )
        {
              document.getElementById("threadTree").setAttribute("focusring","false");

		}
		
		if ( event == 'focus' )
        {
        	//alert("focus")
              document.getElementById("threadTree").setAttribute("focusring","true");

		}		
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
			case "cmd_nextMsg":
			case "cmd_nextUnreadMsg":
			case "cmd_nextFlaggedMsg":
			case "cmd_nextUnreadThread":
			case "cmd_previousMsg":
			case "cmd_previousUnreadMsg":
			case "cmd_previousFlaggedMsg":
			case "cmd_viewAllMsgs":
			case "cmd_viewUnreadMsgs":
            case "cmd_undo":
            case "cmd_redo":
			case "cmd_expandAllThreads":
			case "cmd_collapseAllThreads":
			case "cmd_renameFolder":
			case "cmd_openMessage":
			case "cmd_print":
			case "cmd_saveAsFile":
			case "cmd_saveAsTemplate":
			case "cmd_viewPageSource":
			case "cmd_reload":
			case "cmd_getNewMessages":
			case "cmd_getNextNMessages":
			case "cmd_find":
			case "cmd_findAgain":
			case "cmd_markAsRead":
			case "cmd_markAllRead":
			case "cmd_markThreadAsRead":
			case "cmd_markAsFlagged":
			case "cmd_file":
			case "cmd_emptyTrash":
			case "cmd_compactFolder":
			case "cmd_sortByThread":
				return true;
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
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
			case "cmd_openMessage":
			case "cmd_print":
			case "cmd_saveAsFile":
			case "cmd_saveAsTemplate":
			case "cmd_viewPageSource":
			case "cmd_reload":
			case "cmd_markThreadAsRead":
			case "cmd_markAsFlagged":
			case "cmd_file":
				var numSelected = GetNumSelectedMessages();

				if ( command == "cmd_delete")
				{
					if ( numSelected < 2 )
						goSetMenuValue(command, 'valueMessage');
					else
						goSetMenuValue(command, 'valueMessages');
				}
				return ( numSelected > 0 );
			case "cmd_nextMsg":
			case "cmd_nextUnreadMsg":
			case "cmd_nextUnreadThread":
			case "cmd_previousMsg":
			case "cmd_previousUnreadMsg":
				return MailAreaHasFocus() && IsViewNavigationItemEnabled();
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
			case "cmd_expandAllThreads":
			case "cmd_collapseAllThreads":
				return messageView.showThreads;
				break;
			case "cmd_nextFlaggedMsg":
			case "cmd_previousFlaggedMsg":
				return IsViewNavigationItemEnabled();
			case "cmd_viewAllMsgs":
				return true;
			case "cmd_sortByThread":
				return (messageView.viewType != viewShowUnread);
				break;
  			case "cmd_viewUnreadMsgs":
  				return (messageView.showThreads == false);
  				break;
            case "cmd_undo":
            case "cmd_redo":
                return SetupUndoRedoCommand(command);
			case "cmd_renameFolder":
				return IsRenameFolderEnabled();
			case "cmd_getNewMessages":
				return IsGetNewMessagesEnabled();
			case "cmd_getNextNMessages":
				return IsGetNextNMessagesEnabled();
			case "cmd_emptyTrash":
				return IsEmptyTrashEnabled();
			case "cmd_compactFolder":
				return IsCompactFolderEnabled();
			default:
				return false;
		}
		return false;
	},

	doCommand: function(command)
	{
   		//dump("ThreadPaneController.doCommand(" + command + ")\n");
   		
        document.getElementById("messagepane").setAttribute("focusring","true");
		switch ( command )
		{
			case "cmd_getNewMessages":
				MsgGetMessage();
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
			case "cmd_delete":
				MsgDeleteMessage(false, false);
				break;
			case "cmd_shiftDelete":
				MsgDeleteMessage(true, false);
				break;
			case "button_delete":
				MsgDeleteMessage(false, true);
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
				MsgViewAllMsgs();
				break;
			case "cmd_viewUnreadMsgs":
				MsgViewUnreadMsg();
				break;
			case "cmd_undo":
				messenger.Undo(msgWindow);
				break;
			case "cmd_redo":
				messenger.Redo(msgWindow);
				break;
			case "cmd_expandAllThreads":
				ExpandOrCollapseThreads(true);
				break;
			case "cmd_collapseAllThreads":
				ExpandOrCollapseThreads(false);
				break;
			case "cmd_renameFolder":
				MsgRenameFolder();
				return;
			case "cmd_openMessage":
				MsgOpenNewWindowForMessage(null, null);
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
			case "cmd_reload":
				MsgReload();
				return;
			case "cmd_find":
				MsgFind();
				return;
			case "cmd_findAgain":
				MsgFindAgain();
				return;
			case "cmd_markAsRead":
				MsgMarkMsgAsRead(null);
				return;
			case "cmd_markThreadAsRead":
				MsgMarkThreadAsRead();
				return;
			case "cmd_markAllRead":
				MsgMarkAllRead();
				return;
			case "cmd_markAsFlagged":
				MsgMarkAsFlagged(null);
				return;
			case "cmd_emptyTrash":
				MsgEmptyTrash();
				return;
			case "cmd_compactFolder":
				MsgCompactFolder();
				return;
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
	var threadTree = GetThreadTree();
	var numSelected = 0;
	if ( threadTree && threadTree.selectedItems )
		numSelected = threadTree.selectedItems.length;
	return numSelected;
}

function CommandUpdate_Mail()
{
	//var messagePane = top.document.getElementById('messagePane');
	//var drawFocusBorder = messagePane.getAttribute('draw-focus-border');
		document.getElementById("messagepanebox").setAttribute("focusring","false");
		document.getElementById("threadTree").setAttribute("focusring","false")
		document.getElementById("folderTree").setAttribute("focusring","false")
	
	if ( MessagePaneHasFocus() )
	{
		//if ( !drawFocusBorder )
		//	messagePane.setAttribute('draw-focus-border', 'true');
		document.getElementById("messagepanebox").setAttribute("focusring","true");
		//document.getElementById("threadTree").setAttribute("focusring","false")
		//document.getElementById("folderTree").setAttribute("focusring","false")

	}
	else
	{
		//if ( drawFocusBorder )
		//	messagePane.removeAttribute('draw-focus-border');
		//document.getElementById("messagepanebox").setAttribute("focusring","false");
		
		if( WhichPaneHasFocus() == "threadTree"){
			document.getElementById("threadTree").setAttribute("focusring","true")
			//document.getElementById("folderTree").setAttribute("focusring","false")
		}
		
// mail3PaneWindowCommands.js
		else{
			if(WhichPaneHasFocus()=="folderTree")
			//document.getElementById("threadTree").setAttribute("focusring","false")
			document.getElementById("folderTree").setAttribute("focusring","true")
		}
		
	}



		

	//goUpdateCommand('button_delete');

	goUpdateCommand('cmd_delete');
	goUpdateCommand('cmd_nextMsg');
	goUpdateCommand('cmd_nextUnreadMsg');
	goUpdateCommand('cmd_nextUnreadThread');
	goUpdateCommand('cmd_nextFlaggedMsg');
	goUpdateCommand('cmd_previousMsg');
	goUpdateCommand('cmd_previousUnreadMsg');
	goUpdateCommand('cmd_previousFlaggedMsg');
	goUpdateCommand('cmd_sortByThread');
	goUpdateCommand('cmd_viewAllMsgs');
	goUpdateCommand('cmd_viewUnreadMsgs');
	goUpdateCommand('cmd_expandAllThreads');
	goUpdateCommand('cmd_collapseAllThreads');
	goUpdateCommand('cmd_renameFolder');
	goUpdateCommand('cmd_getNewMessages');
	goUpdateCommand('cmd_getNextNMessages');
	goUpdateCommand('cmd_find');
	goUpdateCommand('cmd_findAgain');
	goUpdateCommand('cmd_markAllRead');
	goUpdateCommand('cmd_emptyTrash');
	goUpdateCommand('cmd_compactFolder');
}

function ThreadTreeUpdate_Mail(command)
{
	goUpdateCommand('button_reply');
	goUpdateCommand('button_replyall');
	goUpdateCommand('button_forward');
	goUpdateCommand('cmd_shiftDelete');
	goUpdateCommand('cmd_reply');
	goUpdateCommand('cmd_replySender');
	goUpdateCommand('cmd_replyGroup');
	goUpdateCommand('cmd_replyall');
	goUpdateCommand('cmd_forward');
	goUpdateCommand('cmd_forwardInline');
	goUpdateCommand('cmd_forwardAttachment');
	goUpdateCommand('cmd_editAsNew');
	goUpdateCommand('cmd_openMessage');
	goUpdateCommand('cmd_print');
	goUpdateCommand('cmd_saveAsFile');
	goUpdateCommand('cmd_saveAsTemplate');
	goUpdateCommand('cmd_viewPageSource');
	goUpdateCommand('cmd_reload');
	goUpdateCommand('cmd_markAsRead');
	goUpdateCommand('cmd_markThreadAsRead');
	goUpdateCommand('cmd_markAsFlagged');
	goUpdateCommand('cmd_file');
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
	var focusedWindow = top.document.commandDispatcher.focusedWindow;
	var messagePaneWindow = top.frames['messagepane'];
	
	if ( focusedWindow && messagePaneWindow && (focusedWindow != top) )
	{
		var hasFocus = IsSubWindowOf(focusedWindow, messagePaneWindow, false);
		dump("...........Focus on MessagePane = " + hasFocus + "\n");
		return hasFocus;
	}
	
	return false;
}

function IsSubWindowOf(search, wind, found)
{
	//dump("IsSubWindowOf(" + search + ", " + wind + ", " + found + ")\n");
	if ( found || (search == wind) )
		return true;
	
	for ( index = 0; index < wind.frames.length; index++ )
	{
		if ( IsSubWindowOf(search, wind.frames[index], false) )
			return true;
	}
	return false;
}


function WhichPaneHasFocus(){
	var whichPane= "none";
	currentNode = top.document.commandDispatcher.focusedElement;	

	
	if(currentNode){
		while(currentNode.parentNode!=null){
			if(currentNode.getAttribute("id") == "threadTree" ){ whichPane="threadTree" }
			
			if(currentNode.getAttribute("id") == "folderTree"){  whichPane="folderTree" } 
		
			currentNode = currentNode.parentNode;
		}
	
	}
	
	
	
	return whichPane

}





function SetupCommandUpdateHandlers()
{
	dump("SetupCommandUpdateHandlers\n");

	var widget;
	
	// folder pane
	widget = GetFolderTree();
	if ( widget )
		widget.controllers.appendController(FolderPaneController);
	
	// thread pane
	widget = GetThreadTree();
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
	return (!IsThreadAndMessagePaneSplitterCollapsed() && (gCurrentDisplayedMessage != null));

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
            dump(folderuri);
            parent = folder.parentNode.parentNode;	
            var parenturi = parent.getAttribute('id');
            if(parenturi)
                dump(parenturi);
            else
                dump("No parenturi");
            dump("folder = " + folder.localName + "\n"); 
            dump("parent = " + parent.localName + "\n"); 
            messenger.DeleteFolders(tree.database,
                                    parent.resource, folder.resource);
        }
	}


}

 //3pane related commands.  Need to go in own file.  Putting here for the moment.

function MsgNextMessage()
{
	GoNextMessage(navigateAny, false );
}

function MsgNextUnreadMessage()
{
	GoNextMessage(navigateUnread, true);
}
function MsgNextFlaggedMessage()
{
	GoNextMessage(navigateFlagged, true);
}

function MsgNextUnreadThread()
{
	//First mark the current thread as read.  Then go to the next one.
	MsgMarkThreadAsRead();
	GoNextThread(navigateUnread, true, true);
}

function MsgPreviousMessage()
{
	GoPreviousMessage(navigateAny, false);
}

function MsgPreviousUnreadMessage()
{
	GoPreviousMessage(navigateUnread, true);
}

function MsgPreviousFlaggedMessage()
{
	GoPreviousMessage(navigateFlagged, true);
}

var viewShowAll =0;
var viewShowRead = 1;
var viewShowUnread =2;
var viewShowWatched = 3;

function MsgViewAllMsgs() 
{
	dump("MsgViewAllMsgs");

	if(messageView)
	{
		messageView.viewType = viewShowAll;
		messageView.showThreads = false;
	}
	RefreshThreadTreeView();
}

function MsgViewUnreadMsg()
{
	dump("MsgViewUnreadMsgs");

	if(messageView)
	{
		messageView.viewType = viewShowUnread;
		messageView.showThreads = false;
	}

	RefreshThreadTreeView();
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


//Sets the thread tree's template's treeitem to be open so that all threads are expanded.
function ExpandOrCollapseThreads(expand)
{
	SetTemplateTreeItemOpen(expand);
	RefreshThreadTreeView();
}

function SetTemplateTreeItemOpen(open)
{
	var templateTreeItem = document.getElementById("threadTreeTemplateTreeItem");
	if(templateTreeItem)
	{
		if(open)
			templateTreeItem.setAttribute("open", "true");
		else
			templateTreeItem.removeAttribute("open");
	}
}

function SwitchPaneFocus(event)
{
	var focusedElement;
	var focusedElementId;

	if (event && (event.shiftKey))
	{
		dump("Inside the SwitchPaneFocus \n");
		focusedElement = document.commandDispatcher.focusedElement;
		focusedElementId="";

		if ( MessagePaneHasFocus() )
			SetFocusThreadPane();
		else 
		{
			try 
			{ 
				focusedElementId = focusedElement.getAttribute('id');
				if(focusedElementId == "threadTree")
					SetFocusFolderPane();
				else if(focusedElementId == "folderTree")
					SetFocusMessagePane();
			}
			catch(e) 
			{
				SetFocusMessagePane();
			}
		}
	}
	else
	{
		dump("Inside the SwitchPaneFocus \n");
		focusedElement = document.commandDispatcher.focusedElement;
		focusedElementId="";

		if ( MessagePaneHasFocus() )
			SetFocusFolderPane();
		else 
		{
			try 
			{ 
				focusedElementId = focusedElement.getAttribute('id');
				if(focusedElementId == "threadTree")
					SetFocusMessagePane();
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
	document.getElementById("folderTree").focus();
	return;
}

function SetFocusThreadPane()
{
	document.getElementById("threadTree").focus();
	return;
}

function SetFocusMessagePane()
{
	top.frames['messagepane'].focus();
	return;
}

