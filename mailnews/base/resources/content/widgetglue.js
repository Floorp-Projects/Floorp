
function MsgStartUp()
{
  dump("StartUp: Messenger\n");
  var appCore = FindMessenger();
  if (appCore != null) {
	dump("In MsgStartUp()");
    dump("Initializing AppCore and setting Window\n");
    appCore.SetWindow(window);
	ChangeFolderByURI("mailbox://Inbox");
	//In the future we'll want to read this in from a preference.
	MsgViewAllMsgs();

  }
}

function MsgLoadNewsMessage(url)
{
  dump("\n\nMsgLoadNewsMessage from XUL\n");
  OpenURL(url);
}

function MsgHome(url)
{
  var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
  if (!toolkitCore) {
      toolkitCore = new ToolkitCore();
      if (toolkitCore) {
        toolkitCore.Init("ToolkitCore");
      }
    }
  if (toolkitCore) {
    toolkitCore.ShowWindow(url, window);
  }
}

function MsgNewMessage() 
{
  dump("\n\nMsgNewMessage from XUL\n");
  NewMessage();
} 

function MsgGetMessage() 
{
  GetNewMessages();
}

function MsgDeleteMessage()
{
  dump("\nMsgDeleteMessage from XUL\n");
  var tree = frames[0].frames[1].document.getElementById('threadTree');
  if(tree)
    dump("tree is valid\n");
  var appCore = FindMessenger();
  if (appCore != null) {
    dump("\nAppcore isn't null in MsgDeleteMessage\n");
    appCore.SetWindow(window);
	//get the selected elements
    var messageList = tree.getElementsByAttribute("selected", "true");
	//get the current folder
	var srcFolder = tree.childNodes[5];
    appCore.DeleteMessages(tree, srcFolder, messageList);
  }
}

function MsgDeleteFolder()
{
	var appCore = FindMessenger();
	if (appCore != null) {
		appCore.SetWindow(window);
		//get the selected elements
		var tree = frames[0].frames[0].document.getElementById('folderTree');
		var folderList = tree.getElementsByAttribute("selected", "true");
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

			appCore.DeleteFolders(tree.database, parent, folder);
		}
	}


}

function MsgReplyMessage()
{
  ComposeMessageWithType(0);
}

function MsgReplyToAllMessage() 
{
  ComposeMessageWithType(1);
}

function MsgForwardMessage()
{
  ComposeMessageWithType(3);
}

function MsgForwardAsAttachment()
{
  ComposeMessageWithType(2);
}

function MsgForwardAsInline()
{
  ComposeMessageWithType(3);
}

function MsgForwardAsQuoted()
{
  ComposeMessageWithType(4);
}

function MsgCopyMessage(destFolder)
{
	// Get the id for the folder we're copying into
    destUri = destFolder.getAttribute('id');
	dump(destUri);

	var tree = frames[0].frames[1].document.getElementById('threadTree');
	if(tree)
	{
		//Get the selected messages to copy
		var messageList = tree.getElementsByAttribute("selected", "true");
		var appCore = FindMessenger();
		if (appCore != null) {
		    appCore.SetWindow(window);
			//get the current folder
			var srcFolder = tree.childNodes[5];
			appCore.CopyMessages(srcFolder, destFolder, messageList);
		}
	}	
}

function MsgMoveMessage(destFolder)
{
	// Get the id for the folder we're copying into
    destUri = destFolder.getAttribute('id');
	dump(destUri);

	var tree = frames[0].frames[1].document.getElementById('threadTree');
	if(tree)
	{
		//Get the selected messages to copy
		var messageList = tree.getElementsByAttribute("selected", "true");
		var appCore = FindMessenger();
		if (appCore != null) {
		    appCore.SetWindow(window);
			//get the current folder
			var srcFolder = tree.childNodes[5];
			appCore.MoveMessages(srcFolder, destFolder, messageList);
		}
	}	
}

function MsgViewAllMsgs() 
{
	dump("MsgViewAllMsgs");

    var tree = frames[0].frames[1].document.getElementById('threadTree'); 

	var appCore = FindMessenger();
	if (appCore != null) {
	    appCore.SetWindow(window);
		appCore.ViewAllMessages(tree.database);
	}

	//hack to make it get new view.  
	var currentFolder = tree.childNodes[5].getAttribute('id');
	tree.childNodes[5].setAttribute('id', currentFolder);

}

function MsgViewUnreadMsg()
{
	dump("MsgViewUnreadMsgs");

    var tree = frames[0].frames[1].document.getElementById('threadTree'); 

	var appCore = FindMessenger();
	if (appCore != null) {
	    appCore.SetWindow(window);
		appCore.ViewUnreadMessages(tree.database);
	}

	//hack to make it get new view.  
	var currentFolder = tree.childNodes[5].getAttribute('id');
	tree.childNodes[5].setAttribute('id', currentFolder);


}

function MsgViewAllThreadMsgs()
{
	dump("MsgViewAllMessagesThreaded");

    var tree = frames[0].frames[1].document.getElementById('threadTree'); 

	var appCore = FindMessenger();
	if (appCore != null) {
	    appCore.SetWindow(window);
		appCore.ViewAllThreadMessages(tree.database);
	}

	//hack to make it get new view.  
	var currentFolder = tree.childNodes[5].getAttribute('id');
	tree.childNodes[5].setAttribute('id', currentFolder);
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
    var folderTree = frames[0].frames[0].document.getElementById('folderTree'); 
	var selectedFolderList = folderTree.getElementsByAttribute("selected", "true");
	var selectedFolder = selectedFolderList[0];

	var appCore = FindMessenger();
	if (appCore != null) {
	    appCore.SetWindow(window);
		//Note this is temporary hardcoding until I can get this from a dialog
		appCore.NewFolder(folderTree.database, selectedFolder, "New Folder");
	}

}


function MsgOpenAttachment() {}
function MsgSaveAsFile() {}
function MsgSaveAsTemplate() {}
function MsgSendUnsentMsg() {}
function MsgUpdateMsgCount() {}
function MsgRenameFolder() {}
function MsgEmptyTrash() {}
function MsgCompactFolders() {}
function MsgImport() {}
function MsgWorkOffline() {}
function MsgSynchronize() {}
function MsgGetSelectedMsg() {}
function MsgGetFlaggedMsg() {}
function MsgEditUndo() {}
function MsgEditRedo() {}
function MsgEditCut() {}
function MsgEditCopy() {}
function MsgEditPaste() {}
function MsgSelectAll() {}
function MsgSelectThread() {}
function MsgSelectFlaggedMsg() {}
function MsgFind() {}
function MsgFindAgain() {}
function MsgSearchMessages() {}
function MsgFilters() {}
function MsgFolderProperties() {}
function MsgPreferences() 
{
    var prefsCore = XPAppCoresManager.Find("PrefsCore");
    if (!prefsCore) {
      prefsCore = new PrefsCore();
      if (prefsCore) {
        prefsCore.Init("PrefsCore");
      }
    }
    if (prefsCore) {
      prefsCore.ShowWindow(window);
    }
}

function MsgShowMsgToolbar() {}
function MsgShowLocationbar() {}
function MsgShowMessage() {}
function MsgShowFolders() {}
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
function MsgStop() {}
function MsgViewPageSource() {}
function MsgViewPageInfo() {}
function MsgFirstUnreadMessage() {}
function MsgFirstFlaggedMessage() {}
function MsgNextMessage() {}
function MsgNextUnreadMessage() {}
function MsgNextFlaggedMessage() {}
function MsgPreviousMessage() {}
function MsgPreviousUnreadMessage() {}
function MsgPreviousFlaggedMessage() {}
function MsgGoBack() {}
function MsgGoForward() {}
function MsgEditMessageAsNew() {}
function MsgAddSenderToAddressBook() {}
function MsgAddAllToAddressBook() {}

function MsgMarkMsgAsRead(markRead)
{
  dump("\MsgMarkMsgAsRead from XUL\n");
  var tree = frames[0].frames[1].document.getElementById('threadTree');
  var appCore = FindMessenger();
  if (appCore != null) {
    appCore.SetWindow(window);
	//get the selected elements
    var messageList = tree.getElementsByAttribute("selected", "true");
    appCore.MarkMessagesRead(tree.database, messageList, markRead);
  }

}

function MsgMarkThreadAsRead() {}
function MsgMarkByDate() {}
function MsgMarkAllRead() {}
function MsgMarkAsFlagged() {}
function MsgIgnoreThread() {}
function MsgWatchThread() {}
