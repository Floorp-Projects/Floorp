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
 *
 * Original Author:
 *   Navin Gupta <naving@netscape.com>
 *
 * Contributor(s):
 * 
 */

var gSearchSession;
var gSearchTimer = null;
var gViewSearchListener;
var gNumOfSearchHits = 0;
var gSearchBundle;
var gStatusBar = null;
var gSearchInProgress = false;
var gSearchInput = null;
var gClearButton = null;

// nsIMsgSearchNotify object
var gSearchNotificationListener =
{
    onSearchHit: function(header, folder)
    {
        gNumOfSearchHits++;
    },

    onSearchDone: function(status)
    {

        var statusMsg;
        // if there are no hits, it means no matches were found in the search.
        if (gNumOfSearchHits == 0) {
            statusMsg = gSearchBundle.getString("searchFailureMessage");
        }
        else 
        {
            if (gNumOfSearchHits == 1) 
                statusMsg = gSearchBundle.getString("searchSuccessMessage");
            else
                statusMsg = gSearchBundle.getFormattedString("searchSuccessMessages", [gNumOfSearchHits]);

            gNumOfSearchHits = 0;
        }

        statusFeedback.showProgress(0);
        statusFeedback.showStatusString(statusMsg);
        gStatusBar.setAttribute("mode","normal");
        gSearchInProgress = false;
    },

    onNewSearch: function()
    {
      statusFeedback.showProgress(0);
      statusFeedback.showStatusString(gSearchBundle.getString("searchingMessage"));
      gStatusBar.setAttribute("mode","undetermined");
      gSearchInProgress = true;
    }
}

function getDocumentElements()
{
  gSearchBundle = document.getElementById("bundle_search");  
  gStatusBar = document.getElementById('statusbar-icon');
  gClearButton = document.getElementById('clearButton');
  GetSearchInput();
}

function addListeners()
{
  gViewSearchListener = gDBView.QueryInterface(Components.interfaces.nsIMsgSearchNotify);
  gSearchSession.registerListener(gViewSearchListener);
}

function removeListeners()
{
  gSearchSession.unregisterListener(gViewSearchListener);
}
  
function onEnterInSearchBar()
{
   if (!gDBView) 
     return;

   if (!gSearchSession)
   {
     getDocumentElements();
     var searchSessionContractID = "@mozilla.org/messenger/searchSession;1";
     gSearchSession = Components.classes[searchSessionContractID].createInstance(Components.interfaces.nsIMsgSearchSession);
     initializeGlobalListeners();
   }
   else
   {
     if (gSearchInProgress)
     {
       onSearchStop();
       gSearchInProgress = false;
     }
     removeListeners();
   }

   if (gSearchInput.value == "") 
   {
     var searchView = gDBView.isSearchView;
     if (searchView)
     {
       statusFeedback.showStatusString("");
       disableQuickSearchClearButton();
       gDBView.reloadFolderAfterQuickSearch(); // that should have initialized gDBView
     }
     return;
   }
   else
     gClearButton.setAttribute("disabled", false); //coming into search enable clear button

   ClearThreadPaneSelection();
   ClearMessagePane();

   addListeners();
   onSearch();
}

function initializeGlobalListeners()
{
  gSearchSession.addFolderListener(folderListener);
  // Setup the javascript object as a listener on the search results
  gSearchSession.registerListener(gSearchNotificationListener);
    
}

function removeGlobalListeners()
{
  removeListeners();
  gSearchSession.removeFolderListener(folderListener);
  gSearchSession.unregisterListener(gSearchNotificationListener); 
}
function onSearch()
{
    var treeView = gDBView.QueryInterface(Components.interfaces.nsITreeView);
    if (treeView)
    {
      var tree = GetThreadTree();
      tree.boxObject.QueryInterface(Components.interfaces.nsITreeBoxObject).view = treeView;
    }
    createSearchTerms();
    gDBView.searchSession = gSearchSession;
    try
    {
      gSearchSession.search(msgWindow);
    }
    catch(ex)
    {
      dump("Search Exception\n");
    }
}

function createSearchTerms()
{
  var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;
  var nsMsgSearchAttrib = Components.interfaces.nsMsgSearchAttrib;
  var nsMsgSearchOp = Components.interfaces.nsMsgSearchOp;

  gSearchSession.clearScopes();
  var searchTerms = gSearchSession.searchTerms;
  var searchTermsArray = searchTerms.QueryInterface(Components.interfaces.nsISupportsArray);
  searchTermsArray.Clear();

  var selectedFolder = GetThreadPaneFolder();
  gSearchSession.addScopeTerm(nsMsgSearchScope.offlineMail, selectedFolder);
  var term = gSearchSession.createTerm();
  var value = term.value;

  if (value)
    value.str = gSearchInput.value;
  gSearchSession.addSearchTerm(nsMsgSearchAttrib.Subject, nsMsgSearchOp.Contains, value, false, null);

  if (IsSpecialFolder(selectedFolder, MSG_FOLDER_FLAG_SENTMAIL | MSG_FOLDER_FLAG_DRAFTS | MSG_FOLDER_FLAG_QUEUE))
    gSearchSession.addSearchTerm(nsMsgSearchAttrib.ToOrCC, nsMsgSearchOp.Contains, value, false, null); 
  else
    gSearchSession.addSearchTerm(nsMsgSearchAttrib.Sender, nsMsgSearchOp.Contains, value, false, null);
}

function onAdvancedSearch()
{
  MsgSearchMessages();
}

function onSearchStop() 
{
  gSearchSession.interruptSearch();
}

function onSearchKeyPress(event)
{
  // 13 == return
  if (event && event.keyCode == 13)
    onSearchInput(true);
}

function onSearchInput(returnKeyHit)
{
  if (gSearchTimer) {
    clearTimeout(gSearchTimer); 
    gSearchTimer = null;
  }

  // only select the text when the return key was hit
  if (returnKeyHit) {
    GetSearchInput();
    gSearchInput.select();
    onEnterInSearchBar();
  }
  else {
    gSearchTimer = setTimeout("onEnterInSearchBar();", 800);
  }
}

function onClearSearch()
{
  if (gSearchInput) 
    gSearchInput.value ="";  //on input does not get fired for some reason
  onSearchInput(true);
}

function disableQuickSearchClearButton()
{
 if (gClearButton)
   gClearButton.setAttribute("disabled", true); //going out of search disable clear button
}
