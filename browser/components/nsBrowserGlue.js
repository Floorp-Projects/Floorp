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
 * The Original Code is the Firefox Browser Glue Service.
 *
 * The Initial Developer of the Original Code is
 * Giorgio Maone
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Giorgio Maone <g.maone@informaction.com>
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



// Constructor

function BrowserGlue() {
  this._init();
  this._profileStarted = false;
}

BrowserGlue.prototype = {
  QueryInterface: function(iid) 
  {
     xpcomCheckInterfaces(iid, kServiceIIds, Components.results.NS_ERROR_NO_INTERFACE);
     return this;
  }
,
  // nsIObserver implementation 
  observe: function(subject, topic, data) 
  {
    switch(topic) {
      case "xpcom-shutdown":
        this._dispose();
        break;
      case "profile-before-change":
        this._onProfileChange();
        break;
      case "profile-change-teardown": 
        this._onProfileShutdown();
        break;
      case "final-ui-startup":
        this._onProfileStartup();
        break;
      case "browser:purge-session-history":
        // reset the console service's error buffer
        const cs = Components.classes["@mozilla.org/consoleservice;1"]
                             .getService(Components.interfaces.nsIConsoleService);
        cs.logStringMessage(null); // clear the console (in case it's open)
        cs.reset();
        break;
    }
  }
, 
  // initialization (called on application startup) 
  _init: function() 
  {
    // observer registration
    const osvr = Components.classes['@mozilla.org/observer-service;1']
                           .getService(Components.interfaces.nsIObserverService);
    osvr.addObserver(this, "profile-before-change", false);
    osvr.addObserver(this, "profile-change-teardown", false);
    osvr.addObserver(this, "xpcom-shutdown", false);
    osvr.addObserver(this, "final-ui-startup", false);
    osvr.addObserver(this, "browser:purge-session-history", false);
  },

  // cleanup (called on application shutdown)
  _dispose: function() 
  {
    // observer removal 
    const osvr = Components.classes['@mozilla.org/observer-service;1']
                           .getService(Components.interfaces.nsIObserverService);
    osvr.removeObserver(this, "profile-before-change");
    osvr.removeObserver(this, "profile-change-teardown");
    osvr.removeObserver(this, "xpcom-shutdown");
    osvr.removeObserver(this, "final-ui-startup");
    osvr.removeObserver(this, "browser:purge-session-history");
  },

  // profile startup handler (contains profile initialization routines)
  _onProfileStartup: function() 
  {
    // check to see if the EULA must be shown on startup
    try {
      var mustDisplayEULA = true;
      var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                  .getService(Components.interfaces.nsIPrefBranch);
      var EULAVersion = prefService.getIntPref("browser.EULA.version");
      mustDisplayEULA = !prefService.getBoolPref("browser.EULA." + EULAVersion + ".accepted");
    } catch(ex) {
    }

    if (mustDisplayEULA) {
      var ww2 = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                         .getService(Components.interfaces.nsIWindowWatcher);
      ww2.openWindow(null, "chrome://browser/content/EULA.xul", 
                     "_blank", "chrome,centerscreen,modal,resizable=yes", null);
    }

    this.Sanitizer.onStartup();
    // check if we're in safe mode
    var app = Components.classes["@mozilla.org/xre/app-info;1"].getService(Components.interfaces.nsIXULAppInfo)
                        .QueryInterface(Components.interfaces.nsIXULRuntime);
    if (app.inSafeMode) {
      var ww = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                         .getService(Components.interfaces.nsIWindowWatcher);
      ww.openWindow(null, "chrome://browser/content/safeMode.xul", 
                    "_blank", "chrome,centerscreen,modal,resizable=no", null);
    }

    // initialize Places
    this._initPlaces();

    // indicate that the profile was initialized
    this._profileStarted = true;
  },

  _onProfileChange: function()
  {
    // this block is for code that depends on _onProfileStartup() having 
    // been called.
    if (this._profileStarted) {
      // final places cleanup
      this._shutdownPlaces();
    }
  },

  // profile shutdown handler (contains profile cleanup routines)
  _onProfileShutdown: function() 
  {
    // here we enter last survival area, in order to avoid multiple
    // "quit-application" notifications caused by late window closings
    const appStartup = Components.classes['@mozilla.org/toolkit/app-startup;1']
                                 .getService(Components.interfaces.nsIAppStartup);
    try {
      appStartup.enterLastWindowClosingSurvivalArea();

      this.Sanitizer.onShutdown();

    } catch(ex) {
    } finally {
      appStartup.exitLastWindowClosingSurvivalArea();
    }
  },

  // returns the (cached) Sanitizer constructor
  get Sanitizer() 
  {
    if(typeof(Sanitizer) != "function") { // we should dynamically load the script
      Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                .getService(Components.interfaces.mozIJSSubScriptLoader)
                .loadSubScript("chrome://browser/content/sanitize.js", null);
    }
    return Sanitizer;
  },

  /**
   * Initialize Places
   * - imports the bookmarks html file if bookmarks datastore is empty
   */
  _initPlaces: function bg__initPlaces() {
#ifdef MOZ_PLACES_BOOKMARKS
    // we need to instantiate the history service before we check the 
    // the browser.places.importBookmarksHTML pref, as 
    // nsNavHistory::ForceMigrateBookmarksDB() will set that pref
    // if we need to force a migration (due to a schema change)
    var histsvc = Components.classes["@mozilla.org/browser/nav-history-service;1"]
                            .getService(Components.interfaces.nsINavHistoryService);

    var importBookmarks = false;
    try {
      var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                  .getService(Components.interfaces.nsIPrefBranch);
      importBookmarks = prefService.getBoolPref("browser.places.importBookmarksHTML");
    } catch(ex) {}

    if (!importBookmarks)
      return;

    var dirService = Components.classes["@mozilla.org/file/directory_service;1"]
                               .getService(Components.interfaces.nsIProperties);

    var bookmarksFile = dirService.get("BMarks", Components.interfaces.nsILocalFile);

    if (bookmarksFile.exists()) {
      // import the file
      try {
        var importer = 
          Components.classes["@mozilla.org/browser/places/import-export-service;1"]
                    .getService(Components.interfaces.nsIPlacesImportExportService);
        importer.importHTMLFromFile(bookmarksFile, true);
      } catch(ex) {
      } finally {
        prefService.setBoolPref("browser.places.importBookmarksHTML", false);
      }

      // backup pre-places bookmarks.html
      // XXXtodo remove this before betas, after import/export is solid
      var profDir = dirService.get("ProfD", Components.interfaces.nsILocalFile);
      var bookmarksBackup = profDir.clone();
      bookmarksBackup.append("bookmarks.preplaces.html");
      if (!bookmarksBackup.exists()) {
        // save old bookmarks.html file as bookmarks.preplaces.html
        try {
          bookmarksFile.copyTo(profDir, "bookmarks.preplaces.html");
        } catch(ex) {
          dump("nsBrowserGlue::_initPlaces(): copy of bookmarks.html to bookmarks.preplaces.html failed: " + ex + "\n");
        }
      }
    }
#endif
  },

  /**
   * Places shut-down tasks
   * - back up and archive bookmarks
   */
  _shutdownPlaces: function bg__shutdownPlaces() {
#ifdef MOZ_PLACES_BOOKMARKS
    // backup bookmarks to bookmarks.html
    var importer =
      Components.classes["@mozilla.org/browser/places/import-export-service;1"]
                .getService(Components.interfaces.nsIPlacesImportExportService);
    importer.backupBookmarksFile();
#endif
  },
  
  // ------------------------------
  // public nsIBrowserGlue members
  // ------------------------------
  
  sanitize: function(aParentWindow) 
  {
    this.Sanitizer.sanitize(aParentWindow);
  }
}


// XPCOM Scaffolding code

// component defined in this file

const kServiceName = "Firefox Browser Glue Service";
const kServiceId = "{eab9012e-5f74-4cbc-b2b5-a590235513cc}";
const kServiceCtrId = "@mozilla.org/browser/browserglue;1";
const kServiceConstructor = BrowserGlue;

const kServiceCId = Components.ID(kServiceId);

// interfaces implemented by this component
const kServiceIIds = [ 
  Components.interfaces.nsIObserver,
  Components.interfaces.nsISupports,
  Components.interfaces.nsISupportsWeakReference,
  Components.interfaces.nsIBrowserGlue
  ];

// categories which this component is registered in
const kServiceCats = ["app-startup"];

// Factory object
const kServiceFactory = {
  _instance: null,
  createInstance: function (outer, iid) 
  {
    if (outer != null) throw Components.results.NS_ERROR_NO_AGGREGATION;

    xpcomCheckInterfaces(iid, kServiceIIds, 
                          Components.results.NS_ERROR_INVALID_ARG);
    return this._instance == null ?
      this._instance = new kServiceConstructor() : this._instance;
  }
};

function xpcomCheckInterfaces(iid, iids, ex) {
  for (var j = iids.length; j-- >0;) {
    if (iid.equals(iids[j])) return true;
  }
  throw ex;
}

// Module

var Module = {
  registered: false,
  
  registerSelf: function(compMgr, fileSpec, location, type) 
  {
    if (!this.registered) {
      compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar)
             .registerFactoryLocation(kServiceCId,
                                      kServiceName,
                                      kServiceCtrId, 
                                      fileSpec,
                                      location, 
                                      type);
      const catman = Components.classes['@mozilla.org/categorymanager;1']
                               .getService(Components.interfaces.nsICategoryManager);
      var len = kServiceCats.length;
      for (var j = 0; j < len; j++) {
        catman.addCategoryEntry(kServiceCats[j],
          kServiceCtrId, kServiceCtrId, true, true);
      }
      this.registered = true;
    } 
  },
  
  unregisterSelf: function(compMgr, fileSpec, location) 
  {
    compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar)
           .unregisterFactoryLocation(kServiceCId, fileSpec);
    const catman = Components.classes['@mozilla.org/categorymanager;1']
                             .getService(Components.interfaces.nsICategoryManager);
    var len = kServiceCats.length;
    for (var j = 0; j < len; j++) {
      catman.deleteCategoryEntry(kServiceCats[j], kServiceCtrId, true);
    }
  },
  
  getClassObject: function(compMgr, cid, iid) 
  {
    if(cid.equals(kServiceCId))
      return kServiceFactory;
    
    throw Components.results[
      iid.equals(Components.interfaces.nsIFactory)
      ? "NS_ERROR_NO_INTERFACE"
      : "NS_ERROR_NOT_IMPLEMENTED"
    ];
    
  },
  
  canUnload: function(compMgr) 
  {
    return true;
  }
};

// entrypoint
function NSGetModule(compMgr, fileSpec) {
  return Module;
}
