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

function MsgGetMessage() 
{
	var folders = GetSelectedMsgFolders();
	var compositeDataSource = GetCompositeDataSource("GetNewMessages");
	GetNewMessages(folders, compositeDataSource);
}

function MsgCopyMessage(destFolder)
{
	// Get the id for the folder we're copying into
    destUri = destFolder.getAttribute('id');
	destResource = RDF.GetResource(destUri);
	destMsgFolder = destResource.QueryInterface(Components.interfaces.nsIMsgFolder);

	var srcFolder = GetLoadedMsgFolder();
	var compositeDataSource = GetCompositeDataSource("Copy");
	var messages = GetSelectedMessages();

	CopyMessages(compositeDataSource, srcFolder, destMsgFolder, messages);
	
}

function MsgNewMessage(event)
{
  var loadedFolder = GetLoadedMsgFolder();
  var messageArray = GetSelectedMessages();

  if (event.shiftKey)
    ComposeMessage(msgComposeType.New, msgComposeFormat.OppositeOfDefault, loadedFolder, messageArray);
  else
    ComposeMessage(msgComposeType.New, msgComposeFormat.Default, loadedFolder, messageArray);
} 

function MsgReplyMessage(event)
{
  var loadedFolder = GetLoadedMsgFolder();
  var messageArray = GetSelectedMessages();

  dump("\nMsgReplyMessage from XUL\n");
  if (event.shiftKey)
    ComposeMessage(msgComposeType.Reply, msgComposeFormat.OppositeOfDefault, loadedFolder, messageArray);
  else
    ComposeMessage(msgComposeType.Reply, msgComposeFormat.Default, loadedFolder, messageArray);
}

function MsgReplyToAllMessage(event) 
{
  var loadedFolder = GetLoadedMsgFolder();
  var messageArray = GetSelectedMessages();

  dump("\nMsgReplyToAllMessage from XUL\n");
  if (event.shiftKey)
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
  if (event.shiftKey)
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
  if (event.shiftKey)
    ComposeMessage(msgComposeType.ForwardInline,
                   msgComposeFormat.OppositeOfDefault, loadedFolder, messageArray);
  else
    ComposeMessage(msgComposeType.ForwardInline, msgComposeFormat.Default, loadedFolder, messageArray);
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

	var useRealSubscribeDialog = false;

	try {
		useRealSubscribeDialog = pref.GetBoolPref("mailnews.use-real-subscribe-dialog");
	}
	catch (ex) {
		useRealSubscribeDialog = false;
	}

	var preselectedFolder = GetFirstSelectedMsgFolder();
	if (useRealSubscribeDialog)  {
			Subscribe(windowTitle, preselectedFolder);
	}
	else {
			CreateNewSubfolder("chrome://messenger/content/subscribeDialog.xul", windowTitle, preselectedFolder);
	}
}

function MsgAccountManager()
{
    window.openDialog("chrome://messenger/content/AccountManager.xul",
                      "AccountManager", "chrome,modal");
}

// we do this from a timer because if this is called from the onload=
// handler, then the parent window doesn't appear until after the wizard
// has closed, and this is confusing to the user
function MsgAccountWizard()
{
    setTimeout("msgOpenAccountWizard();", 0);
}

function msgOpenAccountWizard()
{
    window.openDialog("chrome://messenger/content/AccountWizard.xul",
                      "AccountWizard", "chrome,modal");
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
			window.openDialog( "chrome://messenger/content/", "_blank", "chrome,all,dialog=no", folderUri );
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
		var folder = GetLoadedMsgFolder();
		var folderResource = folder.QueryInterface(Components.interfaces.nsIRDFResource);
		folderUri = folderResource.Value;
	}

	if(messageUri && folderUri)
	{
		window.openDialog( "chrome://messenger/content/messageWindow.xul", "_blank", "chrome,all,dialog=no", messageUri, folderUri );
	}


}

