/***************************************************************
* InspectorApp -------------------------------------------------
*  The primary object that controls the Inspector application.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/ViewerRegistry.js
*   chrome://inspector/content/Flasher.js
*   chrome://inspector/content/search/inSearchService.js
*   chrome://inspector/content/search/inSearchModule.js
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
*   chrome://inspector/content/jsutil/rdf/RDFArray.js
*   chrome://inspector/content/jsutil/rdf/RDFU.js
*   chrome://inspector/content/jsutil/xul/FrameExchange.js
*   chrome://inspector/content/jsutil/system/file.js
****************************************************************/

//////////// global variables /////////////////////

var inspector;

var kHistoryURL            = "chrome/inspector/inspector-history.rdf";
var kViewerRegURL          = "chrome/inspector/viewer-registry.rdf";
var kSearchRegURL          = "chrome/inspector/search-registry.rdf";

//////////// global constants ////////////////////

const kInstallDirId = "CurProcD";

const kFlasherCID          = "@mozilla.org/inspector/flasher;1"
const kWindowMediatorIID   = "@mozilla.org/rdf/datasource;1?name=window-mediator";
const kObserverServiceIID  = "@mozilla.org/observer-service;1";
const kDirServiceCID       = "@mozilla.org/file/directory_service;1"

const kClipboardHelperCID  = "@mozilla.org/widget/clipboardhelper;1";
const kGlobalClipboard     = Components.interfaces.nsIClipboard.kGlobalClipboard

const nsIWebNavigation = Components.interfaces.nsIWebNavigation;

//////////////////////////////////////////////////

window.addEventListener("load", InspectorApp_initialize, false);

function InspectorApp_initialize()
{
  inspector = new InspectorApp();
  inspector.initialize();
}

////////////////////////////////////////////////////////////////////////////
//// class InspectorApp

function InspectorApp() // implements inIViewerPaneContainer
{
  // XXX HACK nsIFile.URL not implemented on unix and mac
  this.mInstallURL = "file://"+this.getSpecialDirectory(kInstallDirId).path+'/';
  //this.mInstallURL = this.getSpecialDirectory(kInstallDirId).URL;

  kHistoryURL = this.prependBaseURL(kHistoryURL);
  kViewerRegURL = this.prependBaseURL(kViewerRegURL);
  kSearchRegURL = this.prependBaseURL(kSearchRegURL);
 
  this.mUFlasher = XPCU.createInstance(kFlasherCID, "inIFlasher");
}

InspectorApp.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization

  mShell: null,
  mHistory: null,
  mViewerReg: null,
  mSearchService: null,
  mPaneCount: 2,
  mCurrentViewer: null,
  mCurrentWindow: null,
  mShowBrowser: false,
  mInstallURL: null,
  mFlashSelected: null,
  mFlashes: 0,
  mInitialized: false,
  mFlasher: null,
  mIsViewingContent: false,
  mClipboardHelper: null,
  
  get document() { return this.mDocViewerPane.viewer.viewee },
  get searchRegistry() { return this.mSearchService },
  
  initialize: function()
  {
    this.initPrefs();
    this.loadViewerRegistry();
    this.loadHistory();
    this.initSearch();

    var el = document.getElementById("bxBrowser");
    el.addEventListener("load", BrowserLoadListener, true);

    this.toggleBrowser(true, false);
    this.toggleSearch(true, false);
    this.setFlashSelected(PrefUtils.getPref("inspector.blink.on"));

    this.mClipboardHelper = XPCU.getService(kClipboardHelperCID, "nsIClipboardHelper");
  },

  initViewerPanes: function()
  {
    var elPane = document.getElementById("bxDocPane");
    this.mDocViewerPane = new ViewerPane();
    this.mDocViewerPane.initialize("Document", this, elPane, this.mViewerReg);

    elPane = document.getElementById("bxObjectPane");
    this.mObjViewerPane = new ViewerPane();
    this.mObjViewerPane.initialize("Object", this, elPane, this.mViewerReg);

    this.setAllViewerCmdAttributes("disabled", "true");
  },

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewerPaneContainer

  get viewerPaneCount() { return this.mPaneCount; },

  onViewerChanged: function(aPane, aOldViewer, aOldEntry, aNewViewer, aNewEntry)
  {
    var ids, el, i;

    if (!this.mInitialized) {
      this.mIsViewingContent = true;
      var bx = document.getElementById("bxInspectorPanes");
      bx.setAttribute("hide", "false");
    } else {
      this.mInitialized = true;
    }

    this.mViewerReg.cacheViewer(aNewViewer, aNewEntry);

    // disable all commands for for the old viewer
    if (aOldViewer)
      this.setViewerCmdAttribute(aOldEntry, "disabled", "true");

    // enable all commands for for the new viewer
    if (aNewViewer)
      this.setViewerCmdAttribute(aNewEntry, "disabled", "false");
  },
  
  onVieweeChanged: function(aPane, aNewObject)
  {
    if (aPane == this.mDocViewerPane) {
      this.mObjViewerPane.viewee = aNewObject;
      if (this.mFlashSelected)
        this.flashElement(aNewObject, this.mCurrentWindow);
    }
  },

  getViewerPaneAt: function(aIndex)
  {
    if (aIndex == 0) {
      return this.mDocViewerPane;
    } else if (aIndex == 1) {
      return this.mObjViewerPane;
    } else {
      return null;
    }
  },

  setCommandAttribute: function(aCmdId, aAttribute, aValue)
  {
    var cmd = document.getElementById(aCmdId);
    if (cmd)
      cmd.setAttribute(aAttribute, aValue);
  },

  getCommandAttribute: function(aCmdId, aAttribute)
  {
    var cmd = document.getElementById(aCmdId);
    return cmd ? cmd.getAttribute(aAttribute) : null;
  },

  ////////////////////////////////////////////////////////////////////////////
  //// UI Commands

  gotoTypedURL: function()
  {
    var url = document.getElementById("tfURLBar").value;
    this.gotoURL(url);
  },

  goToWindow: function(aMenuitem)
  {
    this.setTargetWindowById(aMenuitem.id);
  },

  goToHistoryItem: function(aMenuitem)
  {
    var url = aMenuitem.getAttribute("value");
    this.gotoURL(url, true);
  },

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

  toggleFlashSelected: function(aExplicit, aValue)
  {
    var val = aExplicit ? aValue : !this.mFlashSelected;
    PrefUtils.setPref("inspector.blink.on", val);
    this.setFlashSelected(val);
  },

  setFlashSelected: function(aValue)
  {
    this.mFlashSelected = aValue;
    var cmd = document.getElementById("cmdFlashSelected");
    cmd.setAttribute("checked", aValue);
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

  showNotes: function()
  {
    window.open("http://www.joehewitt.com/inspector/releasenotes.html");
  },

  showAbout: function()
  {
    alert("Document Inspector - a debugging tool that's fun for the whole family!");
  },

  exit: function()
  {
    window.close();
    // Todo: remove observer service here
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Navigation
  
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

  setTargetWindowById: function(aResId)
  {
    var windowManager = XPCU.getService(kWindowMediatorIID, "nsIWindowMediator");
    var win = windowManager.getWindowForResource(aResId);

    if (win) {
      this.setTargetWindow(win);
      this.setURLText("window: " + aResId);
      this.toggleBrowser(true, false);
    } else
      alert("Unable to switch to window.");
  },

  setTargetWindow: function(aWindow)
  {
    this.mCurrentWindow = aWindow;
    this.setTargetDocument(aWindow.document);
  },

  setTargetDocument: function(aDoc)
  {
    this.mDocViewerPane.viewee = aDoc;

    var doc = this.mDocViewerPane.viewee;
  },

  // turn these into getters and setters

  setURLText: function(aText)
  {
    document.getElementById("tfURLBar").value = aText;
  },

  setStatus: function(aText)
  {
    document.getElementById("txStatus").setAttribute("value", aText);
  },

  getStatus: function(aText)
  {
    return document.getElementById("txStatus").getAttribute("value");
  },

  setProgress: function(aPercent)
  {
    document.getElementById("pmStatus").setAttribute("value", aPercent);
  },

  setSearchTitle: function(aTitle)
  {
    var splitter = document.getElementById("splSearch");
    splitter.setAttribute("value", "Search" + (aTitle ? " - " + aTitle : ""));
  },

  get webNavigation()
  {
    var browser = document.getElementById("ifBrowser");
    return browser.webNavigation;
  },

  
  ////////////////////////////////////////////////////////////////////////////
  //// Document Loading 

  documentLoaded: function()
  {
    this.setTargetWindow(_content);
  
    // add url to the history, unless explicity told not to
    if (!this.mPendingNoSave)
      this.addToHistory(this.mPendingURL);
    // put the url into the urlbar
    this.setURLText(this.mPendingURL);

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
    this.setSearchTitle(null);
    this.setStatus("");
    this.setProgress(0);
    
    this.mSearchService.clearSearch();
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// interface inISearchObserver

  onSearchStart: function(aModule) 
  {
    this.setSearchTitle(aModule.title);
    this.toggleSearch(true, true);
  },

  onSearchResult: function(aModule)
  {
    if (aModule.isPastMilestone) {
      this.setStatus("Searching - " + aModule.resultCount + " results found.");
      this.setProgress(aModule.progressPercent);
    }
  },

  onSearchEnd: function(aModule, aResult)
  {
    var diff = Math.round(aModule.elapsed / 100) / 10;
    this.setProgress(100);
    this.setStatus("Search complete - " + aModule.resultCount + " results found ("+diff+"s)");
  },

  onSearchError: function(aModule, aMessage)
  {
    alert("Unable to complete this search due to the following error:\n" + aMessage);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Viewer Registry Interaction

  loadViewerRegistry: function()
  {
    this.mViewerReg = new ViewerRegistry();
    this.mViewerReg.load(kViewerRegURL, this);
  },

  onViewerRegistryLoad: function()
  {
    this.initViewerPanes();
  },
  
  onViewerRegistryLoadError: function(aStatus, aErrorMsg)
  {
    // fatal error
    alert("Unable to load viewer registry.");
    this.exit();
  },

  getViewer: function(aUID)
  {
    return this.mViewerReg.getViewerByUID(aUID);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// History 

  loadHistory: function()
  {
    RDFU.loadDataSource(kHistoryURL, HistoryLoadListener);
  },

  prepareHistory: function(aDS)
  {
    this.mHistory = RDFArray.fromContainer(aDS, "inspector:history", kInspectorNSURI);
    var mppHistory = document.getElementById("mppHistory");
    mppHistory.database.AddDataSource(aDS);
    mppHistory.builder.rebuild();
  },

  addToHistory: function(aURL)
  {
    this.mHistory.add({ URL: aURL});
    this.mHistory.save();
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Preferences

  initPrefs: function()
  {
    PrefUtils.addObserver("inspector", PrefChangeObserver);
  },
  
  onPrefChanged: function(aName)
  {
    if (aName == "inspector.blink.on")
      this.setFlashSelected(PrefUtils.getPref("inspector.blink.on"));

    if (this.mFlasher) {
      if (aName == "inspector.blink.border-color") {
        this.mFlasher.color = PrefUtils.getPref("inspector.blink.border-color");
      } else if (aName == "inspector.blink.border-width") {
        this.mFlasher.thickness = PrefUtils.getPref("inspector.blink.border-width");
      } else if (aName == "inspector.blink.duration") {
        this.mFlasher.duration = PrefUtils.getPref("inspector.blink.duration");
      } else if (aName == "inspector.blink.speed") {
        this.mFlasher.speed = PrefUtils.getPref("inspector.blink.speed");
      }
    }
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Uncategorized
  
  get isViewingContent() { return this.mIsViewingContent },
  
  getSpecialDirectory: function(aName)
  {
    var dirService = XPCU.getService(kDirServiceCID, "nsIProperties");
    return dirService.get(aName, Components.interfaces.nsIFile);
  },

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
      fn = item.isDisabled;
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

  onTreeItemSelected: function()
  {
    var tree = this.mDOMTree;
    var items = tree.selectedItems;
    if (items.length > 0) {
      var node = this.getNodeFromTreeItem(items[0]);
      this.startViewingNode(node, items[0].id);
    }
  },

  ///////////////////////////////////////////////////////////////////////////
  // The string we get back from shell.getInstallationURL starts with
  // "file://" and so we need to add an extra slash, or the load fails.
  //
  // @param wstring aURL - the url to repair
  ///////////////////////////////////////////////////////////////////////////

  prependBaseURL: function(aURL)
  {
    return "file:///" + this.mInstallURL.substr(7) + aURL;
  },

  flashElement: function(aElement, aWindow)
  {
    // make sure we only try to flash element nodes
    if (aElement.nodeType == 1) {
      if (!this.mFlasher)
        this.mFlasher = new Flasher(this.mUFlasher, 
          PrefUtils.getPref("inspector.blink.border-color"), 
          PrefUtils.getPref("inspector.blink.border-width"), 
          PrefUtils.getPref("inspector.blink.duration"), 
          PrefUtils.getPref("inspector.blink.speed"));

      if (this.mFlasher.flashing) 
        this.mFlasher.stop();
        
      try {
        this.mFlasher.element = aElement;
        this.mFlasher.window = aWindow;
        this.mFlasher.start();
      } catch (ex) {
      }
    }
  },

  setViewerCmdAttribute: function(aEntry, aAttr, aValue)
  {
    var uid = this.mViewerReg.getEntryProperty(aEntry, "uid");
    var cmds = document.getElementById("brsGlobalCommands");
    var els = cmds.getElementsByAttribute("viewer", uid);
    for (var i = 0; i < els.length; i++) {
      if (els[i].getAttribute("exclusive") != "false")
        els[i].setAttribute(aAttr, aValue);
    }
  },

  setAllViewerCmdAttributes: function(aAttr, aValue)
  {
    var count = this.mViewerReg.getEntryCount();
    for (var i = 0; i < count; i++) {
      this.setViewerCmdAttribute(i, aAttr, aValue);
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
  }
};

////////////////////////////////////////////////////////////////////////////
//// event listeners

var HistoryLoadListener = {
  onDataSourceReady: function(aDS) 
  {
    inspector.prepareHistory(aDS);
  },

  onError: function()
  {
  }
};

var PrefChangeObserver = {
  Observe: function(aSubject, aTopic, aData)
  {
    inspector.onPrefChanged(aData);
  }
};

function BrowserLoadListener(aEvent) 
{
  inspector.documentLoaded();
}

function UtilWindowOpenListener(aWindow)
{
  inspector.doViewSearchItem(aWindow);
}
