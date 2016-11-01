/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["AutoMigrate"];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

const kAutoMigrateEnabledPref = "browser.migrate.automigrate.enabled";
const kUndoUIEnabledPref = "browser.migrate.automigrate.ui.enabled";

const kAutoMigrateStartedPref = "browser.migrate.automigrate.started";
const kAutoMigrateFinishedPref = "browser.migrate.automigrate.finished";
const kAutoMigrateBrowserPref = "browser.migrate.automigrate.browser";

const kAutoMigrateLastUndoPromptDateMsPref = "browser.migrate.automigrate.lastUndoPromptDateMs";
const kAutoMigrateDaysToOfferUndoPref = "browser.migrate.automigrate.daysToOfferUndo";

const kPasswordManagerTopic = "passwordmgr-storage-changed";
const kPasswordManagerTopicTypes = new Set([
  "addLogin",
  "modifyLogin",
]);

const kSyncTopic = "fxaccounts:onlogin";

const kNotificationId = "abouthome-automigration-undo";

Cu.import("resource:///modules/MigrationUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

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
    if (this.enabled) {
      this.maybeInitUndoObserver();
    }
  },

  maybeInitUndoObserver() {
    if (!this.canUndo()) {
      return;
    }
    // Now register places, password and sync observers:
    this.onItemAdded = this.onItemMoved = this.onItemChanged =
      this.removeUndoOption.bind(this, this.UNDO_REMOVED_REASON_BOOKMARK_CHANGE);
    PlacesUtils.addLazyBookmarkObserver(this, true);
    for (let topic of [kSyncTopic, kPasswordManagerTopic]) {
      Services.obs.addObserver(this, topic, true);
    }
  },

  observe(subject, topic, data) {
    if (topic == kPasswordManagerTopic) {
      // As soon as any login gets added or modified, disable undo:
      // (Note that this ignores logins being removed as that doesn't
      //  impair the 'undo' functionality of the import.)
      if (kPasswordManagerTopicTypes.has(data)) {
        // Ignore chrome:// things like sync credentials:
        let loginInfo = subject.QueryInterface(Ci.nsILoginInfo);
        if (!loginInfo.hostname || !loginInfo.hostname.startsWith("chrome://")) {
          this.removeUndoOption(this.UNDO_REMOVED_REASON_PASSWORD_CHANGE);
        }
      }
    } else if (topic == kSyncTopic) {
      this.removeUndoOption(this.UNDO_REMOVED_REASON_SYNC_SIGNIN);
    }
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
        Services.prefs.setCharPref(kAutoMigrateStartedPref, startTime.toString());
        Services.prefs.setCharPref(kAutoMigrateFinishedPref, Date.now().toString());
        Services.prefs.setCharPref(kAutoMigrateBrowserPref, pickedKey);
        // Need to manually start listening to new bookmarks/logins being created,
        // because, when we were initialized, there wasn't the possibility to
        // 'undo' anything.
        this.maybeInitUndoObserver();
      }
    };

    Services.obs.addObserver(migrationObserver, "Migration:Ended", false);
    Services.obs.addObserver(migrationObserver, "Migration:ItemError", false);
    // We'll save this when the migration has finished, at which point the pref
    // service will be available.
    let startTime = Date.now();
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
      throw new Error("Migrator specified or a default was found, but the migrator object is not available.");
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

  getUndoRange() {
    let start, finish;
    try {
      start = parseInt(Preferences.get(kAutoMigrateStartedPref, "0"), 10);
      finish = parseInt(Preferences.get(kAutoMigrateFinishedPref, "0"), 10);
    } catch (ex) {
      Cu.reportError(ex);
    }
    if (!finish || !start) {
      return null;
    }
    return [new Date(start), new Date(finish)];
  },

  canUndo() {
    return !!this.getUndoRange();
  },

  undo: Task.async(function* () {
    let histogram = Services.telemetry.getHistogramById("FX_STARTUP_MIGRATION_AUTOMATED_IMPORT_UNDO");
    histogram.add(0);
    if (!this.canUndo()) {
      histogram.add(5);
      throw new Error("Can't undo!");
    }

    histogram.add(10);

    yield PlacesUtils.bookmarks.eraseEverything();
    histogram.add(15);

    // NB: we drop the start time of the migration for now. This is because
    // imported history will always end up being 'backdated' to the actual
    // visit time recorded by the browser from which we imported. As a result,
    // a lower bound on this item doesn't really make sense.
    // Note that for form data this could be different, but we currently don't
    // support form data import from any non-Firefox browser, so it isn't
    // imported from other browsers by the automigration code, nor do we
    // remove it here.
    let range = this.getUndoRange();
    yield PlacesUtils.history.removeVisitsByFilter({
      beginDate: new Date(0),
      endDate: range[1]
    });
    histogram.add(20);

    try {
      Services.logins.removeAllLogins();
    } catch (ex) {
      // ignore failure.
    }
    histogram.add(25);
    this.removeUndoOption(this.UNDO_REMOVED_REASON_UNDO_USED);
    histogram.add(30);
  }),

  removeUndoOption(reason) {
    // Remove observers, and ensure that exceptions doing so don't break
    // removing the pref.
    for (let topic of [kSyncTopic, kPasswordManagerTopic]) {
      try {
        Services.obs.removeObserver(this, topic);
      } catch (ex) {
        Cu.reportError("Error removing observer for " + topic + ": " + ex);
      }
    }
    try {
      PlacesUtils.removeLazyBookmarkObserver(this);
    } catch (ex) {
      Cu.reportError("Error removing lazy bookmark observer: " + ex);
    }

    let migrationBrowser = Preferences.get(kAutoMigrateBrowserPref, "unknown");
    Services.prefs.clearUserPref(kAutoMigrateStartedPref);
    Services.prefs.clearUserPref(kAutoMigrateFinishedPref);
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

  maybeShowUndoNotification(target) {
    // The tab might have navigated since we requested the undo state:
    if (!this.canUndo() || target.currentURI.spec != "about:home" ||
        !Preferences.get(kUndoUIEnabledPref, false)) {
      return;
    }

    let win = target.ownerGlobal;
    let notificationBox = win.gBrowser.getNotificationBox(target);
    if (!notificationBox || notificationBox.getNotificationWithValue("abouthome-automigration-undo")) {
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
  },

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

  QueryInterface: XPCOMUtils.generateQI(
    [Ci.nsIObserver, Ci.nsINavBookmarkObserver, Ci.nsISupportsWeakReference]
  ),
};

AutoMigrate.init();
