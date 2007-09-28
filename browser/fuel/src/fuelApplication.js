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
 * The Original Code is FUEL.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Finkle <mfinkle@mozilla.com> (Original Author)
 *  John Resig  <jresig@mozilla.com> (Original Author)
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

const Ci = Components.interfaces;
const Cc = Components.classes;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

//=================================================
// Shutdown - used to store cleanup functions which will
//            be called on Application shutdown
var gShutdown = [];

//=================================================
// Console constructor
function Console() {
  this._console = Components.classes["@mozilla.org/consoleservice;1"]
    .getService(Ci.nsIConsoleService);
}

//=================================================
// Console implementation
Console.prototype = {
  log : function cs_log(aMsg) {
    this._console.logStringMessage(aMsg);
  },

  open : function cs_open() {
    var wMediator = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                              .getService(Ci.nsIWindowMediator);
    var console = wMediator.getMostRecentWindow("global:console");
    if (!console) {
      var wWatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                             .getService(Ci.nsIWindowWatcher);
      wWatch.openWindow(null, "chrome://global/content/console.xul", "_blank",
                        "chrome,dialog=no,all", null);
    } else {
      // console was already open
      console.focus();
    }
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIConsole])
};


//=================================================
// EventItem constructor
function EventItem(aType, aData) {
  this._type = aType;
  this._data = aData;
}

//=================================================
// EventItem implementation
EventItem.prototype = {
  _cancel : false,

  get type() {
    return this._type;
  },

  get data() {
    return this._data;
  },

  preventDefault : function ei_pd() {
    this._cancel = true;
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIEventItem])
};


//=================================================
// Events constructor
function Events() {
  this._listeners = [];
}

//=================================================
// Events implementation
Events.prototype = {
  addListener : function evts_al(aEvent, aListener) {
    if (this._listeners.some(hasFilter))
      return;

    this._listeners.push({
      event: aEvent,
      listener: aListener
    });

    function hasFilter(element) {
      return element.event == aEvent && element.listener == aListener;
    }
  },

  removeListener : function evts_rl(aEvent, aListener) {
    this._listeners = this._listeners.filter(function(element){
      return element.event != aEvent && element.listener != aListener;
    });
  },

  dispatch : function evts_dispatch(aEvent, aEventItem) {
    eventItem = new EventItem(aEvent, aEventItem);

    this._listeners.forEach(function(key){
      if (key.event == aEvent) {
        key.listener.handleEvent ?
          key.listener.handleEvent(eventItem) :
          key.listener(eventItem);
      }
    });

    return !eventItem._cancel;
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIEvents])
};


//=================================================
// PreferenceBranch constructor
function PreferenceBranch(aBranch) {
  if (!aBranch)
    aBranch = "";

  this._root = aBranch;
  this._prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Ci.nsIPrefService);

  if (aBranch)
    this._prefs = this._prefs.getBranch(aBranch);

  this._prefs.QueryInterface(Ci.nsIPrefBranch);
  this._prefs.QueryInterface(Ci.nsIPrefBranch2);

  // we want to listen to "all" changes for this branch, so pass in a blank domain
  this._prefs.addObserver("", this, true);
  this._events = new Events();

  var self = this;
  gShutdown.push(function() { self._shutdown(); });
}

//=================================================
// PreferenceBranch implementation
PreferenceBranch.prototype = {
  // cleanup observer so we don't leak
  _shutdown: function prefs_shutdown() {
    this._prefs.removeObserver(this._root, this);

    this._prefs = null;
    this._events = null;
  },

  // for nsIObserver
  observe: function prefs_observe(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed")
      this._events.dispatch("change", aData);
  },

  get root() {
    return this._root;
  },

  get all() {
    return this.find({});
  },

  get events() {
    return this._events;
  },

  // XXX: Disabled until we can figure out the wrapped object issues
  // name: "name" or /name/
  // path: "foo.bar." or "" or /fo+\.bar/
  // type: Boolean, Number, String (getPrefType)
  // locked: true, false (prefIsLocked)
  // modified: true, false (prefHasUserValue)
  find : function prefs_find(aOptions) {
    var retVal = [];
    var items = this._prefs.getChildList("", []);

    for (var i = 0; i < items.length; i++) {
      retVal.push(new Preference(items[i], this));
    }

    return retVal;
  },

  has : function prefs_has(aName) {
    return (this._prefs.getPrefType(aName) != Ci.nsIPrefBranch.PREF_INVALID);
  },

  get : function prefs_get(aName) {
    return this.has(aName) ? new Preference(aName, this) : null;
  },

  getValue : function prefs_gv(aName, aValue) {
    var type = this._prefs.getPrefType(aName);

    switch (type) {
      case Ci.nsIPrefBranch2.PREF_STRING:
        aValue = this._prefs.getComplexValue(aName, Ci.nsISupportsString).data;
        break;
      case Ci.nsIPrefBranch2.PREF_BOOL:
        aValue = this._prefs.getBoolPref(aName);
        break;
      case Ci.nsIPrefBranch2.PREF_INT:
        aValue = this._prefs.getIntPref(aName);
        break;
    }

    return aValue;
  },

  setValue : function prefs_sv(aName, aValue) {
    var type = aValue != null ? aValue.constructor.name : "";

    switch (type) {
      case "String":
        var str = Components.classes["@mozilla.org/supports-string;1"]
                            .createInstance(Ci.nsISupportsString);
        str.data = aValue;
        this._prefs.setComplexValue(aName, Ci.nsISupportsString, str);
        break;
      case "Boolean":
        this._prefs.setBoolPref(aName, aValue);
        break;
      case "Number":
        this._prefs.setIntPref(aName, aValue);
        break;
      default:
        throw("Unknown preference value specified.");
    }
  },

  reset : function prefs_reset() {
    this._prefs.resetBranch("");
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIPreferenceBranch, Ci.nsISupportsWeakReference])
};


//=================================================
// Preference constructor
function Preference(aName, aBranch) {
  this._name = aName;
  this._branch = aBranch;
  this._events = new Events();

  var self = this;

  this.branch.events.addListener("change", function(aEvent){
    if (aEvent.data == self.name)
      self.events.dispatch(aEvent.type, aEvent.data);
  });
}

//=================================================
// Preference implementation
Preference.prototype = {
  get name() {
    return this._name;
  },

  get type() {
    var value = "";
    var type = this._prefs.getPrefType(name);

    switch (type) {
      case Ci.nsIPrefBranch2.PREF_STRING:
        value = "String";
        break;
      case Ci.nsIPrefBranch2.PREF_BOOL:
        value = "Boolean";
        break;
      case Ci.nsIPrefBranch2.PREF_INT:
        value = "Number";
        break;
    }

    return value;
  },

  get value() {
    return this.branch.getValue(this._name, null);
  },

  set value(aValue) {
    return this.branch.setValue(this._name, aValue);
  },

  get locked() {
    return this.branch._prefs.prefIsLocked(this.name);
  },

  set locked(aValue) {
    this.branch._prefs[ aValue ? "lockPref" : "unlockPref" ](this.name);
  },

  get modified() {
    return this.branch._prefs.prefHasUserValue(this.name);
  },

  get branch() {
    return this._branch;
  },

  get events() {
    return this._events;
  },

  reset : function pref_reset() {
    this.branch._prefs.clearUserPref(this.name);
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIPreference])
};


//=================================================
// SessionStorage constructor
function SessionStorage() {
  this._storage = {};
  this._events = new Events();
}

//=================================================
// SessionStorage implementation
SessionStorage.prototype = {
  get events() {
    return this._events;
  },

  has : function ss_has(aName) {
    return this._storage.hasOwnProperty(aName);
  },

  set : function ss_set(aName, aValue) {
    this._storage[aName] = aValue;
    this._events.dispatch("change", aName);
  },

  get : function ss_get(aName, aDefaultValue) {
    return this.has(aName) ? this._storage[aName] : aDefaultValue;
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelISessionStorage])
};


//=================================================
// Extension constructor
function Extension(aItem) {
  this._item = aItem;
  this._firstRun = false;
  this._prefs = new PreferenceBranch("extensions." + this._item.id + ".");
  this._storage = new SessionStorage();
  this._events = new Events();

  var installPref = "install-event-fired";
  if (!this._prefs.has(installPref)) {
    this._prefs.setValue(installPref, true);
    this._firstRun = true;
  }

  this._enabled = false;
  const PREFIX_ITEM_URI = "urn:mozilla:item:";
  const PREFIX_NS_EM = "http://www.mozilla.org/2004/em-rdf#";
  var rdf = Cc["@mozilla.org/rdf/rdf-service;1"].getService(Ci.nsIRDFService);
  var itemResource = rdf.GetResource(PREFIX_ITEM_URI + this._item.id);
  if (itemResource) {
    var extmgr = Cc["@mozilla.org/extensions/manager;1"].getService(Ci.nsIExtensionManager);
    var ds = extmgr.datasource;
    var target = ds.GetTarget(itemResource, rdf.GetResource(PREFIX_NS_EM + "isDisabled"), true);
    if (target && target instanceof Ci.nsIRDFLiteral)
      this._enabled = (target.Value != "true");
  }

  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Ci.nsIObserverService);
  os.addObserver(this, "em-action-requested", false);

  var self = this;
  gShutdown.push(function(){ self._shutdown(); });
}

//=================================================
// Extension implementation
Extension.prototype = {
  // cleanup observer so we don't leak
  _shutdown: function ext_shutdown() {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Ci.nsIObserverService);
    os.removeObserver(this, "em-action-requested");

    this._prefs = null;
    this._storage = null;
    this._events = null;
  },

  // for nsIObserver
  observe: function ext_observe(aSubject, aTopic, aData)
  {
    if ((aSubject instanceof Ci.nsIUpdateItem) && (aSubject.id == this._item.id))
    {
      if (aData == "item-uninstalled")
        this._events.dispatch("uninstall", this._item.id);
      else if (aData == "item-disabled")
        this._events.dispatch("disable", this._item.id);
      else if (aData == "item-enabled")
        this._events.dispatch("enable", this._item.id);
      else if (aData == "item-cancel-action")
        this._events.dispatch("cancel", this._item.id);
      else if (aData == "item-upgraded")
        this._events.dispatch("upgrade", this._item.id);
    }
  },

  get id() {
    return this._item.id;
  },

  get name() {
    return this._item.name;
  },

  get enabled() {
    return this._enabled;
  },

  get version() {
    return this._item.version;
  },

  get firstRun() {
    return this._firstRun;
  },

  get storage() {
    return this._storage;
  },

  get prefs() {
    return this._prefs;
  },

  get events() {
    return this._events;
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIExtension])
};


//=================================================
// Extensions constructor
function Extensions() {
  this._extmgr = Components.classes["@mozilla.org/extensions/manager;1"]
                           .getService(Ci.nsIExtensionManager);

  this._cache = {};

  var self = this;
  gShutdown.push(function() { self._shutdown(); });
}

//=================================================
// Extensions implementation
Extensions.prototype = {
  _shutdown : function exts_shutdown() {
    this._extmgr = null;
    this._cache = null;
  },

  /*
   * Helper method to check cache before creating a new extension
   */
  _get : function exts_get(aId) {
    if (this._cache.hasOwnProperty(aId))
      return this._cache[aId];

    var newExt = new Extension(this._extmgr.getItemForID(aId));
    this._cache[aId] = newExt;
    return newExt;
  },

  get all() {
    return this.find({});
  },

  // XXX: Disabled until we can figure out the wrapped object issues
  // id: "some@id" or /id/
  // name: "name" or /name/
  // version: "1.0.1"
  // minVersion: "1.0"
  // maxVersion: "2.0"
  find : function exts_find(aOptions) {
    var retVal = [];
    var items = this._extmgr.getItemList(Ci.nsIUpdateItem.TYPE_EXTENSION, {});

    for (var i = 0; i < items.length; i++) {
      retVal.push(this._get(items[i].id));
    }

    return retVal;
  },

  has : function exts_has(aId) {
    return this._extmgr.getItemForID(aId) != null;
  },

  get : function exts_get(aId) {
    return this.has(aId) ? this._get(aId) : null;
  },

  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIExtensions])
};

//=================================================
// Singleton that holds services and utilities
var Utilities = {
  _bookmarks : null,
  get bookmarks() {
    if (!this._bookmarks) {
      this._bookmarks = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                        getService(Ci.nsINavBookmarksService);
    }
    return this._bookmarks;
  },

  _livemarks : null,
  get livemarks() {
    if (!this._livemarks) {
      this._livemarks = Cc["@mozilla.org/browser/livemark-service;2"].
                        getService(Ci.nsILivemarkService);
    }
    return this._livemarks;
  },

  _annotations : null,
  get annotations() {
    if (!this._annotations) {
      this._annotations = Cc["@mozilla.org/browser/annotation-service;1"].
                          getService(Ci.nsIAnnotationService);
    }
    return this._annotations;
  },

  _history : null,
  get history() {
    if (!this._history) {
      this._history = Cc["@mozilla.org/browser/nav-history-service;1"].
                      getService(Ci.nsINavHistoryService);
    }
    return this._history;
  },

  _windowMediator : null,
  get windowMediator() {
    if (!this._windowMediator) {
      this._windowMediator = Cc["@mozilla.org/appshell/window-mediator;1"].
                             getService(Ci.nsIWindowMediator);
    }
    return this._windowMediator;
  },

  makeURI : function(aSpec) {
    if (!aSpec)
      return null;
    var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
    return ios.newURI(aSpec, null, null);
  },

  free : function() {
    this._bookmarks = null;
    this._livemarks = null;
    this._annotations = null;
    this._history = null;
    this._windowMediator = null;
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
    this._tabbrowser.addEventListener(aType,
      this._cleanup[aType] = function(e){ self._event(e); },
      true);
  },

  /*
   * Helper event callback used to redirect events made on the XBL element
   */
  _event : function win_event(aEvent) {
    this._events.dispatch(aEvent.type, "");
  },

  get tabs() {
    var tabs = [];
    var browsers = this._tabbrowser.browsers;

    for (var i=0; i<browsers.length; i++)
      tabs.push(new BrowserTab(this._window, browsers[i]));

    return tabs;
  },

  get activeTab() {
    return new BrowserTab(this._window, this._tabbrowser.selectedBrowser);
  },

  open : function win_open(aURI) {
    return new BrowserTab(this._window, this._tabbrowser.addTab(aURI.spec).linkedBrowser);
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
function BrowserTab(aWindow, aBrowser) {
  this._window = aWindow;
  this._tabbrowser = aWindow.getBrowser();
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
    var tabs = this._tabbrowser.mTabs;
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
      if (!(aEvent.originalTarget instanceof Ci.nsIDOMHTMLDocument))
        return;

      if (aEvent.originalTarget.defaultView instanceof Ci.nsIDOMWindowInternal &&
          aEvent.originalTarget.defaultView.frameElement)
        return;
    }

    this._events.dispatch(aEvent.type, "");
  },

  /*
   * Helper used to determine the index offset of the browsertab
   */
  _getTab : function bt_gettab() {
    var tabs = this._tabbrowser.mTabs;
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
    return Utilities.annotations.getItemAnnotationNames(this._id, {});
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

  onItemAdded : function bm_oia(aId, aFolder, aIndex) {
    // bookmark object doesn't exist at this point
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
  if (this._id == null)
    this._id = Utilities.bookmarks.bookmarksRoot;

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
    Utilities.bookmarks.removeFolder(this._id);
  },

  // observer
  onBeginUpdateBatch : function bmf_obub() {
  },

  onEndUpdateBatch : function bmf_oeub() {
  },

  onItemAdded : function bmf_oia(aId, aFolder, aIndex) {
    // handle root folder events
    if (!this._parent)
      this._events.dispatch("add", aId);

    // handle this folder events
    if (this._id == aFolder)
      this._events.dispatch("addchild", aId);
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
  this._console = null;
  this._prefs = null;
  this._storage = null;
  this._events = null;
  this._bookmarks = null;

  this._info = Components.classes["@mozilla.org/xre/app-info;1"]
                     .getService(Ci.nsIXULAppInfo);

  var os = Components.classes["@mozilla.org/observer-service;1"]
                     .getService(Ci.nsIObserverService);

  os.addObserver(this, "final-ui-startup", false);
  os.addObserver(this, "quit-application-requested", false);
  os.addObserver(this, "quit-application-granted", false);
  os.addObserver(this, "quit-application", false);
  os.addObserver(this, "xpcom-shutdown", false);
}

//=================================================
// Application implementation
Application.prototype = {
  // for nsIClassInfo + XPCOMUtils
  classDescription: "Application",
  classID:          Components.ID("fe74cf80-aa2d-11db-abbd-0800200c9a66"),
  contractID:       "@mozilla.org/fuel/application;1",

  // redefine the default factory for XPCOMUtils
  _xpcom_factory: ApplicationFactory,

  // get this contractID registered for certain categories via XPCOMUtils
  _xpcom_categories: [
    // make Application a startup observer
    { category: "app-startup", service: true },

    // add Application as a global property for easy access
    { category: "JavaScript global privileged property" }
  ],

  get id() {
    return this._info.ID;
  },

  get name() {
    return this._info.name;
  },

  get version() {
    return this._info.version;
  },

  // for nsIObserver
  observe: function app_observe(aSubject, aTopic, aData) {
    if (aTopic == "app-startup") {
      this._extensions = new Extensions();
      this.events.dispatch("load", "application");
    }
    else if (aTopic == "final-ui-startup") {
      this.events.dispatch("ready", "application");
    }
    else if (aTopic == "quit-application-requested") {
      // we can stop the quit by checking the return value
      if (this.events.dispatch("quit", "application") == false)
        aSubject.data = true;
    }
    else if (aTopic == "xpcom-shutdown") {
      this.events.dispatch("unload", "application");

      // call the cleanup functions and empty the array
      while (gShutdown.length) {
        gShutdown.shift()();
      }

      // release our observers
      var os = Components.classes["@mozilla.org/observer-service;1"]
                         .getService(Ci.nsIObserverService);

      os.removeObserver(this, "final-ui-startup");

      os.removeObserver(this, "quit-application-requested");
      os.removeObserver(this, "quit-application-granted");
      os.removeObserver(this, "quit-application");

      os.removeObserver(this, "xpcom-shutdown");

      this._info = null;
      this._console = null;
      this._prefs = null;
      this._storage = null;
      this._events = null;
      this._extensions = null;
      this._bookmarks = null;

      Utilities.free();
    }
  },

  // for nsIClassInfo
  flags : Ci.nsIClassInfo.SINGLETON,
  implementationLanguage : Ci.nsIProgrammingLanguage.JAVASCRIPT,

  getInterfaces : function app_gi(aCount) {
    var interfaces = [Ci.fuelIApplication, Ci.nsIObserver, Ci.nsIClassInfo];
    aCount.value = interfaces.length;
    return interfaces;
  },

  getHelperForLanguage : function app_ghfl(aCount) {
    return null;
  },

  // for nsISupports
  QueryInterface : XPCOMUtils.generateQI([Ci.fuelIApplication, Ci.nsIObserver, Ci.nsIClassInfo]),

  get console() {
    if (this._console == null)
        this._console = new Console();

    return this._console;
  },

  get storage() {
    if (this._storage == null)
        this._storage = new SessionStorage();

    return this._storage;
  },

  get prefs() {
    if (this._prefs == null)
        this._prefs = new PreferenceBranch("");

    return this._prefs;
  },

  get extensions() {
    return this._extensions;
  },

  get events() {
    if (this._events == null)
        this._events = new Events();

    return this._events;
  },

  get bookmarks() {
    if (this._bookmarks == null)
      this._bookmarks = new BookmarkFolder(null, null);

    return this._bookmarks;
  },

  get windows() {
    var win = [];
    var enum = Utilities.windowMediator.getEnumerator("navigator:browser");

    while (enum.hasMoreElements())
      win.push(new Window(enum.getNext()));

    return win;
  },

  get activeWindow() {
    return new Window(Utilities.windowMediator.getMostRecentWindow("navigator:browser"));
  }
};

//module initialization
function NSGetModule(aCompMgr, aFileSpec) {
  return XPCOMUtils.generateModule([Application]);
}
