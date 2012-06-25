/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let EXPORTED_SYMBOLS = ["MigrationUtils", "MigratorPrototype"];

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;

const TOPIC_WILL_IMPORT_BOOKMARKS = "initial-migration-will-import-default-bookmarks";
const TOPIC_DID_IMPORT_BOOKMARKS = "initial-migration-did-import-default-bookmarks";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Dict",
                                  "resource://gre/modules/Dict.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BookmarkHTMLUtils",
                                  "resource://gre/modules/BookmarkHTMLUtils.jsm");

let gMigrators = null;
let gProfileStartup = null;
let gMigrationBundle = null;

function getMigrationBundle() {
  if (!gMigrationBundle) {
    gMigrationBundle = Services.strings.createBundle(
     "chrome://browser/locale/migration/migration.properties"); 
  }
  return gMigrationBundle;
}

/**
 * Figure out what is the default browser, and if there is a migrator
 * for it, return that migrator's internal name.
 * For the time being, the "internal name" of a migraotr is its contract-id
 * trailer (e.g. ie for @mozilla.org/profile/migrator;1?app=browser&type=ie),
 * but it will soon be exposed properly.
 */
function getMigratorKeyForDefaultBrowser() {
  // Don't map Firefox to the Firefox migrator, because we don't
  // expect it to ever show up as an option in the wizard.
  // We may want to revise this if/when we use separate profiles
  // for each Firefox-update channel.
  const APP_DESC_TO_KEY = {
    "Internet Explorer": "ie",
    "Safari":            "safari",
    "Google Chrome":     "chrome",  // Windows, Linux
    "Chrome":            "chrome",  // OS X
  };

  let browserDesc = "";
  try {
    let browserDesc =
      Cc["@mozilla.org/uriloader/external-protocol-service;1"].
      getService(Ci.nsIExternalProtocolService).
      getApplicationDescription("http");
    return APP_DESC_TO_KEY[browserDesc] || "";
  }
  catch(ex) {
    Cu.reportError("Could not detect default browser: " + ex);
  }
  return "";
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
let MigratorPrototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIBrowserProfileMigrator]),

  /**
   * OVERRIDE IF AND ONLY IF the source supports multiple profiles.
   *
   * Returns array of profiles (by names) from which data may be imported.
   *
   * Only profiles from which data can be imported should be listed.  Otherwise
   * the behavior of the migration wizard isn't well-defined.
   *
   * For a single-profile source (e.g. safari, ie), this returns null,
   * and not an empty array.  That is the default implementation.
   */
  get sourceProfiles() null,

  /**
   * MUST BE OVERRIDDEN.
   *
   * Returns an array of "migration resources" objects for the given profile,
   * or for the "default" profile, if the migrator does not support multiple
   * profiles.
   *
   * Each migration resource should provide:
   * - a |type| getter, retunring any of the migration types (see
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
  getResources: function MP_getResources(aProfile) {
    throw new Error("getResources must be overridden");
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
  get startupOnlyMigrator() false,

  /**
   * OVERRIDE IF AND ONLY IF your migrator supports importing the homepage.
   * @see nsIBrowserProfileMigrator
   */
  get sourceHomePageURL() "",

  /**
   * DO NOT OVERRIDE - After deCOMing migration, the UI will just call
   * getResources.
   *
   * @see nsIBrowserProfileMigrator
   */
  getMigrateData: function MP_getMigrateData(aProfile) {
    let types = [r.type for each (r in this._getMaybeCachedResources(aProfile))];
    return types.reduce(function(a, b) a |= b, 0);
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
      resources = [r for each (r in resources) if (aItems & r.type)];

    // Called either directly or through the bookmarks import callback.
    function doMigrate() {
      // TODO: use Map (for the items) and Set (for the resources)
      // once they are iterable.
      let resourcesGroupedByItems = new Dict();
      resources.forEach(function(resource) {
        if (resourcesGroupedByItems.has(resource.type))
          resourcesGroupedByItems.get(resource.type).push(resource);
        else
          resourcesGroupedByItems.set(resource.type, [resource]);
      });

      if (resourcesGroupedByItems.count == 0)
        throw new Error("No items to import");

      let notify = function(aMsg, aItemType) {
        Services.obs.notifyObservers(null, aMsg, aItemType);
      }

      notify("Migration:Started");
      resourcesGroupedByItems.listkeys().forEach(function(migrationType) {
        let migrationTypeA = migrationType;
        let itemResources = resourcesGroupedByItems.get(migrationType);
        notify("Migration:ItemBeforeMigrate", migrationType);

        let itemSuccess = false;
        itemResources.forEach(function(resource) {
          let resourceDone = function(aSuccess) {
            let resourceIndex = itemResources.indexOf(resource);
            if (resourceIndex != -1) {
              itemResources.splice(resourceIndex, 1);
              itemSuccess |= aSuccess;
              if (itemResources.length == 0) {
                resourcesGroupedByItems.del(migrationType);
                notify(itemSuccess ?
                       "Migration:ItemAfterMigrate" : "Migration:ItemError",
                       migrationType);
                if (resourcesGroupedByItems.count == 0)
                  notify("Migration:Ended");
              }
            }
          };

          Services.tm.mainThread.dispatch(function() {
            // If migrate throws, an error occurred, and the callback
            // (itemMayBeDone) might haven't been called.
            try {
              resource.migrate(resourceDone);
            }
            catch(ex) {
              Cu.reportError(ex);
              resourceDone(false);
            }
          }, Ci.nsIThread.DISPATCH_NORMAL);
        });
      });
    }

    if (MigrationUtils.isStartupMigration && !this.startupOnlyMigrator) {
      MigrationUtils.profileStartup.doStartup();

      // If we're about to migrate bookmarks, first import the default bookmarks.
      // Note We do not need to do so for the Firefox migrator
      // (=startupOnlyMigrator), as it just copies over the places database
      // from another profile.
      const BOOKMARKS = MigrationUtils.resourceTypes.BOOKMARKS;
      let migratingBookmarks = resources.some(function(r) r.type == BOOKMARKS);
      if (migratingBookmarks) {
        let browserGlue = Cc["@mozilla.org/browser/browserglue;1"].
                          getService(Ci.nsIObserver);
        browserGlue.observe(null, TOPIC_WILL_IMPORT_BOOKMARKS, "");

        // Note doMigrate doesn't care about the success value of the
        // callback.
        BookmarkHTMLUtils.importFromURL(
          "resource:///defaults/profile/bookmarks.html", true, function(a) {
            browserGlue.observe(null, TOPIC_DID_IMPORT_BOOKMARKS, "");
            doMigrate();
          });
        return;
      }
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
      }
      else {
        exists = profiles.length > 0;
      }
    }
    catch(ex) {
      Cu.reportError(ex);
    }
    return exists;
  },

  /*** PRIVATE STUFF - DO NOT OVERRIDE ***/
  _getMaybeCachedResources: function PMB__getMaybeCachedResources(aProfile) {
    if (this._resourcesByProfile) {
      if (aProfile in this._resourcesByProfile)
        return this._resourcesByProfile[aProfile];
    }
    else {
      this._resourcesByProfile = { };
    }
    return this._resourcesByProfile[aProfile] = this.getResources(aProfile);
  }
};

let MigrationUtils = Object.freeze({
  resourceTypes: {
    SETTINGS:   Ci.nsIBrowserProfileMigrator.SETTINGS,
    COOKIES:    Ci.nsIBrowserProfileMigrator.COOKIES,
    HISTORY:    Ci.nsIBrowserProfileMigrator.HISTORY,
    FORMDATA:   Ci.nsIBrowserProfileMigrator.FORMDATA,
    PASSWORDS:  Ci.nsIBrowserProfileMigrator.PASSWORDS,
    BOOKMARKS:  Ci.nsIBrowserProfileMigrator.BOOKMARKS,
    OTHERDATA:  Ci.nsIBrowserProfileMigrator.OTHERDATA
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
      }
      catch(ex) {
        Cu.reportError(ex);
      }
      // Do not change this to call aCallback directly in try try & catch
      // blocks, because if aCallback throws, we may end up calling aCallback
      // twice.
      aCallback(success);
    }
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
   * @param aReplacemts
   *        [optioanl] Array of replacements to run on the retrieved string.
   * @return the retrieved string.
   *
   * @see nsIStringBundle
   */
  getLocalizedString: function MU_getLocalizedString(aKey, aReplacements) {
    const OVERRIDES = {
      "4_firefox": "4_firefox_history_and_bookmarks"
    };
    aKey = OVERRIDES[aKey] || aKey;

    if (aReplacements === undefined)
      return getMigrationBundle().GetStringFromName(aKey);
    return getMigrationBundle().formatStringFromName(
      aKey, aReplacements, aReplacements.length);
  },

  /**
   * Helper for creating a folder for imported bookmarks from a particular
   * migration source.  The folder is created at the end of the given folder.
   *
   * @param aSourceNameStr
   *        the source name (first letter capitalized).  This is used
   *        for reading the localized source name from the migration
   *        bundle (e.g. if aSourceNameStr is Mosaic, this will try to read
   *        sourceNameMosaic from the migration bundle).
   * @param aParentId
   *        the item-id of the folder in which the new folder should be
   *        created.
   * @return the item-id of the new folder.
   */
  createImportedBookmarksFolder:
  function MU_createImportedBookmarksFolder(aSourceNameStr, aParentId) {
    let source = this.getLocalizedString("sourceName" + aSourceNameStr);
    let label = this.getLocalizedString("importedBookmarksFolder", [source]);
    return PlacesUtils.bookmarks.createFolder(
      aParentId, label, PlacesUtils.bookmarks.DEFAULT_INDEX);
  },

  get _migrators() gMigrators ? gMigrators : gMigrators = new Dict(),

  /*
   * Returns the migrator for the given source, if any data is available
   * for this source, or null otherwise.
   *
   * @param aKey internal name of the migration source.
   *             Supported values: ie (windows),
   *                               safari (mac/windows),
   *                               chrome (mac/windows/linux),
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
    }
    else {
      try {
        migrator = Cc["@mozilla.org/profile/migrator;1?app=browser&type=" +
                      aKey].createInstance(Ci.nsIBrowserProfileMigrator);
      }
      catch(ex) { }
      this._migrators.set(aKey, migrator);
    }

    return migrator && migrator.sourceExists ? migrator : null;
  },

  // Iterates the available migrators, in the most suitable
  // order for the running platform.
  get migrators() {
    let migratorKeysOrdered = [
#ifdef XP_WIN
      "ie", "chrome", "safari"
#elifdef XP_MACOSX
      "safari", "chrome"
#elifdef XP_UNIX
      "chrome"
#endif
    ];

    // If a supported default browser is found check it first
    // so that the wizard defaults to import from that browser.
    let defaultBrowserKey = getMigratorKeyForDefaultBrowser();
    if (defaultBrowserKey)
      migratorKeysOrdered.sort(function (a, b) b == defaultBrowserKey ? 1 : 0);

    for (let migratorKey of migratorKeysOrdered) {
      let migrator = this.getMigrator(migratorKey);
      if (migrator)
        yield migrator;
    }
  },

  // Whether or not we're in the process of startup migration
  get isStartupMigration() gProfileStartup != null,

  /**
   * In the case of startup migration, this is set to the nsIProfileStartup
   * instance passed to ProfileMigrator's migrate.
   *
   * @see showMigrationWizard
   */
  get profileStartup() gProfileStartup,

  /**
   * Show the migration wizard.  On mac, this may just focus the wizard if it's
   * already running, in which case aOpener and aParams are ignored.
   *
   * @param [optional] aOpener
   *        the window that asks to open the wizard.
   * @param [optioanl] aParams
   *        arguments for the migration wizard, in the form of an nsIArray.
   *        This is passed as-is for the params argument of
   *        nsIWindowWatcher.openWindow.
   */
  showMigrationWizard:
  function MU_showMigrationWizard(aOpener, aParams) {
    let features = "chrome,dialog,modal,centerscreen,titlebar,resizable=no";
#ifdef XP_MACOSX
    if (!this.isStartupMigration) {
      let win = Services.wm.getMostRecentWindow("Browser:MigrationWizard");
      if (win) {
        win.focus();
        return;
      }
      // On mac, the migration wiazrd should only be modal in the case of
      // startup-migration.
      features = "centerscreen,chrome,resizable=no";
    }
#endif

    Services.ww.openWindow(aOpener,
                           "chrome://browser/content/migration/migration.xul",
                           "_blank",
                           features,
                           aParams);
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
   *          
   * @throws if aMigratorKey is invalid or if it points to a non-existent
   *         source.
   */
  startupMigration:
  function MU_startupMigrator(aProfileStartup, aMigratorKey) {
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
    }
    else {
      let defaultBrowserKey = getMigratorKeyForDefaultBrowser();
      if (defaultBrowserKey) {
        migrator = this.getMigrator(defaultBrowserKey);
        if (migrator)
          migratorKey = defaultBrowserKey;
      }
    }

    if (!migrator) {
      // If there's no migrator set so far, ensure that there is at least one
      // migrator available before opening the wizard.
      try {
        this.migrators.next();
      }
      catch(ex) {
        this.finishMigration();
        if (!(ex instanceof StopIteration))
          throw ex;
        return;
      }
    }

    let params = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    let keyCSTR = Cc["@mozilla.org/supports-cstring;1"].
                  createInstance(Ci.nsISupportsCString);
    keyCSTR.data = migratorKey;
    let skipImportSourcePageBool = Cc["@mozilla.org/supports-PRBool;1"].
                                   createInstance(Ci.nsISupportsPRBool);
    skipImportSourcePageBool.data = skipSourcePage;
    params.appendElement(keyCSTR, false);
    params.appendElement(migrator, false);
    params.appendElement(aProfileStartup, false);
    params.appendElement(skipImportSourcePageBool, false);

    this.showMigrationWizard(null, params);
  },

  /**
   * Cleans up references to migrators and nsIProfileInstance instances.
   */
  finishMigration: function MU_finishMigration() {
    gMigrators = null;
    gProfileStartup = null;
    gMigrationBundle = null;
  }
});
