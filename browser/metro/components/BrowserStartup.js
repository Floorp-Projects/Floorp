/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// Custom factory object to ensure that we're a singleton
const BrowserStartupServiceFactory = {
  _instance: null,
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return this._instance || (this._instance = new BrowserStartup());
  }
};

function BrowserStartup() {
  this._init();
}

BrowserStartup.prototype = {
  // for XPCOM
  classID: Components.ID("{1d542abc-c88b-4636-a4ef-075b49806317}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]),

  _xpcom_factory: BrowserStartupServiceFactory,

  _init: function() {
    Services.obs.addObserver(this, "places-init-complete", false);
    Services.obs.addObserver(this, "final-ui-startup", false);
  },

  _initDefaultBookmarks: function() {
    // We must instantiate the history service since it will tell us if we
    // need to import or restore bookmarks due to first-run, corruption or
    // forced migration (due to a major schema change).
    let histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                  getService(Ci.nsINavHistoryService);

    // If the database is corrupt or has been newly created we should
    // import bookmarks.
    let databaseStatus = histsvc.databaseStatus;
    let importBookmarks = databaseStatus == histsvc.DATABASE_STATUS_CREATE ||
                          databaseStatus == histsvc.DATABASE_STATUS_CORRUPT;

    if (!importBookmarks) {
      // Check to see whether "mobile" root already exists. This is to handle
      // existing profiles created with pre-1.0 builds (which won't have mobile
      // bookmarks root). We can remove this eventually when we stop
      // caring about users migrating to current builds with pre-1.0 profiles.
      let annos = Cc["@mozilla.org/browser/annotation-service;1"].
                  getService(Ci.nsIAnnotationService);
      let metroRootItems = annos.getItemsWithAnnotation("metro/bookmarksRoot", {});
      if (metroRootItems.length > 0)
        return; // no need to do initial import
    }

    Cu.import("resource://gre/modules/BookmarkJSONUtils.jsm");

    BookmarkJSONUtils.importFromURL("chrome://browser/locale/bookmarks.json", false);
  },

  _startupActions: function() {
  },

  // nsIObserver
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "places-init-complete":
        Services.obs.removeObserver(this, "places-init-complete");
        this._initDefaultBookmarks();
        break;
      case "final-ui-startup":
        Services.obs.removeObserver(this, "final-ui-startup");
        this._startupActions();
        break;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([BrowserStartup]);
