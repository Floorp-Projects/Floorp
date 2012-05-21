/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const APPLICATION_CID = Components.ID("fe74cf80-aa2d-11db-abbd-0800200c9a66");
const APPLICATION_CONTRACTID = "@mozilla.org/fuel/application;1";

//=================================================
// Singleton that holds services and utilities
var Utilities = {
  get bookmarks() {
    let bookmarks = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                    getService(Ci.nsINavBookmarksService);
    this.__defineGetter__("bookmarks", function() bookmarks);
    return this.bookmarks;
  },

  get livemarks() {
    let livemarks = Cc["@mozilla.org/browser/livemark-service;2"].
                    getService[Ci.mozIAsyncLivemarks].
                    QueryInterface(Ci.nsILivemarkService);
    this.__defineGetter__("livemarks", function() livemarks);
    return this.livemarks;
  },

  get annotations() {
    let annotations = Cc["@mozilla.org/browser/annotation-service;1"].
                      getService(Ci.nsIAnnotationService);
    this.__defineGetter__("annotations", function() annotations);
    return this.annotations;
  },

  get history() {
    let history = Cc["@mozilla.org/browser/nav-history-service;1"].
                  getService(Ci.nsINavHistoryService);
    this.__defineGetter__("history", function() history);
    return this.history;
  },

  get windowMediator() {
    let windowMediator = Cc["@mozilla.org/appshell/window-mediator;1"].
                         getService(Ci.nsIWindowMediator);
    this.__defineGetter__("windowMediator", function() windowMediator);
    return this.windowMediator;
  },

  makeURI : function(aSpec) {
    if (!aSpec)
      return null;
    var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
    return ios.newURI(aSpec, null, null);
  },

  free : function() {
    delete this.bookmarks;
    delete this.livemarks
    delete this.annotations;
    delete this.history;
    delete this.windowMediator;
  }
};


//=================================================
// Window implementation
function Window(aWindow) {
  this._window = aWindow;
  this._tabbrowser = aWindow.getBrowser();
  this._events = new Events();
  this._cleanup = {};

  this._watch("TabOpen");
  this._watch("TabMove");
  this._watch("TabClose");
  this._watch("TabSelect");

  var self = this;
  gShutdown.push(function() { self._shutdown(); });
}

Window.prototype = {
  get events() {
    return this._events;
  },

  /*
   * Helper used to setup event handlers on the XBL element. Note that the events
   * are actually dispatched to tabs, so we capture them.
   */
  _watch : function win_watch(aType) {
    var self = this;
    this._tabbrowser.tabContainer.addEventListener(aType,
      this._cleanup[aType] = function(e){ self._event(e); },
      true);
  },

  /*
   * Helper event callback used to redirect events made on the XBL element
   */
  _event : function win_event(aEvent) {
    this._events.dispatch(aEvent.type, new BrowserTab(this, aEvent.originalTarget.linkedBrowser));
  },
  get tabs() {
    var tabs = [];
    var browsers = this._tabbrowser.browsers;
    for (var i=0; i<browsers.length; i++)
      tabs.push(new BrowserTab(this, browsers[i]));
    return tabs;
  },
  get activeTab() {
    return new BrowserTab(this, this._tabbrowser.selectedBrowser);
  },
  open : function win_open(aURI) {
    return new BrowserTab(this, this._tabbrowser.addTab(aURI.spec).linkedBrowser);
  },
  _shutdown : function win_shutdown() {
    for (var type in this._cleanup)
      this._tabbrowser.removeEventListener(type, this._cleanup[type], true);
    this._cleanup = null;

    this._window = null;
    this._tabbrowser = null;
    this._events = null;
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIWindow])
};

//=================================================
// BrowserTab implementation
function BrowserTab(aFUELWindow, aBrowser) {
  this._window = aFUELWindow;
  this._tabbrowser = aFUELWindow._tabbrowser;
  this._browser = aBrowser;
  this._events = new Events();
  this._cleanup = {};

  this._watch("load");

  var self = this;
  gShutdown.push(function() { self._shutdown(); });
}

BrowserTab.prototype = {
  get uri() {
    return this._browser.currentURI;
  },

  get index() {
    var tabs = this._tabbrowser.tabs;
    for (var i=0; i<tabs.length; i++) {
      if (tabs[i].linkedBrowser == this._browser)
        return i;
    }
    return -1;
  },

  get events() {
    return this._events;
  },

  get window() {
    return this._window;
  },

  get document() {
    return this._browser.contentDocument;
  },

  /*
   * Helper used to setup event handlers on the XBL element
   */
  _watch : function bt_watch(aType) {
    var self = this;
    this._browser.addEventListener(aType,
      this._cleanup[aType] = function(e){ self._event(e); },
      true);
  },

  /*
   * Helper event callback used to redirect events made on the XBL element
   */
  _event : function bt_event(aEvent) {
    if (aEvent.type == "load") {
      if (!(aEvent.originalTarget instanceof Ci.nsIDOMDocument))
        return;

      if (aEvent.originalTarget.defaultView instanceof Ci.nsIDOMWindow &&
          aEvent.originalTarget.defaultView.frameElement)
        return;
    }
    this._events.dispatch(aEvent.type, this);
  },
  /*
   * Helper used to determine the index offset of the browsertab
   */
  _getTab : function bt_gettab() {
    var tabs = this._tabbrowser.tabs;
    return tabs[this.index] || null;
  },

  load : function bt_load(aURI) {
    this._browser.loadURI(aURI.spec, null, null);
  },

  focus : function bt_focus() {
    this._tabbrowser.selectedTab = this._getTab();
    this._tabbrowser.focus();
  },

  close : function bt_close() {
    this._tabbrowser.removeTab(this._getTab());
  },

  moveBefore : function bt_movebefore(aBefore) {
    this._tabbrowser.moveTabTo(this._getTab(), aBefore.index);
  },

  moveToEnd : function bt_moveend() {
    this._tabbrowser.moveTabTo(this._getTab(), this._tabbrowser.browsers.length);
  },

  _shutdown : function bt_shutdown() {
    for (var type in this._cleanup)
      this._browser.removeEventListener(type, this._cleanup[type], true);
    this._cleanup = null;

    this._window = null;
    this._tabbrowser = null;
    this._browser = null;
    this._events = null;
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIBrowserTab])
};


//=================================================
// Annotations implementation
function Annotations(aId) {
  this._id = aId;
}

Annotations.prototype = {
  get names() {
    return Utilities.annotations.getItemAnnotationNames(this._id);
  },

  has : function ann_has(aName) {
    return Utilities.annotations.itemHasAnnotation(this._id, aName);
  },

  get : function(aName) {
    if (this.has(aName))
      return Utilities.annotations.getItemAnnotation(this._id, aName);
    return null;
  },

  set : function(aName, aValue, aExpiration) {
    Utilities.annotations.setItemAnnotation(this._id, aName, aValue, 0, aExpiration);
  },

  remove : function ann_remove(aName) {
    if (aName)
      Utilities.annotations.removeItemAnnotation(this._id, aName);
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIAnnotations])
};


//=================================================
// Bookmark implementation
function Bookmark(aId, aParent, aType) {
  this._id = aId;
  this._parent = aParent;
  this._type = aType || "bookmark";
  this._annotations = new Annotations(this._id);
  this._events = new Events();

  Utilities.bookmarks.addObserver(this, false);

  var self = this;
  gShutdown.push(function() { self._shutdown(); });
}

Bookmark.prototype = {
  _shutdown : function bm_shutdown() {
    this._annotations = null;
    this._events = null;

    Utilities.bookmarks.removeObserver(this);
  },

  get id() {
    return this._id;
  },

  get title() {
    return Utilities.bookmarks.getItemTitle(this._id);
  },

  set title(aTitle) {
    Utilities.bookmarks.setItemTitle(this._id, aTitle);
  },

  get uri() {
    return Utilities.bookmarks.getBookmarkURI(this._id);
  },

  set uri(aURI) {
    return Utilities.bookmarks.changeBookmarkURI(this._id, aURI);
  },

  get description() {
    return this._annotations.get("bookmarkProperties/description");
  },

  set description(aDesc) {
    this._annotations.set("bookmarkProperties/description", aDesc, Ci.nsIAnnotationService.EXPIRE_NEVER);
  },

  get keyword() {
    return Utilities.bookmarks.getKeywordForBookmark(this._id);
  },

  set keyword(aKeyword) {
    Utilities.bookmarks.setKeywordForBookmark(this._id, aKeyword);
  },

  get type() {
    return this._type;
  },

  get parent() {
    return this._parent;
  },

  set parent(aFolder) {
    Utilities.bookmarks.moveItem(this._id, aFolder.id, Utilities.bookmarks.DEFAULT_INDEX);
    // this._parent is updated in onItemMoved
  },

  get annotations() {
    return this._annotations;
  },

  get events() {
    return this._events;
  },

  remove : function bm_remove() {
    Utilities.bookmarks.removeItem(this._id);
  },

  // observer
  onBeginUpdateBatch : function bm_obub() {
  },

  onEndUpdateBatch : function bm_oeub() {
  },

  onItemAdded : function bm_oia(aId, aFolder, aIndex, aItemType, aURI) {
    // bookmark object doesn't exist at this point
  },

  onBeforeItemRemoved : function bm_obir(aId) {
  },

  onItemRemoved : function bm_oir(aId, aFolder, aIndex) {
    if (this._id == aId)
      this._events.dispatch("remove", aId);
  },

  onItemChanged : function bm_oic(aId, aProperty, aIsAnnotationProperty, aValue) {
    if (this._id == aId)
      this._events.dispatch("change", aProperty);
  },

  onItemVisited: function bm_oiv(aId, aVisitID, aTime) {
  },

  onItemMoved: function bm_oim(aId, aOldParent, aOldIndex, aNewParent, aNewIndex) {
    if (this._id == aId) {
      this._parent = new BookmarkFolder(aNewParent, Utilities.bookmarks.getFolderIdForItem(aNewParent));
      this._events.dispatch("move", aId);
    }
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIBookmark, Ci.nsINavBookmarkObserver])
};


//=================================================
// BookmarkFolder implementation
function BookmarkFolder(aId, aParent) {
  this._id = aId;
  this._parent = aParent;
  this._annotations = new Annotations(this._id);
  this._events = new Events();

  Utilities.bookmarks.addObserver(this, false);

  var self = this;
  gShutdown.push(function() { self._shutdown(); });
}

BookmarkFolder.prototype = {
  _shutdown : function bmf_shutdown() {
    this._annotations = null;
    this._events = null;

    Utilities.bookmarks.removeObserver(this);
  },

  get id() {
    return this._id;
  },

  get title() {
    return Utilities.bookmarks.getItemTitle(this._id);
  },

  set title(aTitle) {
    Utilities.bookmarks.setItemTitle(this._id, aTitle);
  },

  get description() {
    return this._annotations.get("bookmarkProperties/description");
  },

  set description(aDesc) {
    this._annotations.set("bookmarkProperties/description", aDesc, Ci.nsIAnnotationService.EXPIRE_NEVER);
  },

  get type() {
    return "folder";
  },

  get parent() {
    return this._parent;
  },

  set parent(aFolder) {
    Utilities.bookmarks.moveItem(this._id, aFolder.id, Utilities.bookmarks.DEFAULT_INDEX);
    // this._parent is updated in onItemMoved
  },

  get annotations() {
    return this._annotations;
  },

  get events() {
    return this._events;
  },

  get children() {
    var items = [];

    var options = Utilities.history.getNewQueryOptions();
    var query = Utilities.history.getNewQuery();
    query.setFolders([this._id], 1);
    var result = Utilities.history.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    for (var i=0; i<cc; ++i) {
      var node = rootNode.getChild(i);
      if (node.type == node.RESULT_TYPE_FOLDER) {
        var folder = new BookmarkFolder(node.itemId, this._id);
        items.push(folder);
      }
      else if (node.type == node.RESULT_TYPE_SEPARATOR) {
        var separator = new Bookmark(node.itemId, this._id, "separator");
        items.push(separator);
      }
      else {
        var bookmark = new Bookmark(node.itemId, this._id, "bookmark");
        items.push(bookmark);
      }
    }
    rootNode.containerOpen = false;

    return items;
  },

  addBookmark : function bmf_addbm(aTitle, aUri) {
    var newBookmarkID = Utilities.bookmarks.insertBookmark(this._id, aUri, Utilities.bookmarks.DEFAULT_INDEX, aTitle);
    var newBookmark = new Bookmark(newBookmarkID, this, "bookmark");
    return newBookmark;
  },

  addSeparator : function bmf_addsep() {
    var newBookmarkID = Utilities.bookmarks.insertSeparator(this._id, Utilities.bookmarks.DEFAULT_INDEX);
    var newBookmark = new Bookmark(newBookmarkID, this, "separator");
    return newBookmark;
  },

  addFolder : function bmf_addfolder(aTitle) {
    var newFolderID = Utilities.bookmarks.createFolder(this._id, aTitle, Utilities.bookmarks.DEFAULT_INDEX);
    var newFolder = new BookmarkFolder(newFolderID, this);
    return newFolder;
  },

  remove : function bmf_remove() {
    Utilities.bookmarks.removeItem(this._id);
  },

  // observer
  onBeginUpdateBatch : function bmf_obub() {
  },

  onEndUpdateBatch : function bmf_oeub() {
  },

  onItemAdded : function bmf_oia(aId, aFolder, aIndex, aItemType, aURI) {
    // handle root folder events
    if (!this._parent)
      this._events.dispatch("add", aId);

    // handle this folder events
    if (this._id == aFolder)
      this._events.dispatch("addchild", aId);
  },

  onBeforeItemRemoved : function bmf_oir(aId) {
  },

  onItemRemoved : function bmf_oir(aId, aFolder, aIndex) {
    // handle root folder events
    if (!this._parent || this._id == aId)
      this._events.dispatch("remove", aId);

    // handle this folder events
    if (this._id == aFolder)
      this._events.dispatch("removechild", aId);
  },

  onItemChanged : function bmf_oic(aId, aProperty, aIsAnnotationProperty, aValue) {
    // handle root folder and this folder events
    if (!this._parent || this._id == aId)
      this._events.dispatch("change", aProperty);
  },

  onItemVisited: function bmf_oiv(aId, aVisitID, aTime) {
  },

  onItemMoved: function bmf_oim(aId, aOldParent, aOldIndex, aNewParent, aNewIndex) {
    // handle this folder event, root folder cannot be moved
    if (this._id == aId) {
      this._parent = new BookmarkFolder(aNewParent, Utilities.bookmarks.getFolderIdForItem(aNewParent));
      this._events.dispatch("move", aId);
    }
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIBookmarkFolder, Ci.nsINavBookmarkObserver])
};

//=================================================
// BookmarkRoots implementation
function BookmarkRoots() {
  var self = this;
  gShutdown.push(function() { self._shutdown(); });
}

BookmarkRoots.prototype = {
  _shutdown : function bmr_shutdown() {
    this._menu = null;
    this._toolbar = null;
    this._tags = null;
    this._unfiled = null;
  },

  get menu() {
    if (!this._menu)
      this._menu = new BookmarkFolder(Utilities.bookmarks.bookmarksMenuFolder, null);

    return this._menu;
  },

  get toolbar() {
    if (!this._toolbar)
      this._toolbar = new BookmarkFolder(Utilities.bookmarks.toolbarFolder, null);

    return this._toolbar;
  },

  get tags() {
    if (!this._tags)
      this._tags = new BookmarkFolder(Utilities.bookmarks.tagsFolder, null);

    return this._tags;
  },

  get unfiled() {
    if (!this._unfiled)
      this._unfiled = new BookmarkFolder(Utilities.bookmarks.unfiledBookmarksFolder, null);

    return this._unfiled;
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIBookmarkRoots])
};


//=================================================
// Factory - Treat Application as a singleton
// XXX This is required, because we're registered for the 'JavaScript global
// privileged property' category, whose handler always calls createInstance.
// See bug 386535.
var gSingleton = null;
var ApplicationFactory = {
  createInstance: function af_ci(aOuter, aIID) {
    if (aOuter != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (gSingleton == null) {
      gSingleton = new Application();
    }

    return gSingleton.QueryInterface(aIID);
  }
};


//=================================================
// Application constructor
function Application() {
  this.initToolkitHelpers();
  this._bookmarks = null;
}

//=================================================
// Application implementation
Application.prototype = {
  // for nsIClassInfo + XPCOMUtils
  classID:          APPLICATION_CID,

  // redefine the default factory for XPCOMUtils
  _xpcom_factory: ApplicationFactory,

  // for nsISupports
  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIApplication, Ci.extIApplication,
                                          Ci.nsIObserver]),

  // for nsIClassInfo
  classInfo: XPCOMUtils.generateCI({classID: APPLICATION_CID,
                                    contractID: APPLICATION_CONTRACTID,
                                    interfaces: [Ci.fuelIApplication,
                                                 Ci.extIApplication,
                                                 Ci.nsIObserver],
                                    flags: Ci.nsIClassInfo.SINGLETON}),

  // for nsIObserver
  observe: function app_observe(aSubject, aTopic, aData) {
    // Call the extApplication version of this function first
    this.__proto__.__proto__.observe.call(this, aSubject, aTopic, aData);
    if (aTopic == "xpcom-shutdown") {
      this._obs.removeObserver(this, "xpcom-shutdown");
      this._bookmarks = null;
      Utilities.free();
    }
  },

  get bookmarks() {
    let bookmarks = new BookmarkRoots();
    this.__defineGetter__("bookmarks", function() bookmarks);
    return this.bookmarks;
  },

  get windows() {
    var win = [];
    var browserEnum = Utilities.windowMediator.getEnumerator("navigator:browser");

    while (browserEnum.hasMoreElements())
      win.push(new Window(browserEnum.getNext()));

    return win;
  },

  get activeWindow() {
    return new Window(Utilities.windowMediator.getMostRecentWindow("navigator:browser"));
  }
};

#include ../../../toolkit/components/exthelper/extApplication.js

// set the proto, defined in extApplication.js
Application.prototype.__proto__ = extApplication.prototype;

var NSGetFactory = XPCOMUtils.generateNSGetFactory([Application]);

