/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Jason Barnabe <jason_barnabe@fastmail.fm>
 *   Shawn Wilsher <me@shawnwilsher.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/***************************************************************
* InspectorApp -------------------------------------------------
*  The primary object that controls the Inspector application.
****************************************************************/

//////////// global variables /////////////////////

var inspector;

//////////// global constants ////////////////////

const kSearchRegURL        = "resource:///res/inspector/search-registry.rdf";

const kClipboardHelperCID  = "@mozilla.org/widget/clipboardhelper;1";
const kPromptServiceCID    = "@mozilla.org/embedcomp/prompt-service;1";
const nsIWebNavigation     = Components.interfaces.nsIWebNavigation;
const nsIDocShellTreeItem  = Components.interfaces.nsIDocShellTreeItem;
const nsIDocShell          = Components.interfaces.nsIDocShell;

//////////////////////////////////////////////////

window.addEventListener("load", InspectorApp_initialize, false);
window.addEventListener("unload", InspectorApp_destroy, false);

function InspectorApp_initialize()
{
  inspector = new InspectorApp();

  // window.arguments may be either a string or a node.
  // If passed via a command line handler, it will be a uri string.
  // If passed via navigator hooks, it will be a dom node to inspect.
  var initNode, initURI;
  if (window.arguments && window.arguments.length) {
    if (typeof window.arguments[0] == "string") {
      initURI = window.arguments[0];
    }
    else if (window.arguments[0] instanceof Components.interfaces.nsIDOMNode) {
      initNode = window.arguments[0];
    }
  }
  inspector.initialize(initNode, initURI);

  // Disables the Mac Specific VK_BACK for delete key for non-mac systems
  if (!/Mac/.test(navigator.platform)) {
    document.getElementById("keyEditDeleteMac")
            .setAttribute("disabled", "true");
  }
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
  mPromptService: null,
  
  get document() { return this.mDocPanel.viewer.subject },
  get searchRegistry() { return this.mSearchService },
  get panelset() { return this.mPanelSet; },
  
  initialize: function(aTarget, aURI)
  {
    this.mInitTarget = aTarget;
    
    //this.initSearch();

    var el = document.getElementById("bxBrowser");
    el.addEventListener("pageshow", BrowserPageShowListener, true);

    this.setBrowser(false, true);
    //this.setSearch(false, true);

    this.mClipboardHelper = XPCU.getService(kClipboardHelperCID, "nsIClipboardHelper");
    this.mPromptService = XPCU.getService(kPromptServiceCID, "nsIPromptService");

    this.mPanelSet = document.getElementById("bxPanelSet");
    this.mPanelSet.addObserver("panelsetready", this, false);
    this.mPanelSet.initialize();

    // check if accessibility service is available
    var cmd = document.getElementById("cmd:toggleAccessibleNodes");
    if (cmd) {
      if (!("@mozilla.org/accessibilityService;1" in Components.classes))
        cmd.setAttribute("disabled", "true");
    }

    if (aURI) {
      this.gotoURL(aURI);
    }
  },

  destroy: function()
  {
    InsUtil.persistAll("bxDocPanel");
    InsUtil.persistAll("bxObjectPanel");
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// Viewer Panels
  
  initViewerPanels: function()
  {
    this.mDocPanel = this.mPanelSet.getPanel(0);
    this.mDocPanel.addObserver("subjectChange", this, false);
    this.mObjectPanel = this.mPanelSet.getPanel(1);

    if (this.mInitTarget) {
      if (this.mInitTarget.nodeType == Node.DOCUMENT_NODE)
        this.setTargetDocument(this.mInitTarget);
      else if (this.mInitTarget.nodeType == Node.ELEMENT_NODE) {
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

          var docTitle = aEvent.subject.title || aEvent.subject.location; 
          if (/Mac/.test(navigator.platform)) {
            document.title = docTitle;
          } else {
            document.title = docTitle + " - " + 
              document.documentElement.getAttribute("title");
          }
        }
        break;
      }
    }
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// UI Commands

  doViewerCommand: function(aCommand)
  {
    this.mPanelSet.execCommand(aCommand);
  },
  
  showOpenURLDialog: function()
  {
    var bundle = this.mPanelSet.stringBundle;
    var msg = bundle.getString("inspectURL.message");
    var title = bundle.getString("inspectURL.title");
    var url = { value: "http://" };
    var dummy = { value: false };
    var go = this.mPromptService.prompt(window, title, msg, url, null, dummy);
    if (go) {
      this.gotoURL(url.value);
    }
  },

  showPrefsDialog: function()
  {
    goPreferences("advancedItem", "chrome://inspector/content/prefs/pref-inspector.xul", "inspector");
  },
  
  toggleBrowser: function(aToggleSplitter)
  {
    this.setBrowser(!this.mShowBrowser, aToggleSplitter)
  },

  setBrowser: function(aValue, aToggleSplitter)
  {
    this.mShowBrowser = aValue;
    if (aToggleSplitter)
      this.openSplitter("Browser", aValue);
    var cmd = document.getElementById("cmdToggleBrowser");
    cmd.setAttribute("checked", aValue);
  },

  toggleSearch: function(aToggleSplitter)
  {
    this.setSearch(!this.mShowSearch, aToggleSplitter);
  },

  setSearch: function(aValue, aToggleSplitter)
  {
    this.mShowSearch = aValue;
    if (aToggleSplitter)
      this.openSplitter("Search", aValue);
    var cmd = document.getElementById("cmdToggleSearch");
    cmd.setAttribute("checked", aValue);
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
  XXXcaa
  The following code needs to evaluated.  What does it do?  Why does nobody
  call it?  If deemed necessary, turn it back on with the right callers,
  and localize it.
 */
/*
  runSearch: function()
  {
    var path = null; // TODO: should persist last path chosen in a pref
    var file = FilePickerUtils.pickFile("Find Search File", path, ["filterXML"], "Open");
    if (file) {
      var ioService = XPCU.getService("@mozilla.org/network/io-service;1","nsIIOService");
      var fileHandler = XPCU.QI(ioService.getProtocolHandler("file"), "nsIFileProtocolHandler");

      var url = fileHandler.getURLSpecFromFile(file);

      this.startSearchModule(url);
    }
  },

  viewSearchItem: function()
  {
    if (this.mCurrentSearch.canViewItems)
      window.openDialog("chrome://inspector/content/utilWindow.xul", "viewItem", "chrome,resizable");
  },

  doViewSearchItem: function(aWindow)
  {
    var idx = this.getSelectedSearchIndex();
    var el = this.mCurrentSearch.viewItemAt(idx);

    aWindow.document.title = this.mCurrentSearch.getItemDescription(idx);
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

  copySearchItemLine: function()
  {
    var mod = this.mSearchService.currentModule;
    var idx = this.mSearchService.getSelectedIndex(0);
    var text = mod.getItemText(idx);
    this.mClipboardHelper.copyString(text);
  },

  // XXX what is this?  It doesn't seem to get called from anywhere?
  copySearchItemAll: function()
  {
    var text = this.getAllSearchItemText();
    this.mClipboardHelper.copyString(text);
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
*/

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
    this.setBrowser(true, true);
  },

  browseToURL: function(aURL)
  {
    try {
      this.webNavigation.loadURI(aURL, nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
    }
    catch(ex) {
      // nsIWebNavigation.loadURI will spit out an appropriate user prompt, so
      // we don't need to do anything here.  See nsDocShell::DisplayLoadError()
    }
  },

 /** 
  * Creates the submenu for Inspect Content/Chrome Document
  */
  showInspectDocumentList: function showInspectDocumentList(aEvent, aChrome)
  {
    const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    var menu = aEvent.target;
    var ww = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    var windows = ww.getXULWindowEnumerator(null);
    var docs = [];

    while (windows.hasMoreElements()) {
      try {
        // Get the window's main docshell
        var windowDocShell = windows.getNext()
                            .QueryInterface(Components.interfaces.nsIXULWindow)
                            .docShell;
        this.appendContainedDocuments(docs, windowDocShell,
                                      aChrome ? nsIDocShellTreeItem.typeChrome
                                              : nsIDocShellTreeItem.typeContent);
      }
      catch (ex) {
        // We've failed with this window somehow, but we're catching the error
        // so the others will still work
        Components.utils.reportError(ex);
      }
    }

    // Clear out any previous menu
    this.emptyChildren(menu);

    // Now add what we found to the menu
    if (!docs.length) {
      var noneMenuItem = document.createElementNS(XULNS, "menuitem");
      noneMenuItem.setAttribute("label",
                                this.mPanelSet.stringBundle
                                    .getString("inspectWindow.noDocuments.message"));
      noneMenuItem.setAttribute("disabled", true);
      menu.appendChild(noneMenuItem);
    } else {
      for (var i = 0; i < docs.length; i++)
        this.addInspectDocumentMenuItem(menu, docs[i], i + 1);
    }
  },

 /** 
  * Appends to the array the documents contained in docShell (including the passed
  * docShell itself).
  *
  * @param array the array to append to
  * @param docShell the docshell to look for documents in
  * @param type one of the types defined in nsIDocShellTreeItem
  */
  appendContainedDocuments: function appendContainedDocuments(array, docShell, type)
  {
    // Load all the window's content docShells
    var containedDocShells = docShell.getDocShellEnumerator(type, 
                                      nsIDocShell.ENUMERATE_FORWARDS);
    while (containedDocShells.hasMoreElements()) {
      try {
        // Get the corresponding document for this docshell
        var childDoc = containedDocShells.getNext().QueryInterface(nsIDocShell)
                                         .contentViewer.DOMDocument;

        // Ignore the DOM Insector's browser docshell if it's not being used
        if (docShell.contentViewer.DOMDocument.location.href != document.location.href ||
            childDoc.location.href != "about:blank") {
          array.push(childDoc);
        }
      }
      catch (ex) {
        // We've failed with this document somehow, but we're catching the error so
        // the others will still work
        dump(ex + "\n");
      }
    }
  },

 /** 
  * Creates a menu item for Inspect Document.
  *
  * @param doc document related to this menu item
  * @param docNumber the position of the document
  */
  addInspectDocumentMenuItem: function addInspectDocumentMenuItem(parent, doc, docNumber)
  {
    const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    var menuItem = document.createElementNS(XULNS, "menuitem");
    menuItem.doc = doc;
    // Use the URL if there's no title
    var title = doc.title || doc.location.href;
    // The first ten items get numeric access keys
    if (docNumber < 10) {
      menuItem.setAttribute("label", docNumber + " " + title);
      menuItem.setAttribute("accesskey", docNumber);
    } else {
      menuItem.setAttribute("label", title);
    }
    parent.appendChild(menuItem);
  },

  setTargetWindow: function(aWindow)
  {
    this.setTargetDocument(aWindow.document);
  },

  setTargetDocument: function(aDoc)
  {
    this.mDocPanel.subject = aDoc;
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
  set statusText(aText) { document.getElementById("txStatus").value = aText; },

  get progress() { return document.getElementById("pmStatus").value; },
  set progress(aPct) { document.getElementById("pmStatus").value = aPct; },

/*
  XXXcaa -- What is this?


  get searchTitle(aTitle) { return document.getElementById("splSearch").label; },
  set searchTitle(aTitle)
  {
    var splitter = document.getElementById("splSearch");
    splitter.setAttribute("label", "Search" + (aTitle ? " - " + aTitle : ""));
  },
*/
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
/*
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

*/

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
        if (item.isDisabled())
          item.setAttribute("disabled", "true");
        else
          item.removeAttribute("disabled");
      }

      fn = null;
    }
  },

  emptyChildren: function(aNode)
  {
    while (aNode.hasChildNodes()) {
      aNode.removeChild(aNode.lastChild);
    }
  },

  onSplitterOpen: function(aSplitter)
  {
    if (aSplitter.id == "splBrowser") {
      this.setBrowser(aSplitter.isOpened, false);
    } else if (aSplitter.id == "splSearch") {
      this.setSearch(aSplitter.isOpened, false);
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

function BrowserPageShowListener(aEvent) 
{
  // since we will also get pageshow events for frame documents,
  // make sure we respond to the top-level document load
  if (aEvent.target.defaultView == _content)
    inspector.documentLoaded();
}

function UtilWindowOpenListener(aWindow)
{
  inspector.doViewSearchItem(aWindow);
}
