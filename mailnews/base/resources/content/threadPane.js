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
    // we only care about button 0 (left click) events
    if (event.button != 0) return;

    // we are already handling marking as read and flagging
    // in nsMsgDBView.cpp
    // so all we need to worry about here is double clicks
    // and column header.
    //
    // we get in here for clicks on the "treecol" (headers)
    // and the "scrollbarbutton" (scrollbar buttons)
    // we don't want those events to cause a "double click"

    var t = event.originalTarget;

    if (t.localName == "treecol") {
       HandleColumnClick(t.id);
    }
    else if (t.localName == "treechildren") {
      var row = new Object;
      var colID = new Object;
      var childElt = new Object;

      var tree = GetThreadTree();
      // figure out what cell the click was in
      tree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, colID, childElt);
      if (row.value == -1)
       return;

      // if the cell is in a "cycler" column
      // or if the user double clicked on the twisty,
      // don't open the message in a new window
      var col = document.getElementById(colID.value);
      if (col) {
        if (event.detail == 2 && col.getAttribute("cycler") != "true" && (childElt.value != "twisty")) {
          ThreadPaneDoubleClick();
          // double clicking should not toggle the open / close state of the 
          // thread.  this will happen if we don't prevent the event from
          // bubbling to the default handler in tree.xml
          event.preventBubble();
        }
        else if (colID.value == "junkStatusCol") {
          MsgJunkMailInfo(true);
        }
      }      
    }
}

function nsMsgDBViewCommandUpdater()
{}

nsMsgDBViewCommandUpdater.prototype = 
{
  updateCommandStatus : function()
    {
      // the back end is smart and is only telling us to update command status
      // when the # of items in the selection has actually changed.
      UpdateMailToolbar("dbview driven, thread pane");
    },

  displayMessageChanged : function(aFolder, aSubject, aKeywords)
  {
    setTitleFromFolder(aFolder, aSubject);
    ClearPendingReadTimer(); // we are loading / selecting a new message so kill the mark as read timer for the currently viewed message
    gHaveLoadedMessage = true;
    SetKeywords(aKeywords);
    goUpdateCommand("button_junk");
  },

  updateNextMessageAfterDelete : function()
  {
    SetNextMessageAfterDelete();
  },

  QueryInterface : function(iid)
   {
     if (iid.equals(Components.interfaces.nsIMsgDBViewCommandUpdater) ||
         iid.equals(Components.interfaces.nsISupports))
       return this;
	  
     throw Components.results.NS_NOINTERFACE;
    }
}

function HandleColumnClick(columnID)
{
  var sortType = ConvertColumnIDToSortType(columnID);

  // if sortType is 0, this is an unsupported sort type
  // return, since we can't sort by that column.
  if (sortType == 0)
    return;

  var dbview = GetDBView();
  var simpleColumns = false;
  try {
    simpleColumns = !pref.getBoolPref("mailnews.thread_pane_column_unthreads");
  }
  catch (ex) {
  }
  if (sortType == nsMsgViewSortType.byThread) {
    if (!dbview.supportsThreading)
      return;

    if (simpleColumns)
      MsgToggleThreaded();
    else if (dbview.viewFlags & nsMsgViewFlagsType.kThreadedDisplay)
      MsgReverseSortThreadPane();
    else
      MsgSortByThread();
  }
  else {
    if (!simpleColumns && (dbview.viewFlags & nsMsgViewFlagsType.kThreadedDisplay)) {
      dbview.viewFlags &= ~nsMsgViewFlagsType.kThreadedDisplay;
      MsgSortThreadPane(sortType);
    }
    else if (dbview.sortType == sortType) {
      MsgReverseSortThreadPane();
    }
    else {
      MsgSortThreadPane(sortType);
    }
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
  if (IsSpecialFolderSelected(MSG_FOLDER_FLAG_DRAFTS)) {
    MsgComposeDraftMessage();
  }
  else if(IsSpecialFolderSelected(MSG_FOLDER_FLAG_TEMPLATES)) {
    var loadedFolder = GetLoadedMsgFolder();
    var messageArray = GetSelectedMessages();
    ComposeMessage(msgComposeType.Template, msgComposeFormat.Default, loadedFolder, messageArray);
  }
  else {
    MsgOpenSelectedMessages();
  }
}

function ThreadPaneKeyPress(event)
{
    if (event.keyCode == 13)
      ThreadPaneDoubleClick();
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

function MsgSortByLabel()
{
    MsgSortThreadPane(nsMsgViewSortType.byLabel);
}

function MsgSortByJunkStatus()
{
    MsgSortThreadPane(nsMsgViewSortType.byJunkStatus);
}

function MsgSortByAttachments()
{
    MsgSortThreadPane(nsMsgViewSortType.byAttachments);
}

function MsgSortBySubject()
{
    MsgSortThreadPane(nsMsgViewSortType.bySubject);
}

function MsgSortByLocation()
{
    MsgSortThreadPane(nsMsgViewSortType.byLocation);
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
  var dbview = GetDBView();
  if(dbview && !dbview.supportsThreading)
    return;
  dbview.viewFlags |= nsMsgViewFlagsType.kThreadedDisplay;
  MsgSortThreadPane(nsMsgViewSortType.byId);
}

function MsgSortThreadPane(sortType)
{
    var dbview = GetDBView();
    dbview.sort(sortType, nsMsgViewSortOrder.ascending);
    UpdateSortIndicators(sortType, nsMsgViewSortOrder.ascending);
}

function MsgReverseSortThreadPane()
{
  var dbview = GetDBView();
  if (dbview.sortOrder == nsMsgViewSortOrder.ascending) {
    MsgSortDescending();
  }
  else {
    MsgSortAscending();
  }
}

function MsgToggleThreaded()
{
    var dbview = GetDBView();
    dbview.viewFlags ^= nsMsgViewFlagsType.kThreadedDisplay;
    dbview.sort(dbview.sortType, dbview.sortOrder);
    UpdateSortIndicators(dbview.sortType, dbview.sortOrder);
}

function MsgSortThreaded()
{
    // Toggle if not already threaded.
    if ((GetDBView().viewFlags & nsMsgViewFlagsType.kThreadedDisplay) == 0)
        MsgToggleThreaded();
}

function MsgSortUnthreaded()
{
    // Toggle if not already unthreaded.
    if ((GetDBView().viewFlags & nsMsgViewFlagsType.kThreadedDisplay) != 0)
        MsgToggleThreaded();
}

function MsgSortAscending()
{
  var dbview = GetDBView();
  dbview.sort(dbview.sortType, nsMsgViewSortOrder.ascending);
  UpdateSortIndicators(dbview.sortType, nsMsgViewSortOrder.ascending);
}

function MsgSortDescending()
{
  var dbview = GetDBView();
  dbview.sort(dbview.sortType, nsMsgViewSortOrder.descending);
  UpdateSortIndicators(dbview.sortType, nsMsgViewSortOrder.descending);
}

function UpdateSortIndicators(sortType, sortOrder)
{
  // show the twisties if the view is threaded
  var currCol = document.getElementById("subjectCol");
  var primary = (gDBView.viewFlags & nsMsgViewFlagsType.kThreadedDisplay) && gDBView.supportsThreading;
  currCol.setAttribute("primary", primary);

  // remove the sort indicator from all the columns
  currCol = document.getElementById("threadCol");
  while (currCol) {
    currCol.removeAttribute("sortDirection");
    currCol = currCol.nextSibling;
  }

  // set the sort indicator on the column we are sorted by
  var colID = ConvertSortTypeToColumnID(sortType);
  if (colID) {
    var sortedColumn = document.getElementById(colID);
    if (sortedColumn) {
      if (sortOrder == nsMsgViewSortOrder.ascending) {
        sortedColumn.setAttribute("sortDirection","ascending");
      }
      else {
        sortedColumn.setAttribute("sortDirection","descending");
      }
			if (sortedColumn != "threadCol")
			{
			  currCol = document.getElementById("threadCol");
				if (currCol)
				{
					if (gDBView.viewFlags & nsMsgViewFlagsType.kThreadedDisplay)
						currCol.setAttribute("sortDirection", "ascending");
					else
						currCol.removeAttribute("sortDirection");
				}
			}
    }
  }
}

function IsSpecialFolderSelected(flags)
{
  var selectedFolder = GetThreadPaneFolder();
  return IsSpecialFolder(selectedFolder, flags);
}

function GetThreadTree()
{
  if (gThreadTree) return gThreadTree;
	gThreadTree = document.getElementById('threadTree');
	return gThreadTree;
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

function EnsureRowInThreadTreeIsVisible(index)
{
  if (index < 0)
    return;

  var tree = GetThreadTree();
  tree.treeBoxObject.ensureRowIsVisible(index); 
}

function RerootThreadPane()
{
  var treeView = gDBView.QueryInterface(Components.interfaces.nsITreeView);
  if (treeView)
  {
    var tree = GetThreadTree();
    tree.boxObject.QueryInterface(Components.interfaces.nsITreeBoxObject).view = treeView;
  }
}

function ThreadPaneOnLoad()
{
  var tree = GetThreadTree();

  tree.addEventListener("click",ThreadPaneOnClick,true);
  
  // The mousedown event listener below should only be added in the thread
  // pane of the mailnews 3pane window, not in the advanced search window.
  if(tree.parentNode.id == "searchResultListBox")
    return;

  tree.addEventListener("mousedown",TreeOnMouseDown,true);
}

function ThreadPaneSelectionChanged()
{
  var treeBoxObj = GetThreadTree().treeBoxObject;
  var treeSelection = treeBoxObj.selection;
  if (!gRightMouseButtonDown)
    treeBoxObj.view.selectionChanged();
}

addEventListener("load",ThreadPaneOnLoad,true);
