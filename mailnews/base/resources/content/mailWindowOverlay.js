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

function view_init()
{
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

function MsgGetMessage() 
{
	var folders = GetSelectedMsgFolders();
	var compositeDataSource = GetCompositeDataSource("GetNewMessages");
	GetNewMessages(folders, compositeDataSource);
}

function MsgDeleteMessage(reallyDelete, fromToolbar)
{

	if(reallyDelete)
		dump("reallyDelete\n");
	var srcFolder = GetLoadedMsgFolder();
	// if from the toolbar, return right away if this is a news message
	// only allow cancel from the menu:  "Edit | Cancel / Delete Message"
	if (fromToolbar)
	{
		var folderResource = srcFolder.QueryInterface(Components.interfaces.nsIRDFResource);
		var uri = folderResource.Value;
		//dump("uri[0:6]=" + uri.substring(0,6) + "\n");
		if (uri.substring(0,6) == "news:/")
		{
			//dump("delete ignored!\n");
			return;
		}
	}
	dump("tree is valid\n");
	//get the selected elements

	var compositeDataSource = GetCompositeDataSource("DeleteMessages");
	var messages = GetSelectedMessages();

	SetNextMessageAfterDelete(null, true);
	DeleteMessages(compositeDataSource, srcFolder, messages, reallyDelete);
}

function MsgCopyMessage(destFolder)
{
	// Get the id for the folder we're copying into
    destUri = destFolder.getAttribute('id');
	destResource = RDF.GetResource(destUri);
	destMsgFolder = destResource.QueryInterface(Components.interfaces.nsIMsgFolder);

	var srcFolder = GetLoadedMsgFolder();
	if(srcFolder)
	{
		var compositeDataSource = GetCompositeDataSource("Copy");
		var messages = GetSelectedMessages();

		CopyMessages(compositeDataSource, srcFolder, destMsgFolder, messages, false);
	}	
}

function MsgMoveMessage(destFolder)
{
	// Get the id for the folder we're copying into
    destUri = destFolder.getAttribute('id');
	destResource = RDF.GetResource(destUri);
	destMsgFolder = destResource.QueryInterface(Components.interfaces.nsIMsgFolder);
	
	var srcFolder = GetLoadedMsgFolder();
	if(srcFolder)
	{
		var compositeDataSource = GetCompositeDataSource("Move");
		var messages = GetSelectedMessages();

		var srcResource = srcFolder.QueryInterface(Components.interfaces.nsIRDFResource);
		var srcUri = srcResource.Value;
		if (srcUri.substring(0,6) == "news:/")
		{
			CopyMessages(compositeDataSource, srcFolder, destMsgFolder, messages, false);
		}
		else
		{
			SetNextMessageAfterDelete(null, true);

			CopyMessages(compositeDataSource, srcFolder, destMsgFolder, messages, true);
		}
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
  var messageArray = GetSelectedMessages();

  dump("\nMsgReplyMessage from XUL\n");
  if (event && event.shiftKey)
    ComposeMessage(msgComposeType.Reply, msgComposeFormat.OppositeOfDefault, loadedFolder, messageArray);
  else
    ComposeMessage(msgComposeType.Reply, msgComposeFormat.Default, loadedFolder, messageArray);
}

function MsgReplyToAllMessage(event) 
{
  var loadedFolder = GetLoadedMsgFolder();
  var messageArray = GetSelectedMessages();

  dump("\nMsgReplyToAllMessage from XUL\n");
  if (event && event.shiftKey)
    ComposeMessage(msgComposeType.ReplyAll, msgComposeFormat.OppositeOfDefault, loadedFolder, messageArray);
  else
    ComposeMessage(msgComposeType.ReplyAll, msgComposeFormat.Default, loadedFolder, messageArray);
}

function MsgForwardMessage(event)
{

  dump("\nMsgForwardMessage from XUL\n");
  var forwardType = 0;
  try {
  	var forwardType = pref.GetIntPref("mail.forward_message_mode");
  } catch (e) {dump ("failed to retrieve pref mail.forward_message_mode");}
  
  if (forwardType == 0)
  	MsgForwardAsAttachment(event);
  else
  	MsgForwardAsInline(event);
}

function MsgForwardAsAttachment(event)
{
  var loadedFolder = GetLoadedMsgFolder();
  var messageArray = GetSelectedMessages();

  dump("\nMsgForwardAsAttachment from XUL\n");
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

 dump("\nMsgForwardAsInline from XUL\n");
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

function MsgHome(url)
{
  window.open( url, "_blank", "chrome,dependent=yes,all" );
}

function MsgNewFolder()
{
	var windowTitle = Bundle.GetStringFromName("newFolderDialogTitle");
	var preselectedFolder = GetFirstSelectedMsgFolder();

	CreateNewSubfolder("chrome://messenger/content/newFolderNameDialog.xul",windowTitle, preselectedFolder);
}


function MsgSubscribe()
{
	var windowTitle = Bundle.GetStringFromName("subscribeDialogTitle");
	var preselectedFolder = GetFirstSelectedMsgFolder();
	Subscribe(windowTitle, preselectedFolder);
}

function MsgSaveAsFile() 
{
	dump("\MsgSaveAsFile from XUL\n");
	var messages = GetSelectedMessages();
	if (messages && messages.length == 1)
	{
		SaveAsFile(messages[0]);
	}
}


function MsgSaveAsTemplate() 
{
	dump("\MsgSaveAsTemplate from XUL\n");
	var folder = GetLoadedMsgFolder();
	var messages = GetSelectedMessages();
	if (messages && messages.length == 1)
	{
		SaveAsTemplate(messages[0], folder);
	}
}

function MsgOpenNewWindowForFolder(folderUri)
{
	if(!folderUri)
	{
		var folder = GetLoadedMsgFolder();
		var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
		folderUri = folderResource.Value;
	}

	if(folderUri)
	{
		var layoutType = pref.GetIntPref("mail.pane_config");
		
		if(layoutType == 0)
			window.openDialog( "chrome://messenger/content/messenger.xul", "_blank", "chrome,all,dialog=no", folderUri );
		else
			window.openDialog("chrome://messenger/content/mail3PaneWindowVertLayout.xul", "_blank", "chrome,all,dialog=no", folderUri );
	}

}

function MsgOpenNewWindowForMessage(messageUri, folderUri)
{
	if(!messageUri)
	{
		var message = GetLoadedMessage();
		var messageResource = message.QueryInterface(Components.interfaces.nsIRDFResource);
		messageUri = messageResource.Value;
	}

	if(!folderUri)
	{
        var message = RDF.GetResource(messageUri);
        message = message.QueryInterface(Components.interfaces.nsIMessage);
        var folder = message.msgFolder;
		var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
		folderUri = folderResource.Value;
	}

	if(messageUri && folderUri)
	{
		window.openDialog( "chrome://messenger/content/messageWindow.xul", "_blank", "chrome,all,dialog=no", messageUri, folderUri );
	}


}

function CloseMailWindow() 
{
	dump("\nClose from XUL\nDo something...\n");
	window.close();
}

function MsgMarkMsgAsRead(markRead)
{
	var selectedMessages = GetSelectedMessages();
	var compositeDataSource = GetCompositeDataSource("MarkMessageRead");

	MarkMessagesRead(compositeDataSource, selectedMessages, markRead);
}

function MsgMarkAsFlagged(markFlagged)
{
	var selectedMessages = GetSelectedMessages();
	var compositeDataSource = GetCompositeDataSource("MarkMessageFlagged");

	MarkMessagesFlagged(compositeDataSource, selectedMessages, markFlagged);
}


function MsgMarkAllRead()
{
	var compositeDataSource = GetCompositeDataSource("MarkAllMessagesRead");
	var folder = GetLoadedMsgFolder();

	if(folder)
		MarkAllMessagesRead(compositeDataSource, folder);
}

function MsgMarkThreadAsRead()
{
	
	var messageList = GetSelectedMessages();
	if(messageList.length == 1)
	{
		var message = messageList[0];
		var compositeDataSource = GetCompositeDataSource("MarkThreadAsRead");

		MarkThreadAsRead(compositeDataSource, message);
	}

}

function MsgViewPageSource() 
{
	dump("MsgViewPageSource(); \n ");
	
	var messages = GetSelectedMessages();
	ViewPageSource(messages);
}

function MsgFind() {
    messenger.find();
}
function MsgFindAgain() {
    messenger.findAgain();
}

function MsgSearchMessages() {
	var preselectedFolder = GetFirstSelectedMsgFolder();
    window.openDialog("chrome://messenger/content/SearchDialog.xul", "SearchMail", "chrome,resizable", { folder: preselectedFolder });
}

function MsgFilters() {
    window.openDialog("chrome://messenger/content/FilterListDialog.xul", "FilterDialog", "chrome,resizable");
}

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

function MsgReload() 
{
	ReloadMessage();
}

function MsgStop()
{
	StopUrls();
}

function MsgSendUnsentMsg() 
{
	var folder = GetFirstSelectedMsgFolder();
	if(folder)
	{
		SendUnsentMessages(folder);
	}
}

function PrintEnginePrint()
{

	var messageList = GetSelectedMessages();
	numMessages = messageList.length;


	if (numMessages == 0)
	{
		dump("PrintEnginePrint(): No messages selected.\n");
		return false;
	}  

	var selectionArray = new Array(numMessages);

	for(var i = 0; i < numMessages; i++)
	{
		var messageResource = messageList[i].QueryInterface(Components.interfaces.nsIRDFResource);
		selectionArray[i] = messageResource.Value;
	}

	printEngineWindow = window.openDialog("chrome://messenger/content/msgPrintEngine.xul",
								                        "",
								                        "chrome,dialog=no,all",
								                        numMessages, selectionArray, statusFeedback);
	return true;
}


function MsgMarkByDate() {}
function MsgOpenAttachment() {}
function MsgUpdateMsgCount() {}
function MsgImport() {}
function MsgWorkOffline() {}
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
function MsgIgnoreThread() {}
function MsgWatchThread() {}
