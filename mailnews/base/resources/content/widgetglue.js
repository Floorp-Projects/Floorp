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

/*
 * widget-specific wrapper glue. There should be one function for every
 * widget/menu item, which gets some context (like the current selection)
 * and then calls a function/command in commandglue
 */
 
var msgComposeType = Components.interfaces.nsIMsgCompType;
var msgComposFormat = Components.interfaces.nsIMsgCompFormat;
var Bundle = srGetStrBundle("chrome://messenger/locale/messenger.properties");

// Controller object for folder pane
var FolderPaneController =
{
   supportsCommand: function(command)
	{
		switch ( command )
		{
			case "cmd_selectAll":
			case "cmd_delete":
			case "button_delete":
				return true;
			
			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
		dump("FolderPaneController.IsCommandEnabled\n");
		switch ( command )
		{
			case "cmd_selectAll":
				return true;
			
			case "cmd_delete":
			case "button_delete":
				if ( command == "cmd_delete" )
					goSetMenuValue(command, 'valueFolder');
				var folderTree = GetFolderTree();
				if ( folderTree && folderTree.selectedItems )
					return true;
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
			case "cmd_selectAll":
				var folderTree = GetFolderTree();
				if ( folderTree )
				{
					dump("select all now!!!!!!" + "\n");
					folderTree.selectAll();
				}
				break;
			
			case "cmd_delete":
			case "button_delete":
				// add this back in when folder delete has warning
				//MsgDeleteFolder();
				break;
		}
	},
	
	onEvent: function(event)
	{
		dump("onEvent("+event+")\n");
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
			case "cmd_delete":
			case "button_delete":
			case "cmd_undo":
			case "cmd_redo":
				return true;

			default:
				return false;
		}
	},

	isCommandEnabled: function(command)
	{
		dump("ThreadPaneController.isCommandEnabled\n");
		switch ( command )
		{
			case "cmd_selectAll":
				return true;
			
			case "cmd_delete":
			case "button_delete":
				var threadTree = GetThreadTree();
				dump("threadTree = " + threadTree + "\n");
				var numSelected = 0;
				if ( threadTree && threadTree.selectedItems )
					numSelected = threadTree.selectedItems.length;
				if ( command == "cmd_delete" )
				{
					if ( numSelected < 2 )
						goSetMenuValue(command, 'valueMessage');
					else
						goSetMenuValue(command, 'valueMessages');
				}
				return ( numSelected > 0 );
			
			case "cmd_undo":
			case "cmd_redo":
				// FIX ME - these commands should be calling the back-end code to determine if
				// they should be enabled, this hack of always enabled can then be fixed.
				return true;

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
					dump("select all now!!!!!!" + "\n");
					threadTree.selectAll();
				}
				break;
			
			case "cmd_delete":
				MsgDeleteMessage(false);
				break;
			
			case "button_delete":
				MsgDeleteMessage(true);
				break;
			
			case "cmd_undo":
				messenger.Undo();
				break;
			
			case "cmd_redo":
				messenger.Redo();
				break;
		}
	},
	
	onEvent: function(event)
	{
		dump("onEvent("+event+")\n");
		// on blur events set the menu item texts back to the normal values
		if ( event == 'blur' )
		{
			goSetMenuValue('cmd_delete', 'valueDefault');
		}
	}
};


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
}


var viewShowAll =0;
var viewShowRead = 1;
var viewShowUnread =2;
var viewShowWatched = 3;


function MsgLoadNewsMessage(url)
{
  dump("\n\nMsgLoadNewsMessage from XUL\n");
  OpenURL(url);
}

function MsgHome(url)
{
  window.open( url, "_blank", "chrome,dependent=yes,all" );
}

function MsgGetMessage() 
{
  GetNewMessages();
}

function MsgDeleteMessage(fromToolbar)
{
  //dump("\nMsgDeleteMessage from XUL\n");
  //dump("from toolbar? " + fromToolbar + "\n");

  var tree = GetThreadTree();
  if(tree) {
	var srcFolder = GetThreadTreeFolder();
	// if from the toolbar, return right away if this is a news message
	// only allow cancel from the menu:  "Edit | Cancel / Delete Message"
	if (fromToolbar) {
		uri = srcFolder.getAttribute('ref');
		//dump("uri[0:6]=" + uri.substring(0,6) + "\n");
		if (uri.substring(0,6) == "news:/") {
			//dump("delete ignored!\n");
			return;
		}
	}
	dump("tree is valid\n");
	//get the selected elements

	var messageList = ConvertDOMListToResourceArray(tree.selectedItems);
    	var nextMessage = GetNextMessageAfterDelete(tree.selectedItems);
	//get the current folder

	messenger.DeleteMessages(tree.database, srcFolder.resource, messageList);
	SelectNextMessage(nextMessage);
  }
}

function ConvertDOMListToResourceArray(nodeList)
{
    var result = Components.classes["component://netscape/supports-array"].createInstance(Components.interfaces.nsISupportsArray);

    for (var i=0; i<nodeList.length; i++) {
        result.AppendElement(nodeList[i].resource);
    }

    return result;
}

function MsgDeleteFolder()
{
	//get the selected elements
	var tree = GetFolderTree();
	var folderList = tree.selectedItems;
	var i;
	var folder, parent;
	for(i = 0; i < folderList.length; i++)
	{
		folder = folderList[i];
	    folderuri = folder.getAttribute('id');
		dump(folderuri);
		parent = folder.parentNode.parentNode;	
	    var parenturi = parent.getAttribute('id');
		if(parenturi)
			dump(parenturi);
		else
			dump("No parenturi");
		dump("folder = " + folder.nodeName + "\n"); 
		dump("parent = " + parent.nodeName + "\n"); 
		messenger.DeleteFolders(tree.database,
                                parent.resource, folder.resource);
	}


}

function MsgNewMessage(event)
{
  ComposeMessage(msgComposeType.New, msgComposFormat.Default);
} 

function MsgReplyMessage(event)
{
  dump("\nMsgReplyMessage from XUL\n");
  ComposeMessage(msgComposeType.Reply, msgComposFormat.Default);
}

function MsgReplyToAllMessage(event) 
{
  dump("\nMsgReplyToAllMessage from XUL\n");
  ComposeMessage(msgComposeType.ReplyAll, msgComposFormat.Default);
}

function MsgForwardMessage(event)
{
  dump("\nMsgForwardMessage from XUL\n");
  var prefs = Components.classes['component://netscape/preferences'].getService();
  prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
  var forwardType = 0;
  try {
  	var forwardType = prefs.GetIntPref("mail.forward_message_mode");
  } catch (e) {dump ("failed to retrieve pref mail.forward_message_mode");}
  
  if (forwardType == 0)
  	MsgForwardAsAttachment(null);
  else
  	MsgForwardAsInline(null);
}

function MsgForwardAsAttachment(event)
{
  dump("\nMsgForwardAsAttachment from XUL\n");
  ComposeMessage(msgComposeType.ForwardAsAttachment, msgComposFormat.Default);
}

function MsgForwardAsInline(event)
{
  dump("\nMsgForwardAsInline from XUL\n");
  ComposeMessage(msgComposeType.ForwardInline, msgComposFormat.Default);
}

function MsgCopyMessage(destFolder)
{
	// Get the id for the folder we're copying into
    destUri = destFolder.getAttribute('id');
	dump(destUri);

	var tree = GetThreadTree();
	if(tree)
	{
		//Get the selected messages to copy
		var messageList = ConvertDOMListToResourceArray(tree.selectedItems);
		//get the current folder

	//	dump('In copy messages.  Num Selected Items = ' + messageList.length);
	//	dump('\n');
		var srcFolder = GetThreadTreeFolder();
		messenger.CopyMessages(tree.database,
                               srcFolder.resource,
                               destFolder.resource, messageList, false);
	}	
}

function MsgMoveMessage(destFolder)
{
	// Get the id for the folder we're copying into
    destUri = destFolder.getAttribute('id');
	dump(destUri);

	var tree = GetThreadTree();
	if(tree)
	{
		//Get the selected messages to copy
		var messageList = ConvertDOMListToResourceArray(tree.selectedItems);
		//get the current folder
		var nextMessage = GetNextMessageAfterDelete(tree.selectedItems);
		var srcFolder = GetThreadTreeFolder();
		messenger.CopyMessages(tree.database,
                               srcFolder.resource,
                               destFolder.resource, messageList, true);
		SelectNextMessage(nextMessage);
	}	
}

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
		view.showThreads = false;
	}

	RefreshThreadTreeView();
}

function MsgViewAllThreadMsgs()
{
	dump("MsgViewAllMessagesThreaded");

	if(messageView)
	{
		view.viewType = viewShowAll;
		view.showThreads = true;
	}
	RefreshThreadTreeView();
}

function MsgSortByDate()
{
	SortThreadPane('DateColumn', 'http://home.netscape.com/NC-rdf#Date');
}

function MsgSortBySender()
{
	SortThreadPane('AuthorColumn', 'http://home.netscape.com/NC-rdf#Sender');
}

function MsgSortByStatus()
{
	SortThreadPane('StatusColumn', 'http://home.netscape.com/NC-rdf#Status');
}

function MsgSortBySubject()
{
	SortThreadPane('SubjectColumn', 'http://home.netscape.com/NC-rdf#Subject');
}

function MsgNewFolder()
{
	var windowTitle = Bundle.GetStringFromName("newFolderDialogTitle");
	MsgNewSubfolder("chrome://messenger/content/newFolderNameDialog.xul",windowTitle);
}

function MsgSubscribe()
{
        var windowTitle = Bundle.GetStringFromName("subscribeDialogTitle");
	MsgNewSubfolder("chrome://messenger/content/subscribeDialog.xul", windowTitle);
}

function GetSelectedFolderURI()
{
	var uri = null;
	var selectedFolder = null;
	try {
		var folderTree = GetFolderTree(); 
		var selectedFolderList = folderTree.selectedItems;
	
		//  you can only select one folder / server to add new folder / subscribe to
		if (selectedFolderList.length == 1) {
			selectedFolder = selectedFolderList[0];
		}
		else {
			//dump("number of selected folder was " + selectedFolderList.length + "\n");
		}
	}
	catch (ex) {
		// dump("failed to get the selected folder\n");
		uri = null;
	}

	try {
       		if (selectedFolder) {
			uri = selectedFolder.getAttribute('id');
			// dump("folder to preselect: " + preselectedURI + "\n");
		}
	}
	catch (ex) {
		uri = null;
	}

	return uri;
}

function MsgNewSubfolder(chromeWindowURL,windowTitle)
{
	var preselectedURI = GetSelectedFolderURI();
	dump("preselectedURI = " + preselectedURI + "\n");
	var dialog = window.openDialog(
				chromeWindowURL,
				"",
				"chrome,modal",
				{preselectedURI:preselectedURI, title:windowTitle,
				okCallback:NewFolder});
}

function NewFolder(name,uri)
{
    	var tree = GetFolderTree();
	dump("uri,name = " + uri + "," + name + "\n");
	if (uri && (uri != "") && name && (name != "")) {
		var selectedFolder = GetResourceFromUri(uri);
		dump("selectedFolder = " + selectedFolder + "\n");
		messenger.NewFolder(tree.database, selectedFolder, name);
	}
	else {
		dump("no name or nothing selected\n");
	}
}


function MsgAccountManager()
{
    var result = {refresh: false};
    window.openDialog("chrome://messenger/content/AccountManager.xul",
                      "AccountManager", "chrome,modal", result);
    if (result.refresh)
        refreshFolderPane();
}

function MsgAccountWizard()
{
    var result = {refresh: false};
    window.openDialog("chrome://messenger/content/AccountWizard.xul",
                      "AccountWizard", "chrome,modal", result);
    if(result.refresh)
        refreshFolderPane();
}

// refresh the folder tree by rerooting it
// hack until the account manager can tell RDF that new accounts
// have been created.
function refreshFolderPane()
{
    var folderTree = GetFolderTree();
    if (folderTree) {
        folderTree.clearItemSelection();
        var root = folderTree.getAttribute('ref');
        folderTree.setAttribute('ref', root);
    }
}

function MsgOpenAttachment() {}

function MsgSaveAsFile() 
{
  dump("\MsgSaveAsFile from XUL\n");
  var tree = GetThreadTree();
  //get the selected elements
  var messageList = tree.selectedItems;
  if (messageList && messageList.length == 1)
  {
      var uri = messageList[0].getAttribute('id');
      dump (uri);
      if (uri)
          messenger.saveAs(uri, true, null);
  }
}

function MsgSaveAsTemplate() 
{
  dump("\MsgSaveAsTemplate from XUL\n");
  var tree = GetThreadTree();
  //get the selected elements
  var messageList = tree.selectedItems;
  if (messageList && messageList.length == 1)
  {
      var uri = messageList[0].getAttribute('id');
      // dump (uri);
      if (uri)
      {
		var folderTree = GetFolderTree();
        var identity = null;
		var selectedFolderList = folderTree.selectedItems;
		if(selectedFolderList.length > 0)
		{
			var selectedFolder = selectedFolderList[0]; 
			var folderUri = selectedFolder.getAttribute('id');
			// dump("selectedFolder uri = " + uri + "\n");

			// get the incoming server associated with this folder uri
			var server = FindIncomingServer(folderUri);
			// dump("server = " + server + "\n");
			// get the identity associated with this server
			var identities = accountManager.GetIdentitiesForServer(server);
			// dump("identities = " + identities + "\n");
			// just get the first one
			if (identities.Count() > 0 ) {
				identity = identities.GetElementAt(0).QueryInterface(Components.interfaces.nsIMsgIdentity);  
			}
		}
        messenger.saveAs(uri, false, identity);
      }
  }
}
function MsgSendUnsentMsg() 
{
	messenger.SendUnsentMessages();
}
function MsgLoadFirstDraft() 
{
	messenger.LoadFirstDraft();
}
function MsgUpdateMsgCount() {}

function MsgRenameFolder() 
{
	var preselectedURI = GetSelectedFolderURI();
	dump("preselectedURI = " + preselectedURI + "\n");
	var windowTitle = Bundle.GetStringFromName("renameFolderDialogTitle");
	var dialog = window.openDialog(
                    "chrome://messenger/content/renameFolderNameDialog.xul",
                    "newFolder",
                    "chrome,modal",
	            {preselectedURI:preselectedURI, title:windowTitle,
                    okCallback:RenameFolder});
}

function RenameFolder(name,uri)
{
    dump("uri,name = " + uri + "," + name + "\n");
    var tree = GetFolderTree();
    if (tree)
    {
	if (uri && (uri != "") && name && (name != "")) {
                var selectedFolder = GetResourceFromUri(uri);
                tree.clearItemSelection();
                messenger.RenameFolder(tree.database, selectedFolder, name);
        }
        else {
                dump("no name or nothing selected\n");
        }   
    }
    else {
	dump("no tree\n");
    }
}

function MsgEmptyTrash() 
{
    var tree = GetFolderTree();
    if (tree)
    {
        var folderList = tree.selectedItems;
        if (folderList)
        {
            var folder;
            folder = folderList[0];
            if (folder)
			{
                var trashUri = GetSelectTrashUri(folder);
                if (trashUri)
                {
                    var trashElement = document.getElementById(trashUri);
                    if (trashElement)
                    {
                        dump ('found trash folder\n');
                        trashElement.setAttribute('open','');
                    }
                }
				if(IsSpecialFolderSelected('Trash'))
				{
                    tree.clearItemSelection();
					RefreshThreadTreeView();
				}
                messenger.EmptyTrash(tree.database, folder.resource);
			}
        }
    }
}

function MsgCompactFolder() 
{
	//get the selected elements
	var tree = GetFolderTree();
    if (tree)
    {
        var folderList = tree.selectedItems;
        if (folderList)
        {
            var i;
            var folder;
            for(i = 0; i < folderList.length; i++)
            {
                folder = folderList[i];
                if (folder)
                {
                    folderuri = folder.getAttribute('id');
                    dump(folderuri);
                    dump("folder = " + folder.nodeName + "\n"); 
                    messenger.CompactFolder(tree.database, folder.resource);
                }
            }
        }
	}
}

function MsgImport() {}
function MsgWorkOffline() {}
function MsgSynchronize() {}
function MsgGetSelectedMsg() {}
function MsgGetFlaggedMsg() {}

function MsgSelectThread() {}
function MsgSelectFlaggedMsg() {}
function MsgFind() {}
function MsgFindAgain() {}

function MsgSearchMessages() {
    window.openDialog("chrome://messenger/content/SearchDialog.xul", "SearchMail", "chrome");
}

function MsgFilters() {
    window.openDialog("chrome://messenger/content/FilterListDialog.xul", "FilterDialog", "chrome");
}

function MsgToggleMessagePane()
{
    MsgToggleSplitter("messagePaneSplitter");
}

function MsgToggleFolderPane()
{
    MsgToggleSplitter("sidebarsplitter");
}

function MsgToggleSplitter(id)
{
    var splitter = document.getElementById(id);
    var state = splitter.getAttribute("state");
    if (state == "collapsed")
        splitter.setAttribute("state", null);
    else
        splitter.setAttribute("state", "collapsed")
}

function MsgShowFolders()
{


}

function MsgFolderProperties() {}

function MsgShowLocationbar() {}
function MsgSortByFlag() {}
function MsgSortByPriority() {}
function MsgSortBySize() {}
function MsgSortByThread() {}
function MsgSortByUnread() {}
function MsgSortByOrderReceived() {}
function MsgSortAscending() {}
function MsgSortDescending() {}
function MsgViewThreadsUnread() {}
function MsgViewWatchedThreadsUnread() {}
function MsgViewIgnoreThread() {}
function MsgViewAllHeaders() {}
function MsgViewNormalHeaders() {}
function MsgViewBriefHeaders() {}
function MsgViewAttachInline() {}
function MsgWrapLongLines() {}
function MsgIncreaseFont() {}
function MsgDecreaseFont() {}
function MsgReload() {}
function MsgShowImages() {}
function MsgRefresh() {}
function MsgViewPageSource() {}
function MsgViewPageInfo() {}
function MsgFirstUnreadMessage() {}
function MsgFirstFlaggedMessage() {}

function MsgStop() {
	StopUrls();
}

function MsgNextMessage()
{
	GoNextMessage(GoMessage, ResourceGoMessage, false);
}

function MsgNextUnreadMessage()
{
	GoNextMessage(GoUnreadMessage, ResourceGoUnreadMessage, true);
}
function MsgNextFlaggedMessage()
{
	GoNextMessage(GoFlaggedMessage, ResourceGoFlaggedMessage, true);
}

function MsgPreviousMessage()
{
	GoPreviousMessage(GoMessage, false);
}

function MsgPreviousUnreadMessage()
{
	GoPreviousMessage(GoUnreadMessage, true);
}

function MsgPreviousFlaggedMessage()
{
	GoPreviousMessage(GoFlaggedMessage, true);
}

function MsgGoBack() {}
function MsgGoForward() {}
function MsgEditMessageAsNew() {}
function MsgAddSenderToAddressBook() {}
function MsgAddAllToAddressBook() {}

function MsgMarkMsgAsRead(markRead)
{
  dump("\MsgMarkMsgAsRead from XUL\n");
  var tree = GetThreadTree();
  //get the selected elements
  var messageList = ConvertDOMListToResourceArray(tree.selectedItems);
  messenger.MarkMessagesRead(tree.database, messageList, markRead);
}

function MsgMarkThreadAsRead() {}
function MsgMarkByDate() {}
function MsgMarkAllRead()
{
	var folderTree = GetFolderTree();
	var selectedFolderList = folderTree.selectedItems;
	if(selectedFolderList.length > 0)
	{
		var selectedFolder = selectedFolderList[0];
		messenger.MarkAllMessagesRead(folderTree.database, selectedFolder.resource);
	}
	else {
		dump("Nothing was selected\n");
	}
}

function MsgMarkAsFlagged(markFlagged)
{
  dump("\MsgMarkMsgAsFlagged from XUL\n");
  var tree = GetThreadTree();
  //get the selected elements
  var messageList = ConvertDOMListToResourceArray(tree.selectedItems);
  messenger.MarkMessagesFlagged(tree.database, messageList, markFlagged);

}

function MsgIgnoreThread() {}
function MsgWatchThread() {}

var gStatusObserver;
        var bindCount = 0;
        function onStatus() {
            if (!gStatusObserver)
                gStatusObserver = document.getElementById("Messenger:Status");
            if ( gStatusObserver ) {
                var text = gStatusObserver.getAttribute("value");
                if ( text == "" ) {
                    text = defaultStatus;
                }
                var statusText = document.getElementById("statusText");
                if ( statusText ) {
                    statusText.setAttribute( "value", text );
                }
            } else {
                dump("Can't find status broadcaster!\n");
            }
        }

var gThrobberObserver;
var gMeterObserver;
		var startTime = 0;
        function onProgress() {
            if (!gThrobberObserver)
                gThrobberObserver = document.getElementById("Messenger:Throbber");
            if (!gMeterObserver)
                gMeterObserver = document.getElementById("Messenger:LoadingProgress");
            if ( gThrobberObserver && gMeterObserver ) {
                var busy = gThrobberObserver.getAttribute("busy");
                var wasBusy = gMeterObserver.getAttribute("mode") == "undetermined" ? "true" : "false";
                if ( busy == "true" ) {
                    if ( wasBusy == "false" ) {
                        // Remember when loading commenced.
    				    startTime = (new Date()).getTime();
                        // Turn progress meter on.
                        gMeterObserver.setAttribute("mode","undetermined");
                    }
                    // Update status bar.
                } else if ( busy == "false" && wasBusy == "true" ) {
                    // Record page loading time.
                    if (!gStatusObserver)
                        gStatusObserver = document.getElementById("Messenger:Status");
                    if ( gStatusObserver ) {
						var elapsed = ( (new Date()).getTime() - startTime ) / 1000;
						var msg = "Document: Done (" + elapsed + " secs)";
						dump( msg + "\n" );
                        gStatusObserver.setAttribute("value",msg);
                        defaultStatus = msg;
                    }
                    // Turn progress meter off.
                    gMeterObserver.setAttribute("mode","normal");
                }
            }
        }
        function dumpProgress() {
            var broadcaster = document.getElementById("Messenger:LoadingProgress");

            dump( "bindCount=" + bindCount + "\n" );
            dump( "broadcaster mode=" + broadcaster.getAttribute("mode") + "\n" );
            dump( "broadcaster value=" + broadcaster.getAttribute("value") + "\n" );
            dump( "meter mode=" + meter.getAttribute("mode") + "\n" );
            dump( "meter value=" + meter.getAttribute("value") + "\n" );
        }


function GetMsgFolderFromUri(uri)
{
	dump("GetMsgFolderFromUri of " + uri + "\n");
	try {
		var resource = GetResourceFromUri(uri);
		var msgfolder = resource.QueryInterface(Components.interfaces.nsIMsgFolder);
		return msgfolder;
	}
	catch (ex) {
		dump("failed to get the folder resource\n");
	}
	return null;
}

function GetResourceFromUri(uri)
{
	var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
	RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
        var resource = RDF.GetResource(uri);

	return resource;
}  
