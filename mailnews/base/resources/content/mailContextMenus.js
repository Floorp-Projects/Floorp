/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

//NOTE: gMessengerBundle must be defined and set or this Overlay won't work

function fillThreadPaneContextMenu()
{
  var numSelected = GetNumSelectedMessages();

  var isNewsgroup = false;
  var selectedMessage = null;

  if(numSelected >= 0) {
    selectedMessage = GetFirstSelectedMessage();
    isNewsgroup = IsNewsMessage(selectedMessage);
  }


  SetupNewMessageWindowMenuItem("threadPaneContext-openNewWindow", numSelected, false);
  SetupEditAsNewMenuItem("threadPaneContext-editAsNew", numSelected, false);

  ShowMenuItem("threadPaneContext-sep-open", (numSelected <= 1));

  SetupReplyToSenderMenuItem("threadPaneContext-replySender", numSelected, false);
  SetupReplyToNewsgroupMenuItem("threadPaneContext-replyNewsgroup", numSelected, isNewsgroup, false);
  SetupReplyAllMenuItem("threadPaneContext-replyAll", numSelected, false);
  SetupForwardMenuItem("threadPaneContext-forward", numSelected, false);
  SetupForwardAsAttachmentMenuItem("threadPaneContext-forwardAsAttachment", numSelected, false);

  ShowMenuItem("threadPaneContext-sep-reply", true);

  SetupMoveMenuItem("threadPaneContext-moveMenu", numSelected, isNewsgroup, false);
  SetupCopyMenuItem("threadPaneContext-copyMenu", numSelected, false);
  SetupSaveAsMenuItem("threadPaneContext-saveAs", numSelected, false);
  SetupPrintMenuItem("threadPaneContext-print", numSelected, false);
  SetupDeleteMenuItem("threadPaneContext-delete", numSelected, isNewsgroup, false);
  SetupAddSenderToABMenuItem("threadPaneContext-addSenderToAddressBook", numSelected, false);
  SetupAddAllToABMenuItem("threadPaneContext-addAllToAddressBook", numSelected, false);


  ShowMenuItem("threadPaneContext-sep-edit", (numSelected <= 1));

  return(true);
}

function SetupNewMessageWindowMenuItem(menuID, numSelected, forceHide)
{
  ShowMenuItem(menuID, (numSelected <= 1) && !forceHide);
  EnableMenuItem(menuID, (numSelected == 1));
}

function SetupEditAsNewMenuItem(menuID, numSelected, forceHide)
{
  ShowMenuItem(menuID, (numSelected <= 1)&& !forceHide);
  EnableMenuItem(menuID, (numSelected == 1));
}

function SetupReplyToSenderMenuItem(menuID, numSelected, forceHide)
{
  ShowMenuItem(menuID, (numSelected <= 1)&& !forceHide);
  EnableMenuItem(menuID, (numSelected == 1));
}

function SetupReplyToNewsgroupMenuItem(menuID, numSelected, isNewsgroup, forceHide)
{
  ShowMenuItem(menuID, (numSelected <= 1) && isNewsgroup && !forceHide);
  EnableMenuItem(menuID,  (numSelected == 1));
}

function SetupReplyAllMenuItem(menuID, numSelected, forceHide)
{
  ShowMenuItem(menuID, (numSelected <= 1) && !forceHide);
  EnableMenuItem(menuID, (numSelected == 1));
}

function SetupForwardMenuItem(menuID, numSelected, forceHide)
{
  ShowMenuItem(menuID,  (numSelected <= 1) && !forceHide);
  EnableMenuItem(menuID, (numSelected > 0));
}

function SetupForwardAsAttachmentMenuItem(menuID, numSelected, forceHide)
{
  ShowMenuItem(menuID,  (numSelected > 1) && !forceHide);
  EnableMenuItem(menuID, (numSelected > 1));
}

function SetupMoveMenuItem(menuID, numSelected, isNewsgroup, forceHide)
{
  ShowMenuItem(menuID, !isNewsgroup && !forceHide);
  EnableMenuItem(menuID, (numSelected > 0));
}

function SetupCopyMenuItem(menuID, numSelected, forceHide)
{
  ShowMenuItem(menuID, !forceHide);
  EnableMenuItem(menuID, (numSelected > 0));
}

function SetupSaveAsMenuItem(menuID, numSelected, forceHide)
{
  ShowMenuItem(menuID, (numSelected <= 1) && !forceHide);
  EnableMenuItem(menuID, (numSelected == 1));
}

function SetupPrintMenuItem(menuID, numSelected, forceHide)
{
  ShowMenuItem(menuID, !forceHide);
  EnableMenuItem(menuID, (numSelected > 0));
}

function SetupDeleteMenuItem(menuID, numSelected, isNewsgroup, forceHide)
{
  var showMenuItem = !forceHide;

  ShowMenuItem(menuID, showMenuItem);
  if(showMenuItem)
  {
    EnableMenuItem(menuID, (numSelected > 0));
    if(!isNewsgroup)
    {
      SetMenuItemLabel(menuID, gMessengerBundle.getString("delete"));
      SetMenuItemAccessKey(menuID, gMessengerBundle.getString("deleteAccessKey"));
    }
    else
    {
      SetMenuItemLabel(menuID, gMessengerBundle.getString("cancel"));
      SetMenuItemAccessKey(menuID, gMessengerBundle.getString("cancelAccessKey"));
    }
  }
}

function SetupAddSenderToABMenuItem(menuID, numSelected, forceHide)
{
  ShowMenuItem(menuID, (numSelected <= 1) && !forceHide);
  EnableMenuItem(menuID, false);
}

function SetupAddAllToABMenuItem(menuID, numSelected, forceHide)
{
  ShowMenuItem(menuID, (numSelected <= 1) && !forceHide);
  EnableMenuItem(menuID, false);
}

function fillFolderPaneContextMenu()
{
  var tree = GetFolderTree();
  var selectedItems = tree.selectedItems;
  var numSelected = selectedItems.length;

  var popupNode = document.getElementById('folderPaneContext');

  var targetFolder = document.popupNode.parentNode.parentNode;
  if (targetFolder.getAttribute('selected') != 'true')
  {
    tree.selectItem(targetFolder);
  }

  var isServer = targetFolder.getAttribute('IsServer') == 'true';
  var serverType = targetFolder.getAttribute('ServerType');
  var specialFolder = targetFolder.getAttribute('SpecialFolder');
  var canSubscribeToFolder = (serverType == "nntp") || (serverType == "imap");
  var isNewsgroup = !isServer && serverType == 'nntp';
  var canGetMessages =  (isServer && (serverType != "nntp") && (serverType !="none")) || isNewsgroup;

  EnableMenuItem("folderPaneContext-properties", !isServer);
  ShowMenuItem("folderPaneContext-getMessages", (numSelected <= 1) && canGetMessages);
  EnableMenuItem("folderPaneContext-getMessages", true);

  ShowMenuItem("folderPaneContext-openNewWindow", (numSelected <= 1) && !isServer);
  EnableMenuItem("folderPaneContext-openNewWindow", (true));

  SetupRenameMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder);
  SetupRemoveMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder);
  SetupCompactMenuItem(targetFolder, numSelected);

  ShowMenuItem("folderPaneContext-emptyTrash", (numSelected <= 1) && (specialFolder == 'Trash'));
  EnableMenuItem("folderPaneContext-emptyTrash", true);

  var showSendUnsentMessages = (numSelected <= 1) && (specialFolder == 'Unsent Messages');
  ShowMenuItem("folderPaneContext-sendUnsentMessages", showSendUnsentMessages);
  if (showSendUnsentMessages) {
    EnableMenuItem("folderPaneContext-sendUnsentMessages", IsSendUnsentMsgsEnabled(targetFolder));
  }

  ShowMenuItem("folderPaneContext-sep-edit", (numSelected <= 1));

  SetupNewMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder);

  ShowMenuItem("folderPaneContext-subscribe", (numSelected <= 1) && canSubscribeToFolder);
  EnableMenuItem("folderPaneContext-subscribe", true);

// News folder context menu =============================================

  ShowMenuItem("folderPaneContext-newsUnsubscribe", (numSelected <= 1) && canSubscribeToFolder && isNewsgroup);
  EnableMenuItem("folderPaneContext-newsUnsubscribe", true);
  ShowMenuItem("folderPaneContext-markAllRead", (numSelected <= 1) && isNewsgroup);
  EnableMenuItem("folderPaneContext-markAllRead", true);

// End of News folder context menu =======================================

  ShowMenuItem("folderPaneContext-searchMessages", (numSelected<=1));
  EnableMenuItem("folderPaneContext-searchMessages", true);

  return(true);
}

function SetupRenameMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder)
{
  var isSpecialFolder = specialFolder != 'none';
  var isMail = serverType != 'nntp';
  var canRename = (targetFolder.getAttribute('CanRename') == "true");

  ShowMenuItem("folderPaneContext-rename", (numSelected <= 1) && !isServer && (specialFolder == "none") && canRename);
  EnableMenuItem("folderPaneContext-rename", !isServer);

  if(canRename)
  {
    SetMenuItemLabel("folderPaneContext-rename", gMessengerBundle.getString("renameFolder"));
  }
}

function SetupRemoveMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder)
{
  var isMail = serverType != 'nntp';
  var isSpecialFolder = specialFolder != "none";
  //Can't currently delete Accounts or special folders.
  var showRemove = (numSelected <=1) && (isMail && !isSpecialFolder) && !isServer;


  ShowMenuItem("folderPaneContext-remove", showRemove);
  EnableMenuItem("folderPaneContext-remove", true);

  if(isMail && !isSpecialFolder)
  {
    SetMenuItemLabel("folderPaneContext-remove", gMessengerBundle.getString("removeFolder"));
  }
}

function SetupCompactMenuItem(targetFolder, numSelected)
{
    var canCompact = (targetFolder.getAttribute('CanCompact') == "true");
  ShowMenuItem("folderPaneContext-compact", (numSelected <=1) && canCompact);
  EnableMenuItem("folderPaneContext-compact", true );

  if(canCompact)
  {
    SetMenuItemLabel("folderPaneContext-compact", gMessengerBundle.getString("compactFolder"));
  }
}

function SetupNewMenuItem(targetFolder, numSelected, isServer, serverType, specialFolder)
{
  var canCreateNew = targetFolder.getAttribute('CanCreateSubfolders') == 'true';
  var isInbox = specialFolder == "Inbox";

  var showNew = ((numSelected <=1) && (serverType != 'nntp') && canCreateNew) || isInbox;
  ShowMenuItem("folderPaneContext-new", showNew);
  EnableMenuItem("folderPaneContext-new", true);
  if(showNew)
  {
    if(isServer || isInbox)
      SetMenuItemLabel("folderPaneContext-new", gMessengerBundle.getString("newFolder"));
    else
      SetMenuItemLabel("folderPaneContext-new", gMessengerBundle.getString("newSubfolder"));
  }
}

function ShowMenuItem(id, showItem)
{
  var item = document.getElementById(id);
  if(item)
  {
    var showing = (item.getAttribute('hidden') !='true');
    if(showItem != showing)
      item.setAttribute('hidden', showItem ? '' : 'true');
  }
}

function EnableMenuItem(id, enableItem)
{
  var item = document.getElementById(id);
  if(item)
  {
    var enabled = (item.getAttribute('disabled') !='true');
    if(enableItem != enabled)
    {
      item.setAttribute('disabled', enableItem ? '' : 'true');
    }
  }
}

function SetMenuItemLabel(id, label)
{
  var item = document.getElementById(id);
  if(item)
    item.setAttribute('label', label);

}

function SetMenuItemAccessKey(id, accessKey)
{
  var item = document.getElementById(id);
  if(item)
    item.setAttribute('accesskey', accessKey);

}

function fillMessagePaneContextMenu(contextMenu)
{
  var message = GetLoadedMessage();
  var numSelected = (message) ? 1 : 0;

  var isNewsgroup = false;

  if (numSelected == 1) {
    isNewsgroup = IsNewsMessage(message);
    }

  var hideMailItems = AreBrowserItemsShowing();

  SetupEditAsNewMenuItem("messagePaneContext-editAsNew", numSelected, (numSelected == 0 || hideMailItems));
  SetupReplyToSenderMenuItem("messagePaneContext-replySender", numSelected, (numSelected == 0 || hideMailItems));
  SetupReplyToNewsgroupMenuItem("messagePaneContext-replyNewsgroup", numSelected, isNewsgroup, (numSelected == 0 || hideMailItems));
  SetupReplyAllMenuItem("messagePaneContext-replyAll" , numSelected, (numSelected == 0 || hideMailItems));
  SetupForwardMenuItem("messagePaneContext-forward", numSelected, (numSelected == 0 || hideMailItems));
  SetupForwardAsAttachmentMenuItem("threadPaneContext-forwardAsAttachment", numSelected, hideMailItems);
  SetupMoveMenuItem("messagePaneContext-moveMenu", numSelected, isNewsgroup, (numSelected == 0 || hideMailItems));
  SetupCopyMenuItem("messagePaneContext-copyMenu", numSelected, (numSelected == 0 || hideMailItems));
  SetupSaveAsMenuItem("messagePaneContext-saveAs", numSelected, (numSelected == 0 || hideMailItems));
  SetupPrintMenuItem("messagePaneContext-print", numSelected, (numSelected == 0 || hideMailItems));
  SetupDeleteMenuItem("messagePaneContext-delete", numSelected, isNewsgroup, (numSelected == 0 || hideMailItems));
  SetupAddSenderToABMenuItem("messagePaneContext-addSenderToAddressBook", numSelected, (numSelected == 0 || hideMailItems));
  SetupAddAllToABMenuItem("messagePaneContext-addAllToAddressBook", numSelected, (numSelected == 0 || hideMailItems));

  //Figure out separators
  ShowMenuItem("messagePaneContext-sep-open", ShowMessagePaneOpenSeparator());
  ShowMenuItem("messagePaneContext-sep-reply", ShowMessagePaneReplySeparator());
  ShowMenuItem("messagePaneContext-sep-edit", ShowMessagePaneEditSeparator());
  ShowMenuItem("messagePaneContext-sep-link", ShowMessagePaneLinkSeparator());
  ShowMenuItem("messagePaneContext-sep-image", ShowMessagePaneImageSeparator());
  ShowMenuItem("messagePaneContext-sep-copy", ShowMessagePaneCopySeparator());
}

function AreBrowserItemsShowing()
{
  return(IsMenuItemShowingWithStyle("context-openlink") ||
    IsMenuItemShowingWithStyle("context-editlink") ||
    IsMenuItemShowingWithStyle("context-viewimage") ||
    IsMenuItemShowingWithStyle("context-copylink") ||
    IsMenuItemShowingWithStyle("context-copyimage") ||
    IsMenuItemShowingWithStyle("context-savelink") ||
    IsMenuItemShowingWithStyle("context-saveimage") ||
    IsMenuItemShowingWithStyle("context-bookmarklink"));
}

function ShowMessagePaneOpenSeparator()
{
  return(IsMenuItemShowingWithStyle("context-selectall") ||
    IsMenuItemShowingWithStyle("context-copy"));
}

function ShowMessagePaneReplySeparator()
{
  return (IsMenuItemShowing("messagePaneContext-replySender") ||
    IsMenuItemShowing("messagePaneContext-replyNewsgroup") ||
    IsMenuItemShowing("messagePaneContext-replyAll") ||
    IsMenuItemShowing("messagePaneContext-forward") ||
    IsMenuItemShowing("messagePaneContext-editAsNew"));
}

function ShowMessagePaneEditSeparator()
{
  return (IsMenuItemShowing("messagePaneContext-moveMenu") ||
    IsMenuItemShowing("messagePaneContext-copyMenu") ||
    IsMenuItemShowing("messagePaneContext-saveAs") ||
    IsMenuItemShowing("messagePaneContext-print") ||
    IsMenuItemShowing("messagePaneContext-delete"));
}

function ShowMessagePaneLinkSeparator()
{
  return (IsMenuItemShowingWithStyle("context-openlink") ||
    IsMenuItemShowingWithStyle("context-editlink"));
}

function ShowMessagePaneImageSeparator()
{
  return (IsMenuItemShowingWithStyle("context-viewimage"));
}

function ShowMessagePaneCopySeparator()
{
  return (IsMenuItemShowingWithStyle("context-copylink") ||
    IsMenuItemShowingWithStyle("context-copyimage"));
}

function IsMenuItemShowing(menuID)
{

  var item = document.getElementById(menuID);
  if(item)
  {
    return(item.getAttribute('hidden') !='true');
  }
  return false;
}

function IsMenuItemShowingWithStyle(menuID)
{
  var item = document.getElementById(menuID);
  if(item)
  {
    var style = item.getAttribute( "style" );
    return ( style.indexOf( "display:none;" ) == -1 )
  }
  return false;
}
