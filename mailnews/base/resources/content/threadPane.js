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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

var gLastMessageUriToLoad = null;
var gThreadPaneCommandUpdater = null;

function ThreadPaneOnClick(event)
{
    // we are already handling marking as read and flagging
    // in nsMsgDBView.cpp
    // so all we need to worry about here is double clicks
    // and column header.
    //
    // we get in here for clicks on the "outlinercol" (headers)
    // and the "scrollbarbutton" (scrollbar buttons)
    // we don't want those events to cause a "double click"

    var t = event.originalTarget;

    if (t.localName == "outlinercol") {
       HandleColumnClick(t.id);
    }
    else if (event.detail == 2 && t.localName == "outlinerbody") {
       ThreadPaneDoubleClick();
    }
}

function SetHiddenAttributeOnThreadOnlyColumns(value)
{
  // todo, cache these?

  var totalCol = document.getElementById("totalCol");
  var unreadCol = document.getElementById("unreadCol");

  totalCol.setAttribute("hidden",value);
  unreadCol.setAttribute("hidden",value);
}


function nsMsgDBViewCommandUpdater()
{}

nsMsgDBViewCommandUpdater.prototype = 
{
  updateCommandStatus : function()
    {
      // the back end is smart and is only telling us to update command status
      // when the # of items in the selection has actually changed.
		  document.commandDispatcher.updateCommands('mail-toolbar');
    },

  displayMessageChanged : function(aFolder, aSubject)
  {
    setTitleFromFolder(aFolder, aSubject);
    gHaveLoadedMessage = true;
  },

  QueryInterface : function(iid)
   {
     if(iid.equals(Components.interfaces.nsIMsgDBViewCommandUpdater))
	    return this;
	  
     throw Components.results.NS_NOINTERFACE;
     return null;
    }
}

function HandleColumnClick(columnID)
{
  // if they click on the "threadCol", we need to show the threaded-only columns
  if ((columnID[0] == 't') && (columnID[1] == 'h')) {  
    SetHiddenAttributeOnThreadOnlyColumns(""); // this will show them
  }
  else {
    SetHiddenAttributeOnThreadOnlyColumns("true");  // this will hide them
  }
  
  ShowAppropriateColumns();
  PersistViewAttributesOnFolder();
}

function PersistViewAttributesOnFolder()
{
  var folder = GetSelectedFolder();

  if (folder) {
    folder.setAttribute("viewType", gDBView.viewType);
    folder.setAttribute("viewFlags", gDBView.viewFlags);
    folder.setAttribute("sortType", gDBView.sortType);
    folder.setAttribute("sortOrder", gDBView.sortOrder);
  }
}

function MsgComposeDraftMessage()
{
    var loadedFolder = GetLoadedMsgFolder();
    var messageArray = GetSelectedMessages();

    ComposeMessage(msgComposeType.Draft, msgComposeFormat.Default, loadedFolder, messageArray);
}

function ThreadPaneDoubleClick()
{
	var loadedFolder;
	var messageArray;
	var messageUri;

	if (IsSpecialFolderSelected(MSG_FOLDER_FLAG_DRAFTS)) {
		MsgComposeDraftMessage();
	}
	else if(IsSpecialFolderSelected(MSG_FOLDER_FLAG_TEMPLATES)) {
		loadedFolder = GetLoadedMsgFolder();
		messageArray = GetSelectedMessages();
		ComposeMessage(msgComposeType.Template, msgComposeFormat.Default, loadedFolder, messageArray);
	}
	else {
        MsgOpenSelectedMessages();
	}
}

function ThreadPaneKeyPress(event)
{
  return;
}

function MsgSortByDate()
{
    MsgSortThreadPane(nsMsgViewSortType.byDate);
}

function MsgSortBySender()
{
    MsgSortThreadPane(nsMsgViewSortType.byAuthor);
}

function MsgSortByRecipient()
{
    MsgSortThreadPane(nsMsgViewSortType.byRecipient);
}

function MsgSortByStatus()
{
    MsgSortThreadPane(nsMsgViewSortType.byStatus);
}

function MsgSortBySubject()
{
    MsgSortThreadPane(nsMsgViewSortType.bySubject);
}

function MsgSortByFlagged() 
{
    MsgSortThreadPane(nsMsgViewSortType.byFlagged);
}

function MsgSortByPriority()
{
    MsgSortThreadPane(nsMsgViewSortType.byPriority);
}

function MsgSortBySize() 
{
    MsgSortThreadPane(nsMsgViewSortType.bySize);
}

function MsgSortByUnread()
{
    MsgSortThreadPane(nsMsgViewSortType.byUnread);
}

function MsgSortByOrderReceived()
{
    MsgSortThreadPane(nsMsgViewSortType.byId);
}

function MsgSortByTotal()
{
    dump("XXX fix MsgSortByTotal\n");
    //MsgSortThreadPane(nsMsgViewSortType.byTotal);
}

function MsgSortByThread()
{
    MsgSortThreadPane(nsMsgViewSortType.byThread);
}

function MsgSortThreadPane(sortType)
{
    gDBView.sort(sortType, nsMsgViewSortOrder.ascending);

    ShowAppropriateColumns();
    PersistViewAttributesOnFolder();
}

function MsgSortAscending()
{
    gDBView.sort(gDBView.sortType, nsMsgViewSortOrder.ascending);
}

function MsgSortDescending()
{
    gDBView.sort(gDBView.sortType, nsMsgViewSortOrder.descending);
}

function IsSpecialFolderSelected(flags)
{
	var selectedFolder = GetThreadPaneFolder();
    if (!selectedFolder) return false;

    if ((selectedFolder.flags & flags) == 0) {
        return false;
    }
    else {
        return true;
    }
}

function GetThreadOutliner()
{
  if (gThreadOutliner) return gThreadOutliner;
	gThreadOutliner = document.getElementById('threadOutliner');
	return gThreadOutliner;
}

function GetThreadPaneFolder()
{
  try {
    return gDBView.msgFolder;
  }
  catch (ex) {
    return null;
  }
}

function EnsureRowInThreadOutlinerIsVisible(index)
{
  var outliner = GetThreadOutliner();
  outliner.boxObject.QueryInterface(Components.interfaces.nsIOutlinerBoxObject).ensureRowIsVisible(index); 
}
