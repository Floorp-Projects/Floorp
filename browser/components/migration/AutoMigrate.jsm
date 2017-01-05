/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["AutoMigrate"];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

const kAutoMigrateEnabledPref = "browser.migrate.automigrate.enabled";
const kUndoUIEnabledPref = "browser.migrate.automigrate.ui.enabled";

const kAutoMigrateBrowserPref = "browser.migrate.automigrate.browser";

const kAutoMigrateLastUndoPromptDateMsPref = "browser.migrate.automigrate.lastUndoPromptDateMs";
const kAutoMigrateDaysToOfferUndoPref = "browser.migrate.automigrate.daysToOfferUndo";

const kNotificationId = "automigration-undo";

Cu.import("resource:///modules/MigrationUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

Cu.importGlobalProperties(["URL"]);

/* globals kUndoStateFullPath */
XPCOMUtils.defineLazyGetter(this, "kUndoStateFullPath", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, "initialMigrationMetadata.jsonlz4");
});

const AutoMigrate = {
  get resourceTypesToUse() {
    let {BOOKMARKS, HISTORY, PASSWORDS} = Ci.nsIBrowserProfileMigrator;
    return BOOKMARKS | HISTORY | PASSWORDS;
  },

  _checkIfEnabled() {
    let pref = Preferences.get(kAutoMigrateEnabledPref, false);
    // User-set values should take precedence:
    if (Services.prefs.prefHasUserValue(kAutoMigrateEnabledPref)) {
      return pref;
    }
    // If we're using the default value, make sure the distribution.ini
    // value is taken into account even early on startup.
    try {
      let distributionFile = Services.dirsvc.get("XREAppDist", Ci.nsIFile);
      distributionFile.append("distribution.ini");
      let parser = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
                 getService(Ci.nsIINIParserFactory).
                 createINIParser(distributionFile);
      return JSON.parse(parser.getString("Preferences", kAutoMigrateEnabledPref));
    } catch (ex) { /* ignore exceptions (file doesn't exist, invalid value, etc.) */ }

    return pref;
  },

  init() {
    this.enabled = this._checkIfEnabled();
  },

  /**
   * Automatically pick a migrator and resources to migrate,
   * then migrate those and start up.
   *
   * @throws if automatically deciding on migrators/data
   *         failed for some reason.
   */
  migrate(profileStartup, migratorKey, profileToMigrate) {
    let histogram = Services.telemetry.getHistogramById(
      "FX_STARTUP_MIGRATION_AUTOMATED_IMPORT_PROCESS_SUCCESS");
    histogram.add(0);
    let {migrator, pickedKey} = this.pickMigrator(migratorKey);
    histogram.add(5);

    profileToMigrate = this.pickProfile(migrator, profileToMigrate);
    histogram.add(10);

    let resourceTypes = migrator.getMigrateData(profileToMigrate, profileStartup);
    if (!(resourceTypes & this.resourceTypesToUse)) {
      throw new Error("No usable resources were found for the selected browser!");
    }
    histogram.add(15);

    let sawErrors = false;
    let migrationObserver = (subject, topic) => {
      if (topic == "Migration:ItemError") {
        sawErrors = true;
      } else if (topic == "Migration:Ended") {
        histogram.add(25);
        if (sawErrors) {
          histogram.add(26);
        }
        Services.obs.removeObserver(migrationObserver, "Migration:Ended");
        Services.obs.removeObserver(migrationObserver, "Migration:ItemError");
        Services.prefs.setCharPref(kAutoMigrateBrowserPref, pickedKey);
        // Save the undo history and block shutdown on that save completing.
        AsyncShutdown.profileBeforeChange.addBlocker(
          "AutoMigrate Undo saving", this.saveUndoState(), () => {
            return {state: this._saveUndoStateTrackerForShutdown};
          });
      }
    };

    MigrationUtils.initializeUndoData();
    Services.obs.addObserver(migrationObserver, "Migration:Ended", false);
    Services.obs.addObserver(migrationObserver, "Migration:ItemError", false);
    migrator.migrate(this.resourceTypesToUse, profileStartup, profileToMigrate);
    histogram.add(20);
  },

  /**
   * Pick and return a migrator to use for automatically migrating.
   *
   * @param {String} migratorKey   optional, a migrator key to prefer/pick.
   * @returns {Object}             an object with the migrator to use for migrating, as
   *                               well as the key we eventually ended up using to obtain it.
   */
  pickMigrator(migratorKey) {
    if (!migratorKey) {
      let defaultKey = MigrationUtils.getMigratorKeyForDefaultBrowser();
      if (!defaultKey) {
        throw new Error("Could not determine default browser key to migrate from");
      }
      migratorKey = defaultKey;
    }
    if (migratorKey == "firefox") {
      throw new Error("Can't automatically migrate from Firefox.");
    }

    let migrator = MigrationUtils.getMigrator(migratorKey);
    if (!migrator) {
      throw new Error("Migrator specified or a default was found, but the migrator object is not available (or has no data).");
    }
    return {migrator, pickedKey: migratorKey};
  },

  /**
   * Pick a source profile (from the original browser) to use.
   *
   * @param {Migrator} migrator     the migrator object to use
   * @param {String}   suggestedId  the id of the profile to migrate, if pre-specified, or null
   * @returns                       the profile to migrate, or null if migrating
   *                                from the default profile.
   */
  pickProfile(migrator, suggestedId) {
    let profiles = migrator.sourceProfiles;
    if (profiles && !profiles.length) {
      throw new Error("No profile data found to migrate.");
    }
    if (suggestedId) {
      if (!profiles) {
        throw new Error("Profile specified but only a default profile found.");
      }
      let suggestedProfile = profiles.find(profile => profile.id == suggestedId);
      if (!suggestedProfile) {
        throw new Error("Profile specified was not found.");
      }
      return suggestedProfile;
    }
    if (profiles && profiles.length > 1) {
      throw new Error("Don't know how to pick a profile when more than 1 profile is present.");
    }
    return profiles ? profiles[0] : null;
  },

  canUndo: Task.async(function* () {
    if (this._savingPromise) {
      yield this._savingPromise;
    }
    let fileExists = false;
    try {
      fileExists = yield OS.File.exists(kUndoStateFullPath);
    } catch (ex) {
      Cu.reportError(ex);
    }
    return fileExists;
  }),

  undo: Task.async(function* () {
    let histogram = Services.telemetry.getHistogramById("FX_STARTUP_MIGRATION_AUTOMATED_IMPORT_UNDO");
    histogram.add(0);
    if (!(yield this.canUndo())) {
      histogram.add(5);
      throw new Error("Can't undo!");
    }

    histogram.add(10);

    let readPromise = OS.File.read(kUndoStateFullPath, {
      encoding: "utf-8",
      compression: "lz4",
    });
    let stateData = this._dejsonifyUndoState(yield readPromise);
    yield this._removeUnchangedBookmarks(stateData.get("bookmarks"));
    histogram.add(15);

    yield this._removeSomeVisits(stateData.get("visits"));
    histogram.add(20);

    yield this._removeUnchangedLogins(stateData.get("logins"));
    histogram.add(25);

    this.removeUndoOption(this.UNDO_REMOVED_REASON_UNDO_USED);
    histogram.add(30);
  }),

  removeUndoOption(reason) {
    // We don't wait for the off-main-thread removal to complete. OS.File will
    // ensure it happens before shutdown.
    OS.File.remove(kUndoStateFullPath, {ignoreAbsent: true});

    let migrationBrowser = Preferences.get(kAutoMigrateBrowserPref, "unknown");
    Services.prefs.clearUserPref(kAutoMigrateBrowserPref);

    let browserWindows = Services.wm.getEnumerator("navigator:browser");
    while (browserWindows.hasMoreElements()) {
      let win = browserWindows.getNext();
      if (!win.closed) {
        for (let browser of win.gBrowser.browsers) {
          let nb = win.gBrowser.getNotificationBox(browser);
          let notification = nb.getNotificationWithValue(kNotificationId);
          if (notification) {
            nb.removeNotification(notification);
          }
        }
      }
    }
    let histogram =
      Services.telemetry.getKeyedHistogramById("FX_STARTUP_MIGRATION_UNDO_REASON");
    histogram.add(migrationBrowser, reason);
  },

  getBrowserUsedForMigration() {
    let browserId = Services.prefs.getCharPref(kAutoMigrateBrowserPref);
    if (browserId) {
      return MigrationUtils.getBrowserName(browserId);
    }
    return null;
  },

  maybeShowUndoNotification: Task.async(function* (target) {
    if (!(yield this.canUndo())) {
      return;
    }

    // The tab might have navigated since we requested the undo state:
    let canUndoFromThisPage = ["about:home", "about:newtab"].includes(target.currentURI.spec);
    if (!canUndoFromThisPage ||
        !Preferences.get(kUndoUIEnabledPref, false)) {
      return;
    }

    let win = target.ownerGlobal;
    let notificationBox = win.gBrowser.getNotificationBox(target);
    if (!notificationBox || notificationBox.getNotificationWithValue(kNotificationId)) {
      return;
    }

    // At this stage we're committed to show the prompt - unless we shouldn't,
    // in which case we remove the undo prefs (which will cause canUndo() to
    // return false from now on.):
    if (!this.shouldStillShowUndoPrompt()) {
      this.removeUndoOption(this.UNDO_REMOVED_REASON_OFFER_EXPIRED);
      return;
    }

    let browserName = this.getBrowserUsedForMigration();
    let message;
    if (browserName) {
      message = MigrationUtils.getLocalizedString("automigration.undo.message",
                                                  [browserName]);
    } else {
      message = MigrationUtils.getLocalizedString("automigration.undo.unknownBrowserMessage");
    }

    let buttons = [
      {
        label: MigrationUtils.getLocalizedString("automigration.undo.keep.label"),
        accessKey: MigrationUtils.getLocalizedString("automigration.undo.keep.accesskey"),
        callback: () => {
          this.removeUndoOption(this.UNDO_REMOVED_REASON_OFFER_REJECTED);
        },
      },
      {
        label: MigrationUtils.getLocalizedString("automigration.undo.dontkeep.label"),
        accessKey: MigrationUtils.getLocalizedString("automigration.undo.dontkeep.accesskey"),
        callback: () => {
          this.undo();
        },
      },
    ];
    notificationBox.appendNotification(
      message, kNotificationId, null, notificationBox.PRIORITY_INFO_HIGH, buttons
    );
    let remainingDays = Preferences.get(kAutoMigrateDaysToOfferUndoPref, 0);
    Services.telemetry.getHistogramById("FX_STARTUP_MIGRATION_UNDO_OFFERED").add(4 - remainingDays);
  }),

  shouldStillShowUndoPrompt() {
    let today = new Date();
    // Round down to midnight:
    today = new Date(today.getFullYear(), today.getMonth(), today.getDate());
    // We store the unix timestamp corresponding to midnight on the last day
    // on which we prompted. Fetch that and compare it to today's date.
    // (NB: stored as a string because int prefs are too small for unix
    // timestamps.)
    let previousPromptDateMsStr = Preferences.get(kAutoMigrateLastUndoPromptDateMsPref, "0");
    let previousPromptDate = new Date(parseInt(previousPromptDateMsStr, 10));
    if (previousPromptDate < today) {
      let remainingDays = Preferences.get(kAutoMigrateDaysToOfferUndoPref, 4) - 1;
      Preferences.set(kAutoMigrateDaysToOfferUndoPref, remainingDays);
      Preferences.set(kAutoMigrateLastUndoPromptDateMsPref, today.valueOf().toString());
      if (remainingDays <= 0) {
        return false;
      }
    }
    return true;
  },

  UNDO_REMOVED_REASON_UNDO_USED: 0,
  UNDO_REMOVED_REASON_SYNC_SIGNIN: 1,
  UNDO_REMOVED_REASON_PASSWORD_CHANGE: 2,
  UNDO_REMOVED_REASON_BOOKMARK_CHANGE: 3,
  UNDO_REMOVED_REASON_OFFER_EXPIRED: 4,
  UNDO_REMOVED_REASON_OFFER_REJECTED: 5,

  _jsonifyUndoState(state) {
    if (!state) {
      return "null";
    }
    // Deal with date serialization.
    let bookmarks = state.get("bookmarks");
    for (let bm of bookmarks) {
      bm.lastModified = bm.lastModified.getTime();
    }
    let serializableState = {
      bookmarks,
      logins: state.get("logins"),
      visits: state.get("visits"),
    };
    return JSON.stringify(serializableState);
  },

  _dejsonifyUndoState(state) {
    state = JSON.parse(state);
    for (let bm of state.bookmarks) {
      bm.lastModified = new Date(bm.lastModified);
    }
    return new Map([
      ["bookmarks", state.bookmarks],
      ["logins", state.logins],
      ["visits", state.visits],
    ]);
  },

  _saveUndoStateTrackerForShutdown: "not running",
  saveUndoState: Task.async(function* () {
    let resolveSavingPromise;
    this._saveUndoStateTrackerForShutdown = "processing undo history";
    this._savingPromise = new Promise(resolve => { resolveSavingPromise = resolve });
    let state = yield MigrationUtils.stopAndRetrieveUndoData();
    this._saveUndoStateTrackerForShutdown = "writing undo history";
    this._undoSavePromise = OS.File.writeAtomic(
      kUndoStateFullPath, this._jsonifyUndoState(state), {
        encoding: "utf-8",
        compression: "lz4",
        tmpPath: kUndoStateFullPath + ".tmp",
      });
    this._undoSavePromise.then(
      rv => {
        resolveSavingPromise(rv);
        delete this._savingPromise;
      },
      e => {
        Cu.reportError("Could not write undo state for automatic migration.");
        throw e;
      });
    return this._undoSavePromise;
  }),

  _removeUnchangedBookmarks: Task.async(function* (bookmarks) {
    if (!bookmarks.length) {
      return;
    }

    let guidToLMMap = new Map(bookmarks.map(b => [b.guid, b.lastModified]));
    let bookmarksFromDB = [];
    let bmPromises = Array.from(guidToLMMap.keys()).map(guid => {
      // Ignore bookmarks where the promise doesn't resolve (ie that are missing)
      // Also check that the bookmark fetch returns isn't null before adding it.
      return PlacesUtils.bookmarks.fetch(guid).then(bm => bm && bookmarksFromDB.push(bm), () => {});
    });
    // We can't use the result of Promise.all because that would include nulls
    // for bookmarks that no longer exist (which we're catching above).
    yield Promise.all(bmPromises);
    let unchangedBookmarks = bookmarksFromDB.filter(bm => {
      return bm.lastModified.getTime() == guidToLMMap.get(bm.guid).getTime();
    });

    // We need to remove items with no ancestors first, followed by their
    // parents, etc. In order to do this, find out how many ancestors each item
    // has that also appear in our list of things to remove, and sort the items
    // by those numbers. This ensures that children are always removed before
    // their parents.
    function determineAncestorCount(bm) {
      if (bm._ancestorCount) {
        return bm._ancestorCount;
      }
      let myCount = 0;
      let parentBM = unchangedBookmarks.find(item => item.guid == bm.parentGuid);
      if (parentBM) {
        myCount = determineAncestorCount(parentBM) + 1;
      }
      bm._ancestorCount = myCount;
      return myCount;
    }
    unchangedBookmarks.forEach(determineAncestorCount);
    unchangedBookmarks.sort((a, b) => b._ancestorCount - a._ancestorCount);
    for (let {guid} of unchangedBookmarks) {
      yield PlacesUtils.bookmarks.remove(guid, {preventRemovalOfNonEmptyFolders: true}).catch(err => {
        if (err && err.message != "Cannot remove a non-empty folder.") {
          Cu.reportError(err);
        }
      });
    }
  }),

  _removeUnchangedLogins: Task.async(function* (logins) {
    for (let login of logins) {
      let foundLogins = LoginHelper.searchLoginsWithObject({guid: login.guid});
      if (foundLogins.length) {
        let foundLogin = foundLogins[0];
        foundLogin.QueryInterface(Ci.nsILoginMetaInfo);
        if (foundLogin.timePasswordChanged == login.timePasswordChanged) {
          Services.logins.removeLogin(foundLogin);
        }
      }
    }
  }),

  _removeSomeVisits: Task.async(function* (visits) {
    for (let urlVisits of visits) {
      let urlObj;
      try {
        urlObj = new URL(urlVisits.url);
      } catch (ex) {
        continue;
      }
      yield PlacesUtils.history.removeVisitsByFilter({
        url: urlObj,
        beginDate: PlacesUtils.toDate(urlVisits.first),
        endDate: PlacesUtils.toDate(urlVisits.last),
        limit: urlVisits.visitCount,
      });
    }
  }),

  QueryInterface: XPCOMUtils.generateQI(
    [Ci.nsIObserver, Ci.nsINavBookmarkObserver, Ci.nsISupportsWeakReference]
  ),
};

AutoMigrate.init();
