# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#   Seth Spitzer <sspitzer@netscape.com>
#   Scott MacGregor <mscott@mozilla.org>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var gSearchSession = null;
var gPreQuickSearchView = null;
var gSearchTimer = null;
var gViewSearchListener;
var gSearchBundle;
var gStatusBar = null;
var gSearchInProgress = false;
var gSearchInput = null;
var gClearButton = null;
var gDefaultSearchViewTerms = null;
var gQSViewIsDirty = false;
var gHighlightedMessageText = false; 

// search criteria mode values 
// Note: If you change these constants, please update the menuitem values in
// quick-search-menupopup. Note: These values are stored in localstore.rdf so we 
// can remember the users last quick search state. If you add values here, you must add
// them to the end of the list!
const kQuickSearchSubject = 0;
const kQuickSearchSender = 1;
const kQuickSearchSenderOrSubject = 2;
const kQuickSearchBody = 3;
const kQuickSearchHighlight = 4;

var gFinder = Components.classes["@mozilla.org/embedcomp/rangefind;1"].createInstance()
                        .QueryInterface(Components.interfaces.nsIFind);
// Colors for highlighting
var gHighlightColors = new Array("yellow", "lightpink", "aquamarine", 
                                 "darkgoldenrod", "darkseagreen", "lightgreen", 
                                 "rosybrown", "seagreen", "chocolate", "violet");


function SetQSStatusText(aNumHits)
{
  var statusMsg;
  // if there are no hits, it means no matches were found in the search.
  if (aNumHits == 0)
    statusMsg = gSearchBundle.getString("searchFailureMessage");
  else 
  {
    if (aNumHits == 1) 
      statusMsg = gSearchBundle.getString("searchSuccessMessage");
    else
      statusMsg = gSearchBundle.getFormattedString("searchSuccessMessages", [aNumHits]);
  }

  statusFeedback.showStatusString(statusMsg);
}

// nsIMsgSearchNotify object
var gSearchNotificationListener =
{
    onSearchHit: function(header, folder)
    {
        // XXX todo
        // update status text?
    },

    onSearchDone: function(status)
    {
        SetQSStatusText(gDBView.QueryInterface(Components.interfaces.nsITreeView).rowCount)
        statusFeedback.showProgress(0);
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

function removeGlobalListeners()
{
  removeListeners();
  gSearchSession.unregisterListener(gSearchNotificationListener); 
}

function initializeGlobalListeners()
{
  // Setup the javascript object as a listener on the search results
  gSearchSession.registerListener(gSearchNotificationListener);
}

function createQuickSearchView()
{
  if (gDBView.viewType != nsMsgViewType.eShowQuickSearchResults)  //otherwise we are already in quick search view
  {
    var treeView = gDBView.QueryInterface(Components.interfaces.nsITreeView);  //clear selection
    treeView.selection.clearSelection();
    gPreQuickSearchView = gDBView;
    CreateDBView(gDBView.msgFolder, nsMsgViewType.eShowQuickSearchResults, nsMsgViewFlagsType.kNone, gDBView.sortType, gDBView.sortOrder);
  }
}

function initializeSearchBar()
{
   createQuickSearchView();
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
   addListeners();
}

function onEnterInSearchBar()
{
   if (gSearchInput.value == "") 
   {
     if (gSearchInput.searchMode == kQuickSearchHighlight)
       removeHighlighting();
     
     if (gDBView.viewType == nsMsgViewType.eShowQuickSearchResults)
     {
       statusFeedback.showStatusString("");
       disableQuickSearchClearButton();

       if (gDefaultSearchViewTerms)
       {
         if (gQSViewIsDirty)
         {
           initializeSearchBar();
           onSearch(gDefaultSearchViewTerms);
         }
       }
       else
        restorePreSearchView();
     }
     
     gSearchInput.showingSearchCriteria = true;
     
     gQSViewIsDirty = false;
     return;
   }

   if (gSearchInput.searchMode == kQuickSearchHighlight)
     highlightMessage(true);
   else
   {
   initializeSearchBar();

   gClearButton.setAttribute("disabled", false); //coming into search enable clear button   

   ClearThreadPaneSelection();
   ClearMessagePane();

   onSearch(null);
   gQSViewIsDirty = false;
}
}

function restorePreSearchView()
{
  var selectedHdr = null;
  //save selection
  try 
  {
    selectedHdr = gDBView.hdrForFirstSelectedMessage;
  }
  catch (ex)
  {}

  //we might have to sort the view coming out of quick search
  var sortType = gDBView.sortType;
  var sortOrder = gDBView.sortOrder;
  var viewFlags = gDBView.viewFlags;
  var folder = gDBView.msgFolder;

  gDBView.close();
  gDBView = null; 

  if (gPreQuickSearchView)
  {
    gDBView = gPreQuickSearchView;

    if (sortType != gDBView.sortType || sortOrder != gDBView.sortOrder)
    {
      gDBView.sort(sortType, sortOrder);
    }
    UpdateSortIndicators(sortType, sortOrder);

    gPreQuickSearchView = null;    
  }
  else //create default view type
    CreateDBView(folder, nsMsgViewType.eShowAllThreads, viewFlags, sortType, sortOrder);

  RerootThreadPane();
   
  var scrolled = false;
  
  // now restore selection
  if (selectedHdr)
  {
    gDBView.selectMsgByKey(selectedHdr.messageKey);
    var treeView = gDBView.QueryInterface(Components.interfaces.nsITreeView);
    var selectedIndex = treeView.selection.currentIndex;
    if (selectedIndex >= 0) 
    {
      // scroll
      EnsureRowInThreadTreeIsVisible(selectedIndex);
      scrolled = true;
    }
    else
      ClearMessagePane();
  }

  // NOTE,
  // if you change the scrolling code below,
  // double check the scrolling logic in
  // msgMail3PaneWindow.js, "FolderLoaded" event code
  if (!scrolled)
  {
    // if we didn't just scroll, 
    // scroll to the first new message
    // but don't select it
    scrolled = ScrollToMessage(nsMsgNavigationType.firstNew, true, false /* selectMessage */);
    if (!scrolled) 
    {
      // if we still haven't scrolled,
      // scroll to the newest, which might be the top or the bottom
      // depending on our sort order and sort type
      if (sortOrder == nsMsgViewSortOrder.ascending) 
      {
        switch (sortType) 
        {
          case nsMsgViewSortType.byDate: 
          case nsMsgViewSortType.byId: 
          case nsMsgViewSortType.byThread: 
            scrolled = ScrollToMessage(nsMsgNavigationType.lastMessage, true, false /* selectMessage */);
            break;
        }
      }
      // if still we haven't scrolled,
      // scroll to the top.
      if (!scrolled)
        EnsureRowInThreadTreeIsVisible(0);
    }
  }
  // NOTE,
  // if you change the scrolling code above,
  // double check the scrolling logic in
  // msgMail3PaneWindow.js, "FolderLoaded" event code
}

function onSearch(aSearchTerms)
{
    RerootThreadPane();

    if (aSearchTerms)
      createSearchTermsWithList(aSearchTerms);
    else
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

function createSearchTermsWithList(aTermsArray)
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

  // add each item in termsArray to the search session

  var termsArray = aTermsArray.QueryInterface(Components.interfaces.nsISupportsArray);
  for (var i = 0; i < termsArray.Count(); i++)
    gSearchSession.appendTerm(termsArray.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgSearchTerm));
}

function createSearchTerms()
{
  var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;
  var nsMsgSearchAttrib = Components.interfaces.nsMsgSearchAttrib;
  var nsMsgSearchOp = Components.interfaces.nsMsgSearchOp;

  // create an i supports array to store our search terms 
  var searchTermsArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
  var selectedFolder = GetThreadPaneFolder();

  var searchAttrib = (IsSpecialFolder(selectedFolder, MSG_FOLDER_FLAG_SENTMAIL | MSG_FOLDER_FLAG_DRAFTS | MSG_FOLDER_FLAG_QUEUE)) ? nsMsgSearchAttrib.ToOrCC : nsMsgSearchAttrib.Sender;
  // implement | for QS
  // does this break if the user types "foo|bar" expecting to see subjects with that string?
  // I claim no, since "foo|bar" will be a hit for "foo" || "bar"
  // they just might get more false positives
  var termList = gSearchInput.value.split("|");
  for (var i = 0; i < termList.length; i ++)
  {
    // if the term is empty, skip it
    if (termList[i] == "")
      continue;

    // create, fill, and append the subject term
    var term;
    var value;

    // if our search criteria is subject or subject|sender then add a term for the subject
    if (gSearchInput.searchMode == kQuickSearchSubject || gSearchInput.searchMode == kQuickSearchSenderOrSubject)
    {
       term = gSearchSession.createTerm();
       value = term.value;
    value.str = termList[i];
    term.value = value;
    term.attrib = nsMsgSearchAttrib.Subject;
    term.op = nsMsgSearchOp.Contains;
    term.booleanAnd = false;
    searchTermsArray.AppendElement(term);
     }

     if (gSearchInput.searchMode == kQuickSearchBody)
     {
       // what do we do for news and imap users that aren't configured for offline use?
       // in these cases the body search will never return any matches. Should we try to 
       // see if body is a valid search scope in this particular case before doing the search?
       // should we switch back to a subject/sender search behind the scenes?
       term = gSearchSession.createTerm();
       value = term.value;
       value.str = termList[i];
       term.value = value;
       term.attrib = nsMsgSearchAttrib.Body;
       term.op = nsMsgSearchOp.Contains; 
       term.booleanAnd = false;
       searchTermsArray.AppendElement(term);       
     }

    // create, fill, and append the sender (or recipient) term
    if (gSearchInput.searchMode == kQuickSearchSender || gSearchInput.searchMode == kQuickSearchSenderOrSubject)
    {
    term = gSearchSession.createTerm();
    value = term.value;
    value.str = termList[i];
    term.value = value;
    term.attrib = searchAttrib;
    term.op = nsMsgSearchOp.Contains; 
    term.booleanAnd = false;
    searchTermsArray.AppendElement(term);
  }
  }

  // now append the default view criteria to the quick search so we don't lose any default
  // view information
  if (gDefaultSearchViewTerms)
  {
    var isupports = null;
    var searchTerm; 
    var termsArray = gDefaultSearchViewTerms.QueryInterface(Components.interfaces.nsISupportsArray);
    for (i = 0; i < termsArray.Count(); i++)
    {
      isupports = termsArray.GetElementAt(i);
      searchTerm = isupports.QueryInterface(Components.interfaces.nsIMsgSearchTerm);
      searchTermsArray.AppendElement(searchTerm);
    }
  }
  
  createSearchTermsWithList(searchTermsArray);
  
  // now that we've added the terms, clear out our input array
  searchTermsArray.Clear();
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
  if (gSearchInput.showingSearchCriteria)
    gSearchInput.showingSearchCriteria = false;

  // 13 == return
  if (event && event.keyCode == 13)
    onSearchInput(true);
}

function onSearchInputFocus(event)
{
  // search bar has focus, ...clear the showing search criteria flag
  if (gSearchInput.showingSearchCriteria)
  {
    gSearchInput.value = "";
    gSearchInput.showingSearchCriteria = false;
  }

  gSearchInput.select();
}

function onSearchInputBlur(event)
{ 
  if (!gSearchInput.value)
    gSearchInput.showingSearchCriteria = true;
    
  if (gSearchInput.showingSearchCriteria)
    gSearchInput.setSearchCriteriaText();
}

function onSearchInput(returnKeyHit)
{
  if (gSearchInput.showingSearchCriteria)
    return;

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
  var focusedElement = gLastFocusedElement;  //save of the last focused element so that focus can be restored
  Search("");
  focusedElement.focus();
}

function disableQuickSearchClearButton()
{
 if (gClearButton)
   gClearButton.setAttribute("disabled", true); //going out of search disable clear button
}

function ClearQSIfNecessary()
{
  GetSearchInput();

  if (gSearchInput.value == "")
    return;

  Search("");
}

function Search(str)
{
  if (gSearchInput.showingSearchCriteria)
    return;

  GetSearchInput();

  if (str != gSearchInput.value)
    gQSViewIsDirty = true; 

  gSearchInput.value = str;  //on input does not get fired for some reason
  onSearchInput(true);
}

// this notification gets generated from layout when it finishes laying out a message
// in the message pane. 
function onQuickSearchNewMsgLoaded()
{
  // if we are in highlighting mode and we have highlight text in the search box then 
  // re-highlight this new message.
  // Optimization: We'll special case Message Body quick searches and highlight those as well as find in message
  // searches.
  if ( (gSearchInput.searchMode == kQuickSearchHighlight || gSearchInput.searchMode == kQuickSearchBody)
       && gSearchInput.value && !gSearchInput.showingSearchCriteria)
  {
    highlightMessage(false);
  }
}

// helper methods for the quick search drop down menu

function changeQuickSearchMode(aMenuItem)
{
  // extract the label and set the search input to match it
  var oldSearchMode = gSearchInput.searchMode;
  gSearchInput.searchMode = aMenuItem.value;

  if (gSearchInput.value == "")
    gSearchInput.showingSearchCriteria = true;
  else if (oldSearchMode != gSearchInput.searchMode) // the search mode just changed so we need to redo the quick search
  {
    if (gHighlightedMessageText)
      removeHighlighting(); // remove any existing highlighting in the message before switching gears
      
    onEnterInSearchBar();
  }
}

// Methods to support highlighting. Most of this was shamelessly copied from the mozdev google toolbar project
function highlightMessage(removeExistingHighlighting)
{
  // remove any existing highlighting
  if (removeExistingHighlighting)
    removeHighlighting(); 

  // wanted to use selection to extend to the word
  // and compare with the found range, in order to match
  // only whole words
    
  // Save selection
  // XXX: Note to self, we may want to break on | and white space here
  // even though normal quick searches treat white spaces as signficant
  var termList = gSearchInput.value.split("|");
  for (var i = 0; i < termList.length; i++) 
      highlight(termList[i], gHighlightColors[i %10]);
  
  gHighlightedMessageText = true;
}

function removeHighlighting()
{
  if (!gHighlightedMessageText)
    return;

  var msgDocument = window.top.content;
  var doc = msgDocument.document;
  var elem = null;
  while ((elem = doc.getElementById('mail-highlight-id'))) 
  {
    var child = null;
    var docfrag = doc.createDocumentFragment();
    var next = elem.nextSibling;
    var parent = elem.parentNode;
    while((child = elem.firstChild))
      docfrag.appendChild(child);
  
    parent.removeChild(elem);
    parent.insertBefore(docfrag, next);
  }  

  gHighlightedMessageText = false;

  return;
}

function highlight(word, color)
{
  var msgDocument = window.top.content;

  var doc = msgDocument.document;
  if (!doc) 
    return;
  
  if (!("body" in doc))
    return;
  
  var body = doc.body; 
  var count = body.childNodes.length;
  searchRange = doc.createRange();
  startPt = doc.createRange();
  endPt = doc.createRange();

  var baseNode = doc.createElement("span");
  baseNode.setAttribute("style", "background-color: " + color + ";");
  baseNode.setAttribute("id", "mail-highlight-id");

  searchRange.setStart(body, 0);
  searchRange.setEnd(body, count);

  startPt.setStart(body, 0);
  startPt.setEnd(body, 0);
  endPt.setStart(body, count);
  endPt.setEnd(body, count);
  highlightText(word, baseNode);
}

// search through the message looking for occurrences of word
// and highlighting them. 
function highlightText(word, baseNode)
{
  var retRange = null;
  while((retRange = gFinder.Find(word, searchRange, startPt, endPt))) 
  {
    // Highlight
    var nodeSurround = baseNode.cloneNode(true);
    var node = highlightRange(retRange, nodeSurround);
    startPt = node.ownerDocument.createRange();
    startPt.setStart(node, node.childNodes.length);
    startPt.setEnd(node, node.childNodes.length);
  }
}

function highlightRange(range, node)
{
  var startContainer = range.startContainer;
  var startOffset = range.startOffset;
  var endOffset = range.endOffset;
  var docfrag = range.extractContents();
  var before = startContainer.splitText(startOffset);
  var parent = before.parentNode;
  node.appendChild(docfrag);
  parent.insertBefore(node, before);
  return node;
}
