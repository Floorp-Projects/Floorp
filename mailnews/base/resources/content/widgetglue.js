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
 
//The eventual goal is for this file to go away and its contents to be brought into
//mailWindowOverlay.js.  This is currently being done.


var viewShowAll =0;
var viewShowRead = 1;
var viewShowUnread =2;
var viewShowWatched = 3;


function MsgDeleteMessage(reallyDelete, fromToolbar)
{
  //dump("\nMsgDeleteMessage from XUL\n");
  //dump("from toolbar? " + fromToolbar + "\n");

  if(reallyDelete)
	dump("reallyDelete\n");
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
	if(nextMessage)
		gNextMessageAfterDelete = nextMessage.getAttribute('id');
	else
		gNextMessageAfterDelete = null;

	messenger.DeleteMessages(tree.database, srcFolder.resource, messageList, reallyDelete);
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
            dump("folder = " + folder.nodeName + "\n"); 
            dump("parent = " + parent.nodeName + "\n"); 
            messenger.DeleteFolders(tree.database,
                                    parent.resource, folder.resource);
        }
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
        var srcUri = srcFolder.getAttribute('ref');
        if (srcUri.substring(0,6) == "news:/")
        {
            messenger.CopyMessages(tree.database,
                                   srcFolder.resource,
                                   destFolder.resource, messageList, false);
        }
        else
        {
			if(nextMessage)
				gNextMessageAfterDelete = nextMessage.getAttribute('id');
			else
				gNextMessageAfterDelete = null;

            messenger.CopyMessages(tree.database,
                                   srcFolder.resource,
                                   destFolder.resource, messageList, true);
        }
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
	SortThreadPane('DateColumn', 'http://home.netscape.com/NC-rdf#Date', null, true, null);
}

function MsgSortBySender()
{
	SortThreadPane('AuthorColumn', 'http://home.netscape.com/NC-rdf#Sender', 'http://home.netscape.com/NC-rdf#Date', true, null);
}

function MsgSortByStatus()
{
	SortThreadPane('StatusColumn', 'http://home.netscape.com/NC-rdf#Status', 'http://home.netscape.com/NC-rdf#Date', true, null);
}

function MsgSortBySubject()
{
	SortThreadPane('SubjectColumn', 'http://home.netscape.com/NC-rdf#Subject', 'http://home.netscape.com/NC-rdf#Date', true, null);
}

function MsgSortByFlagged() 
{
	SortThreadPane('FlaggedButtonColumn', 'http://home.netscape.com/NC-rdf#Flagged', 'http://home.netscape.com/NC-rdf#Date', true, null);
}
function MsgSortByPriority()
{
	SortThreadPane('PriorityColumn', 'http://home.netscape.com/NC-rdf#Priority', 'http://home.netscape.com/NC-rdf#Date',true, null);
}
function MsgSortBySize() 
{
	SortThreadPane('SizeColumn', 'http://home.netscape.com/NC-rdf#Size', 'http://home.netscape.com/NC-rdf#Date', true, null);
}
function MsgSortByThread()
{
	ChangeThreadView()
}
function MsgSortByUnread()
{
	SortThreadPane('UnreadColumn', 'http://home.netscape.com/NC-rdf#TotalUnreadMessages','http://home.netscape.com/NC-rdf#Date', true, null);
}
function MsgSortByOrderReceived()
{
	SortThreadPane('OrderReceivedColumn', 'http://home.netscape.com/NC-rdf#OrderReceived','http://home.netscape.com/NC-rdf#Date', true, null);
}
function MsgSortAscending() 
{
	dump("not implemented yet.\n");
}
function MsgSortDescending()
{
	dump("not implemented yet.\n");
}
function MsgSortByRead()
{
	SortThreadPane('UnreadButtonColumn', 'http://home.netscape.com/NC-rdf#HasUnreadMessages','http://home.netscape.com/NC-rdf#Date', true, null);
}

function MsgSortByTotal()
{
	SortThreadPane('TotalColumn', 'http://home.netscape.com/NC-rdf#TotalMessages', 'http://home.netscape.com/NC-rdf#Date', true, null);
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






function MsgOpenAttachment() {}

function MsgSendUnsentMsg() 
{
    var identity = getIdentityForSelectedServer();
	messenger.SendUnsentMessages(identity);
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
    var folderTree = GetFolderTree();
    if (folderTree)
    {
	if (uri && (uri != "") && name && (name != "")) {
                var selectedFolder = GetResourceFromUri(uri);
                folderTree.clearItemSelection();
                folderTree.clearCellSelection();
                messenger.RenameFolder(folderTree.database, selectedFolder, name);
        }
        else {
                dump("no name or nothing selected\n");
        }   
    }
    else {
	dump("no folder tree\n");
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
                    var trashSelected = IsSpecialFolderSelected('Trash');
                    if(trashSelected)
                    {
                        tree.clearItemSelection();
                        RefreshThreadTreeView();
                    }
                    messenger.EmptyTrash(tree.database, folder.resource);
                    if (trashSelected)
                    {
                        trashElement = document.getElementById(trashUri);
                        if (trashElement)
                            ChangeSelection(tree, trashElement);
                    }
                }
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
            var messageUri = "";
            var selctedFolderUri = "";
            var isImap = false;
            if (folderList.length == 1)
            {
                selectedFolderUri = folderList[0].getAttribute('id');
                var threadTree = GetThreadTree();
                var messageList = threadTree.selectedItems;
                if (messageList && messageList.length == 1)
                    messageUri = messageList[0].getAttribute('id');
                if (selectedFolderUri.indexOf("imap:") != -1)
                {
                    isImap = true;
                }
                else
                {
                    ClearThreadTreeSelection();
                    ClearMessagePane();
                }
            }
            var i;
            var folder;
            for(i = 0; i < folderList.length; i++)
            {
                folder = folderList[i];
                if (folder)
                {
                    folderuri = folder.getAttribute('id');
                    dump(folderuri + "\n");
                    dump("folder = " + folder.nodeName + "\n"); 
                    messenger.CompactFolder(tree.database, folder.resource);
                }
            }
            if (!isImap && selectedFolderUri && selectedFolderUri != "")
            {
                /* this doesn't work; local compact is now an async operation
                dump("selected folder = " + selectedFolderUri + "\n");
                var selectedFolder =
                    document.getElementById(selectedFolderUri);
                ChangeSelection(tree, selectedFolder);
                */
                tree.clearItemSelection();
                tree.clearCellSelection();
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

function MsgFind() {
    messenger.find();
}
function MsgFindAgain() {
    messenger.findAgain();
}

function MsgSearchMessages() {
    window.openDialog("chrome://messenger/content/SearchDialog.xul", "SearchMail", "chrome");
}

function MsgFilters() {
    window.openDialog("chrome://messenger/content/FilterListDialog.xul", "FilterDialog", "chrome");
}

function MsgToggleMessagePane()
{
	//OnClickThreadAndMessagePaneSplitter is based on the value before the splitter is toggled.
	OnClickThreadAndMessagePaneSplitter();
    MsgToggleSplitter("gray_horizontal_splitter");
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
function MsgViewThreadsUnread() {}
function MsgViewWatchedThreadsUnread() {}
function MsgViewIgnoreThread() {}

function MsgViewAllHeaders() 
{
	pref.SetIntPref("mail.show_headers",2);
	MsgReload();
	return true;
}
function MsgViewNormalHeaders() 
{
	pref.SetIntPref("mail.show_headers",1);
	MsgReload();
	return true;
}
function MsgViewBriefHeaders() 
{
	pref.SetIntPref("mail.show_headers",0);
	MsgReload();
	return true;
}

function MsgViewAttachInline() {}
function MsgWrapLongLines() {}
function MsgIncreaseFont() {}
function MsgDecreaseFont() {}

function MsgReload() 
{
	ThreadPaneSelectionChange()
}

function MsgShowImages() {}
function MsgRefresh() {}

function MsgViewPageSource() 
{
	dump("MsgViewPageSource(); \n ");
	
	var tree = GetThreadTree();
	var selectedItems = tree.selectedItems;
	var numSelected = selectedItems.length;
  var url;
  var uri;
  var mailSessionProgID      = "component://netscape/messenger/services/session";

  if (numSelected == 0)
  {
    dump("MsgViewPageSource(): No messages selected.\n");
    return false;
  }

  // First, get the mail session
  var mailSession = Components.classes[mailSessionProgID].getService();
  if (!mailSession)
    return false;

  mailSession = mailSession.QueryInterface(Components.interfaces.nsIMsgMailSession);
  if (!mailSession)
    return false;

	for(var i = 0; i < numSelected; i++)
	{
    uri = selectedItems[i].getAttribute("id");
  
    // Now, we need to get a URL from a URI
    url = mailSession.ConvertMsgURIToMsgURL(uri, msgWindow);
    if (url)
      url += "?header=src";

    // Use a browser window to view source
    window.openDialog( "chrome://navigator/content/",
                       "_blank",
                       "chrome,menubar,status,dialog=no,resizable",
                       url,
                       "view-source" );
	}
}

function MsgViewPageInfo() {}
function MsgFirstUnreadMessage() {}
function MsgFirstFlaggedMessage() {}

function MsgStop() {
	StopUrls();
}

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

function MsgGoBack() {}
function MsgGoForward() {}

function MsgEditMessageAsNew()
{
    ComposeMessage(msgComposeType.Template, msgComposeFormat.Default);
}

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

function MsgMarkThreadAsRead()
{
	var tree = GetThreadTree();
	var messageList = ConvertDOMListToResourceArray(tree.selectedItems);
	if(messageList.Count() == 1)
	{
		var messageSupports = messageList.GetElementAt(0);
		if(messageSupports)
		{
			var message = messageSupports.QueryInterface(Components.interfaces.nsIMessage);
			if(message)
			{
				var folder = message.GetMsgFolder();
				if(folder)
				{
					var thread = folder.getThreadForMessage(message);
					if(thread)
					{
						messenger.markThreadRead(tree.database, folder, thread);
					}
				}
			}
		}
	}

}

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


function GetMsgFolderFromUri(uri)
{
	//dump("GetMsgFolderFromUri of " + uri + "\n");
	try {
		var resource = GetResourceFromUri(uri);
		var msgfolder = resource.QueryInterface(Components.interfaces.nsIMsgFolder);
		return msgfolder;
	}
	catch (ex) {
		//dump("failed to get the folder resource\n");
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

var accountManagerProgID   = "component://netscape/messenger/account-manager";
var messengerMigratorProgID   = "component://netscape/messenger/migrator";

function verifyAccounts() {
    var openWizard = false;
    var prefillAccount;
    
    try {
        var am = Components.classes[accountManagerProgID].getService(Components.interfaces.nsIMsgAccountManager);

        var accounts = am.accounts;

        // as long as we have some accounts, we're fine.
        var accountCount = accounts.Count();
        if (accountCount > 0) {
            prefillAccount = getFirstInvalidAccount(accounts);
            dump("prefillAccount = " + prefillAccount + "\n");
        } else {
            try {
                messengerMigrator = Components.classes[messengerMigratorProgID].getService(Components.interfaces.nsIMessengerMigrator);  
                dump("attempt to UpgradePrefs.  If that fails, open the account wizard.\n");
                messengerMigrator.UpgradePrefs();
            }
            catch (ex) {
                // upgrade prefs failed, so open account wizard
                openWizard = true;
            }
        }

        if (openWizard || prefillAccount) {
            MsgAccountWizard(prefillAccount);
        }

    }
    catch (ex) {
        dump("error verifying accounts " + ex + "\n");
        return;
    }
}

