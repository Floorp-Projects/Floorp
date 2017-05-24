/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["AutoMigrate"];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

const kAutoMigrateEnabledPref = "browser.migrate.automigrate.enabled";
const kUndoUIEnabledPref = "browser.migrate.automigrate.ui.enabled";

const kInPageUIEnabledPref = "browser.migrate.automigrate.inpage.ui.enabled";

const kAutoMigrateBrowserPref = "browser.migrate.automigrate.browser";
const kAutoMigrateImportedItemIds = "browser.migrate.automigrate.imported-items";

const kAutoMigrateLastUndoPromptDateMsPref = "browser.migrate.automigrate.lastUndoPromptDateMs";
const kAutoMigrateDaysToOfferUndoPref = "browser.migrate.automigrate.daysToOfferUndo";

const kAutoMigrateUndoSurveyPref = "browser.migrate.automigrate.undo-survey";
const kAutoMigrateUndoSurveyLocalePref = "browser.migrate.automigrate.undo-survey-locales";

const kNotificationId = "automigration-undo";

Cu.import("resource:///modules/MigrationUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
                                  "resource://gre/modules/NewTabUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStopwatch",
                                  "resource://gre/modules/TelemetryStopwatch.jsm");

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function() {
  const kBrandBundle = "chrome://branding/locale/brand.properties";
  return Services.strings.createBundle(kBrandBundle);
});

Cu.importGlobalProperties(["URL"]);

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
    Services.obs.addObserver(migrationObserver, "Migration:Ended");
    Services.obs.addObserver(migrationObserver, "Migration:ItemError");
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

  _pendingUndoTasks: false,
  async canUndo() {
    if (this._savingPromise) {
      await this._savingPromise;
    }
    if (this._pendingUndoTasks) {
      return false;
    }
    let fileExists = false;
    try {
      fileExists = await OS.File.exists(kUndoStateFullPath);
    } catch (ex) {
      Cu.reportError(ex);
    }
    return fileExists;
  },

  async undo() {
    let browserId = Preferences.get(kAutoMigrateBrowserPref, "unknown");
    TelemetryStopwatch.startKeyed("FX_STARTUP_MIGRATION_UNDO_TOTAL_MS", browserId);
    let histogram = Services.telemetry.getHistogramById("FX_STARTUP_MIGRATION_AUTOMATED_IMPORT_UNDO");
    histogram.add(0);
    if (!(await this.canUndo())) {
      histogram.add(5);
      throw new Error("Can't undo!");
    }

    this._pendingUndoTasks = true;
    this._removeNotificationBars();
    histogram.add(10);

    let readPromise = OS.File.read(kUndoStateFullPath, {
      encoding: "utf-8",
      compression: "lz4",
    });
    let stateData = this._dejsonifyUndoState(await readPromise);
    histogram.add(12);

    this._errorMap = {bookmarks: 0, visits: 0, logins: 0};
    let reportErrorTelemetry = (type) => {
      let histogramId = `FX_STARTUP_MIGRATION_UNDO_${type.toUpperCase()}_ERRORCOUNT`;
      Services.telemetry.getKeyedHistogramById(histogramId).add(browserId, this._errorMap[type]);
    };

    let startTelemetryStopwatch = resourceType => {
      let histogramId = `FX_STARTUP_MIGRATION_UNDO_${resourceType.toUpperCase()}_MS`;
      TelemetryStopwatch.startKeyed(histogramId, browserId);
    };
    let stopTelemetryStopwatch = resourceType => {
      let histogramId = `FX_STARTUP_MIGRATION_UNDO_${resourceType.toUpperCase()}_MS`;
      TelemetryStopwatch.finishKeyed(histogramId, browserId);
    };
    startTelemetryStopwatch("bookmarks");
    await this._removeUnchangedBookmarks(stateData.get("bookmarks")).catch(ex => {
      Cu.reportError("Uncaught exception when removing unchanged bookmarks!");
      Cu.reportError(ex);
    });
    stopTelemetryStopwatch("bookmarks");
    reportErrorTelemetry("bookmarks");
    histogram.add(15);

    startTelemetryStopwatch("visits");
    await this._removeSomeVisits(stateData.get("visits")).catch(ex => {
      Cu.reportError("Uncaught exception when removing history visits!");
      Cu.reportError(ex);
    });
    stopTelemetryStopwatch("visits");
    reportErrorTelemetry("visits");
    histogram.add(20);

    startTelemetryStopwatch("logins");
    await this._removeUnchangedLogins(stateData.get("logins")).catch(ex => {
      Cu.reportError("Uncaught exception when removing unchanged logins!");
      Cu.reportError(ex);
    });
    stopTelemetryStopwatch("logins");
    reportErrorTelemetry("logins");
    histogram.add(25);

    // This is async, but no need to wait for it.
    NewTabUtils.links.populateCache(() => {
      NewTabUtils.allPages.update();
    }, true);

    this._purgeUndoState(this.UNDO_REMOVED_REASON_UNDO_USED);
    histogram.add(30);
    TelemetryStopwatch.finishKeyed("FX_STARTUP_MIGRATION_UNDO_TOTAL_MS", browserId);
  },

  _removeNotificationBars() {
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
  },

  _purgeUndoState(reason) {
    // We don't wait for the off-main-thread removal to complete. OS.File will
    // ensure it happens before shutdown.
    OS.File.remove(kUndoStateFullPath, {ignoreAbsent: true}).then(() => {
      this._pendingUndoTasks = false;
    });

    let migrationBrowser = Preferences.get(kAutoMigrateBrowserPref, "unknown");
    Services.prefs.clearUserPref(kAutoMigrateBrowserPref);

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

  /**
   * Decide if we need to show [the user] a prompt indicating we automatically
   * imported their data.
   * @param target (xul:browser)
   *        The browser in which we should show the notification.
   * @returns {Boolean} return true when need to show the prompt.
   */
  async shouldShowMigratePrompt(target) {
    if (!(await this.canUndo())) {
      return false;
    }

    // The tab might have navigated since we requested the undo state:
    let canUndoFromThisPage = ["about:home", "about:newtab"].includes(target.currentURI.spec);
    if (!canUndoFromThisPage ||
        !Preferences.get(kUndoUIEnabledPref, false)) {
      return false;
    }

    // At this stage we're committed to show the prompt - unless we shouldn't,
    // in which case we remove the undo prefs (which will cause canUndo() to
    // return false from now on.):
    if (this.isMigratePromptExpired()) {
      this._purgeUndoState(this.UNDO_REMOVED_REASON_OFFER_EXPIRED);
      this._removeNotificationBars();
      return false;
    }

    let remainingDays = Preferences.get(kAutoMigrateDaysToOfferUndoPref, 0);
    Services.telemetry.getHistogramById("FX_STARTUP_MIGRATION_UNDO_OFFERED").add(4 - remainingDays);

    return true;
  },

  /**
   * Return the message that denotes the user data is migrated from the other browser.
   * @returns {String} imported message with the brand and the browser name
   */
  getUndoMigrationMessage() {
    let browserName = this.getBrowserUsedForMigration();
    if (!browserName) {
      browserName = MigrationUtils.getLocalizedString("automigration.undo.unknownbrowser");
    }
    const kMessageId = "automigration.undo.message2." +
                      Preferences.get(kAutoMigrateImportedItemIds, "all");
    const kBrandShortName = gBrandBundle.GetStringFromName("brandShortName");
    return MigrationUtils.getLocalizedString(kMessageId,
                                             [kBrandShortName, browserName]);
  },

  /**
   * Show the user a notification bar indicating we automatically imported
   * their data and offering them the possibility of removing it.
   * @param target (xul:browser)
   *        The browser in which we should show the notification.
   */
  showUndoNotificationBar(target) {
    let isInPage = Preferences.get(kInPageUIEnabledPref, false);
    let win = target.ownerGlobal;
    let notificationBox = win.gBrowser.getNotificationBox(target);
    if (isInPage || !notificationBox || notificationBox.getNotificationWithValue(kNotificationId)) {
      return;
    }
    let message = this.getUndoMigrationMessage();
    let buttons = [
      {
        label: MigrationUtils.getLocalizedString("automigration.undo.keep2.label"),
        accessKey: MigrationUtils.getLocalizedString("automigration.undo.keep2.accesskey"),
        callback: () => {
          this.keepAutoMigration();
          this._removeNotificationBars();
        },
      },
      {
        label: MigrationUtils.getLocalizedString("automigration.undo.dontkeep2.label"),
        accessKey: MigrationUtils.getLocalizedString("automigration.undo.dontkeep2.accesskey"),
        callback: () => {
          this.undoAutoMigration(win);
        },
      },
    ];
    notificationBox.appendNotification(
      message, kNotificationId, null, notificationBox.PRIORITY_INFO_HIGH, buttons
    );
  },


  /**
   * Return true if we have shown the prompt to user several days.
   * (defined in kAutoMigrateDaysToOfferUndoPref)
   */
  isMigratePromptExpired() {
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
        return true;
      }
    }
    return false;
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
    if (!state) {
      return new Map();
    }
    for (let bm of state.bookmarks) {
      bm.lastModified = new Date(bm.lastModified);
    }
    return new Map([
      ["bookmarks", state.bookmarks],
      ["logins", state.logins],
      ["visits", state.visits],
    ]);
  },

  /**
   * Store the items we've saved into a pref. We use this to be able to show
   * a detailed message to the user indicating what we've imported.
   * @param state (Map)
   *        The 'undo' state for the import, which contains info about
   *        how many items of each kind we've (tried to) import.
   */
  _setImportedItemPrefFromState(state) {
    let itemsWithData = [];
    if (state) {
      for (let itemType of state.keys()) {
        if (state.get(itemType).length) {
          itemsWithData.push(itemType);
        }
      }
    }
    if (itemsWithData.length == 3) {
      itemsWithData = "all";
    } else {
      itemsWithData = itemsWithData.sort().join(".");
    }
    if (itemsWithData) {
      Preferences.set(kAutoMigrateImportedItemIds, itemsWithData);
    }
  },

  /**
   * Used for the shutdown blocker's information field.
   */
  _saveUndoStateTrackerForShutdown: "not running",
  /**
   * Store the information required for using 'undo' of the automatic
   * migration in the user's profile.
   */
  async saveUndoState() {
    let resolveSavingPromise;
    this._saveUndoStateTrackerForShutdown = "processing undo history";
    this._savingPromise = new Promise(resolve => { resolveSavingPromise = resolve });
    let state = await MigrationUtils.stopAndRetrieveUndoData();

    if (!state || ![...state.values()].some(ary => ary.length > 0)) {
      // If we didn't import anything, abort now.
      resolveSavingPromise();
      return Promise.resolve();
    }

    this._saveUndoStateTrackerForShutdown = "saving imported item list";
    this._setImportedItemPrefFromState(state);

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
  },

  async _removeUnchangedBookmarks(bookmarks) {
    if (!bookmarks.length) {
      return;
    }

    let guidToLMMap = new Map(bookmarks.map(b => [b.guid, b.lastModified]));
    let bookmarksFromDB = [];
    let bmPromises = Array.from(guidToLMMap.keys()).map(guid => {
      // Ignore bookmarks where the promise doesn't resolve (ie that are missing)
      // Also check that the bookmark fetch returns isn't null before adding it.
      try {
        return PlacesUtils.bookmarks.fetch(guid).then(bm => bm && bookmarksFromDB.push(bm), () => {});
      } catch (ex) {
        // Ignore immediate exceptions, too.
      }
      return Promise.resolve();
    });
    // We can't use the result of Promise.all because that would include nulls
    // for bookmarks that no longer exist (which we're catching above).
    await Promise.all(bmPromises);
    let unchangedBookmarks = bookmarksFromDB.filter(bm => {
      return bm.lastModified.getTime() == guidToLMMap.get(bm.guid).getTime();
    });

    // We need to remove items without children first, followed by their
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
      // Can't just use a .catch() because Bookmarks.remove() can throw (rather
      // than returning rejected promises).
      try {
        await PlacesUtils.bookmarks.remove(guid, {preventRemovalOfNonEmptyFolders: true});
      } catch (err) {
        if (err && err.message != "Cannot remove a non-empty folder.") {
          this._errorMap.bookmarks++;
          Cu.reportError(err);
        }
      }
    }
  },

  async _removeUnchangedLogins(logins) {
    for (let login of logins) {
      let foundLogins = LoginHelper.searchLoginsWithObject({guid: login.guid});
      if (foundLogins.length) {
        let foundLogin = foundLogins[0];
        foundLogin.QueryInterface(Ci.nsILoginMetaInfo);
        if (foundLogin.timePasswordChanged == login.timePasswordChanged) {
          try {
            Services.logins.removeLogin(foundLogin);
          } catch (ex) {
            Cu.reportError("Failed to remove a login for " + foundLogins.hostname);
            Cu.reportError(ex);
            this._errorMap.logins++;
          }
        }
      }
    }
  },

  async _removeSomeVisits(visits) {
    for (let urlVisits of visits) {
      let urlObj;
      try {
        urlObj = new URL(urlVisits.url);
      } catch (ex) {
        continue;
      }
      let visitData = {
        url: urlObj,
        beginDate: PlacesUtils.toDate(urlVisits.first),
        endDate: PlacesUtils.toDate(urlVisits.last),
        limit: urlVisits.visitCount,
      };
      try {
        await PlacesUtils.history.removeVisitsByFilter(visitData);
      } catch (ex) {
        this._errorMap.visits++;
        try {
          visitData.url = visitData.url.href;
        } catch (ignoredEx) {}
        Cu.reportError("Failed to remove a visit: " + JSON.stringify(visitData));
        Cu.reportError(ex);
      }
    }
  },

  /**
   * Maybe open a new tab with a survey. The tab will only be opened if all of
   * the following are true:
   * - the 'browser.migrate.automigrate.undo-survey' pref is not empty.
   *   It should contain the URL of the survey to open.
   * - the 'browser.migrate.automigrate.undo-survey-locales' pref, a
   *   comma-separated list of language codes, contains the language code
   *   that is currently in use for the 'global' chrome pacakge (ie the
   *   locale in which the user is currently using Firefox).
   *   The URL will be passed through nsIURLFormatter to allow including
   *   build ids etc. The special additional formatting variable
   *   "%IMPORTEDBROWSER" is also replaced with the name of the browser
   *   from which we imported data.
   *
   * @param {Window} chromeWindow   A reference to the window in which to open a link.
   */
  _maybeOpenUndoSurveyTab(chromeWindow) {
    let canDoSurveyInLocale = false;
    try {
      let surveyLocales = Preferences.get(kAutoMigrateUndoSurveyLocalePref, "");
      surveyLocales = surveyLocales.split(",").map(str => str.trim());
      // Strip out any empty elements, so an empty pref doesn't
      // lead to a an array with 1 empty string in it.
      surveyLocales = new Set(surveyLocales.filter(str => !!str));
      canDoSurveyInLocale =
        surveyLocales.has(Services.locale.getAppLocaleAsLangTag());
    } catch (ex) {
      /* ignore exceptions and just don't do the survey. */
    }

    let migrationBrowser = this.getBrowserUsedForMigration();
    let rawURL = Preferences.get(kAutoMigrateUndoSurveyPref, "");
    if (!canDoSurveyInLocale || !migrationBrowser || !rawURL) {
      return;
    }

    let url = Services.urlFormatter.formatURL(rawURL);
    url = url.replace("%IMPORTEDBROWSER%", encodeURIComponent(migrationBrowser));
    chromeWindow.openUILinkIn(url, "tab");
  },

  QueryInterface: XPCOMUtils.generateQI(
    [Ci.nsIObserver, Ci.nsINavBookmarkObserver, Ci.nsISupportsWeakReference]
  ),

  /**
   * Undo action called by the UndoNotification or by the newtab
   * @param chromeWindow A reference to the window in which to open a link.
   */
  undoAutoMigration(chromeWindow) {
    this._maybeOpenUndoSurveyTab(chromeWindow);
    this.undo();
  },

  /**
   * Keep the automigration result and not prompt anymore
   */
  keepAutoMigration() {
    this._purgeUndoState(this.UNDO_REMOVED_REASON_OFFER_REJECTED);
  },
};

AutoMigrate.init();
