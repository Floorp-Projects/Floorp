/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
  Sqlite: "resource://gre/modules/Sqlite.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  WindowsRegistry: "resource://gre/modules/WindowsRegistry.sys.mjs",
});
ChromeUtils.defineModuleGetter(
  lazy,
  "LoginHelper",
  "resource://gre/modules/LoginHelper.jsm"
);

var gMigrators = null;
var gProfileStartup = null;
var gL10n = null;
var gPreviousDefaultBrowserKey = "";

let gForceExitSpinResolve = false;
let gKeepUndoData = false;
let gUndoData = null;

function getL10n() {
  if (!gL10n) {
    gL10n = new Localization(["browser/migration.ftl"]);
  }
  return gL10n;
}

/**
 * The singleton MigrationUtils service. This service is the primary mechanism
 * by which migrations from other browsers to this browser occur. The singleton
 * instance of this class is exported from this module as `MigrationUtils`.
 */
class MigrationUtils {
  resourceTypes = Object.freeze({
    COOKIES: Ci.nsIBrowserProfileMigrator.COOKIES,
    HISTORY: Ci.nsIBrowserProfileMigrator.HISTORY,
    FORMDATA: Ci.nsIBrowserProfileMigrator.FORMDATA,
    PASSWORDS: Ci.nsIBrowserProfileMigrator.PASSWORDS,
    BOOKMARKS: Ci.nsIBrowserProfileMigrator.BOOKMARKS,
    OTHERDATA: Ci.nsIBrowserProfileMigrator.OTHERDATA,
    SESSION: Ci.nsIBrowserProfileMigrator.SESSION,
  });

  /**
   * Helper for implementing simple asynchronous cases of migration resources'
   * |migrate(aCallback)| (see MigratorBase).  If your |migrate| method
   * just waits for some file to be read, for example, and then migrates
   * everything right away, you can wrap the async-function with this helper
   * and not worry about notifying the callback.
   *
   * @example
   * // For example, instead of writing:
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
   * // You may write:
   * setTimeout(MigrationUtils.wrapMigrateFunction(function() {
   *   if (importingFromMosaic)
   *     throw Cr.NS_ERROR_UNEXPECTED;
   * }, aCallback), 0);
   *
   * // ... and aCallback will be called with aSuccess=false when importing
   * // from Mosaic, or with aSuccess=true otherwise.
   *
   * @param {Function} aFunction
   *   the function that will be called sometime later.  If aFunction
   *   throws when it's called, aCallback(false) is called, otherwise
   *   aCallback(true) is called.
   * @param {Function} aCallback
   *   the callback function passed to |migrate|.
   * @returns {Function}
   *   the wrapped function.
   */
  wrapMigrateFunction(aFunction, aCallback) {
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
  }

  /**
   * Gets localized string corresponding to l10n-id
   *
   * @param {string} aKey
   *   The key of the id of the localization to retrieve.
   * @param {object} [aArgs=undefined]
   *   An optional map of arguments to the id.
   * @returns {Promise<string>}
   *   A promise that resolves to the retrieved localization.
   */
  getLocalizedString(aKey, aArgs) {
    let l10n = getL10n();
    return l10n.formatValue(aKey, aArgs);
  }

  /**
   * Get all the rows corresponding to a select query from a database, without
   * requiring a lock on the database. If fetching data fails (because someone
   * else tried to write to the DB at the same time, for example), we will
   * retry the fetch after a 100ms timeout, up to 10 times.
   *
   * @param {string} path
   *   The file path to the database we want to open.
   * @param {string} description
   *   A developer-readable string identifying what kind of database we're
   *   trying to open.
   * @param {string} selectQuery
   *   The SELECT query to use to fetch the rows.
   *
   * @returns {Promise<object[]|Error>}
   *   A promise that resolves to an array of rows. The promise will be
   *   rejected if the read/fetch failed even after retrying.
   */
  getRowsFromDBWithoutLocks(path, description, selectQuery) {
    let dbOptions = {
      readOnly: true,
      ignoreLockingMode: true,
      path,
    };

    const RETRYLIMIT = 10;
    const RETRYINTERVAL = 100;
    return (async function innerGetRows() {
      let rows = null;
      for (let retryCount = RETRYLIMIT; retryCount && !rows; retryCount--) {
        // Attempt to get the rows. If this succeeds, we will bail out of the loop,
        // close the database in a failsafe way, and pass the rows back.
        // If fetching the rows throws, we will wait RETRYINTERVAL ms
        // and try again. This will repeat a maximum of RETRYLIMIT times.
        let db;
        let didOpen = false;
        let previousException = { message: null };
        try {
          db = await lazy.Sqlite.openConnection(dbOptions);
          didOpen = true;
          rows = await db.execute(selectQuery);
        } catch (ex) {
          if (previousException.message != ex.message) {
            Cu.reportError(ex);
          }
          previousException = ex;
        } finally {
          try {
            if (didOpen) {
              await db.close();
            }
          } catch (ex) {}
        }
        if (previousException) {
          await new Promise(resolve => lazy.setTimeout(resolve, RETRYINTERVAL));
        }
      }
      if (!rows) {
        throw new Error(
          "Couldn't get rows from the " + description + " database."
        );
      }
      return rows;
    })();
  }

  get #migrators() {
    if (!gMigrators) {
      gMigrators = new Map();
    }
    return gMigrators;
  }

  forceExitSpinResolve() {
    gForceExitSpinResolve = true;
  }

  spinResolve(promise) {
    if (!(promise instanceof Promise)) {
      return promise;
    }
    let done = false;
    let result = null;
    let error = null;
    gForceExitSpinResolve = false;
    promise
      .catch(e => {
        error = e;
      })
      .then(r => {
        result = r;
        done = true;
      });

    Services.tm.spinEventLoopUntil(
      "MigrationUtils.jsm:MU_spinResolve",
      () => done || gForceExitSpinResolve
    );
    if (!done) {
      throw new Error("Forcefully exited event loop.");
    } else if (error) {
      throw error;
    } else {
      return result;
    }
  }

  /**
   * Returns the migrator for the given source, if any data is available
   * for this source, or null otherwise.
   *
   * If null is returned,  either no data can be imported for the given migrator,
   * or aMigratorKey is invalid  (e.g. ie on mac, or mosaic everywhere).  This
   * method should be used rather than direct getService for future compatibility
   * (see bug 718280).
   *
   * @param {string} aKey
   *   Internal name of the migration source. See `availableMigratorKeys`
   *   for supported values by OS.
   *
   * @returns {MigratorBase}
   *   A profile migrator implementing nsIBrowserProfileMigrator, if it can
   *   import any data, null otherwise.
   */
  async getMigrator(aKey) {
    let migrator = null;
    if (this.#migrators.has(aKey)) {
      migrator = this.#migrators.get(aKey);
    } else {
      try {
        migrator = Cc[
          "@mozilla.org/profile/migrator;1?app=browser&type=" + aKey
        ].createInstance(Ci.nsIBrowserProfileMigrator);
      } catch (ex) {
        Cu.reportError(ex);
      }
      this.#migrators.set(aKey, migrator);
    }

    try {
      return migrator && (await migrator.isSourceAvailable()) ? migrator : null;
    } catch (ex) {
      Cu.reportError(ex);
      return null;
    }
  }

  /**
   * Figure out what is the default browser, and if there is a migrator
   * for it, return that migrator's internal name.
   *
   * For the time being, the "internal name" of a migrator is its contract-id
   * trailer (e.g. ie for @mozilla.org/profile/migrator;1?app=browser&type=ie),
   * but it will soon be exposed properly.
   *
   * @returns {string}
   */
  getMigratorKeyForDefaultBrowser() {
    // Canary uses the same description as Chrome so we can't distinguish them.
    // Edge Beta on macOS uses "Microsoft Edge" with no "beta" indication.
    const APP_DESC_TO_KEY = {
      "Internet Explorer": "ie",
      "Microsoft Edge": "edge",
      Safari: "safari",
      Firefox: "firefox",
      Nightly: "firefox",
      Opera: "opera",
      Vivaldi: "vivaldi",
      "Opera GX": "opera-gx",
      "Brave Web Browser": "brave", // Windows, Linux
      Brave: "brave", // OS X
      "Google Chrome": "chrome", // Windows, Linux
      Chrome: "chrome", // OS X
      Chromium: "chromium", // Windows, OS X
      "Chromium Web Browser": "chromium", // Linux
      "360\u5b89\u5168\u6d4f\u89c8\u5668": "chromium-360se",
    };

    let key = "";
    try {
      let browserDesc = Cc["@mozilla.org/uriloader/external-protocol-service;1"]
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
    if (
      key == "firefox" &&
      AppConstants.isPlatformAndVersionAtMost("win", "6.2")
    ) {
      // Because we remove the registry key, reading the registry key only works once.
      // We save the value for subsequent calls to avoid hard-to-trace bugs when multiple
      // consumers ask for this key.
      if (gPreviousDefaultBrowserKey) {
        key = gPreviousDefaultBrowserKey;
      } else {
        // We didn't have a saved value, so check the registry.
        const kRegPath = "Software\\Mozilla\\Firefox";
        let oldDefault = lazy.WindowsRegistry.readRegKey(
          Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
          kRegPath,
          "OldDefaultBrowserCommand"
        );
        if (oldDefault) {
          // Remove the key:
          lazy.WindowsRegistry.removeRegKey(
            Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
            kRegPath,
            "OldDefaultBrowserCommand"
          );
          try {
            let file = Cc["@mozilla.org/file/local;1"].createInstance(
              Ci.nsILocalFileWin
            );
            file.initWithCommandLine(oldDefault);
            key =
              APP_DESC_TO_KEY[file.getVersionInfoField("FileDescription")] ||
              key;
            // Save the value for future callers.
            gPreviousDefaultBrowserKey = key;
          } catch (ex) {
            Cu.reportError(
              "Could not convert old default browser value to description."
            );
          }
        }
      }
    }
    return key;
  }

  /**
   * True if we're in the process of a startup migration.
   *
   * @type {boolean}
   */
  get isStartupMigration() {
    return gProfileStartup != null;
  }

  /**
   * In the case of startup migration, this is set to the nsIProfileStartup
   * instance passed to ProfileMigrator's migrate.
   *
   * @see showMigrationWizard
   * @type {nsIProfileStartup|null}
   */
  get profileStartup() {
    return gProfileStartup;
  }

  /**
   * Show the migration wizard.  On mac, this may just focus the wizard if it's
   * already running, in which case aOpener and aParams are ignored.
   *
   * @param {Window} [aOpener=null]
   *   optional; the window that asks to open the wizard.
   * @param {Array} [aParams=null]
   *   optional arguments for the migration wizard, in the form of an array
   *   This is passed as-is for the params argument of
   *   nsIWindowWatcher.openWindow. The array elements we expect are, in
   *   order:
   *   - {Number} migration entry point constant (see below)
   *   - {String} source browser identifier
   *   - {nsIBrowserProfileMigrator} actual migrator object
   *   - {Boolean} whether this is a startup migration
   *   - {Boolean} whether to skip the 'source' page
   *   - {String} an identifier for the profile to use when migrating
   *   NB: If you add new consumers, please add a migration entry point
   *   constant below, and specify at least the first element of the array
   *   (the migration entry point for purposes of telemetry).
   */
  showMigrationWizard(aOpener, aParams) {
    const DIALOG_URL = "chrome://browser/content/migration/migration.xhtml";
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
              comtaminatedVal = Cc[
                "@mozilla.org/supports-PRBool;1"
              ].createInstance(Ci.nsISupportsPRBool);
              comtaminatedVal.data = item;
              break;
            case "number":
              comtaminatedVal = Cc[
                "@mozilla.org/supports-PRUint32;1"
              ].createInstance(Ci.nsISupportsPRUint32);
              comtaminatedVal.data = item;
              break;
            case "string":
              comtaminatedVal = Cc[
                "@mozilla.org/supports-cstring;1"
              ].createInstance(Ci.nsISupportsCString);
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
              throw new Error(
                "Unexpected parameter type " + typeof item + ": " + item
              );
          }
        }
        params.appendElement(comtaminatedVal);
      }
    } else {
      params = aParams;
    }

    if (
      Services.prefs.getBoolPref(
        "browser.migrate.content-modal.enabled",
        false
      ) &&
      aOpener &&
      aOpener.gBrowser
    ) {
      const { gBrowser } = aOpener;
      const { selectedBrowser } = gBrowser;
      gBrowser.getTabDialogBox(selectedBrowser).open(DIALOG_URL, params);
    } else {
      Services.ww.openWindow(aOpener, DIALOG_URL, "_blank", features, params);
    }
  }

  /**
   * Show the migration wizard for startup-migration.  This should only be
   * called by ProfileMigrator (see ProfileMigrator.js), which implements
   * nsIProfileMigrator. This runs asynchronously if we are running an
   * automigration.
   *
   * @param {nsIProfileStartup} aProfileStartup
   *   the nsIProfileStartup instance provided to ProfileMigrator.migrate.
   * @param {string|null} [aMigratorKey=null]
   *   If set, the migration wizard will import from the corresponding
   *   migrator, bypassing the source-selection page.  Otherwise, the
   *   source-selection page will be displayed, either with the default
   *   browser selected, if it could be detected and if there is a
   *   migrator for it, or with the first option selected as a fallback
   *   (The first option is hardcoded to be the most common browser for
   *    the OS we run on.  See migration.xhtml).
   * @param {string|null} [aProfileToMigrate=null]
   *   If set, the migration wizard will import from the profile indicated.
   * @throws
   *   if aMigratorKey is invalid or if it points to a non-existent
   *   source.
   */
  startupMigration(aProfileStartup, aMigratorKey, aProfileToMigrate) {
    this.spinResolve(
      this.asyncStartupMigration(
        aProfileStartup,
        aMigratorKey,
        aProfileToMigrate
      )
    );
  }

  async asyncStartupMigration(
    aProfileStartup,
    aMigratorKey,
    aProfileToMigrate
  ) {
    if (!aProfileStartup) {
      throw new Error(
        "an profile-startup instance is required for startup-migration"
      );
    }
    gProfileStartup = aProfileStartup;

    let skipSourcePage = false,
      migrator = null,
      migratorKey = "";
    if (aMigratorKey) {
      migrator = await this.getMigrator(aMigratorKey);
      if (!migrator) {
        // aMigratorKey must point to a valid source, so, if it doesn't
        // cleanup and throw.
        this.finishMigration();
        throw new Error(
          "startMigration was asked to open auto-migrate from " +
            "a non-existent source: " +
            aMigratorKey
        );
      }
      migratorKey = aMigratorKey;
      skipSourcePage = true;
    } else {
      let defaultBrowserKey = this.getMigratorKeyForDefaultBrowser();
      if (defaultBrowserKey) {
        migrator = await this.getMigrator(defaultBrowserKey);
        if (migrator) {
          migratorKey = defaultBrowserKey;
        }
      }
    }

    if (!migrator) {
      let migrators = await Promise.all(
        this.availableMigratorKeys.map(key => this.getMigrator(key))
      );
      // If there's no migrator set so far, ensure that there is at least one
      // migrator available before opening the wizard.
      // Note that we don't need to check the default browser first, because
      // if that one existed we would have used it in the block above this one.
      if (!migrators.some(m => m)) {
        // None of the keys produced a usable migrator, so finish up here:
        this.finishMigration();
        return;
      }
    }

    let isRefresh =
      migrator && skipSourcePage && migratorKey == AppConstants.MOZ_APP_NAME;

    let migrationEntryPoint = this.MIGRATION_ENTRYPOINTS.FIRSTRUN;
    if (isRefresh) {
      migrationEntryPoint = this.MIGRATION_ENTRYPOINTS.FXREFRESH;
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
  }

  /**
   * This is only pseudo-private because some tests and helper functions
   * still expect to be able to directly access it.
   */
  _importQuantities = {
    bookmarks: 0,
    logins: 0,
    history: 0,
  };

  getImportedCount(type) {
    if (!this._importQuantities.hasOwnProperty(type)) {
      throw new Error(
        `Unknown import data type "${type}" passed to getImportedCount`
      );
    }
    return this._importQuantities[type];
  }

  insertBookmarkWrapper(bookmark) {
    this._importQuantities.bookmarks++;
    let insertionPromise = lazy.PlacesUtils.bookmarks.insert(bookmark);
    if (!gKeepUndoData) {
      return insertionPromise;
    }
    // If we keep undo data, add a promise handler that stores the undo data once
    // the bookmark has been inserted in the DB, and then returns the bookmark.
    let { parentGuid } = bookmark;
    return insertionPromise.then(bm => {
      let { guid, lastModified, type } = bm;
      gUndoData.get("bookmarks").push({
        parentGuid,
        guid,
        lastModified,
        type,
      });
      return bm;
    });
  }

  insertManyBookmarksWrapper(bookmarks, parent) {
    let insertionPromise = lazy.PlacesUtils.bookmarks.insertTree({
      guid: parent,
      children: bookmarks,
    });
    return insertionPromise.then(
      insertedItems => {
        this._importQuantities.bookmarks += insertedItems.length;
        if (gKeepUndoData) {
          let bmData = gUndoData.get("bookmarks");
          for (let bm of insertedItems) {
            let { parentGuid, guid, lastModified, type } = bm;
            bmData.push({ parentGuid, guid, lastModified, type });
          }
        }
        if (parent == lazy.PlacesUtils.bookmarks.toolbarGuid) {
          lazy.PlacesUIUtils.maybeToggleBookmarkToolbarVisibility(
            true /* aForceVisible */
          );
        }
      },
      ex => Cu.reportError(ex)
    );
  }

  insertVisitsWrapper(pageInfos) {
    let now = new Date();
    // Ensure that none of the dates are in the future. If they are, rewrite
    // them to be now. This means we don't loose history entries, but they will
    // be valid for the history store.
    for (let pageInfo of pageInfos) {
      for (let visit of pageInfo.visits) {
        if (visit.date && visit.date > now) {
          visit.date = now;
        }
      }
    }
    this._importQuantities.history += pageInfos.length;
    if (gKeepUndoData) {
      this.#updateHistoryUndo(pageInfos);
    }
    return lazy.PlacesUtils.history.insertMany(pageInfos);
  }

  async insertLoginsWrapper(logins) {
    this._importQuantities.logins += logins.length;
    let inserted = await lazy.LoginHelper.maybeImportLogins(logins);
    // Note that this means that if we import a login that has a newer password
    // than we know about, we will update the login, and an undo of the import
    // will not revert this. This seems preferable over removing the login
    // outright or storing the old password in the undo file.
    if (gKeepUndoData) {
      for (let { guid, timePasswordChanged } of inserted) {
        gUndoData.get("logins").push({ guid, timePasswordChanged });
      }
    }
  }

  /**
   * Iterates through the favicons, sniffs for a mime type,
   * and uses the mime type to properly import the favicon.
   *
   * @param {object[]} favicons
   *   An array of Objects with these properties:
   *     {Uint8Array} faviconData: The binary data of a favicon
   *     {nsIURI} uri: The URI of the associated page
   */
  insertManyFavicons(favicons) {
    let sniffer = Cc["@mozilla.org/image/loader;1"].createInstance(
      Ci.nsIContentSniffer
    );
    for (let faviconDataItem of favicons) {
      let mimeType = sniffer.getMIMETypeFromContent(
        null,
        faviconDataItem.faviconData,
        faviconDataItem.faviconData.length
      );
      let fakeFaviconURI = Services.io.newURI(
        "fake-favicon-uri:" + faviconDataItem.uri.spec
      );
      lazy.PlacesUtils.favicons.replaceFaviconData(
        fakeFaviconURI,
        faviconDataItem.faviconData,
        mimeType
      );
      lazy.PlacesUtils.favicons.setAndFetchFaviconForPage(
        faviconDataItem.uri,
        fakeFaviconURI,
        true,
        lazy.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        null,
        Services.scriptSecurityManager.getSystemPrincipal()
      );
    }
  }

  initializeUndoData() {
    gKeepUndoData = true;
    gUndoData = new Map([
      ["bookmarks", []],
      ["visits", []],
      ["logins", []],
    ]);
  }

  async #postProcessUndoData(state) {
    if (!state) {
      return state;
    }
    let bookmarkFolders = state
      .get("bookmarks")
      .filter(b => b.type == lazy.PlacesUtils.bookmarks.TYPE_FOLDER);

    let bookmarkFolderData = [];
    let bmPromises = bookmarkFolders.map(({ guid }) => {
      // Ignore bookmarks where the promise doesn't resolve (ie that are missing)
      // Also check that the bookmark fetch returns isn't null before adding it.
      return lazy.PlacesUtils.bookmarks.fetch(guid).then(
        bm => bm && bookmarkFolderData.push(bm),
        () => {}
      );
    });

    await Promise.all(bmPromises);
    let folderLMMap = new Map(
      bookmarkFolderData.map(b => [b.guid, b.lastModified])
    );
    for (let bookmark of bookmarkFolders) {
      let lastModified = folderLMMap.get(bookmark.guid);
      // If the bookmark was deleted, the map will be returning null, so check:
      if (lastModified) {
        bookmark.lastModified = lastModified;
      }
    }
    return state;
  }

  stopAndRetrieveUndoData() {
    let undoData = gUndoData;
    gUndoData = null;
    gKeepUndoData = false;
    return this.#postProcessUndoData(undoData);
  }

  #updateHistoryUndo(pageInfos) {
    let visits = gUndoData.get("visits");
    let visitMap = new Map(visits.map(v => [v.url, v]));
    for (let pageInfo of pageInfos) {
      let visitCount = pageInfo.visits.length;
      let first, last;
      if (visitCount > 1) {
        let dates = pageInfo.visits.map(v => v.date);
        first = Math.min.apply(Math, dates);
        last = Math.max.apply(Math, dates);
      } else {
        first = last = pageInfo.visits[0].date;
      }
      let url = pageInfo.url;
      if (url instanceof Ci.nsIURI) {
        url = pageInfo.url.spec;
      } else if (typeof url != "string") {
        pageInfo.url.href;
      }

      try {
        new URL(url);
      } catch (ex) {
        // This won't save and we won't need to 'undo' it, so ignore this URL.
        continue;
      }
      if (!visitMap.has(url)) {
        visitMap.set(url, { url, visitCount, first, last });
      } else {
        let currentData = visitMap.get(url);
        currentData.visitCount += visitCount;
        currentData.first = Math.min(currentData.first, first);
        currentData.last = Math.max(currentData.last, last);
      }
    }
    gUndoData.set("visits", Array.from(visitMap.values()));
  }

  /**
   * Cleans up references to migrators and nsIProfileInstance instances.
   */
  finishMigration() {
    gMigrators = null;
    gProfileStartup = null;
    gL10n = null;
  }

  get availableMigratorKeys() {
    if (AppConstants.platform == "win") {
      return [
        "firefox",
        "edge",
        "ie",
        "opera",
        "opera-gx",
        "vivaldi",
        "brave",
        "chrome",
        "chromium-edge",
        "chromium-edge-beta",
        "chrome-beta",
        "chromium",
        "chromium-360se",
        "canary",
      ];
    }
    if (AppConstants.platform == "macosx") {
      return [
        "firefox",
        "safari",
        "opera",
        "opera-gx",
        "vivaldi",
        "brave",
        "chrome",
        "chromium-edge",
        "chromium-edge-beta",
        "chromium",
        "canary",
      ];
    }
    if (AppConstants.XP_UNIX) {
      return [
        "firefox",
        "opera",
        "vivaldi",
        "brave",
        "chrome",
        "chrome-beta",
        "chrome-dev",
        "chromium",
        "opera-gx",
      ];
    }
    return [];
  }

  /**
   * Enum for the entrypoint that is being used to start migration.
   * Callers can use the MIGRATION_ENTRYPOINTS getter to use these.
   *
   * These values are what's written into the FX_MIGRATION_ENTRY_POINT
   * histogram after a migration.
   *
   * @see MIGRATION_ENTRYPOINTS
   * @readonly
   * @enum {number}
   */
  #MIGRATION_ENTRYPOINTS_ENUM = Object.freeze({
    /** The entrypoint was not supplied */
    UNKNOWN: 0,

    /** Migration is occurring at startup */
    FIRSTRUN: 1,

    /** Migration is occurring at after a profile refresh */
    FXREFRESH: 2,

    /** Migration is being started from the Library window */
    PLACES: 3,

    /** Migration is being started from our password management UI */
    PASSWORDS: 4,

    /** Migration is being started from the default about:home/about:newtab */
    NEWTAB: 5,

    /** Migration is being started from the File menu */
    FILE_MENU: 6,

    /** Migration is being started from the Help menu */
    HELP_MENU: 7,

    /** Migration is being started from the Bookmarks Toolbar */
    BOOKMARKS_TOOLBAR: 8,
  });

  /**
   * Returns an enum that should be used to record the entrypoint for
   * starting a migration.
   *
   * @returns {number}
   */
  get MIGRATION_ENTRYPOINTS() {
    return this.#MIGRATION_ENTRYPOINTS_ENUM;
  }

  /**
   * Enum for the numeric value written to the FX_MIGRATION_SOURCE_BROWSER,
   * and FX_STARTUP_MIGRATION_EXISTING_DEFAULT_BROWSER histograms.
   *
   * @see getSourceIdForTelemetry
   * @readonly
   * @enum {number}
   */
  #SOURCE_NAME_TO_ID_MAPPING_ENUM = Object.freeze({
    nothing: 1,
    firefox: 2,
    edge: 3,
    ie: 4,
    chrome: 5,
    "chrome-beta": 5,
    "chrome-dev": 5,
    chromium: 6,
    canary: 7,
    safari: 8,
    "chromium-360se": 9,
    "chromium-edge": 10,
    "chromium-edge-beta": 10,
    brave: 11,
    opera: 12,
    "opera-gx": 14,
    vivaldi: 13,
  });

  getSourceIdForTelemetry(sourceName) {
    return this.#SOURCE_NAME_TO_ID_MAPPING_ENUM[sourceName] || 0;
  }
}

const MigrationUtilsSingleton = new MigrationUtils();

export { MigrationUtilsSingleton as MigrationUtils };
