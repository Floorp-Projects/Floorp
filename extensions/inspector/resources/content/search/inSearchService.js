/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/***************************************************************
* inSearchService -----------------------------------------------
*  The centry registry where information about all installed
*  search modules is kept.  
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
* Until Bug 54237 is fixed, there be some ugly hacks in this 
* file.  We'll need to load search modules via a XUL document
* within an iframe, instead of using the xml loader for now.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
*   chrome://inspector/content/search/inSearchTreeBuilder.js
*   chrome://inspector/content/search/inSearchModule.js
****************************************************************/

//////////// global variables /////////////////////

//////////// global constants ////////////////////

var kSearchURLPrefix = "chrome://inspector/content/search/";

////////////////////////////////////////////////////////////////////////////
//// class inSearchService

function inSearchService() 
{
  this.mInstances = {};
  this.mObservers = [];
  
  // the browser and webnav are a hack.  We should be able to use 
  // the xmlextras facility for loading xml, but it's broken, so 
  // we use a browser for onw
  var browser = document.getElementById("inSearchServiceLoader");
  browser.addEventListener("load", inSearchService_LoadListener, true);
  this.mWebNav = browser.webNavigation;
}

// constants
inSearchService.INSERT_BEFORE = 1;
inSearchService.INSERT_AFTER = 2;

inSearchService.prototype = 
{
  mInstances: null,
  mObservers: null,
  mCurrentModule: null,
  
  mTree: null,
  mContextMenu: null,
  mCMInsertPt: null,
  mCMInsert: inSearchService.INSERT_BEFORE,
  
  ////////////////////////////////////////////////////////////////////////////
  //// Properties
  
  get currentModule() { return this.mCurrentModule },

  get resultsTree() { return this.mTree },
  set resultsTree(aTree) {
    // XX this condition could be fixed with a little bit of effort
    if (this.mTree) throw "inSearchService.tree should only be set once"
    
    this.mTree = aTree;
    aTree._searchService = this;

    this.mTreeBuilder = new inSearchTreeBuilder(aTree, kInspectorNSURI, "results");
    this.mTreeBuilder.isIconic = true;

    // XX HACKERY AT IT'S FINEST - the click event won't fire when I add it to the tree -- go figure
    // in the mean time I'll add it to the parentNode, which seems to work. FIX ME!
    var parent = aTree.parentNode;
    parent._tempTreeYuckyHack = aTree;
    parent.addEventListener("click", inSearchService_TreeClickListener, false);
  },

  get contextMenu() { return this.mContextMenu },
  set contextMenu(aVal) 
  { 
    this.mContextMenu = aVal;
    aVal._searchService = this;
  },
  
  get contextMenuInsertPt() { return this.mCMInsertPt },
  set contextMenuInsertPt(aVal) { this.mCMInsertPt = aVal },
  
  get contextMenuInsert() { return this.mCMInsert },
  set contextMenuInsert(aVal) { this.mCMInsert = aVal },
  
  ////////////////////////////////////////////////////////////////////////////
  //// Running Modules

  startModule: function(aURL)
  {
    var instance = this.mInstances[aURL];
    if (instance)
      this.doStartModule(instance);
    else
      this.loadModule(aURL);
  },
  
  doStartModule: function(aModule)
  {
    aModule.startSearch();
  },
 
  startSearch: function(aModule)
  { 
    this.mCurrentModule = aModule;
    
    // build up the context menu
    this.installContextMenu();
    
    // build up the search results tree
    this.mTreeBuilder.module = aModule;
  },    

  clearSearch: function()
  {
    var mod = this.mCurrentModule;
    if (mod) {
      // clear datasource from search tree
      this.mTreeBuilder.module = null;
      this.mTreeBuilder.buildContent();
      
      // clear context menu
      this.uninstallContextMenu();
    } 

    this.mCurrentModule = null;
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Loading Modules

  loadModule: function(aURL)
  {
    this.mWebNav.loadURI(aURL, nsIWebNavigation.LOAD_FLAGS_NONE);
    this.mLoadingURL = aURL;
    /* 
    // This method of loading the xml doesn't work, but it should.  See bug 54237... 
    var doc = document.implementation.createDocument("", "", null);
    doc.addEventListener("load", SearchFileLoadListener, false);
    doc.load(aURL, "text/xml");
    */ 
  },
  
  searchFileLoaded: function()
  {
    var mod = this.createModule(this.mWebNav.document);
    mod.addSearchObserver(this);
    this.mInstances[this.mLoadingURL] = mod;
    this.doStartModule(mod);
  },
  
  createModule: function(aDocument)
  {
    var mod = new inSearchModule(aDocument.location);
    mod.searchService = this;
    mod.initFromElement(aDocument.documentElement);
    
    return mod;    
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// interface inISearchObserver

  onSearchStart: function(aModule) 
  {
    this.startSearch(aModule);
    this.notifySearchStart();
  },

  onSearchResult: function(aModule)
  {
    this.notifySearchResult();
  },

  onSearchEnd: function(aModule, aResult)
  {
    this.notifySearchEnd(aResult);
  },

  onSearchError: function(aModule, aMessage)
  {
    this.notifySearchError(aMessage);
    this.clearSearch();
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Results Tree

  get selectedItemCount()
  {
    return this.mTree ? this.mTree.selectedItems.length : null;
  },
  
  getSelectedIndex: function(aIdx)
  {
    if (this.mTree) {
      var items = this.mTree.selectedItems;
      return this.mTree.getIndexOfItem(items[aIdx]);
    }
    return null;
  },
  
  onTreeDblClick: function()
  {
    this.mCurrentModule.callDefaultCommand();
  },

  //////////////////////////////////////////////////////////////////////////
  //// ContextMenu

  installContextMenu: function()
  {
    var mod = this.mCurrentModule;
    if (mod) {
      var menu = this.mContextMenu;
      menu.addEventListener("popupshowing", inSearchService_onCreatePopup, true);
      mod.installContextMenu(menu, this.mCMInsertPt, this.mCMInsert);
    }
  },
  
  uninstallContextMenu: function()
  {
    var mod = this.mCurrentModule;
    if (mod) {
      // remove the createion listener
      var menu = this.mContextMenu;
      menu.removeEventListener("popupshowing", inSearchService_onCreatePopup, true);
      mod.uninstallContextMenu(menu, this.mCMInsertPt, this.mCMInsert);
    }
  },

  onCreatePopup: function(aMenu)
  {
    
  },
  
  //////////////////////////////////////////////////////////////////////////
  //// Event Notification
  
  // NOTE TO SELF - this code could be cut down to nothing if you write a module
  // called "ObserverManager" to do the work for you
  
  addSearchObserver: function(aObserver)
  {
    this.mObservers.push(aObserver);
  },
  
  removeSearchObserver: function(aObserver)
  {
    var o;
    var obs = this.mObservers;
    for (var i = 0; i < obs.length; i++) {
      o = obs[i];
      if (o == aObserver) {
        obs.splice(i, 1);
        return;
      }
    }
  },

  notifySearchStart: function()
  {
    var o = this.mObservers;
    for (var i = 0; i < o.length; i++)
      o[i].onSearchStart(this.mCurrentModule);
  },
  
  notifySearchResult: function()
  {
    var o = this.mObservers;
    for (var i = 0; i < o.length; i++)
      o[i].onSearchResult(this.mCurrentModule);
  },

  notifySearchEnd: function(aResult)
  {
    var o = this.mObservers;
    for (var i = 0; i < o.length; i++)
      o[i].onSearchEnd(this.mCurrentModule, aResult);
  },

  notifySearchError: function(aMsg)
  {
    var o = this.mObservers;
    for (var i = 0; i < o.length; i++)
      o[i].onSearchError(this.mCurrentModule, aMsg);
  }
  
};

////////////////////////////////////////////////////////////////////////////
//// Event Listeners

function inSearchService_LoadListener(aEvent)
{
  inspector.searchRegistry.searchFileLoaded();
}

function inSearchService_TreeClickListener(aEvent)
{
  if (aEvent.detail == 2) {
    var tree = this._tempTreeYuckyHack;
    tree._searchService.onTreeDblClick();
  }
}

function inSearchService_onCreatePopup(aEvent)
{
  // event.target is returning null for this event - I should file a bug
  // in the mean time I'll just back off this feature for now...
  
/*  var menu = aEvent.target;
  var svc = menu._searchService;
  svc.onCreatePopup(menu);
*/
}

// This code is from when there was an RDF "search registry"... I might want to bring that back,
// so I'll keep this code sitting here for a little while...

/*
function inSearchServiceLoadObserver(aTarget) 
{
  this.mTarget = aTarget;
}

inSearchServiceLoadObserver.prototype = {
  mTarget: null,

  onError: function(aErrorMsg) 
  {
    this.mTarget.onLoadError(aErrorMsg);
  },

  onDataSourceReady: function(aDS) 
  {
    this.mTarget.onLoad(aDS);
  }
};

  load: function(aURL, aObserver)
  {
    this.mURL = aURL;
    this.mObserver = aObserver;
    RDFU.loadDataSource(aURL, new inSearchServiceLoadObserver(this));
  },

  onLoad: function(aDS)
  {
    this.mDS = aDS;
    this.prepareRegistry();
    this.mObserver.oninSearchServiceLoad();
  },

  onLoadError: function(aErrorMsg)
  {
    this.mObserver.oninSearchServiceLoadError(aErrorMsg);
  },

  prepareRegistry: function()
  {
    this.mModuleSeq = RDFU.findSeq(this.mDS, "inspector:search");

    var el, uid, fnName, factory;
    var els = this.mModuleSeq.GetElements();
    while (els.hasMoreElements()) {
      el = els.getNext();
      uid = RDFU.readAttribute(this.mDS, el, kInspectorNSURI+"uid");
      fnName = RDFU.readAttribute(this.mDS, el, kInspectorNSURI+"factory");
      factory = eval(fnName);
      if (factory)
        this.mFactories[uid] = factory;
    }

  },
  */

