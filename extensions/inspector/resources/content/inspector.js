/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/***************************************************************
* InspectorApp -------------------------------------------------
*  The primary object that controls the Inspector application.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
*   chrome://inspector/content/jsutil/rdf/RDFArray.js
*   chrome://inspector/content/jsutil/rdf/RDFU.js
*   chrome://inspector/content/jsutil/xul/FrameExchange.js
*   chrome://inspector/content/jsutil/system/file.js
*   chrome://inspector/content/search/inSearchService.js
*   chrome://inspector/content/search/inSearchModule.js
****************************************************************/

//////////// global variables /////////////////////

var inspector;

//////////// global constants ////////////////////

const kSearchRegURL        = "resource:///res/inspector/search-registry.rdf";

const kWindowMediatorIID   = "@mozilla.org/rdf/datasource;1?name=window-mediator";
const kClipboardHelperCID  = "@mozilla.org/widget/clipboardhelper;1";
const kGlobalClipboard     = Components.interfaces.nsIClipboard.kGlobalClipboard
const nsIWebNavigation = Components.interfaces.nsIWebNavigation;

//////////////////////////////////////////////////

window.addEventListener("load", InspectorApp_initialize, false);
window.addEventListener("unload", InspectorApp_destroy, false);

function InspectorApp_initialize()
{
  inspector = new InspectorApp();
  inspector.initialize(window.arguments && window.arguments.length > 0 ? window.arguments[0] : null);
}

function InspectorApp_destroy()
{
  inspector.destroy();
}

////////////////////////////////////////////////////////////////////////////
//// class InspectorApp

function InspectorApp()
{
}

InspectorApp.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization

  mSearchService: null,
  mShowBrowser: false,
  mClipboardHelper: null,
  
  get document() { return this.mDocPanel.viewer.subject },
  get searchRegistry() { return this.mSearchService },
  
  initialize: function(aTarget)
  {
    this.mInitTarget = aTarget;
    
    this.initSearch();

    var el = document.getElementById("bxBrowser");
    el.addEventListener("load", BrowserLoadListener, true);

    this.toggleBrowser(true, false);
    this.toggleSearch(true, false);

    this.mClipboardHelper = XPCU.getService(kClipboardHelperCID, "nsIClipboardHelper");

    this.mPanelSet = document.getElementById("bxPanelSet");
    this.mPanelSet.addObserver("panelsetready", this, false);
    this.mPanelSet.initialize();
  },

  destroy: function()
  {
    InsUtil.persistAll("bxDocPanel");
    InsUtil.persistAll("bxObjectPanel");
  },
  
  ////////////////////////////////////////////////////////////////// //////////
  //// Viewer Panels

  initViewerPanels: function()
  {
    this.mDocPanel = this.mPanelSet.getPanel(0);
    this.mDocPanel.addObserver("subjectChange", this, false);
    this.mObjectPanel = this.mPanelSet.getPanel(1);

    if (this.mInitTarget) {
      if (this.mInitTarget.nodeType == 9)
        this.setTargetDocument(this.mInitTarget);
      else if (this.mInitTarget.nodeType == 1) {
        this.setTargetDocument(this.mInitTarget.ownerDocument);
        this.mDocPanel.params = this.mInitTarget;
      }
      this.mInitTarget = null;
    }
  },

  onEvent: function(aEvent)
  {
    switch (aEvent.type) {
      case "panelsetready":
        this.initViewerPanels();
        break;
      case "subjectChange":
        if (aEvent.target == this.mDocPanel.viewer &&
            aEvent.subject && "location" in aEvent.subject) {
          this.locationText = aEvent.subject.location; // display document url
        }
    }
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// UI Commands

  showOpenURLDialog: function()
  {
    var defaultURL = "chrome://inspector/content/tests/allskin.xul"; // for testing
    var url = prompt("Enter a URL:", defaultURL);
    if (url) {
      this.gotoURL(url);
    }
  },

  showPrefsDialog: function()
  {
    goPreferences("inspector.xul", "chrome://inspector/content/prefs/pref-inspector.xul", "inspector");
  },
  
  runSearch: function()
  {
    var path = null; // TODO: should persist last path chosen in a pref
    var file = FilePickerUtils.pickFile("Find Search File", path, ["filterXML"], "Open");
    if (file) {
      var url = file.URL;
      // XX temporary until 56354 is fixed
      url = url.replace("file://", "file:///");      
      this.startSearchModule(url);
    }
  },
  
  toggleBrowser: function(aExplicit, aValue, aSetOnly)
  {
    var val = aExplicit ? aValue : !this.mShowBrowser;
    this.mShowBrowser = val;
    if (!aSetOnly)
      this.openSplitter("Browser", val);
    var cmd = document.getElementById("cmdToggleBrowser");
    cmd.setAttribute("checked", val);
  },

  toggleSearch: function(aExplicit, aValue, aSetOnly)
  {
    var val = aExplicit ? aValue : !this.mShowSearch;
    this.mShowSearch = val;
    if (!aSetOnly)
      this.openSplitter("Search", val);
    var cmd = document.getElementById("cmdToggleSearch");
    cmd.setAttribute("checked", val);
  },

  openSplitter: function(aName, aTruth)
  {
    var splitter = document.getElementById("spl" + aName);
    if (aTruth)
      splitter.open();
    else
      splitter.close();
  },

/*
  viewSearchItem: function()
  {
    if (this.mCurrentSearch.canViewItems)
      window.openDialog("chrome://inspector/content/utilWindow.xul", "viewItem", "chrome,resizable");
  },

  doViewSearchItem: function(aWindow)
  {
    var idx = this.getSelectedSearchIndex();
    var el = this.mCurrentSearch.viewItemAt(idx);

    aWindow.title = this.mCurrentSearch.getItemDescription(idx);
    aWindow.document.getElementById("bxCenter").appendChild(el);
  },

  editSearchItem: function()
  {
  },
  
  onSearchTreeClick: function(aEvent)
  {
    if (aEvent.detail == 2) { // double click
      this.viewSearchItem();
    }
  },
*/
  copySearchItemLine: function()
  {
    var mod = this.mSearchService.currentModule;
    var idx = this.mSearchService.getSelectedIndex(0);
    var text = mod.getItemText(idx);
    this.mClipboardHelper.writeStringToClipboard(text, kGlobalClipboard);
  },

  copySearchItemAll: function()
  {
    var text = this.getAllSearchItemText();
    this.mClipboardHelper.writeStringToClipboard(text, kGlobalClipboard);
  },

  saveSearchItemText: function()
  {
    var target = FilePickerUtils.pickFile("Save Results As", null, ["filterAll", "filterText"], "Save");

    var text = this.getAllSearchItemText();

    var file = new File(target.path);
    file.open('w');
    file.write(text);
    file.close();
  },

  getAllSearchItemText: function()
  {
    var mod = this.mSearchService.currentModule;
    var len = mod.resultCount;
    var text = "";
    for (var i = 0; i < len; i++) {
      text += mod.getItemText(i) + "\r";
    }

    return text;
  },

  exit: function()
  {
    window.close();
    // Todo: remove observer service here
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Navigation
  
  gotoTypedURL: function()
  {
    var url = document.getElementById("tfURLBar").value;
    this.gotoURL(url);
  },

  gotoURL: function(aURL, aNoSaveHistory)
  {
    this.mPendingURL = aURL;
    this.mPendingNoSave = aNoSaveHistory;
    this.browseToURL(aURL);
    this.toggleBrowser(true, true);
  },

  browseToURL: function(aURL)
  {
    this.webNavigation.loadURI(aURL, nsIWebNavigation.LOAD_FLAGS_NONE);
  },

  goToWindow: function(aMenuitem)
  {
    this.setTargetWindowById(aMenuitem.id);
  },

  setTargetWindowById: function(aResId)
  {
    var windowManager = XPCU.getService(kWindowMediatorIID, "nsIWindowMediator");
    var win = windowManager.getWindowForResource(aResId);

    if (win) {
      this.setTargetWindow(win);
      this.toggleBrowser(true, false);
    } else
      alert("Unable to switch to window.");
  },

  setTargetWindow: function(aWindow)
  {
    this.setTargetDocument(aWindow.document);
  },

  setTargetDocument: function(aDoc)
  {
    this.mDocPanel.subject = aDoc;
    this.mObjectPanel.subject = null;
  },

  get webNavigation()
  {
    var browser = document.getElementById("ifBrowser");
    return browser.webNavigation;
  },

  ////////////////////////////////////////////////////////////////////////////
  //// UI Labels getters and setters

  get locationText() { return document.getElementById("tfURLBar").value; },
  set locationText(aText) { document.getElementById("tfURLBar").value = aText; },

  get statusText() { return document.getElementById("txStatus").value; },
  set statusText(aText) { document.getElementById("txStatus").setAttribute("value", aText); },

  get progress() { return document.getElementById("pmStatus").getAttribute("value"); },
  set progress(aPct) { document.getElementById("pmStatus").setAttribute("value", aPct); },

  get searchTitle(aTitle) { return document.getElementById("splSearch").label; },
  set searchTitle(aTitle)
  {
    var splitter = document.getElementById("splSearch");
    splitter.setAttribute("label", "Search" + (aTitle ? " - " + aTitle : ""));
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Document Loading 

  documentLoaded: function()
  {
    this.setTargetWindow(_content);

    var url = this.webNavigation.currentURI.spec;
    
    // put the url into the urlbar
    this.locationText = url;

    // add url to the history, unless explicity told not to
    if (!this.mPendingNoSave)
      this.addToHistory(url);

    this.mPendingURL = null;
    this.mPendingNoSave = null;
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Search 
  
  initSearch: function()
  {
    var ss = new inSearchService();
    this.mSearchService = ss;
    
    ss.addSearchObserver(this);
    ss.resultsTree = document.getElementById("trSearch");
    ss.contextMenu = document.getElementById("ppSearchResults");
    ss.contextMenuInsertPt = document.getElementById("ppSearchResults-insertion");
    ss.contextMenuInsert = inSearchService.INSERT_BEFORE;
  },

  startSearchModule: function(aModuleURL)
  {
    this.mSearchService.startModule(aModuleURL);
  },
  
  clearSearchResults: function()
  {
    this.searchTitle = null;
    this.statusText = "";
    this.progress = 0;
    
    this.mSearchService.clearSearch();
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// interface inISearchObserver

  onSearchStart: function(aModule) 
  {
    this.searchTitle = aModule.title;
    this.toggleSearch(true, true);
  },

  onSearchResult: function(aModule)
  {
    if (aModule.isPastMilestone) {
      this.statusText = "Searching - " + aModule.resultCount + " results found."; // XXX localize
      this.progress = aModule.progressPercent;
    }
  },

  onSearchEnd: function(aModule, aResult)
  {
    var diff = Math.round(aModule.elapsed / 100) / 10;
    this.progress = 100;
    this.statusText = "Search complete - " + aModule.resultCount + " results found ("+diff+"s)"; // XXX localize
  },

  onSearchError: function(aModule, aMessage)
  {
    alert("Unable to complete this search due to the following error:\n" + aMessage);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// History 

  addToHistory: function(aURL)
  {
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Uncategorized
  
  get isViewingContent() { return this.mPanelSet.getPanel(0).subject != null; },
  
  fillInTooltip: function(tipElement)
  {
    var retVal = false;
    var textNode = document.getElementById("txTooltip");
    if (textNode) {
      try {  
        var tipText = tipElement.getAttribute("tooltiptext");
        if (tipText != "") {
          textNode.setAttribute("value", tipText);
          retVal = true;
        }
      }
      catch (e) { }
    }
    
    return retVal;
  },

  initPopup: function(aPopup)
  {
    var items = aPopup.getElementsByTagName("menuitem");
    var js, fn, item;
    for (var i = 0; i < items.length; i++) {
      item = items[i];
      fn = "isDisabled" in item ? item.isDisabled : null;
      if (!fn) {
        js = item.getAttribute("isDisabled");
        if (js) {
          fn = new Function(js);
          item.isDisabled = fn;
        } else {
          item.isDisabled = null; // to prevent annoying "strict" warning messages
        }
      } 
      if (fn) {
        item.setAttribute("disabled", item.isDisabled());
      }

      fn = null;
    }
  },

  emptyChildren: function(aNode)
  {
    while (aNode.childNodes.length > 0) {
      aNode.removeChild(aNode.lastChild);
    }
  },
  
  onSplitterOpen: function(aSplitter)
  {
    if (aSplitter.id == "splBrowser") {
      this.toggleBrowser(true, aSplitter.isOpened, true);
    } else if (aSplitter.id == "splSearch") {
      this.toggleSearch(true, aSplitter.isOpened, true);
    }
  },
  
  // needed by overlayed commands from viewer to get references to a specific
  // viewer object by name
  getViewer: function(aUID)
  {
    return this.mPanelSet.registry.getViewerByUID(aUID);
  }
  
};

////////////////////////////////////////////////////////////////////////////
//// event listeners

function BrowserLoadListener(aEvent) 
{
  inspector.documentLoaded();
}

function UtilWindowOpenListener(aWindow)
{
  inspector.doViewSearchItem(aWindow);
}
