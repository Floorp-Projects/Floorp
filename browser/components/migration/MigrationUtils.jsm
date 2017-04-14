/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MigrationUtils", "MigratorPrototype"];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;
const TOPIC_WILL_IMPORT_BOOKMARKS = "initial-migration-will-import-default-bookmarks";
const TOPIC_DID_IMPORT_BOOKMARKS = "initial-migration-did-import-default-bookmarks";
const TOPIC_PLACES_DEFAULTS_FINISHED = "places-browser-init-complete";

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.importGlobalProperties(["URL"]);

XPCOMUtils.defineLazyModuleGetter(this, "AutoMigrate",
                                  "resource:///modules/AutoMigrate.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BookmarkHTMLUtils",
                                  "resource://gre/modules/BookmarkHTMLUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
                                  "resource://gre/modules/PromiseUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ResponsivenessMonitor",
                                  "resource://gre/modules/ResponsivenessMonitor.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch",
                                  "resource://gre/modules/TelemetryStopwatch.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WindowsRegistry",
                                  "resource://gre/modules/WindowsRegistry.jsm");

var gMigrators = null;
var gProfileStartup = null;
var gMigrationBundle = null;
var gPreviousDefaultBrowserKey = "";

let gKeepUndoData = false;
let gUndoData = null;

XPCOMUtils.defineLazyGetter(this, "gAvailableMigratorKeys", function() {
  if (AppConstants.platform == "win") {
    return [
      "firefox", "edge", "ie", "chrome", "chromium", "360se",
      "canary"
    ];
  }
  if (AppConstants.platform == "macosx") {
    return ["firefox", "safari", "chrome", "chromium", "canary"];
  }
  if (AppConstants.XP_UNIX) {
    return ["firefox", "chrome", "chromium"];
  }
  return [];
});

function getMigrationBundle() {
  if (!gMigrationBundle) {
    gMigrationBundle = Services.strings.createBundle(
     "chrome://browser/locale/migration/migration.properties");
  }
  return gMigrationBundle;
}

/**
 * Shared prototype for migrators, implementing nsIBrowserProfileMigrator.
 *
 * To implement a migrator:
 * 1. Import this module.
 * 2. Create the prototype for the migrator, extending MigratorPrototype.
 *    Namely: MosaicMigrator.prototype = Object.create(MigratorPrototype);
 * 3. Set classDescription, contractID and classID for your migrator, and set
 *    NSGetFactory appropriately.
 * 4. If the migrator supports multiple profiles, override the sourceProfiles
 *    Here we default for single-profile migrator.
 * 5. Implement getResources(aProfile) (see below).
 * 6. If the migrator supports reading the home page of the source browser,
 *    override |sourceHomePageURL| getter.
 * 7. For startup-only migrators, override |startupOnlyMigrator|.
 */
this.MigratorPrototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserProfileMigrator]),

  /**
   * OVERRIDE IF AND ONLY IF the source supports multiple profiles.
   *
   * Returns array of profile objects from which data may be imported. The object
   * should have the following keys:
   *   id - a unique string identifier for the profile
   *   name - a pretty name to display to the user in the UI
   *
   * Only profiles from which data can be imported should be listed.  Otherwise
   * the behavior of the migration wizard isn't well-defined.
   *
   * For a single-profile source (e.g. safari, ie), this returns null,
   * and not an empty array.  That is the default implementation.
   */
  get sourceProfiles() {
    return null;
  },

  /**
   * MUST BE OVERRIDDEN.
   *
   * Returns an array of "migration resources" objects for the given profile,
   * or for the "default" profile, if the migrator does not support multiple
   * profiles.
   *
   * Each migration resource should provide:
   * - a |type| getter, returning any of the migration types (see
   *   nsIBrowserProfileMigrator).
   *
   * - a |migrate| method, taking a single argument, aCallback(bool success),
   *   for migrating the data for this resource.  It may do its job
   *   synchronously or asynchronously.  Either way, it must call
   *   aCallback(bool aSuccess) when it's done.  In the case of an exception
   *   thrown from |migrate|, it's taken as if aCallback(false) is called.
   *
   *   Note: In the case of a simple asynchronous implementation, you may find
   *   MigrationUtils.wrapMigrateFunction handy for handling aCallback easily.
   *
   * For each migration type listed in nsIBrowserProfileMigrator, multiple
   * migration resources may be provided.  This practice is useful when the
   * data for a certain migration type is independently stored in few
   * locations.  For example, the mac version of Safari stores its "reading list"
   * bookmarks in a separate property list.
   *
   * Note that the importation of a particular migration type is reported as
   * successful if _any_ of its resources succeeded to import (that is, called,
   * |aCallback(true)|).  However, completion-status for a particular migration
   * type is reported to the UI only once all of its migrators have called
   * aCallback.
   *
   * @note  The returned array should only include resources from which data
   *        can be imported.  So, for example, before adding a resource for the
   *        BOOKMARKS migration type, you should check if you should check that the
   *        bookmarks file exists.
   *
   * @param aProfile
   *        The profile from which data may be imported, or an empty string
   *        in the case of a single-profile migrator.
   *        In the case of multiple-profiles migrator, it is guaranteed that
   *        aProfile is a value returned by the sourceProfiles getter (see
   *        above).
   */
  getResources: function MP_getResources(/* aProfile */) {
    throw new Error("getResources must be overridden");
  },

  /**
   * OVERRIDE in order to provide an estimate of when the last time was
   * that somebody used the browser. It is OK that this is somewhat fuzzy -
   * history may not be available (or be wiped or not present due to e.g.
   * incognito mode).
   *
   * @return a Promise that resolves to the last used date.
   *
   * @note If not overridden, the promise will resolve to the unix epoch.
   */
  getLastUsedDate() {
    return Promise.resolve(new Date(0));
  },

  /**
   * OVERRIDE IF AND ONLY IF the migrator is a startup-only migrator (For now,
   * that is just the Firefox migrator, see bug 737381).  Default: false.
   *
   * Startup-only migrators are different in two ways:
   * - they may only be used during startup.
   * - the user-profile is half baked during migration.  The folder exists,
   *   but it's only accessible through MigrationUtils.profileStartup.
   *   The migrator can call MigrationUtils.profileStartup.doStartup
   *   at any point in order to initialize the profile.
   */
  get startupOnlyMigrator() {
    return false;
  },

  /**
   * OVERRIDE IF AND ONLY IF your migrator supports importing the homepage.
   * @see nsIBrowserProfileMigrator
   */
  get sourceHomePageURL() {
    return "";
  },

  /**
   * Override if the data to migrate is locked/in-use and the user should
   * probably shutdown the source browser.
   */
  get sourceLocked() {
    return false;
  },

  /**
   * DO NOT OVERRIDE - After deCOMing migration, the UI will just call
   * getResources.
   *
   * @see nsIBrowserProfileMigrator
   */
  getMigrateData: function MP_getMigrateData(aProfile) {
    let resources = this._getMaybeCachedResources(aProfile);
    if (!resources) {
      return [];
    }
    let types = resources.map(r => r.type);
    return types.reduce((a, b) => { a |= b; return a }, 0);
  },

  getBrowserKey: function MP_getBrowserKey() {
    return this.contractID.match(/\=([^\=]+)$/)[1];
  },

  /**
   * DO NOT OVERRIDE - After deCOMing migration, the UI will just call
   * migrate for each resource.
   *
   * @see nsIBrowserProfileMigrator
   */
  migrate: function MP_migrate(aItems, aStartup, aProfile) {
    let resources = this._getMaybeCachedResources(aProfile);
    if (resources.length == 0)
      throw new Error("migrate called for a non-existent source");

    if (aItems != Ci.nsIBrowserProfileMigrator.ALL)
      resources = resources.filter(r => aItems & r.type);

    // Used to periodically give back control to the main-thread loop.
    let unblockMainThread = function() {
      return new Promise(resolve => {
        Services.tm.mainThread.dispatch(resolve, Ci.nsIThread.DISPATCH_NORMAL);
      });
    };

    let getHistogramIdForResourceType = (resourceType, template) => {
      if (resourceType == MigrationUtils.resourceTypes.HISTORY) {
        return template.replace("*", "HISTORY");
      }
      if (resourceType == MigrationUtils.resourceTypes.BOOKMARKS) {
        return template.replace("*", "BOOKMARKS");
      }
      if (resourceType == MigrationUtils.resourceTypes.PASSWORDS) {
        return template.replace("*", "LOGINS");
      }
      return null;
    };

    let browserKey = this.getBrowserKey();

    let maybeStartTelemetryStopwatch = resourceType => {
      let histogramId = getHistogramIdForResourceType(resourceType, "FX_MIGRATION_*_IMPORT_MS");
      if (histogramId) {
        TelemetryStopwatch.startKeyed(histogramId, browserKey);
      }
      return histogramId;
    };

    let maybeStartResponsivenessMonitor = resourceType => {
      let responsivenessMonitor;
      let responsivenessHistogramId =
        getHistogramIdForResourceType(resourceType, "FX_MIGRATION_*_JANK_MS");
      if (responsivenessHistogramId) {
        responsivenessMonitor = new ResponsivenessMonitor();
      }
      return {responsivenessMonitor, responsivenessHistogramId};
    };

    let maybeFinishResponsivenessMonitor = (responsivenessMonitor, histogramId) => {
      if (responsivenessMonitor) {
        let accumulatedDelay = responsivenessMonitor.finish();
        if (histogramId) {
          try {
            Services.telemetry.getKeyedHistogramById(histogramId)
                    .add(browserKey, accumulatedDelay);
          } catch (ex) {
            Cu.reportError(histogramId + ": " + ex);
          }
        }
      }
    };

    let collectQuantityTelemetry = () => {
      for (let resourceType of Object.keys(MigrationUtils._importQuantities)) {
        let histogramId =
          "FX_MIGRATION_" + resourceType.toUpperCase() + "_QUANTITY";
        try {
          Services.telemetry.getKeyedHistogramById(histogramId)
                  .add(browserKey, MigrationUtils._importQuantities[resourceType]);
        } catch (ex) {
          Cu.reportError(histogramId + ": " + ex);
        }
      }
    };

    // Called either directly or through the bookmarks import callback.
    let doMigrate = Task.async(function*() {
      let resourcesGroupedByItems = new Map();
      resources.forEach(function(resource) {
        if (!resourcesGroupedByItems.has(resource.type)) {
          resourcesGroupedByItems.set(resource.type, new Set());
        }
        resourcesGroupedByItems.get(resource.type).add(resource);
      });

      if (resourcesGroupedByItems.size == 0)
        throw new Error("No items to import");

      let notify = function(aMsg, aItemType) {
        Services.obs.notifyObservers(null, aMsg, aItemType);
      };

      for (let resourceType of Object.keys(MigrationUtils._importQuantities)) {
        MigrationUtils._importQuantities[resourceType] = 0;
      }
      notify("Migration:Started");
      for (let [migrationType, itemResources] of resourcesGroupedByItems) {
        notify("Migration:ItemBeforeMigrate", migrationType);

        let stopwatchHistogramId = maybeStartTelemetryStopwatch(migrationType);

        let {responsivenessMonitor, responsivenessHistogramId} =
          maybeStartResponsivenessMonitor(migrationType);

        let itemSuccess = false;
        for (let res of itemResources) {
          let completeDeferred = PromiseUtils.defer();
          let resourceDone = function(aSuccess) {
            itemResources.delete(res);
            itemSuccess |= aSuccess;
            if (itemResources.size == 0) {
              notify(itemSuccess ?
                     "Migration:ItemAfterMigrate" : "Migration:ItemError",
                     migrationType);
              resourcesGroupedByItems.delete(migrationType);

              if (stopwatchHistogramId) {
                TelemetryStopwatch.finishKeyed(stopwatchHistogramId, browserKey);
              }

              maybeFinishResponsivenessMonitor(responsivenessMonitor, responsivenessHistogramId);

              if (resourcesGroupedByItems.size == 0) {
                collectQuantityTelemetry();
                notify("Migration:Ended");
              }
            }
            completeDeferred.resolve();
          };

          // If migrate throws, an error occurred, and the callback
          // (itemMayBeDone) might haven't been called.
          try {
            res.migrate(resourceDone);
          } catch (ex) {
            Cu.reportError(ex);
            resourceDone(false);
          }

          // Certain resources must be ran sequentially or they could fail,
          // for example bookmarks and history (See bug 1272652).
          if (migrationType == MigrationUtils.resourceTypes.BOOKMARKS ||
              migrationType == MigrationUtils.resourceTypes.HISTORY) {
            yield completeDeferred.promise;
          }

          yield unblockMainThread();
        }
      }
    });

    if (MigrationUtils.isStartupMigration && !this.startupOnlyMigrator) {
      MigrationUtils.profileStartup.doStartup();
      // First import the default bookmarks.
      // Note: We do not need to do so for the Firefox migrator
      // (=startupOnlyMigrator), as it just copies over the places database
      // from another profile.
      Task.spawn(function* () {
        // Tell nsBrowserGlue we're importing default bookmarks.
        let browserGlue = Cc["@mozilla.org/browser/browserglue;1"].
                          getService(Ci.nsIObserver);
        browserGlue.observe(null, TOPIC_WILL_IMPORT_BOOKMARKS, "");

        // Import the default bookmarks. We ignore whether or not we succeed.
        yield BookmarkHTMLUtils.importFromURL(
          "chrome://browser/locale/bookmarks.html", true).catch(r => r);

        // We'll tell nsBrowserGlue we've imported bookmarks, but before that
        // we need to make sure we're going to know when it's finished
        // initializing places:
        let placesInitedPromise = new Promise(resolve => {
          let onPlacesInited = function() {
            Services.obs.removeObserver(onPlacesInited, TOPIC_PLACES_DEFAULTS_FINISHED);
            resolve();
          };
          Services.obs.addObserver(onPlacesInited, TOPIC_PLACES_DEFAULTS_FINISHED);
        });
        browserGlue.observe(null, TOPIC_DID_IMPORT_BOOKMARKS, "");
        yield placesInitedPromise;
        doMigrate();
      });
      return;
    }
    doMigrate();
  },

  /**
   * DO NOT OVERRIDE - After deCOMing migration, this code
   * won't be part of the migrator itself.
   *
   * @see nsIBrowserProfileMigrator
   */
  get sourceExists() {
    if (this.startupOnlyMigrator && !MigrationUtils.isStartupMigration)
      return false;

    // For a single-profile source, check if any data is available.
    // For multiple-profiles source, make sure that at least one
    // profile is available.
    let exists = false;
    try {
      let profiles = this.sourceProfiles;
      if (!profiles) {
        let resources = this._getMaybeCachedResources("");
        if (resources && resources.length > 0)
          exists = true;
      } else {
        exists = profiles.length > 0;
      }
    } catch (ex) {
      Cu.reportError(ex);
    }
    return exists;
  },

  /** * PRIVATE STUFF - DO NOT OVERRIDE ***/
  _getMaybeCachedResources: function PMB__getMaybeCachedResources(aProfile) {
    let profileKey = aProfile ? aProfile.id : "";
    if (this._resourcesByProfile) {
      if (profileKey in this._resourcesByProfile)
        return this._resourcesByProfile[profileKey];
    } else {
      this._resourcesByProfile = { };
    }
    this._resourcesByProfile[profileKey] = this.getResources(aProfile);
    return this._resourcesByProfile[profileKey];
  }
};

this.MigrationUtils = Object.freeze({
  resourceTypes: {
    SETTINGS:   Ci.nsIBrowserProfileMigrator.SETTINGS,
    COOKIES:    Ci.nsIBrowserProfileMigrator.COOKIES,
    HISTORY:    Ci.nsIBrowserProfileMigrator.HISTORY,
    FORMDATA:   Ci.nsIBrowserProfileMigrator.FORMDATA,
    PASSWORDS:  Ci.nsIBrowserProfileMigrator.PASSWORDS,
    BOOKMARKS:  Ci.nsIBrowserProfileMigrator.BOOKMARKS,
    OTHERDATA:  Ci.nsIBrowserProfileMigrator.OTHERDATA,
    SESSION:    Ci.nsIBrowserProfileMigrator.SESSION,
  },

  /**
   * Helper for implementing simple asynchronous cases of migration resources'
   * |migrate(aCallback)| (see MigratorPrototype).  If your |migrate| method
   * just waits for some file to be read, for example, and then migrates
   * everything right away, you can wrap the async-function with this helper
   * and not worry about notifying the callback.
   *
   * For example, instead of writing:
   * setTimeout(function() {
   *   try {
   *     ....
   *     aCallback(true);
   *   }
   *   catch() {
   *     aCallback(false);
   *   }
   * }, 0);
   *
   * You may write:
   * setTimeout(MigrationUtils.wrapMigrateFunction(function() {
   *   if (importingFromMosaic)
   *     throw Cr.NS_ERROR_UNEXPECTED;
   * }, aCallback), 0);
   *
   * ... and aCallback will be called with aSuccess=false when importing
   * from Mosaic, or with aSuccess=true otherwise.
   *
   * @param aFunction
   *        the function that will be called sometime later.  If aFunction
   *        throws when it's called, aCallback(false) is called, otherwise
   *        aCallback(true) is called.
   * @param aCallback
   *        the callback function passed to |migrate|.
   * @return the wrapped function.
   */
  wrapMigrateFunction: function MU_wrapMigrateFunction(aFunction, aCallback) {
    return function() {
      let success = false;
      try {
        aFunction.apply(null, arguments);
        success = true;
      } catch (ex) {
        Cu.reportError(ex);
      }
      // Do not change this to call aCallback directly in try try & catch
      // blocks, because if aCallback throws, we may end up calling aCallback
      // twice.
      aCallback(success);
    };
  },

  /**
   * Gets a string from the migration bundle.  Shorthand for
   * nsIStringBundle.GetStringFromName, if aReplacements isn't passed, or for
   * nsIStringBundle.formatStringFromName if it is.
   *
   * This method also takes care of "bumped" keys (See bug 737381 comment 8 for
   * details).
   *
   * @param aKey
   *        The key of the string to retrieve.
   * @param aReplacements
   *        [optioanl] Array of replacements to run on the retrieved string.
   * @return the retrieved string.
   *
   * @see nsIStringBundle
   */
  getLocalizedString: function MU_getLocalizedString(aKey, aReplacements) {
    aKey = aKey.replace(/_(canary|chromium)$/, "_chrome");

    const OVERRIDES = {
      "4_firefox": "4_firefox_history_and_bookmarks",
      "64_firefox": "64_firefox_other"
    };
    aKey = OVERRIDES[aKey] || aKey;

    if (aReplacements === undefined)
      return getMigrationBundle().GetStringFromName(aKey);
    return getMigrationBundle().formatStringFromName(
      aKey, aReplacements, aReplacements.length);
  },

  _getLocalePropertyForBrowser(browserId) {
    switch (browserId) {
      case "edge":
        return "sourceNameEdge";
      case "ie":
        return "sourceNameIE";
      case "safari":
        return "sourceNameSafari";
      case "canary":
        return "sourceNameCanary";
      case "chrome":
        return "sourceNameChrome";
      case "chromium":
        return "sourceNameChromium";
      case "firefox":
        return "sourceNameFirefox";
      case "360se":
        return "sourceName360se";
    }
    return null;
  },

  getBrowserName(browserId) {
    let prop = this._getLocalePropertyForBrowser(browserId);
    if (prop) {
      return this.getLocalizedString(prop);
    }
    return null;
  },

  /**
   * Helper for creating a folder for imported bookmarks from a particular
   * migration source.  The folder is created at the end of the given folder.
   *
   * @param sourceNameStr
   *        the source name (first letter capitalized).  This is used
   *        for reading the localized source name from the migration
   *        bundle (e.g. if aSourceNameStr is Mosaic, this will try to read
   *        sourceNameMosaic from the migration bundle).
   * @param parentGuid
   *        the GUID of the folder in which the new folder should be created.
   * @return the GUID of the new folder.
   */
  createImportedBookmarksFolder: Task.async(function* (sourceNameStr, parentGuid) {
    let source = this.getLocalizedString("sourceName" + sourceNameStr);
    let title = this.getLocalizedString("importedBookmarksFolder", [source]);
    return (yield PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER, parentGuid, title
    })).guid;
  }),

  /**
   * Get all the rows corresponding to a select query from a database, without
   * requiring a lock on the database. If fetching data fails (because someone
   * else tried to write to the DB at the same time, for example), we will
   * retry the fetch after a 100ms timeout, up to 10 times.
   *
   * @param path
   *        the file path to the database we want to open.
   * @param description
   *        a developer-readable string identifying what kind of database we're
   *        trying to open.
   * @param selectQuery
   *        the SELECT query to use to fetch the rows.
   *
   * @return a promise that resolves to an array of rows. The promise will be
   *         rejected if the read/fetch failed even after retrying.
   */
  getRowsFromDBWithoutLocks(path, description, selectQuery) {
    let dbOptions = {
      readOnly: true,
      ignoreLockingMode: true,
      path,
    };

    const RETRYLIMIT = 10;
    const RETRYINTERVAL = 100;
    return Task.spawn(function* innerGetRows() {
      let rows = null;
      for (let retryCount = RETRYLIMIT; retryCount && !rows; retryCount--) {
        // Attempt to get the rows. If this succeeds, we will bail out of the loop,
        // close the database in a failsafe way, and pass the rows back.
        // If fetching the rows throws, we will wait RETRYINTERVAL ms
        // and try again. This will repeat a maximum of RETRYLIMIT times.
        let db;
        let didOpen = false;
        let exceptionSeen;
        try {
          db = yield Sqlite.openConnection(dbOptions);
          didOpen = true;
          rows = yield db.execute(selectQuery);
        } catch (ex) {
          if (!exceptionSeen) {
            Cu.reportError(ex);
          }
          exceptionSeen = ex;
        } finally {
          try {
            if (didOpen) {
              yield db.close();
            }
          } catch (ex) {}
        }
        if (exceptionSeen) {
          yield new Promise(resolve => setTimeout(resolve, RETRYINTERVAL));
        }
      }
      if (!rows) {
        throw new Error("Couldn't get rows from the " + description + " database.");
      }
      return rows;
    });
  },

  get _migrators() {
    if (!gMigrators) {
      gMigrators = new Map();
    }
    return gMigrators;
  },

  /*
   * Returns the migrator for the given source, if any data is available
   * for this source, or null otherwise.
   *
   * @param aKey internal name of the migration source.
   *             Supported values: ie (windows),
   *                               edge (windows),
   *                               safari (mac),
   *                               canary (mac/windows),
   *                               chrome (mac/windows/linux),
   *                               chromium (mac/windows/linux),
   *                               360se (windows),
   *                               firefox.
   *
   * If null is returned,  either no data can be imported
   * for the given migrator, or aMigratorKey is invalid  (e.g. ie on mac,
   * or mosaic everywhere).  This method should be used rather than direct
   * getService for future compatibility (see bug 718280).
   *
   * @return profile migrator implementing nsIBrowserProfileMigrator, if it can
   *         import any data, null otherwise.
   */
  getMigrator: function MU_getMigrator(aKey) {
    let migrator = null;
    if (this._migrators.has(aKey)) {
      migrator = this._migrators.get(aKey);
    } else {
      try {
        migrator = Cc["@mozilla.org/profile/migrator;1?app=browser&type=" +
                      aKey].createInstance(Ci.nsIBrowserProfileMigrator);
      } catch (ex) { Cu.reportError(ex) }
      this._migrators.set(aKey, migrator);
    }

    try {
      return migrator && migrator.sourceExists ? migrator : null;
    } catch (ex) { Cu.reportError(ex); return null }
  },

  /**
   * Figure out what is the default browser, and if there is a migrator
   * for it, return that migrator's internal name.
   * For the time being, the "internal name" of a migrator is its contract-id
   * trailer (e.g. ie for @mozilla.org/profile/migrator;1?app=browser&type=ie),
   * but it will soon be exposed properly.
   */
  getMigratorKeyForDefaultBrowser() {
    // Canary uses the same description as Chrome so we can't distinguish them.
    const APP_DESC_TO_KEY = {
      "Internet Explorer":                 "ie",
      "Microsoft Edge":                    "edge",
      "Safari":                            "safari",
      "Firefox":                           "firefox",
      "Nightly":                           "firefox",
      "Google Chrome":                     "chrome",  // Windows, Linux
      "Chrome":                            "chrome",  // OS X
      "Chromium":                          "chromium", // Windows, OS X
      "Chromium Web Browser":              "chromium", // Linux
      "360\u5b89\u5168\u6d4f\u89c8\u5668": "360se",
    };

    let key = "";
    try {
      let browserDesc =
        Cc["@mozilla.org/uriloader/external-protocol-service;1"]
          .getService(Ci.nsIExternalProtocolService)
          .getApplicationDescription("http");
      key = APP_DESC_TO_KEY[browserDesc] || "";
      // Handle devedition, as well as "FirefoxNightly" on OS X.
      if (!key && browserDesc.startsWith("Firefox")) {
        key = "firefox";
      }
    } catch (ex) {
      Cu.reportError("Could not detect default browser: " + ex);
    }

    // "firefox" is the least useful entry here, and might just be because we've set
    // ourselves as the default (on Windows 7 and below). In that case, check if we
    // have a registry key that tells us where to go:
    if (key == "firefox" && AppConstants.isPlatformAndVersionAtMost("win", "6.2")) {
      // Because we remove the registry key, reading the registry key only works once.
      // We save the value for subsequent calls to avoid hard-to-trace bugs when multiple
      // consumers ask for this key.
      if (gPreviousDefaultBrowserKey) {
        key = gPreviousDefaultBrowserKey;
      } else {
        // We didn't have a saved value, so check the registry.
        const kRegPath = "Software\\Mozilla\\Firefox";
        let oldDefault = WindowsRegistry.readRegKey(
            Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, kRegPath, "OldDefaultBrowserCommand");
        if (oldDefault) {
          // Remove the key:
          WindowsRegistry.removeRegKey(
            Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, kRegPath, "OldDefaultBrowserCommand");
          try {
            let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFileWin);
            file.initWithCommandLine(oldDefault);
            key = APP_DESC_TO_KEY[file.getVersionInfoField("FileDescription")] || key;
            // Save the value for future callers.
            gPreviousDefaultBrowserKey = key;
          } catch (ex) {
            Cu.reportError("Could not convert old default browser value to description.");
          }
        }
      }
    }
    return key;
  },

  // Whether or not we're in the process of startup migration
  get isStartupMigration() {
    return gProfileStartup != null;
  },

  /**
   * In the case of startup migration, this is set to the nsIProfileStartup
   * instance passed to ProfileMigrator's migrate.
   *
   * @see showMigrationWizard
   */
  get profileStartup() {
    return gProfileStartup;
  },

  /**
   * Show the migration wizard.  On mac, this may just focus the wizard if it's
   * already running, in which case aOpener and aParams are ignored.
   *
   * @param {Window} [aOpener]
   *        optional; the window that asks to open the wizard.
   * @param {Array} [aParams]
   *        optional arguments for the migration wizard, in the form of an array
   *        This is passed as-is for the params argument of
   *        nsIWindowWatcher.openWindow. The array elements we expect are, in
   *        order:
   *        - {Number} migration entry point constant (see below)
   *        - {String} source browser identifier
   *        - {nsIBrowserProfileMigrator} actual migrator object
   *        - {Boolean} whether this is a startup migration
   *        - {Boolean} whether to skip the 'source' page
   *        - {String} an identifier for the profile to use when migrating
   *        NB: If you add new consumers, please add a migration entry point
   *        constant below, and specify at least the first element of the array
   *        (the migration entry point for purposes of telemetry).
   */
  showMigrationWizard:
  function MU_showMigrationWizard(aOpener, aParams) {
    let features = "chrome,dialog,modal,centerscreen,titlebar,resizable=no";
    if (AppConstants.platform == "macosx" && !this.isStartupMigration) {
      let win = Services.wm.getMostRecentWindow("Browser:MigrationWizard");
      if (win) {
        win.focus();
        return;
      }
      // On mac, the migration wiazrd should only be modal in the case of
      // startup-migration.
      features = "centerscreen,chrome,resizable=no";
    }

    // nsIWindowWatcher doesn't deal with raw arrays, so we convert the input
    let params;
    if (Array.isArray(aParams)) {
      params = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
      for (let item of aParams) {
        let comtaminatedVal;
        if (item && item instanceof Ci.nsISupports) {
          comtaminatedVal = item;
        } else {
          switch (typeof item) {
            case "boolean":
              comtaminatedVal = Cc["@mozilla.org/supports-PRBool;1"].
                                createInstance(Ci.nsISupportsPRBool);
              comtaminatedVal.data = item;
              break;
            case "number":
              comtaminatedVal = Cc["@mozilla.org/supports-PRUint32;1"].
                                createInstance(Ci.nsISupportsPRUint32);
              comtaminatedVal.data = item;
              break;
            case "string":
              comtaminatedVal = Cc["@mozilla.org/supports-cstring;1"].
                                createInstance(Ci.nsISupportsCString);
              comtaminatedVal.data = item;
              break;

            case "undefined":
            case "object":
              if (!item) {
                comtaminatedVal = null;
                break;
              }
              /* intentionally falling through to error out here for
                 non-null/undefined things: */
            default:
              throw new Error("Unexpected parameter type " + (typeof item) + ": " + item);
          }
        }
        params.appendElement(comtaminatedVal);
      }
    } else {
      params = aParams;
    }

    Services.ww.openWindow(aOpener,
                           "chrome://browser/content/migration/migration.xul",
                           "_blank",
                           features,
                           params);
  },

  /**
   * Show the migration wizard for startup-migration.  This should only be
   * called by ProfileMigrator (see ProfileMigrator.js), which implements
   * nsIProfileMigrator.
   *
   * @param aProfileStartup
   *        the nsIProfileStartup instance provided to ProfileMigrator.migrate.
   * @param [optional] aMigratorKey
   *        If set, the migration wizard will import from the corresponding
   *        migrator, bypassing the source-selection page.  Otherwise, the
   *        source-selection page will be displayed, either with the default
   *        browser selected, if it could be detected and if there is a
   *        migrator for it, or with the first option selected as a fallback
   *        (The first option is hardcoded to be the most common browser for
   *         the OS we run on.  See migration.xul).
   * @param [optional] aProfileToMigrate
   *        If set, the migration wizard will import from the profile indicated.
   * @throws if aMigratorKey is invalid or if it points to a non-existent
   *         source.
   */
  startupMigration:
  function MU_startupMigrator(aProfileStartup, aMigratorKey, aProfileToMigrate) {
    if (!aProfileStartup) {
      throw new Error("an profile-startup instance is required for startup-migration");
    }
    gProfileStartup = aProfileStartup;

    let skipSourcePage = false, migrator = null, migratorKey = "";
    if (aMigratorKey) {
      migrator = this.getMigrator(aMigratorKey);
      if (!migrator) {
        // aMigratorKey must point to a valid source, so, if it doesn't
        // cleanup and throw.
        this.finishMigration();
        throw new Error("startMigration was asked to open auto-migrate from " +
                        "a non-existent source: " + aMigratorKey);
      }
      migratorKey = aMigratorKey;
      skipSourcePage = true;
    } else {
      let defaultBrowserKey = this.getMigratorKeyForDefaultBrowser();
      if (defaultBrowserKey) {
        migrator = this.getMigrator(defaultBrowserKey);
        if (migrator)
          migratorKey = defaultBrowserKey;
      }
    }

    if (!migrator) {
      // If there's no migrator set so far, ensure that there is at least one
      // migrator available before opening the wizard.
      // Note that we don't need to check the default browser first, because
      // if that one existed we would have used it in the block above this one.
      if (!gAvailableMigratorKeys.some(key => !!this.getMigrator(key))) {
        // None of the keys produced a usable migrator, so finish up here:
        this.finishMigration();
        return;
      }
    }

    let isRefresh = migrator && skipSourcePage &&
                    migratorKey == AppConstants.MOZ_APP_NAME;

    if (!isRefresh && AutoMigrate.enabled) {
      try {
        AutoMigrate.migrate(aProfileStartup, migratorKey, aProfileToMigrate);
        return;
      } catch (ex) {
        // If automigration failed, continue and show the dialog.
        Cu.reportError(ex);
      }
    }

    let migrationEntryPoint = this.MIGRATION_ENTRYPOINT_FIRSTRUN;
    if (isRefresh) {
      migrationEntryPoint = this.MIGRATION_ENTRYPOINT_FXREFRESH;
    }

    let params = [
      migrationEntryPoint,
      migratorKey,
      migrator,
      aProfileStartup,
      skipSourcePage,
      aProfileToMigrate,
    ];
    this.showMigrationWizard(null, params);
  },

  _importQuantities: {
    bookmarks: 0,
    logins: 0,
    history: 0,
  },

  insertBookmarkWrapper(bookmark) {
    this._importQuantities.bookmarks++;
    let insertionPromise = PlacesUtils.bookmarks.insert(bookmark);
    if (!gKeepUndoData) {
      return insertionPromise;
    }
    // If we keep undo data, add a promise handler that stores the undo data once
    // the bookmark has been inserted in the DB, and then returns the bookmark.
    let {parentGuid} = bookmark;
    return insertionPromise.then(bm => {
      let {guid, lastModified, type} = bm;
      gUndoData.get("bookmarks").push({
        parentGuid, guid, lastModified, type
      });
      return bm;
    });
  },

  insertManyBookmarksWrapper(bookmarks, parent) {
    let insertionPromise = PlacesUtils.bookmarks.insertTree({guid: parent, children: bookmarks});
    return insertionPromise.then(insertedItems => {
      this._importQuantities.bookmarks += insertedItems.length;
      if (gKeepUndoData) {
        let bmData = gUndoData.get("bookmarks");
        for (let bm of insertedItems) {
          let {parentGuid, guid, lastModified, type} = bm;
          bmData.push({parentGuid, guid, lastModified, type});
        }
      }
    }, ex => Cu.reportError(ex));
  },

  insertVisitsWrapper(places, options) {
    this._importQuantities.history += places.length;
    if (gKeepUndoData) {
      this._updateHistoryUndo(places);
    }
    return PlacesUtils.asyncHistory.updatePlaces(places, options, true);
  },

  insertLoginWrapper(login) {
    this._importQuantities.logins++;
    let insertedLogin = LoginHelper.maybeImportLogin(login);
    // Note that this means that if we import a login that has a newer password
    // than we know about, we will update the login, and an undo of the import
    // will not revert this. This seems preferable over removing the login
    // outright or storing the old password in the undo file.
    if (insertedLogin && gKeepUndoData) {
      let {guid, timePasswordChanged} = insertedLogin;
      gUndoData.get("logins").push({guid, timePasswordChanged});
    }
  },

  initializeUndoData() {
    gKeepUndoData = true;
    gUndoData = new Map([["bookmarks", []], ["visits", []], ["logins", []]]);
  },

  _postProcessUndoData: Task.async(function*(state) {
    if (!state) {
      return state;
    }
    let bookmarkFolders = state.get("bookmarks").filter(b => b.type == PlacesUtils.bookmarks.TYPE_FOLDER);

    let bookmarkFolderData = [];
    let bmPromises = bookmarkFolders.map(({guid}) => {
      // Ignore bookmarks where the promise doesn't resolve (ie that are missing)
      // Also check that the bookmark fetch returns isn't null before adding it.
      return PlacesUtils.bookmarks.fetch(guid).then(bm => bm && bookmarkFolderData.push(bm), () => {});
    });

    yield Promise.all(bmPromises);
    let folderLMMap = new Map(bookmarkFolderData.map(b => [b.guid, b.lastModified]));
    for (let bookmark of bookmarkFolders) {
      let lastModified = folderLMMap.get(bookmark.guid);
      // If the bookmark was deleted, the map will be returning null, so check:
      if (lastModified) {
        bookmark.lastModified = lastModified;
      }
    }
    return state;
  }),

  stopAndRetrieveUndoData() {
    let undoData = gUndoData;
    gUndoData = null;
    gKeepUndoData = false;
    return this._postProcessUndoData(undoData);
  },

  _updateHistoryUndo(places) {
    let visits = gUndoData.get("visits");
    let visitMap = new Map(visits.map(v => [v.url, v]));
    for (let place of places) {
      let visitCount = place.visits.length;
      let first, last;
      if (visitCount > 1) {
        let visitDates = place.visits.map(v => v.visitDate);
        first = Math.min.apply(Math, visitDates);
        last = Math.max.apply(Math, visitDates);
      } else {
        first = last = place.visits[0].visitDate;
      }
      let url = place.uri.spec;
      try {
        new URL(url);
      } catch (ex) {
        // This won't save and we won't need to 'undo' it, so ignore this URL.
        continue;
      }
      if (!visitMap.has(url)) {
        visitMap.set(url, {url, visitCount, first, last});
      } else {
        let currentData = visitMap.get(url);
        currentData.visitCount += visitCount;
        currentData.first = Math.min(currentData.first, first);
        currentData.last = Math.max(currentData.last, last);
      }
    }
    gUndoData.set("visits", Array.from(visitMap.values()));
  },

  /**
   * Cleans up references to migrators and nsIProfileInstance instances.
   */
  finishMigration: function MU_finishMigration() {
    gMigrators = null;
    gProfileStartup = null;
    gMigrationBundle = null;
  },

  gAvailableMigratorKeys,

  MIGRATION_ENTRYPOINT_UNKNOWN: 0,
  MIGRATION_ENTRYPOINT_FIRSTRUN: 1,
  MIGRATION_ENTRYPOINT_FXREFRESH: 2,
  MIGRATION_ENTRYPOINT_PLACES: 3,
  MIGRATION_ENTRYPOINT_PASSWORDS: 4,

  _sourceNameToIdMapping: {
    "nothing":    1,
    "firefox":    2,
    "edge":       3,
    "ie":         4,
    "chrome":     5,
    "chromium":   6,
    "canary":     7,
    "safari":     8,
    "360se":      9,
  },
  getSourceIdForTelemetry(sourceName) {
    return this._sourceNameToIdMapping[sourceName] || 0;
  },
});
