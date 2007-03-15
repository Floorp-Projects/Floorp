# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   timeless
#   slucy@objectivesw.co.uk
#   Håkan Waara <hwaara@chello.se>
#   Jan Varga <varga@nixcorp.com>
#   Seth Spitzer <sspitzer@netscape.com>
#   David Bienvenu <bienvenu@nventure.com>
#   Karsten Düsterloh <mnyromyr@tprac.de>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

const MSG_FLAG_READ              = 0x000001;
const MSG_FLAG_IMAP_DELETED      = 0x200000;
const MSG_FLAG_MDN_REPORT_NEEDED = 0x400000;
const MSG_FLAG_MDN_REPORT_SENT   = 0x800000;
const MDN_DISPOSE_TYPE_DISPLAYED = 0;
const ADDR_DB_LARGE_COMMIT       = 1;
const MSG_DB_LARGE_COMMIT        = 1;

const kClassicMailLayout = 0;
const kWideMailLayout = 1;
const kVerticalMailLayout = 2;

// Per message header flags to keep track of whether the user is allowing remote
// content for a particular message. 
// if you change or add more values to these constants, be sure to modify
// the corresponding definitions in nsMsgContentPolicy.cpp
const kNoRemoteContentPolicy = 0;
const kBlockRemoteContent = 1;
const kAllowRemoteContent = 2;

const kIsAPhishMessage = 0;
const kNotAPhishMessage = 1;

const kMsgNotificationPhishingBar = 1;
const kMsgNotificationJunkBar = 2;
const kMsgNotificationRemoteImages = 3;

var gMessengerBundle;
var gWindowManagerInterface;
var gPrefBranch = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService).getBranch(null);
var gPrintSettings = null;
var gWindowReuse  = 0;
var gMarkViewedMessageAsReadTimer = null; // if the user has configured the app to mark a message as read if it is viewed for more than n seconds
var gTimelineService = null;
var gTimelineEnabled = ("@mozilla.org;timeline-service;1" in Components.classes);
if (gTimelineEnabled) {
  try {
    gTimelineEnabled = gPrefBranch.getBoolPref("mailnews.timeline_is_enabled");
    if (gTimelineEnabled) {
      gTimelineService = 
        Components.classes["@mozilla.org;timeline-service;1"].getService(Components.interfaces.nsITimelineService);
    }
  }
  catch (ex)
  {
    gTimelineEnabled = false;
  }
}

// the user preference,
// if HTML is not allowed. I assume, that the user could have set this to a
// value > 1 in his prefs.js or user.js, but that the value will not
// change during runtime other than through the MsgBody*() functions below.
var gDisallow_classes_no_html = 1; 

// Disable the new account menu item if the account preference is locked.
// Two other affected areas are the account central and the account manager
// dialog.
function menu_new_init()
{
  if (!gMessengerBundle)
    gMessengerBundle = document.getElementById("bundle_messenger");

  var newAccountItem = document.getElementById('newAccountMenuItem');
  if (gPrefBranch.prefIsLocked("mail.disable_new_account_addition"))
    newAccountItem.setAttribute("disabled","true");

  // Change "New Folder..." menu according to the context
  var folderArray = GetSelectedMsgFolders();
  if (folderArray.length == 0)
    return;
  var msgFolder = folderArray[0];
  var isServer = msgFolder.isServer;
  var serverType = msgFolder.server.type;
  var canCreateNew = msgFolder.canCreateSubfolders;
  var isInbox = IsSpecialFolder(msgFolder, MSG_FOLDER_FLAG_INBOX, false);
  var isIMAPFolder = serverType == "imap";
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                         .getService(Components.interfaces.nsIIOService);
  var showNew = ((serverType != 'nntp') && canCreateNew) || isInbox;
  ShowMenuItem("menu_newFolder", showNew);
  ShowMenuItem("menu_newVirtualFolder", showNew);

  EnableMenuItem("menu_newFolder", !isIMAPFolder || MailOfflineMgr.isOnline());
  EnableMenuItem("menu_newVirtualFolder", true);
  if (showNew)
    SetMenuItemLabel("menu_newFolder", gMessengerBundle.getString((isServer || isInbox) ? "newFolderMenuItem" : "newSubfolderMenuItem"));
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
  document.commandDispatcher.updateCommands('create-menu-file');
}

function InitEditMessagesMenu()
{
  goSetMenuValue('cmd_delete', 'valueDefault');
  goSetAccessKey('cmd_delete', 'valueDefaultAccessKey');
  document.commandDispatcher.updateCommands('create-menu-edit');

  // initialize the favorite Folder checkbox in the edit menu
  var favoriteFolderMenu = document.getElementById('menu_favoriteFolder');
  if (favoriteFolderMenu && !favoriteFolderMenu.disabled)
  {
    var folderTree = GetFolderTree();
    var startIndex = {};
    var endIndex = {};
    folderTree.view.selection.getRangeAt(0, startIndex, endIndex);
    if (startIndex.value >= 0)
    {
      var numSelected = endIndex.value - startIndex.value + 1;
      var folderResource = GetFolderResource(folderTree, startIndex.value);
      var isServer = GetFolderAttribute(folderTree, folderResource, "IsServer") == 'true';
      SetupFavoritesMenuItem(folderResource, numSelected, isServer, 'menu_favoriteFolder');
    }
  }
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
          message_menuitem.setAttribute('checked', !IsMessagePaneCollapsed());
          message_menuitem.setAttribute('disabled', gAccountCentralLoaded);
      }
  }

  // Disable some menus if account manager is showing
  var sort_menuitem = document.getElementById('viewSortMenu');
  if (sort_menuitem) {
    sort_menuitem.setAttribute("disabled", gAccountCentralLoaded);
  }
  var view_menuitem = document.getElementById('viewMessageViewMenu');
  if (view_menuitem) {
    view_menuitem.setAttribute("disabled", gAccountCentralLoaded);
  }
  var threads_menuitem = document.getElementById('viewMessagesMenu');
  if (threads_menuitem) {
    threads_menuitem.setAttribute("disabled", gAccountCentralLoaded);
  }

  // hide the views menu item if the user doesn't have the views toolbar button visible
  var viewsToolbarButton = document.getElementById("mailviews-container");
  document.getElementById('viewMessageViewMenu').hidden = !viewsToolbarButton;

  // Initialize the View Attachment Inline menu
  var viewAttachmentInline = pref.getBoolPref("mail.inline_attachments");
  document.getElementById("viewAttachmentsInlineMenuitem").setAttribute("checked", viewAttachmentInline ? "true" : "false");

  document.commandDispatcher.updateCommands('create-menu-view');
}

function InitViewLayoutStyleMenu(event)
{
  var paneConfig = pref.getIntPref("mail.pane_config.dynamic");
  var layoutStyleMenuitem = event.target.childNodes[paneConfig];
  if (layoutStyleMenuitem)
    layoutStyleMenuitem.setAttribute("checked", "true"); 
}

function InitViewFolderViewsMenu(event)
{
  var layoutStyleMenuitem = event.target.childNodes[gCurrentFolderView];
  if (layoutStyleMenuitem)
    layoutStyleMenuitem.setAttribute("checked", "true"); 
}

function setSortByMenuItemCheckState(id, value)
{
  var menuitem = document.getElementById(id);
  if (menuitem) 
    menuitem.setAttribute("checked", value);
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
  setSortByMenuItemCheckState("sortByUnreadMenuitem", (sortType == nsMsgViewSortType.byUnread));
  setSortByMenuItemCheckState("sortByTagsMenuitem", (sortType == nsMsgViewSortType.byTags));
  setSortByMenuItemCheckState("sortByJunkStatusMenuitem", (sortType == nsMsgViewSortType.byJunkStatus));
  setSortByMenuItemCheckState("sortBySenderMenuitem", (sortType == nsMsgViewSortType.byAuthor));
  setSortByMenuItemCheckState("sortByRecipientMenuitem", (sortType == nsMsgViewSortType.byRecipient)); 
  setSortByMenuItemCheckState("sortByAttachmentsMenuitem", (sortType == nsMsgViewSortType.byAttachments)); 	

  var sortOrder = gDBView.sortOrder;
  var sortTypeSupportsGrouping = (sortType == nsMsgViewSortType.byAuthor 
      || sortType == nsMsgViewSortType.byDate || sortType == nsMsgViewSortType.byPriority
      || sortType == nsMsgViewSortType.bySubject || sortType == nsMsgViewSortType.byTags
      || sortType == nsMsgViewSortType.byRecipient || sortType == nsMsgViewSortType.byAccount
      || sortType == nsMsgViewSortType.byStatus || sortType == nsMsgViewSortType.byFlagged
      || sortType == nsMsgViewSortType.byAttachments);

  setSortByMenuItemCheckState("sortAscending", (sortOrder == nsMsgViewSortOrder.ascending));
  setSortByMenuItemCheckState("sortDescending", (sortOrder == nsMsgViewSortOrder.descending));

  var grouped = ((gDBView.viewFlags & nsMsgViewFlagsType.kGroupBySort) != 0);
  var threaded = ((gDBView.viewFlags & nsMsgViewFlagsType.kThreadedDisplay) != 0 && !grouped);
  var sortThreadedMenuItem = document.getElementById("sortThreaded");
  var sortUnthreadedMenuItem = document.getElementById("sortUnthreaded");

  sortThreadedMenuItem.setAttribute("checked", threaded);
  sortUnthreadedMenuItem.setAttribute("checked", !threaded && !grouped);

  sortThreadedMenuItem.setAttribute("disabled", !gDBView.supportsThreading);
  sortUnthreadedMenuItem.setAttribute("disabled", !gDBView.supportsThreading);

  var groupBySortOrderMenuItem = document.getElementById("groupBySort");

  groupBySortOrderMenuItem.setAttribute("disabled", !gDBView.supportsThreading || !sortTypeSupportsGrouping);
  groupBySortOrderMenuItem.setAttribute("checked", grouped);
}

function InitViewMessagesMenu()
{
  var viewFlags = (gDBView) ? gDBView.viewFlags : 0;
  var viewType = (gDBView) ? gDBView.viewType : 0;

  var allMenuItem = document.getElementById("viewAllMessagesMenuItem");
  if(allMenuItem)
      allMenuItem.setAttribute("checked",  (viewFlags & nsMsgViewFlagsType.kUnreadOnly) == 0 && (viewType == nsMsgViewType.eShowAllThreads));

  var unreadMenuItem = document.getElementById("viewUnreadMessagesMenuItem");
  if(unreadMenuItem)
      unreadMenuItem.setAttribute("checked", (viewFlags & nsMsgViewFlagsType.kUnreadOnly) != 0);

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
  if(aMessage)
    isNews = IsNewsMessage(aMessage);

  //We show reply to Newsgroups only for news messages.
  var replyNewsgroupMenuItem = document.getElementById("replyNewsgroupMainMenu");
  if(replyNewsgroupMenuItem)
    replyNewsgroupMenuItem.setAttribute("hidden", isNews ? "" : "true");

  //For mail messages we say reply. For news we say ReplyToSender.
  var replyMenuItem = document.getElementById("replyMainMenu");
  if(replyMenuItem)
    replyMenuItem.setAttribute("hidden", !isNews ? "" : "true");

  var replySenderMenuItem = document.getElementById("replySenderMainMenu");
  if(replySenderMenuItem)
    replySenderMenuItem.setAttribute("hidden", isNews ? "" : "true");

  // we only kill and watch threads for news
  var threadMenuSeparator = document.getElementById("threadItemsSeparator");
  if (threadMenuSeparator)
    threadMenuSeparator.setAttribute("hidden", isNews ? "" : "true");
  var killThreadMenuItem = document.getElementById("killThread");
  if (killThreadMenuItem)
    killThreadMenuItem.setAttribute("hidden", isNews ? "" : "true");
  var watchThreadMenuItem = document.getElementById("watchThread");
  if (watchThreadMenuItem)
    watchThreadMenuItem.setAttribute("hidden", isNews ? "" : "true");

  // disable the move and copy menus if there are no messages selected.
  // disable the move menu if we can't delete msgs from the folder
  var moveMenu = document.getElementById("moveMenu");
  var msgFolder = GetLoadedMsgFolder();
  if(moveMenu)
  {
    var enableMenuItem = aMessage && msgFolder && msgFolder.canDeleteMessages;
    moveMenu.setAttribute("disabled", !enableMenuItem);
  }

  var copyMenu = document.getElementById("copyMenu");
  if(copyMenu)
    copyMenu.setAttribute("disabled", !aMessage);
  
  initMoveToFolderAgainMenu(document.getElementById("moveToFolderAgain"));

  // Disable Forward as/Label menu items if no message is selected
  var forwardAsMenu = document.getElementById("forwardAsMenu");
  if(forwardAsMenu)
    forwardAsMenu.setAttribute("disabled", !aMessage);

  var labelMenu = document.getElementById("labelMenu");
  if(labelMenu)
    labelMenu.setAttribute("disabled", !aMessage);

  // Disable mark menu when we're not in a folder
  var markMenu = document.getElementById("markMenu");
  if(markMenu)
    markMenu.setAttribute("disabled", !msgFolder);

  document.commandDispatcher.updateCommands('create-menu-message');
}

// initMoveToFolderAgainMenu
// enables / disables aMenuItem based on the value of mail.last_msg_movecopy_target_uri
// Adjusts the label and accesskey for aMenuItem to include the folder name
function initMoveToFolderAgainMenu(aMenuItem)
{
  var lastFolderURI = pref.getCharPref("mail.last_msg_movecopy_target_uri");
  var isMove = pref.getBoolPref("mail.last_msg_movecopy_was_move");  
  if (lastFolderURI)
  {
    var destResource = RDF.GetResource(lastFolderURI);
    var destMsgFolder = destResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    aMenuItem.label = gMessengerBundle.getFormattedString(isMove ? 
                                                          "moveToFolderAgain" : "copyToFolderAgain",
                                                          [destMsgFolder.prettyName], 1);
    aMenuItem.setAttribute('accesskey', 
                           gMessengerBundle.getString(isMove ? 
                                                      "moveToFolderAgainAccessKey" : "copyToFolderAgainAccessKey"));
  }
}

function InitViewHeadersMenu()
{
  var id = null;
  var headerchoice = 1;
  try 
  {
    headerchoice = pref.getIntPref("mail.show_headers");
  }
  catch (ex) 
  {
    dump("failed to get the header pref\n");
  }

  switch (headerchoice) 
  {
	case 2:	
		id = "viewallheaders";
		break;
	case 1:	
	default:
		id = "viewnormalheaders";
		break;
  }

  var menuitem = document.getElementById(id);
  if (menuitem)
    menuitem.setAttribute("checked", "true"); 
}

function InitViewBodyMenu()
{
  var html_as = 0;
  var prefer_plaintext = false;
  var disallow_classes = 0;
  try
  {
    prefer_plaintext = pref.getBoolPref("mailnews.display.prefer_plaintext");
    html_as = pref.getIntPref("mailnews.display.html_as");
    disallow_classes = pref.getIntPref("mailnews.display.disallow_mime_handlers");
    if (disallow_classes > 0)
      gDisallow_classes_no_html = disallow_classes;
    // else gDisallow_classes_no_html keeps its inital value (see top)
  }
  catch (ex)
  {
    dump("failed to get the body plaintext vs. HTML prefs\n");
  }

  var AllowHTML_checked = false;
  var Sanitized_checked = false;
  var AsPlaintext_checked = false;
  if (!prefer_plaintext && !html_as && !disallow_classes)
    AllowHTML_checked = true;
  else if (!prefer_plaintext && html_as == 3 && disallow_classes > 0)
    Sanitized_checked = true;
  else if (prefer_plaintext && html_as == 1 && disallow_classes > 0)
    AsPlaintext_checked = true;
  // else (the user edited prefs/user.js) check none of the radio menu items

  var AllowHTML_menuitem = document.getElementById("bodyAllowHTML");
  var Sanitized_menuitem = document.getElementById("bodySanitized");
  var AsPlaintext_menuitem = document.getElementById("bodyAsPlaintext");
  if (AllowHTML_menuitem && Sanitized_menuitem && AsPlaintext_menuitem)
  {
    AllowHTML_menuitem.setAttribute("checked", AllowHTML_checked ? "true" : "false");
    Sanitized_menuitem.setAttribute("checked", Sanitized_checked ? "true" : "false");
    AsPlaintext_menuitem.setAttribute("checked", AsPlaintext_checked ? "true" : "false");
  }
  else
    dump("Where is my View|Body menu?\n");
}

function IsNewsMessage(messageUri)
{
  return (/^news-message:/.test(messageUri));
}

function IsImapMessage(messageUri)
{
  return (/^imap-message:/.test(messageUri));
}

function SetMenuItemLabel(menuItemId, customLabel)
{
  var menuItem = document.getElementById(menuItemId);
  if (menuItem)
    menuItem.setAttribute('label', customLabel);
}

function RemoveAllMessageTags()
{
  var selectedMsgUris = GetSelectedMessages();
  if (!selectedMsgUris.length)
    return;

  var messages = Components.classes["@mozilla.org/supports-array;1"]
                           .createInstance(Components.interfaces.nsISupportsArray);
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                             .getService(Components.interfaces.nsIMsgTagService);
  var tagArray = tagService.getAllTags({});

  var allKeys = "";
  for (j = 0; j < tagArray.length; ++j)
  {
    if (j)
      allKeys += " ";
    allKeys += tagArray[j].key;
  }

  var prevHdrFolder = null;
  // this crudely handles cross-folder virtual folders with selected messages
  // that spans folders, by coalescing consecutive messages in the selection
  // that happen to be in the same folder. nsMsgSearchDBView does this better,
  // but nsIMsgDBView doesn't handle commands with arguments, and untag takes a
  // key argument. Furthermore, we only delete legacy labels and known tags,
  // keeping other keywords like (non)junk intact.
  var j;
  for (var i = 0; i < selectedMsgUris.length; ++i)
  {
    var msgHdr = messenger.msgHdrFromURI(selectedMsgUris[i]);
    msgHdr.label = 0; // remove legacy label
    if (prevHdrFolder != msgHdr.folder)
    {
      if (prevHdrFolder)
        prevHdrFolder.removeKeywordsFromMessages(messages, allKeys);
      messages.Clear();
      prevHdrFolder = msgHdr.folder;
    }
    messages.AppendElement(msgHdr);
  }
  if (prevHdrFolder)
    prevHdrFolder.removeKeywordsFromMessages(messages, allKeys);
  OnTagsChange();
}

function ToggleMessageTagKey(index)
{
  if (GetNumSelectedMessages() < 1)
    return;
  // set the tag state based upon that of the first selected message,
  // just like we do for markAsRead etc.
  var msgHdr = gDBView.hdrForFirstSelectedMessage;
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                             .getService(Components.interfaces.nsIMsgTagService);
  var tagArray = tagService.getAllTags({});
  for (var i = 0; i < tagArray.length; ++i)
  {
    var key = tagArray[i].key;
    if (!--index) 
    {
      // found the key, now toggle its state
      var curKeys = msgHdr.getStringProperty("keywords");
      if (msgHdr.label)
        curKeys += " $label" + msgHdr.label;
      var addKey  = (" " + curKeys + " ").indexOf(" " + key + " ") < 0;
      ToggleMessageTag(key, addKey);
      return;
    }
  }
}

function ToggleMessageTagMenu(target)
{
  var key    = target.getAttribute("value");
  var addKey = target.getAttribute("checked") == "true";
  ToggleMessageTag(key, addKey);
}

function ToggleMessageTag(key, addKey)
{
  var messages = Components.classes["@mozilla.org/supports-array;1"]
                           .createInstance(Components.interfaces.nsISupportsArray);
  var msg = Components.classes["@mozilla.org/supports-array;1"]
                          .createInstance(Components.interfaces.nsISupportsArray);
  var selectedMsgUris = GetSelectedMessages();
  var toggler = addKey ? "addKeywordsToMessages" : "removeKeywordsFromMessages";
  var prevHdrFolder = null;
  // this crudely handles cross-folder virtual folders with selected messages
  // that spans folders, by coalescing consecutive msgs in the selection
  // that happen to be in the same folder. nsMsgSearchDBView does this
  // better, but nsIMsgDBView doesn't handle commands with arguments,
  // and (un)tag takes a key argument.
  for (var i = 0; i < selectedMsgUris.length; ++i)
  {
    var msgHdr = messenger.msgHdrFromURI(selectedMsgUris[i]);
    if (msgHdr.label)
    {
      // Since we touch all these messages anyway, migrate the label now.
      // If we don't, the thread tree won't always show the correct tag state,
      // because resetting a label doesn't update the tree anymore...
      msg.Clear();
      msg.AppendElement(msgHdr);
      msgHdr.folder.addKeywordsToMessages(msg, "$label" + msgHdr.label);
      msgHdr.label = 0; // remove legacy label
    }
    if (prevHdrFolder != msgHdr.folder)
    {
      if (prevHdrFolder)
        prevHdrFolder[toggler](messages, key);
      messages.Clear();
      prevHdrFolder = msgHdr.folder;
    }
    messages.AppendElement(msgHdr);
  }
  if (prevHdrFolder)
    prevHdrFolder[toggler](messages, key);
  OnTagsChange();
}

function AddTag()
{
  var args = {result: "", okCallback: AddTagCallback};
  var dialog = window.openDialog("chrome://messenger/content/newTagDialog.xul",
                                 "",
                                 "chrome,titlebar,modal",
                                 args);
}

function AddTagCallback(name, color)
{
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                             .getService(Components.interfaces.nsIMsgTagService);
  tagService.addTag(name, color, '');
  try
  {
    ToggleMessageTag(tagService.getKeyForTag(name), true);
  }
  catch(ex)
  {
    return false;
  }
  return true;
}

function SetMessageTagLabel(menuitem, index, name)
{
  // if a <key> is defined for this tag, use its key as the accesskey
  // (the key for the tag at index n needs to have the id key_tag<n>)
  var shortcutkey = document.getElementById("key_tag" + index);
  var accesskey = shortcutkey ? shortcutkey.getAttribute("key") : "";
  if (accesskey)
    menuitem.setAttribute("accesskey", accesskey);
  var label = gMessengerBundle.getFormattedString("mailnews.tags.format", 
                                                  [accesskey, name]);
  menuitem.setAttribute("label", label);
}

function InitMessageTags(menuPopup)
{
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                             .getService(Components.interfaces.nsIMsgTagService);
  var tagArray = tagService.getAllTags({});
  var tagCount = tagArray.length;

  // remove any existing non-static entries...
  var menuseparator = menuPopup.lastChild.previousSibling;
  for (var i = menuPopup.childNodes.length; i > 4; --i)
    menuPopup.removeChild(menuseparator.previousSibling);

  // hide double menuseparator
  menuseparator.previousSibling.hidden = !tagCount;

  // create label and accesskey for the static remove item
  var tagRemoveLabel = gMessengerBundle.getString("mailnews.tags.remove");
  SetMessageTagLabel(menuPopup.firstChild, 0, tagRemoveLabel);

  // now rebuild the list
  var msgHdr = gDBView.hdrForFirstSelectedMessage;
  var curKeys = msgHdr.getStringProperty("keywords");
  if (msgHdr.label)
    curKeys += " $label" + msgHdr.label;

  for (var i = 0; i < tagCount; ++i)
  {
    var taginfo = tagArray[i];
    // TODO we want to either remove or "check" the tags that already exist
    var newMenuItem = document.createElement("menuitem");
    SetMessageTagLabel(newMenuItem, i + 1, taginfo.tag);
    newMenuItem.setAttribute("value", taginfo.key);
    newMenuItem.setAttribute("type", "checkbox");
    var removeKey = (" " + curKeys + " ").indexOf(" " + taginfo.key + " ") > -1;
    newMenuItem.setAttribute('checked', removeKey);
    newMenuItem.setAttribute('oncommand', 'ToggleMessageTagMenu(event.target);');
    var color = taginfo.color;
    if (color)
      newMenuItem.setAttribute("class", "lc-" + color.substr(1));    
    menuPopup.insertBefore(newMenuItem, menuseparator);
  }
}

function backToolbarMenu_init(menuPopup)
{
  populateHistoryMenu(menuPopup, true);
}

var gNavDebug = false;
function navDebug(str)
{
  if (gNavDebug)
    dump(str);
}

function populateHistoryMenu(menuPopup, isBackMenu)
{
  
  // remove existing entries
  while (menuPopup.firstChild)
    menuPopup.removeChild(menuPopup.firstChild);
  var curPos = new Object;
  var numEntries = new Object;
  var historyEntries = new Object;
  messenger.getNavigateHistory(curPos, numEntries, historyEntries);
  curPos.value = curPos.value * 2;
  navDebug("curPos = " + curPos.value + " numEntries = " + numEntries.value + "\n");
  var historyArray = historyEntries.value;
  var folder;
  var newMenuItem;
  if (GetLoadedMessage())
  {
    if (!isBackMenu)
      curPos.value += 2;
    else
      curPos.value -= 2;
  }
  // for populating the back menu, we want the most recently visited
  // messages first in the menu. So we go backward from curPos to 0.
  // For the forward menu, we want to go forward from curPos to the end.
  
  var relPos = 0;
  for (var i = curPos.value; (isBackMenu) ? i >= 0 : i < historyArray.length; i += ((isBackMenu) ? -2 : 2))
  {
    navDebug("history[" + i + "] = " + historyArray[i] + "\n");
    navDebug("history[" + i + "] = " + historyArray[i + 1] + "\n");
    folder = RDF.GetResource(historyArray[i + 1]).QueryInterface(Components.interfaces.nsIMsgFolder);
    navDebug("folder URI = " + folder.URI + "pretty name " + folder.prettyName + "\n");
    var menuText = "";
    
    var msgHdr = messenger.msgHdrFromURI(historyArray[i]);
    if (!IsCurrentLoadedFolder(folder))
      menuText = folder.prettyName + " - ";
    menuText += msgHdr.mime2DecodedSubject;
    menuText += ":";
    menuText += msgHdr.mime2DecodedAuthor;
    newMenuItem = document.createElement('menuitem');
    newMenuItem.setAttribute('label', menuText);
    relPos += isBackMenu ? -1 : 1;
    newMenuItem.setAttribute('value',  relPos);
    newMenuItem.setAttribute('oncommand', 'NavigateToUri(event.target); event.stopPropagation();');
    menuPopup.appendChild(newMenuItem);
    if (! (relPos % 20))
      break;
  }
  
}

function NavigateToUri(target)
{
  var historyIndex = target.getAttribute('value');
  var folderUri = messenger.getFolderUriAtNavigatePos(historyIndex);
  var msgUri = messenger.getMsgUriAtNavigatePos(historyIndex);
  var folder = RDF.GetResource(folderUri).QueryInterface(Components.interfaces.nsIMsgFolder);
  var msgHdr = messenger.msgHdrFromURI(msgUri);
  navDebug("navigating from " + messenger.navigatePos + " by " + historyIndex + " to " + msgUri + "\n");
  navDebug("folderUri = " + folderUri + "\n");
  // this "- 0" seems to ensure that historyIndex is treated as an int, not a string.
  messenger.navigatePos += (historyIndex - 0);
  LoadNavigatedToMessage(msgHdr, folder, folderUri);
}

function forwardToolbarMenu_init(menuPopup)
{
  populateHistoryMenu(menuPopup, false);
}

function InitMessageMark()
{
  var areMessagesRead = SelectedMessagesAreRead();
  var readItem = document.getElementById("cmd_markAsRead");
  if(readItem)
     readItem.setAttribute("checked", areMessagesRead);

  var areMessagesFlagged = SelectedMessagesAreFlagged();
  var flaggedItem = document.getElementById("cmd_markAsFlagged");
  if(flaggedItem)
     flaggedItem.setAttribute("checked", areMessagesFlagged);

  document.commandDispatcher.updateCommands('create-menu-mark');
}

function UpdateJunkToolbarButton()
{
  var junkButtonDeck = document.getElementById("junk-deck");
  if (junkButtonDeck)
    junkButtonDeck.selectedIndex = SelectedMessagesAreJunk() ? 1 : 0;
}

function UpdateDeleteCommand()
{
  var value = "value";
  var uri = GetFirstSelectedMessage();
  if (IsNewsMessage(uri))
    value += "News";
  else if (SelectedMessagesAreDeleted())
    value += "IMAPDeleted";
  if (GetNumSelectedMessages() < 2)
    value += "Message";
  else
    value += "Messages";
  goSetMenuValue("cmd_delete", value);
  goSetAccessKey("cmd_delete", value + "AccessKey");
}

function SelectedMessagesAreDeleted()
{
    try {
        return gDBView.hdrForFirstSelectedMessage.flags & MSG_FLAG_IMAP_DELETED;
    }
    catch (ex) {
        return 0;
    }
}

function SelectedMessagesAreJunk()
{
    var isJunk;
    try {
        var junkScore = gDBView.hdrForFirstSelectedMessage.getStringProperty("junkscore");
        isJunk =  ((junkScore != "") && (junkScore != "0"));
    }
    catch (ex) {
        isJunk = false;
    }
    return isJunk;
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

    var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
    return (gWindowManagerInterface = windowManager.QueryInterface(Components.interfaces.nsIWindowMediator));
}

function GetInboxFolder(server)
{
    try {
        var rootMsgFolder = server.rootMsgFolder;

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

  // if the server doesn't support an inbox it could be an RSS server or some other server type..
  // just use the root folder and the server implementation can figure out what to do.
  if (!inboxFolder)
    inboxFolder = server.rootFolder;    

  var folders = new Array(1);
  folders[0] = inboxFolder;

  var compositeDataSource = GetCompositeDataSource("GetNewMessages");
  GetNewMessages(folders, server, compositeDataSource);
}

function MsgGetMessage()
{
  gNewAccountToLoad = null;
  // if offline, prompt for getting messages
  if (MailOfflineMgr.isOnline() || MailOfflineMgr.getNewMail())
    GetFolderMessages();
}

function MsgGetMessagesForAllServers(defaultServer)
{
    // now log into any server
    try
    {
        var allServers = accountManager.allServers;
        var i;
        // array of isupportsarrays of servers for a particular folder
        var pop3DownloadServersArray = new Array();
        // parallel isupports array of folders to download to...
        var localFoldersToDownloadTo = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
        var pop3Server;

        for (i = 0; i < allServers.Count(); ++i)
        {
            var currentServer = allServers.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgIncomingServer);
            var protocolinfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + currentServer.type].getService(Components.interfaces.nsIMsgProtocolInfo);
            if (protocolinfo.canLoginAtStartUp && currentServer.loginAtStartUp)
            {
                if (defaultServer && defaultServer.equals(currentServer) && 
                  !defaultServer.isDeferredTo &&
                  defaultServer.rootFolder == defaultServer.rootMsgFolder)
                {
                    dump(currentServer.serverURI + "...skipping, already opened\n");
                }
                else if (currentServer.type == "pop3" && currentServer.downloadOnBiff)
                {
                    CoalesceGetMsgsForPop3ServersByDestFolder(currentServer, pop3DownloadServersArray, localFoldersToDownloadTo);
                    pop3Server = currentServer.QueryInterface(Components.interfaces.nsIPop3IncomingServer);
                }
                else
                {		 
                    // Check to see if there are new messages on the server
                    currentServer.PerformBiff(msgWindow);
                }
            }
        }
        for (i = 0; i < pop3DownloadServersArray.length; ++i)
        {
          // any ol' pop3Server will do - the serversArray specifies which servers to download from
          pop3Server.downloadMailFromServers(pop3DownloadServersArray[i], msgWindow, localFoldersToDownloadTo.GetElementAt(i), null);
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
  if (MailOfflineMgr.isOnline() || MailOfflineMgr.getNewMail())
    GetMessagesForAllAuthenticatedAccounts();
}

/**
  * Get messages for the account selected from Menu dropdowns.
  * if offline, prompt for getting messages.
  */
function MsgGetMessagesForAccount(aEvent)
{
  if (!aEvent)
    return;

  if (MailOfflineMgr.isOnline() || MailOfflineMgr.getNewMail())
    GetMessagesForAccount(aEvent);
}

// if offline, prompt for getNextNMessages
function MsgGetNextNMessages()
{
  var folder;
  if (MailOfflineMgr.isOnline() || MailOfflineMgr.getNewMail()) 
  {
    folder = GetFirstSelectedMsgFolder();
    if (folder) 
      GetNextNMessages(folder);
  }
}   

function MsgDeleteMessage(reallyDelete, fromToolbar)
{
    // if from the toolbar, return right away if this is a news message
    // only allow cancel from the menu:  "Edit | Cancel / Delete Message"
    if (fromToolbar)
    {
        var srcFolder = GetLoadedMsgFolder();
        var folderResource = srcFolder.QueryInterface(Components.interfaces.nsIRDFResource);
        var uri = folderResource.Value;
        if (isNewsURI(uri)) {
            // if news, don't delete
            return;
        }
    }

    SetNextMessageAfterDelete();
    if (reallyDelete) {
        gDBView.doCommand(nsMsgViewCommandType.deleteNoTrash);
    }
    else {
        gDBView.doCommand(nsMsgViewCommandType.deleteMsg);
    }
}

// MsgCopyMessage
//   Copies the selected messages to the destination folder
//   aDestFolderURI -- the URI of the destination folder
function MsgCopyMessage(aDestFolderURI)
{
  try {
    // get the msg folder we're copying messages into
    var destResource = RDF.GetResource(aDestFolderURI);
    var destMsgFolder = destResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    gDBView.doCommandWithFolder(nsMsgViewCommandType.copyMessages, destMsgFolder);
    pref.setCharPref("mail.last_msg_movecopy_target_uri", aDestFolderURI);
    pref.setBoolPref("mail.last_msg_movecopy_was_move", false);  
  }
  catch (ex) {
    dump("MsgCopyMessage failed: " + ex + "\n");
  }
}

// MsgMoveMessage
//   Moves the selected messages to the destination folder
//   aDestFolderURI -- the URI of the destination folder
function MsgMoveMessage(aDestFolderURI)
{
  try {
    // get the msg folder we're moving messages into
    // var destUri = destFolder.getAttribute('id');
    var destResource = RDF.GetResource(aDestFolderURI);
    var destMsgFolder = destResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    // we don't move news messages, we copy them
    if (isNewsURI(gDBView.msgFolder.URI))
      gDBView.doCommandWithFolder(nsMsgViewCommandType.copyMessages, destMsgFolder);
    else 
    {
      SetNextMessageAfterDelete();
      gDBView.doCommandWithFolder(nsMsgViewCommandType.moveMessages, destMsgFolder);
    }    
    pref.setCharPref("mail.last_msg_movecopy_target_uri", aDestFolderURI);
    pref.setBoolPref("mail.last_msg_movecopy_was_move", true);  
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
  if (loadedFolder)
  {
    var server = loadedFolder.server;

    if(server && server.type == "nntp")
    {
      MsgReplyGroup(event);
      return;
    }
  }
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

  ComposeMessage(msgComposeType.ReplyAll, 
    (event && event.shiftKey) ? msgComposeFormat.OppositeOfDefault : msgComposeFormat.Default,
    loadedFolder, messageArray);
}

function MsgForwardMessage(event)
{
  var forwardType = 0;
  try {
    forwardType = gPrefBranch.getIntPref("mail.forward_message_mode");
  }
  catch (ex) {
    dump("failed to retrieve pref mail.forward_message_mode");
  }

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
  // retrieve Sender direct from selected message's headers
  var msgHdr = gDBView.hdrForFirstSelectedMessage;
  var headerParser = Components.classes["@mozilla.org/messenger/headerparser;1"].getService(Components.interfaces.nsIMsgHeaderParser);
  var emailAddress = headerParser.extractHeaderAddressMailboxes(null, msgHdr.author);
  if (emailAddress)
    top.MsgFilters(emailAddress, null);
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
        destinationFolder = server.rootMsgFolder;

        var verifyCreateSubfolders = null;
        if (destinationFolder)
            verifyCreateSubfolders = destinationFolder.canCreateSubfolders;

        // in case the server cannot have subfolders,
        // get default account and set its incoming server as parent folder
        if (!verifyCreateSubfolders)
        {
            try {
                var defaultFolder = GetDefaultAccountRootFolder();
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

function ToggleFavoriteFolderFlag()
{
  var folder = GetFirstSelectedMsgFolder();
  folder.toggleFlag(MSG_FOLDER_FLAG_FAVORITE);
}

function MsgSaveAsFile()
{
  if (GetNumSelectedMessages() == 1)
    SaveAsFile(GetFirstSelectedMessage());
}

function MsgSaveAsTemplate()
{
  var folder = GetLoadedMsgFolder();
  if (GetNumSelectedMessages() == 1)
    SaveAsTemplate(GetFirstSelectedMessage(), folder);
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
    // use GetSelectedFolderURI() to find out which message to open instead of
    // GetLoadedMsgFolder().QueryIntervace(Components.interfaces.nsIRDFResource).value.
    // This is required because on a right-click, the currentIndex value will be
    // different from the actual row that is highlighted.  GetSelectedFolderURI()
    // will return the message that is highlighted.
    uriToOpen = GetSelectedFolderURI();

  if (uriToOpen)
    window.openDialog("chrome://messenger/content/", "_blank", "chrome,all,dialog=no", uriToOpen, keyToSelect);
}

// passing in the view, so this will work for search and the thread pane
function MsgOpenSelectedMessages()
{
  var dbView = GetDBView();

  var indices = GetSelectedIndices(dbView);
  var numMessages = indices.length;

  gWindowReuse = gPrefBranch.getBoolPref("mailnews.reuse_message_window");
  // This is a radio type button pref, currently with only 2 buttons.
  // We need to keep the pref type as 'bool' for backwards compatibility
  // with 4.x migrated prefs.  For future radio button(s), please use another
  // pref (either 'bool' or 'int' type) to describe it.
  //
  // gWindowReuse values: false, true
  //    false: open new standalone message window for each message
  //    true : reuse existing standalone message window for each message
  if (gWindowReuse && numMessages == 1 && MsgOpenSelectedMessageInExistingWindow())
    return;
    
  var openWindowWarning = gPrefBranch.getIntPref("mailnews.open_window_warning");
  if ((openWindowWarning > 1) && (numMessages >= openWindowWarning)) {
    if (!gMessengerBundle)
        gMessengerBundle = document.getElementById("bundle_messenger");
    var title = gMessengerBundle.getString("openWindowWarningTitle");
    var text = gMessengerBundle.getFormattedString("openWindowWarningText", [numMessages]);
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                          .getService(Components.interfaces.nsIPromptService);    
    if (!promptService.confirm(window, title, text))
      return;
  }

  for (var i = 0; i < numMessages; i++) {
    MsgOpenNewWindowForMessage(dbView.getURIForViewIndex(indices[i]), dbView.getFolderForViewIndex(indices[i]).URI);
  }
}

function MsgOpenSelectedMessageInExistingWindow()
{
    var windowID = GetWindowByWindowType("mail:messageWindow");
    if (!windowID)
      return false;

    try {
        var messageURI = gDBView.URIForFirstSelectedMessage;
        var msgHdr = gDBView.hdrForFirstSelectedMessage;

        // Reset the window's message uri and folder uri vars, and
        // update the command handlers to what's going to be used.
        // This has to be done before the call to CreateView().
        windowID.gCurrentMessageUri = messageURI;
        windowID.gCurrentFolderUri = msgHdr.folder.URI;
        windowID.UpdateMailToolbar('MsgOpenExistingWindowForMessage');

        // even if the folder uri's match, we can't use the existing view
        // (msgHdr.folder.URI == windowID.gCurrentFolderUri)
        // the reason is quick search and mail views.
        // see bug #187673
        //
        // for the sake of simplicity,
        // let's always call CreateView(gDBView)
        // which will clone gDBView
        windowID.CreateView(gDBView);
        windowID.LoadMessageByMsgKey(msgHdr.messageKey);

        // bring existing window to front
        windowID.focus();
        return true;
    }
    catch (ex) {
        dump("reusing existing standalone message window failed: " + ex + "\n");
    }
    return false;
}

const nsIFilePicker = Components.interfaces.nsIFilePicker;

function MsgOpenFromFile()
{
   var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);

   var strBundleService = Components.classes["@mozilla.org/intl/stringbundle;1"].getService();
   strBundleService = strBundleService.QueryInterface(Components.interfaces.nsIStringBundleService);
   var extbundle = strBundleService.createBundle("chrome://messenger/locale/messenger.properties");
   var filterLabel = extbundle.GetStringFromName("EMLFiles");
   var windowTitle = extbundle.GetStringFromName("OpenEMLFiles");

   fp.init(window, windowTitle, nsIFilePicker.modeOpen);
   fp.appendFilter(filterLabel, "*.eml");

   // Default or last filter is "All Files"
   fp.appendFilters(nsIFilePicker.filterAll);

  try {
     var ret = fp.show();
     if (ret == nsIFilePicker.returnCancel)
       return;
   }
   catch (ex) {
     dump("filePicker.chooseInputFile threw an exception\n");
     return;
   }

   var uri = fp.fileURL;
   uri.query = "type=application/x-message-display";

  window.openDialog( "chrome://messenger/content/messageWindow.xul", "_blank", "all,chrome,dialog=no,status,toolbar", uri, null, null );
}

function MsgOpenNewWindowForMessage(messageUri, folderUri)
{
    if (!messageUri)
        // use GetFirstSelectedMessage() to find out which message to open
        // instead of gDBView.getURIForViewIndex(currentIndex).  This is
        // required because on a right-click, the currentIndex value will be
        // different from the actual row that is highlighted.
        // GetFirstSelectedMessage() will return the message that is
        // highlighted.
        messageUri = GetFirstSelectedMessage();

    if (!folderUri)
        // use GetSelectedFolderURI() to find out which message to open
        // instead of gDBView.getURIForViewIndex(currentIndex).  This is
        // required because on a right-click, the currentIndex value will be
        // different from the actual row that is highlighted.
        // GetSelectedFolderURI() will return the message that is
        // highlighted.
        folderUri = GetSelectedFolderURI();

    // be sure to pass in the current view....
    if (messageUri && folderUri) {
        window.openDialog( "chrome://messenger/content/messageWindow.xul", "_blank", "all,chrome,dialog=no,status,toolbar", messageUri, folderUri, gDBView );
    }
}

function CloseMailWindow()
{
    //dump("\nClose from XUL\nDo something...\n");
    window.close();
}

function MsgJunk()
{
  MsgJunkMailInfo(true);
  JunkSelectedMessages(!SelectedMessagesAreJunk());
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

function MsgMarkReadByDate()
{
  window.openDialog( "chrome://messenger/content/markByDate.xul","",
                     "chrome,modal,titlebar,centerscreen",
                     GetLoadedMsgFolder() );
}

function MsgMarkAllRead()
{
    var compositeDataSource = GetCompositeDataSource("MarkAllMessagesRead");
    var folder = GetMsgFolderFromUri(GetSelectedFolderURI(), true);

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
  ClearPendingReadTimer();
  gDBView.doCommand(nsMsgViewCommandType.markThreadRead);
}

function MsgViewPageSource()
{
    var messages = GetSelectedMessages();
    ViewPageSource(messages);
}

function MsgFind()
{
  document.getElementById("FindToolbar").onFindCommand();
}

function MsgFindAgain(reverse)
{
  document.getElementById("FindToolbar").onFindAgainCommand(reverse);
}

function MsgFilters(emailAddress, folder)
{
    if (!folder)
    {
      // try to determine the folder from the selected message. 
      if (gDBView)
      {
        try
        {
          var msgHdr = gDBView.hdrForFirstSelectedMessage;
          var accountKey = msgHdr.accountKey;
          if (accountKey.length > 0)
          {
            var account = accountManager.getAccount(accountKey);
            if (account)
            {
              server = account.incomingServer;
              if (server)
                folder = server.rootFolder;
            }
          }
        }
        catch (ex) {}
      }
      if (!folder)
      {
        folder = GetFirstSelectedMsgFolder();
        // if this is the local folders account, check if the default account
        // defers to it; if so, we'll use the default account so the simple case
        // of one pop3 account with the global inbox creates filters for the right server.
        if (folder && folder.server.type == "none" && folder.server.isDeferredTo)
        {
          var defaultServer = accountManager.defaultAccount.incomingServer;
          if (defaultServer.rootMsgFolder == folder.server.rootFolder)
            folder = defaultServer.rootFolder;
        }
      }

    }
    var args;
    if (emailAddress)
    {
      /* we have to do prefill filter so we are going to 
         launch the filterEditor dialog
         and prefill that with the emailAddress */
         
      var curFilterList = folder.getFilterList(msgWindow);
      args = {filterList: curFilterList};
      args.filterName = emailAddress;
      window.openDialog("chrome://messenger/content/FilterEditor.xul", "", 
                        "chrome, modal, resizable,centerscreen,dialog=yes", args);

      /* if the user hits ok in the filterEditor dialog we set 
         args.refresh=true there
         we check this here in args to show filterList dialog */
      if ("refresh" in args && args.refresh)
      {
         args = { refresh: true, folder: folder };
         MsgFilterList(args);
      }
    }
    else  // just launch filterList dialog
    {
      args = { refresh: false, folder: folder };
      MsgFilterList(args);
    }
}

function MsgApplyFilters()
{
  var filterService = Components.classes["@mozilla.org/messenger/services/filters;1"].getService(Components.interfaces.nsIMsgFilterService);

  var preselectedFolder = GetFirstSelectedMsgFolder();
  var selectedFolders = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
  selectedFolders.AppendElement(preselectedFolder);
         
  var curFilterList = preselectedFolder.getFilterList(msgWindow);
  // create a new filter list and copy over the enabled filters to it.
  // We do this instead of having the filter after the fact code ignore
  // disabled filters because the Filter Dialog filter after the fact
  // code would have to clone filters to allow disabled filters to run,
  // and we don't support cloning filters currently.
  var tempFilterList = filterService.getTempFilterList(preselectedFolder);
  var numFilters = curFilterList.filterCount;
  // make sure the temp filter list uses the same log stream
  tempFilterList.logStream = curFilterList.logStream;
  tempFilterList.loggingEnabled = curFilterList.loggingEnabled;
  var newFilterIndex = 0;
  for (var i = 0; i < numFilters; i++)
  {
    var curFilter = curFilterList.getFilterAt(i);
    if (curFilter.enabled && !curFilter.temporary) // only add enabled, UI visibile filters
    {
      tempFilterList.insertFilterAt(newFilterIndex, curFilter);
      newFilterIndex++;
    }
  }
  filterService.applyFiltersToFolders(tempFilterList, selectedFolders, msgWindow);
}

function ChangeMailLayout(newLayout)
{
  gPrefBranch.setIntPref("mail.pane_config.dynamic", newLayout);
  return true;
}

function MsgViewAllHeaders()
{
    gPrefBranch.setIntPref("mail.show_headers",2);
    MsgReload();
    return true;
}

function MsgViewNormalHeaders()
{
    gPrefBranch.setIntPref("mail.show_headers",1);
    MsgReload();
    return true;
}

function MsgViewBriefHeaders()
{
    gPrefBranch.setIntPref("mail.show_headers",0);
    MsgReload();
    return true;
}

function MsgBodyAllowHTML()
{
    gPrefBranch.setBoolPref("mailnews.display.prefer_plaintext", false);
    gPrefBranch.setIntPref("mailnews.display.html_as", 0);
    gPrefBranch.setIntPref("mailnews.display.disallow_mime_handlers", 0);
    MsgReload();
    return true;
}

function MsgBodySanitized()
{
    gPrefBranch.setBoolPref("mailnews.display.prefer_plaintext", false);
    gPrefBranch.setIntPref("mailnews.display.html_as", 3);
    gPrefBranch.setIntPref("mailnews.display.disallow_mime_handlers",
                      gDisallow_classes_no_html);
    MsgReload();
    return true;
}

function MsgBodyAsPlaintext()
{
    gPrefBranch.setBoolPref("mailnews.display.prefer_plaintext", true);
    gPrefBranch.setIntPref("mailnews.display.html_as", 1);
    gPrefBranch.setIntPref("mailnews.display.disallow_mime_handlers", gDisallow_classes_no_html);
    MsgReload();
    return true;
}

function ToggleInlineAttachment(target)
{
    var viewAttachmentInline = !pref.getBoolPref("mail.inline_attachments");
    pref.setBoolPref("mail.inline_attachments", viewAttachmentInline)
    target.setAttribute("checked", viewAttachmentInline ? "true" : "false");
    MsgReload();
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
  if (MailOfflineMgr.isOnline())
    SendUnsentMessages();
  else
    MailOfflineMgr.goOnlineToSendMessages(msgWindow);
}

function PrintEnginePrintInternal(messageList, numMessages, doPrintPreview, msgType)
{
    if (numMessages == 0) {
        dump("PrintEnginePrint(): No messages selected.\n");
        return false;
    }

    if (gPrintSettings == null) {
      gPrintSettings = PrintUtils.getPrintSettings();
    }
    printEngineWindow = window.openDialog("chrome://messenger/content/msgPrintEngine.xul",
                                          "",
                                          "chrome,dialog=no,all,centerscreen",
                                          numMessages, messageList, statusFeedback, gPrintSettings, doPrintPreview, msgType, window);
    return true;

}

function PrintEnginePrint()
{
    var messageList = GetSelectedMessages();
    return PrintEnginePrintInternal(messageList, messageList.length, false, Components.interfaces.nsIMsgPrintEngine.MNAB_PRINT_MSG);
}

function PrintEnginePrintPreview()
{
    var messageList = GetSelectedMessages();
    return PrintEnginePrintInternal(messageList, 1, true, Components.interfaces.nsIMsgPrintEngine.MNAB_PRINTPREVIEW_MSG);
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
  // users don't like it when the "Get Msgs" button is disabled
  // so let's never do that. 
  // we'll just handle it as best we can in GetFolderMessages()
  // when they click "Get Msgs" and
  // Local Folders or a news server is selected
  // see bugs #89404 and #111102
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
    if ((serverType == "nntp") && !folder.isServer) {
        var newsServer = server.QueryInterface(Components.interfaces.nsINntpIncomingServer);
        var menuLabel = gMessengerBundle.getFormattedString("getNextNMessages",
                                                            [ newsServer.maxArticles ]);
        menuItem.setAttribute("label",menuLabel);
        menuItem.removeAttribute("hidden");
        return true;
    }

    menuItem.setAttribute("hidden","true");
    return false;
}

function IsEmptyTrashEnabled()
{
  var folderURI = GetSelectedFolderURI();
  var server = GetServer(folderURI);
  return (server && server.canEmptyTrashOnExit?IsMailFolderSelected():false);
}

function IsCompactFolderEnabled()
{
  var server = GetServer(GetSelectedFolderURI());
  return (server && 
      ((server.type != 'imap') || server.canCompactFoldersOnServer) &&
      isCommandEnabled("cmd_compactFolder"));   // checks e.g. if IMAP is offline
}

var gDeleteButton = null;
var gMarkButton = null;

function SetUpToolbarButtons(uri)
{
    //dump("SetUpToolbarButtons("+uri+")\n");

    // eventually, we might want to set up the toolbar differently for imap,
    // pop, and news.  for now, just tweak it based on if it is news or not.
    var forNews = isNewsURI(uri);

    if(!gDeleteButton) gDeleteButton = document.getElementById("button-delete");

    var buttonToHide = null;
    var buttonToShow = null;

    if (forNews) {
        buttonToHide = gDeleteButton;
    }
    else {
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
function MsgAddSenderToAddressBook() {}
function MsgAddAllToAddressBook() {}

function SpaceHit(event)
{
  var contentWindow = window.top._content;
  var rssiframe = contentWindow.document.getElementById('_mailrssiframe');

  // if we are displaying an RSS article, we really want to scroll the nested iframe
  if (rssiframe)
    contentWindow = rssiframe.contentWindow;

  if (event && event.shiftKey) {
    // if at the start of the message, go to the previous one
    if (contentWindow.scrollY > 0) {
      contentWindow.scrollByPages(-1);
    }
    else {
      goDoCommand("cmd_previousUnreadMsg");
    }
  }
  else {
    // if at the end of the message, go to the next one
    if (contentWindow.scrollY < contentWindow.scrollMaxY) {
      contentWindow.scrollByPages(1);
    }
    else {
      goDoCommand("cmd_nextUnreadMsg");
    }
  }
}

function IsAccountOfflineEnabled()
{
  var selectedFolders = GetSelectedMsgFolders();

  if (selectedFolders && (selectedFolders.length == 1))
      return selectedFolders[0].supportsOffline;
     
  return false;
}

function GetDefaultAccountRootFolder()
{
  try {
    var account = accountManager.defaultAccount;
    var defaultServer = account.incomingServer;
    var defaultFolder = defaultServer.rootMsgFolder;
    return defaultFolder;
  }
  catch (ex) {
  }
  return null;
}

function GetFolderMessages()
{
  var selectedFolders = GetSelectedMsgFolders();
  var defaultAccountRootFolder = GetDefaultAccountRootFolder();
  
  // if no default account, get msg isn't going do anything anyways
  // so bail out
  if (!defaultAccountRootFolder)
    return;

  // if nothing selected, use the default
  var folder = selectedFolders.length ? selectedFolders[0] : defaultAccountRootFolder;

  var serverType = folder.server.type;

  if (folder.isServer && (serverType == "nntp")) {
    // if we're doing "get msgs" on a news server
    // update unread counts on this server
    folder.server.performExpand(msgWindow);
    return;
  }
  else if (serverType == "none") {
    // if "Local Folders" is selected
    // and the user does "Get Msgs"
    // and LocalFolders is not deferred to,
    // get new mail for the default account
    //
    // XXX TODO
    // should shift click get mail for all (authenticated) accounts?
    // see bug #125885
    if (!folder.server.isDeferredTo)
      folder = defaultAccountRootFolder;
  }

  var folders = new Array(1);
  folders[0] = folder;

  var compositeDataSource = GetCompositeDataSource("GetNewMessages");
  GetNewMessages(folders, folder.server, compositeDataSource);
}

function SendUnsentMessages()
{
  var msgSendlater = Components.classes["@mozilla.org/messengercompose/sendlater;1"]
               .getService(Components.interfaces.nsIMsgSendLater);
  var identitiesCount, allIdentities, currentIdentity, numMessages, msgFolder;

  if (accountManager) { 
    allIdentities = accountManager.allIdentities;
    identitiesCount = allIdentities.Count();
    for (var i = 0; i < identitiesCount; i++) {
      currentIdentity = allIdentities.QueryElementAt(i, Components.interfaces.nsIMsgIdentity);
      msgFolder = msgSendlater.getUnsentMessagesFolder(currentIdentity);
      if(msgFolder) {
        numMessages = msgFolder.getTotalMessages(false /* include subfolders */);
        if(numMessages > 0) {
          messenger.sendUnsentMessages(currentIdentity, msgWindow);
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

function CoalesceGetMsgsForPop3ServersByDestFolder(currentServer, pop3DownloadServersArray, localFoldersToDownloadTo)
{
  var outNumFolders = new Object();
  var inboxFolder = currentServer.rootMsgFolder.getFoldersWithFlag(0x1000, 1, outNumFolders); 
  // coalesce the servers that download into the same folder...
  var index = localFoldersToDownloadTo.GetIndexOf(inboxFolder);
  if (index == -1)
  {
    if(inboxFolder) 
    {
      inboxFolder.biffState =  Components.interfaces.nsIMsgFolder.nsMsgBiffState_NoMail;
      inboxFolder.clearNewMessages();
    }
    localFoldersToDownloadTo.AppendElement(inboxFolder);
    index = pop3DownloadServersArray.length
    pop3DownloadServersArray[index] = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
  }
  pop3DownloadServersArray[index].AppendElement(currentServer);
}

function GetMessagesForAllAuthenticatedAccounts()
{
  // now log into any server
  try
  {
    var allServers = accountManager.allServers;
    var i;
    // array of isupportsarrays of servers for a particular folder
    var pop3DownloadServersArray = new Array();
    // parallel isupports array of folders to download to...
    var localFoldersToDownloadTo = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
    var pop3Server;

    for (i = 0; i < allServers.Count(); ++i)
    {
      var currentServer = allServers.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgIncomingServer);
      var protocolinfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + currentServer.type].getService(Components.interfaces.nsIMsgProtocolInfo);
      if (protocolinfo.canGetMessages && !currentServer.passwordPromptRequired)
      {
        if (currentServer.type == "pop3")
        {
          CoalesceGetMsgsForPop3ServersByDestFolder(currentServer, pop3DownloadServersArray, localFoldersToDownloadTo);
          pop3Server = currentServer.QueryInterface(Components.interfaces.nsIPop3IncomingServer);
        }
        else
        // get new messages on the server for imap or rss
          GetMessagesForInboxOnServer(currentServer);
      }
    }
    for (i = 0; i < pop3DownloadServersArray.length; ++i)
    {
      // any ol' pop3Server will do - the serversArray specifies which servers to download from
      pop3Server.downloadMailFromServers(pop3DownloadServersArray[i], msgWindow, localFoldersToDownloadTo.GetElementAt(i), null);
    }
  }
  catch(ex)
  {
      dump(ex + "\n");
  }
}

function GetMessagesForAccount(aEvent)
{
  var uri = aEvent.target.id;
  var server = GetServer(uri);
  GetMessagesForInboxOnServer(server);
  aEvent.stopPropagation();
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
    var loadedFolder = GetLoadedMsgFolder();

    // if we have selected a server, and are viewing account central
    // there is no loaded folder
    if (!loadedFolder)
      return false;

    var server = loadedFolder.server;
    if (!(server.canUndoDeleteOnServer))
      return false;

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

function HandleJunkStatusChanged(folder)
{
  // this might be the stand alone window, open to a message that was
  // and attachment (or on disk), in which case, we want to ignore it.
  var loadedMessage = GetLoadedMessage();
  if (loadedMessage && (!(/type=application\/x-message-display/.test(loadedMessage))) && IsCurrentLoadedFolder(folder))
  {
    // if multiple message are selected and we change the junk status
    // we don't want to show the junk bar (since the message pane is blank)
    var msgHdr = null;
    if (GetNumSelectedMessages() == 1)
      msgHdr = messenger.msgHdrFromURI(loadedMessage);
    var junkBarWasDisplayed = gMessageNotificationBar.isFlagSet(kMsgNotificationJunkBar);
    gMessageNotificationBar.setJunkMsg(msgHdr);

    // only reload message if junk bar display state has changed.    
    if (msgHdr && junkBarWasDisplayed != gMessageNotificationBar.isFlagSet(kMsgNotificationJunkBar))
    {
      // we may be forcing junk mail to be rendered with sanitized html. In that scenario, we want to 
      // reload the message if the status has just changed to not junk. 
      var sanitizeJunkMail = gPrefBranch.getBoolPref("mail.spam.display.sanitize");
      if (sanitizeJunkMail) // only bother doing this if we are modifying the html for junk mail....
      {
        var moveJunkMail = (folder && folder.server && folder.server.spamSettings) ?
	                          folder.server.spamSettings.manualMark : false;

        var junkScore = msgHdr.getStringProperty("junkscore"); 
        var isJunk = ((junkScore != "") && (junkScore != "0"));

        // we used to only reload the message if we were toggling the message to NOT JUNK from junk
        // but it can be useful to see the HTML in the message get converted to sanitized form when a message
        // is marked as junk.
        // Furthermore, if we are about to move the message that was just marked as junk, 
        // then don't bother reloading it.
        if (!(isJunk && moveJunkMail)) 
          MsgReload();
      }
    }
  }
}

var gMessageNotificationBar = 
{
  mBarStatus: 0,
  // flag bit values for mBarStatus, indexed by kMsgNotificationXXX
  mBarFlagValues: [
                    0, // for no msgNotificationBar
                    1, // 1 << (kMsgNotificationPhishingBar - 1)
                    2, // 1 << (kMsgNotificationJunkBar - 1)
                    4  // 1 << (kMsgNotificationRemoteImages - 1)
                  ],

  mMsgNotificationBar: document.getElementById('msgNotificationBar'),

  setJunkMsg: function(aMsgHdr)
  {
    var isJunk = false;
  
    if (aMsgHdr) 
    {
      var junkScore = aMsgHdr.getStringProperty("junkscore"); 
      isJunk = ((junkScore != "") && (junkScore != "0"));
    }

    this.updateMsgNotificationBar(kMsgNotificationJunkBar, isJunk);

    goUpdateCommand('button_junk');
  },

  setRemoteContentMsg: function(aMsgHdr)
  {  
    // update the allow remote content for sender string
    var headerParser = Components.classes["@mozilla.org/messenger/headerparser;1"].getService(Components.interfaces.nsIMsgHeaderParser);
    var emailAddress = headerParser.extractHeaderAddressMailboxes(null, aMsgHdr.author);
    document.getElementById('allowRemoteContentForAuthorDesc').textContent  = 
      gMessengerBundle.getFormattedString('alwaysLoadRemoteContentForSender', [emailAddress ? emailAddress : aMsgHdr.author]);
    this.updateMsgNotificationBar(kMsgNotificationRemoteImages, true);
  },

  setPhishingMsg: function()
  {
    this.updateMsgNotificationBar(kMsgNotificationPhishingBar, true);
  },

  clearMsgNotifications: function()
  {
    this.mBarStatus = 0;
    this.mMsgNotificationBar.selectedIndex = 0;
    this.mMsgNotificationBar.collapsed = true;
  },

  updateMsgNotificationBar: function(aIndex, aSet)
  {
    var chunk = this.mBarFlagValues[aIndex];
    var status = aSet ? this.mBarStatus | chunk : this.mBarStatus & ~chunk;
    this.mBarStatus = status;

    // the phishing message takes precedence over the junk message
    // which takes precedence over the remote content message
    this.mMsgNotificationBar.selectedIndex = this.mBarFlagValues.indexOf(status & -status);
    this.mMsgNotificationBar.collapsed = !status;
  },
  
  /**
   * @param aFlag (kMsgNotificationPhishingBar, kMsgNotificationJunkBar, kMsgNotificationRemoteImages
   * @return true if aFlag is currently set for the loaded message
   */
  isFlagSet: function(aFlag)
  {
    var chunk = this.mBarFlagValues[aFlag];
    return this.mBarStatus & chunk;
  }
};

/**
 * loadMsgWithRemoteContent
 *   Reload the current message, allowing remote content
 */
function loadMsgWithRemoteContent()
{
  // we want to get the msg hdr for the currently selected message
  // change the "remoteContentBar" property on it
  // then reload the message

  setMsgHdrPropertyAndReload("remoteContentPolicy", kAllowRemoteContent);
}

/**
 *  msgHdrForCurrentMessage
 *   Returns the msg hdr associated with the current loaded message.
 */
function msgHdrForCurrentMessage()
{
  var msgURI = GetLoadedMessage();
  return (msgURI && !(/type=application\/x-message-display/.test(msgURI))) ? messenger.msgHdrFromURI(msgURI) : null;
}

/**
 *  Reloads the message after adjusting the remote content policy for the sender.
 *  Iterate through the local address books looking for a card with the same e-mail address as the 
 *  sender of the current loaded message. If we find a card, update the allow remote content field.
 *  If we can't find a card, prompt the user with a new AB card dialog, pre-selecting the remote content field.
 */
function allowRemoteContentForSender()
{
  // get the sender of the msg hdr
  var msgHdr = msgHdrForCurrentMessage();
  if (!msgHdr)
    return;
  
  var headerParser = Components.classes["@mozilla.org/messenger/headerparser;1"]
                      .getService(Components.interfaces.nsIMsgHeaderParser);
  var names = {};
  var addresses = {};
  var fullNames = {};
  var numAddresses;
  
  numAddresses = headerParser.parseHeadersWithArray(msgHdr.author, addresses, names, fullNames);
  var authorEmailAddress = addresses.value[0];
  if (!authorEmailAddress)
    return;
  
  // search through all of our local address books looking for a match.
  var parentDir = RDF.GetResource("moz-abdirectory://").QueryInterface(Components.interfaces.nsIAbDirectory);
  var enumerator = parentDir.childNodes;
  var cardForEmailAddress;
  var addrbook;
  while (!cardForEmailAddress && enumerator.hasMoreElements())
  {
    addrbook = enumerator.getNext();
    if (addrbook instanceof Components.interfaces.nsIAbMDBDirectory)
      cardForEmailAddress = addrbook.cardForEmailAddress(authorEmailAddress);
  }

  var allowRemoteContent = false;
  if (cardForEmailAddress && addrbook instanceof Components.interfaces.nsIAbDirectory)
  {
    // set the property for remote content
    cardForEmailAddress.allowRemoteContent = true;
    addrbook.modifyCard(cardForEmailAddress);
    allowRemoteContent = true;
  }
  else
  {
    var args = {primaryEmail:authorEmailAddress, displayName:names.value[0],
                allowRemoteContent:true};
    // create a new card and set the property
    window.openDialog("chrome://messenger/content/addressbook/abNewCardDialog.xul",
                      "", "chrome,resizable=no,titlebar,modal,centerscreen", args);
    allowRemoteContent = args.allowRemoteContent;
  } 
  
  // reload the message if we've updated the remote content policy for the sender  
  if (allowRemoteContent)
    MsgReload();
}

function MsgIsNotAScam()
{
  // we want to get the msg hdr for the currently selected message
  // change the "isPhishingMsg" property on it
  // then reload the message

  setMsgHdrPropertyAndReload("notAPhishMessage", kNotAPhishMessage);
}

function setMsgHdrPropertyAndReload(aProperty, aValue)
{
  // we want to get the msg hdr for the currently selected message
  // change the appropiate property on it then reload the message

  var msgHdr = msgHdrForCurrentMessage();
  if (msgHdr)
  {
    msgHdr.setUint32Property(aProperty, aValue);
    MsgReload();
  }
}

function checkMsgHdrPropertyIsNot(aProperty, aValue)
{
  // we want to get the msg hdr for the currently selected message,
  // get the appropiate property on it and then test against value.
  var msgHdr = msgHdrForCurrentMessage();
  return (msgHdr && msgHdr.getUint32Property(aProperty) != aValue);
}

function MarkCurrentMessageAsRead()
{
  ClearPendingReadTimer();
  gDBView.doCommand(nsMsgViewCommandType.markMessagesRead);
}

function ClearPendingReadTimer()
{
  if (gMarkViewedMessageAsReadTimer)
  {
    clearTimeout(gMarkViewedMessageAsReadTimer);
    gMarkViewedMessageAsReadTimer = null;
  }
}

// this is called when layout is actually finished rendering a 
// mail message. OnMsgLoaded is called when libmime is done parsing the message
function OnMsgParsed(aUrl)
{
  // browser doesn't do this, but I thought it could be a useful thing to test out...
  // If the find bar is visible and we just loaded a new message, re-run 
  // the find command. This means the new message will get highlighted and
  // we'll scroll to the first word in the message that matches the find text.
  var findBar = document.getElementById("FindToolbar");
  if (!findBar.hidden)
    findBar.onFindAgainCommand(false);
    
  // run the phishing detector on the message
  if (!checkMsgHdrPropertyIsNot("notAPhishMessage", kIsAPhishMessage))
    gPhishingDetector.analyzeMsgForPhishingURLs(aUrl);
}

function OnMsgLoaded(aUrl)
{
    if (!aUrl)
      return;

    // nsIMsgMailNewsUrl.folder throws an error when opening .eml files.
    var folder;
    try {
      folder = aUrl.folder;
    }
    catch (ex) {}
    
    var msgURI = GetLoadedMessage();
    
    if (!folder || !msgURI)
      return;

    // If we are in the middle of a delete or move operation, make sure that
    // if the user clicks on another message then that message stays selected
    // and the selection does not "snap back" to the message chosen by
    // SetNextMessageAfterDelete() when the operation completes (bug 243532).
    // But the just loaded message might be getting deleted, if the user
    // deletes it before the message is loaded (bug 183394)
    var wintype = document.documentElement.getAttribute('windowtype');
    if (wintype == "mail:messageWindow" || GetThreadTree().view.selection.currentIndex != gSelectedIndexWhenDeleting)
      gNextMessageViewIndexAfterDelete = -2;

    var msgHdr = msgHdrForCurrentMessage();
     
    gMessageNotificationBar.setJunkMsg(msgHdr);

    // we just finished loading a message. set a timer to actually mark the message is read after n seconds
    // where n can be configured by the user.

    var markReadOnADelay = gPrefBranch.getBoolPref("mailnews.mark_message_read.delay");

    if (msgHdr && !msgHdr.isRead)
    {
      if (markReadOnADelay && wintype == "mail:3pane") // only use the timer if viewing using the 3-pane preview pane and the user has set the pref
      {
        ClearPendingReadTimer();
        gMarkViewedMessageAsReadTimer = setTimeout(MarkCurrentMessageAsRead, gPrefBranch.getIntPref("mailnews.mark_message_read.delay.interval") * 1000);
      }
      else
        MarkCurrentMessageAsRead();
    }

    // See if MDN was requested but has not been sent.
    HandleMDNResponse(aUrl);

    var currentMsgFolder = folder.QueryInterface(Components.interfaces.nsIMsgFolder);
    if (!IsImapMessage(msgURI))
      return;

    var imapServer = currentMsgFolder.server.QueryInterface(Components.interfaces.nsIImapIncomingServer);
    var storeReadMailInPFC = imapServer.storeReadMailInPFC;
    if (storeReadMailInPFC)
    {
      var messageID;

      var copyToOfflineFolder = true;

      // look in read mail PFC for msg with same msg id - if we find one,
      // don't put this message in the read mail pfc.
      var outputPFC = imapServer.GetReadMailPFC(true);

      if (msgHdr)
      {
        messageID = msgHdr.messageId;
        if (messageID.length > 0)
        {
          var readMailDB = outputPFC.getMsgDatabase(msgWindow);
          if (readMailDB)
          {
            var hdrInDestDB = readMailDB.getMsgHdrForMessageID(messageID);
            if (hdrInDestDB)
              copyToOfflineFolder = false;
          }
        }
      }
      if (copyToOfflineFolder)
      {
        var messages = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
        messages.AppendElement(msgHdr);

        res = outputPFC.copyMessages(currentMsgFolder, messages, false /*isMove*/, msgWindow /* nsIMsgWindow */, null /* listener */, false /* isFolder */, false /*allowUndo*/ );
      }
   }
}

//
// This function handles all mdn response generation (ie, imap and pop).
// For pop the msg uid can be 0 (ie, 1st msg in a local folder) so no
// need to check uid here. No one seems to set mimeHeaders to null so
// no need to check it either.
//
function HandleMDNResponse(aUrl)
{
  if (!aUrl)
    return;

  var msgFolder = aUrl.folder;
  var msgURI = GetLoadedMessage();
  if (!msgFolder || !msgURI || IsNewsMessage(msgURI))
    return;

  // if the message is marked as junk, do NOT attempt to process a return receipt
  // in order to better protect the user
  if (SelectedMessagesAreJunk())
    return;

  var msgHdr = messenger.msgHdrFromURI(msgURI);
  var mimeHdr;

  try {
    mimeHdr = aUrl.mimeHeaders;
  } catch (ex) {
    return;
  }

  // If we didn't get the message id when we downloaded the message header,
  // we cons up an md5: message id. If we've done that, we'll try to extract
  // the message id out of the mime headers for the whole message.
  var msgId = msgHdr.messageId;
  if (msgId.split(":")[0] == "md5")
  {
    var mimeMsgId = mimeHdr.extractHeader("Message-Id", false);
    if (mimeMsgId)
      msgHdr.messageId = mimeMsgId;
  }

  // After a msg is downloaded it's already marked READ at this point so we must check if
  // the msg has a "Disposition-Notification-To" header and no MDN report has been sent yet.
  var msgFlags = msgHdr.flags;
  if ((msgFlags & MSG_FLAG_IMAP_DELETED) || (msgFlags & MSG_FLAG_MDN_REPORT_SENT))
    return;

  var DNTHeader = mimeHdr.extractHeader("Disposition-Notification-To", false);
  var oldDNTHeader = mimeHdr.extractHeader("Return-Receipt-To", false);
  if (!DNTHeader && !oldDNTHeader)
    return;

  // Everything looks good so far, let's generate the MDN response.
  var mdnGenerator = Components.classes["@mozilla.org/messenger-mdn/generator;1"].
                                  createInstance(Components.interfaces.nsIMsgMdnGenerator);
  mdnGenerator.process(MDN_DISPOSE_TYPE_DISPLAYED, msgWindow, msgFolder, msgHdr.messageKey, mimeHdr, false);

  // Reset mark msg MDN "Sent" and "Not Needed".
  msgHdr.flags = (msgFlags & ~MSG_FLAG_MDN_REPORT_NEEDED);
  msgHdr.OrFlags(MSG_FLAG_MDN_REPORT_SENT);

  // Commit db changes.
  var msgdb = msgFolder.getMsgDatabase(msgWindow);
  if (msgdb)
    msgdb.Commit(ADDR_DB_LARGE_COMMIT);
}

function QuickSearchFocus() 
{
  var quickSearchTextBox = document.getElementById('searchInput');
  if (quickSearchTextBox)
    quickSearchTextBox.focus();
}

function MsgSearchMessages()
{
  var preselectedFolder = null;
  if ("GetFirstSelectedMsgFolder" in window)
    preselectedFolder = GetFirstSelectedMsgFolder();

  var args = { folder: preselectedFolder };
  OpenOrFocusWindow(args, "mailnews:search", "chrome://messenger/content/SearchDialog.xul");
}

function MsgJunkMailInfo(aCheckFirstUse)
{
  if (aCheckFirstUse) {
    if (!pref.getBoolPref("mailnews.ui.junk.firstuse"))
      return;
    pref.setBoolPref("mailnews.ui.junk.firstuse", false);

    // check to see if this is an existing profile where the user has started using
    // the junk mail feature already
    var junkmailPlugin = Components.classes["@mozilla.org/messenger/filter-plugin;1?name=bayesianfilter"]
                                   .getService(Components.interfaces.nsIJunkMailPlugin);
    if (junkmailPlugin.userHasClassified)
      return;
  }

  var desiredWindow = GetWindowByWindowType("mailnews:junkmailinfo");

  if (desiredWindow)
    desiredWindow.focus();
  else
    window.openDialog("chrome://messenger/content/junkMailInfo.xul", "mailnews:junkmailinfo", "centerscreen,resizeable=no,titlebar,chrome,modal", null);
}

function MsgSearchAddresses()
{
  var args = { directory: null };
  OpenOrFocusWindow(args, "mailnews:absearch", "chrome://messenger/content/ABSearchDialog.xul");
}
 
function MsgFilterList(args)
{
  OpenOrFocusWindow(args, "mailnews:filterlist", "chrome://messenger/content/FilterListDialog.xul");
}

function GetWindowByWindowType(windowType)
{
  var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService(Components.interfaces.nsIWindowMediator);
  return windowManager.getMostRecentWindow(windowType);
}

function OpenOrFocusWindow(args, windowType, chromeURL)
{
  var desiredWindow = GetWindowByWindowType(windowType);

  if (desiredWindow) {
    desiredWindow.focus();
    if ("refresh" in args && args.refresh)
      desiredWindow.refresh();
  }
  else
    window.openDialog(chromeURL, "", "chrome,resizable,status,centerscreen,dialog=no", args);
}

/**
 * Opens the update manager and checks for updates to the application.
 */

function checkForUpdates()
{
  var um = 
      Components.classes["@mozilla.org/updates/update-manager;1"].
      getService(Components.interfaces.nsIUpdateManager);
  var prompter = 
      Components.classes["@mozilla.org/updates/update-prompt;1"].
      createInstance(Components.interfaces.nsIUpdatePrompt);

  // If there's an update ready to be applied, show the "Update Downloaded"
  // UI instead and let the user know they have to restart the application for
  // the changes to be applied. 
  if (um.activeUpdate && um.activeUpdate.state == "pending")
    prompter.showUpdateDownloaded(um.activeUpdate);
  else
  prompter.checkForUpdates();
}

function buildHelpMenu()
{
  var updates = 
      Components.classes["@mozilla.org/updates/update-service;1"].
      getService(Components.interfaces.nsIApplicationUpdateService);
  var um = 
      Components.classes["@mozilla.org/updates/update-manager;1"].
      getService(Components.interfaces.nsIUpdateManager);
  
  // Disable the UI if the update enabled pref has been locked by the
  // administrator or if we cannot update for some other reason 
  var checkForUpdates = document.getElementById("checkForUpdates");
  var canUpdate = updates.canUpdate;
  checkForUpdates.setAttribute("disabled", !canUpdate);
  if (!canUpdate)
    return;

  if (!gMessengerBundle)
    gMessengerBundle = document.getElementById("bundle_messenger");
  
  var activeUpdate = um.activeUpdate;
  
  // If there's an active update, substitute its name into the label
  // we show for this item, otherwise display a generic label.
  function getStringWithUpdateName(key) {
    if (activeUpdate && activeUpdate.name)
      return gMessengerBundle.getFormattedString(key, [activeUpdate.name]);
    return gMessengerBundle.getString(key + "Fallback");
    }
  
  // By default, show "Check for Updates..."
  var key = "default";
  if (activeUpdate) {
    switch (activeUpdate.state) {
    case "downloading":
      // If we're downloading an update at present, show the text:
      // "Downloading Firefox x.x..." otherwise we're paused, and show
      // "Resume Downloading Firefox x.x..."
      key = updates.isDownloading ? "downloading" : "resume";
      break;
    case "pending":
      // If we're waiting for the user to restart, show: "Apply Downloaded
      // Updates Now..."
      key = "pending";
      break;
    }
  }

  checkForUpdates.label = getStringWithUpdateName("updatesItem_" + key);
  if (um.activeUpdate && updates.isDownloading)
    checkForUpdates.setAttribute("loading", "true");
  else
    checkForUpdates.removeAttribute("loading");
}
