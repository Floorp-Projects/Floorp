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
 * Contributor(s):
 *   Navin Gupta <naving@netscape.com> (Original author)
 * 
 */

var gSearchSession;
var gSearchTimer = null;
var gViewSearchListener;
var gNumOfSearchHits = 0;
var gSearchBundle;
var gStatusBar = null;
var gSearchInProgress = false;
var gSearchCriteria = null;
var gSearchInput = null;

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
  gSearchInput = document.getElementById('searchInput');
  gSearchCriteria =document.getElementById('searchCriteria');
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
   if (!gDBView) return;
   ClearThreadPaneSelection();
   ClearMessagePane();
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
       gSearchInProgress =false;
     }
     removeListeners();
   }
   if (gSearchInput.value == "") 
   {
     var searchView = gDBView.isSearchView;
     if (searchView)
     {
       statusFeedback.showStatusString("");
       gDBView.reloadFolderAfterQuickSearch(); // that should have initialized gDBView, now re-root the thread pane
       var outlinerView = gDBView.QueryInterface(Components.interfaces.nsIOutlinerView);
       if (outlinerView)
       {
         var outliner = GetThreadOutliner();
         outliner.boxObject.QueryInterface(Components.interfaces.nsIOutlinerBoxObject).view = outlinerView;
       }
       restoreSelection();
     }
     return;
   }

   addListeners();
   onSearch();
}

function restoreSelection()
{
  var msgToSelect = gDBView.currentlyDisplayedMessage
  if (msgToSelect != nsMsgViewIndex_None)
  {
    var outlinerView = gDBView.QueryInterface(Components.interfaces.nsIOutlinerView);
    var outlinerSelection = outlinerView.selection;
    outlinerSelection.select(msgToSelect);
    if (outlinerSelection.isSelected(msgToSelect))
    {
      if (outlinerView)
        outlinerView.selectionChanged();
      gDBView.reloadMessage();

      EnsureRowInThreadOutlinerIsVisible(msgToSelect);
      SetFocusThreadPane();
    }
  }
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
    var outlinerView = gDBView.QueryInterface(Components.interfaces.nsIOutlinerView);
    if (outlinerView)
    {
      var outliner = GetThreadOutliner();
      outliner.boxObject.QueryInterface(Components.interfaces.nsIOutlinerBoxObject).view = outlinerView;
    }
    createSearchTerms();
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
  gSearchSession.addScopeTerm(nsMsgSearchScope.offlineMail, GetThreadPaneFolder());
  var term = gSearchSession.createTerm();
  var value = term.value;
  if (value)
    value.str = gSearchInput.value;
  gSearchSession.addSearchTerm(nsMsgSearchAttrib.Subject, nsMsgSearchOp.Contains, value, false, null);
  var isSender = new String;
  isSender = gSearchCriteria.getAttribute("value");
  if (isSender.search("Sender") != -1)
    gSearchSession.addSearchTerm(nsMsgSearchAttrib.Sender, nsMsgSearchOp.Contains, value, false, null);
  else
    gSearchSession.addSearchTerm(nsMsgSearchAttrib.ToOrCC, nsMsgSearchOp.Contains, value, false, null); 
}

function onAdvancedSearch()
{
  MsgSearchMessages();
}

function onSearchStop() 
{
    gSearchSession.interruptSearch();
}

function onSearchInput(event)
{
  if (gSearchTimer) {
    clearTimeout(gSearchTimer); 
    gSearchTimer=null;
  }

  if (event && event.keyCode == 13) {
    onEnterInSearchBar();
  }
  else {
    gSearchTimer = setTimeout("onEnterInSearchBar();", 800);
  }
}

