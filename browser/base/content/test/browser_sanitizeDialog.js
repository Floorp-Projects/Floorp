/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests the sanitize dialog (a.k.a. the clear recent history dialog).
 * See bug 480169.
 *
 * The purpose of this test is not to fully flex the sanitize timespan code;
 * browser/base/content/test/browser_sanitize-timespans.js does that.  This
 * test checks the UI of the dialog and makes sure it's correctly connected to
 * the sanitize timespan code.
 *
 * Some of this code, especially the history creation parts, was taken from
 * browser/base/content/test/browser_sanitize-timespans.js.
 */

let tempScope = {};
Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader)
                                           .loadSubScript("chrome://browser/content/sanitize.js", tempScope);
let Sanitizer = tempScope.Sanitizer;

const dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
const formhist = Cc["@mozilla.org/satchel/form-history;1"].
                 getService(Ci.nsIFormHistory2);

const kUsecPerMin = 60 * 1000000;

// Add tests here.  Each is a function that's called by doNextTest().
var gAllTests = [

  /**
   * Initializes the dialog to its default state.
   */
  function () {
    let wh = new WindowHelper();
    wh.onload = function () {
      // Select "Last Hour"
      this.selectDuration(Sanitizer.TIMESPAN_HOUR);
      // Hide details
      if (!this.getItemList().collapsed)
        this.toggleDetails();
      this.acceptDialog();
    };
    wh.open();
  },

  /**
   * Cancels the dialog, makes sure history not cleared.
   */
  function () {
    // Add history (within the past hour)
    let uris = [];
    for (let i = 0; i < 30; i++) {
      uris.push(addHistoryWithMinutesAgo(i));
    }

    let wh = new WindowHelper();
    wh.onload = function () {
      this.selectDuration(Sanitizer.TIMESPAN_HOUR);
      this.checkPrefCheckbox("history", false);
      this.checkDetails(false);

      // Show details
      this.toggleDetails();
      this.checkDetails(true);

      // Hide details
      this.toggleDetails();
      this.checkDetails(false);
      this.cancelDialog();

      ensureHistoryClearedState(uris, false);
      blankSlate();
      ensureHistoryClearedState(uris, true);
    };
    wh.open();
  },

  /**
   * Ensures that the combined history-downloads checkbox clears both history
   * visits and downloads when checked; the dialog respects simple timespan.
   */
  function () {
    // Add history and downloads (within the past hour).
    let uris = [];
    for (let i = 0; i < 30; i++) {
      uris.push(addHistoryWithMinutesAgo(i));
    }
    let downloadIDs = [];
    for (let i = 0; i < 5; i++) {
      downloadIDs.push(addDownloadWithMinutesAgo(i));
    }
    // Add history and downloads (over an hour ago).
    let olderURIs = [];
    for (let i = 0; i < 5; i++) {
      olderURIs.push(addHistoryWithMinutesAgo(61 + i));
    }
    let olderDownloadIDs = [];
    for (let i = 0; i < 5; i++) {
      olderDownloadIDs.push(addDownloadWithMinutesAgo(61 + i));
    }
    let totalHistoryVisits = uris.length + olderURIs.length;

    let wh = new WindowHelper();
    wh.onload = function () {
      this.selectDuration(Sanitizer.TIMESPAN_HOUR);
      this.checkPrefCheckbox("history", true);
      this.acceptDialog();

      intPrefIs("sanitize.timeSpan", Sanitizer.TIMESPAN_HOUR,
                "timeSpan pref should be hour after accepting dialog with " +
                "hour selected");
      boolPrefIs("cpd.history", true,
                 "history pref should be true after accepting dialog with " +
                 "history checkbox checked");
      boolPrefIs("cpd.downloads", true,
                 "downloads pref should be true after accepting dialog with " +
                 "history checkbox checked");

      // History visits and downloads within one hour should be cleared.
      ensureHistoryClearedState(uris, true);
      ensureDownloadsClearedState(downloadIDs, true);

      // Visits and downloads > 1 hour should still exist.
      ensureHistoryClearedState(olderURIs, false);
      ensureDownloadsClearedState(olderDownloadIDs, false);

      // OK, done, cleanup after ourselves.
      blankSlate();
      ensureHistoryClearedState(olderURIs, true);
      ensureDownloadsClearedState(olderDownloadIDs, true);
    };
    wh.open();
  },

  /**
   * Ensures that the combined history-downloads checkbox removes neither
   * history visits nor downloads when not checked.
   */
  function () {
    // Add history, downloads, form entries (within the past hour).
    let uris = [];
    for (let i = 0; i < 5; i++) {
      uris.push(addHistoryWithMinutesAgo(i));
    }
    let downloadIDs = [];
    for (let i = 0; i < 5; i++) {
      downloadIDs.push(addDownloadWithMinutesAgo(i));
    }
    let formEntries = [];
    for (let i = 0; i < 5; i++) {
      formEntries.push(addFormEntryWithMinutesAgo(i));
    }

    let wh = new WindowHelper();
    wh.onload = function () {
      is(this.isWarningPanelVisible(), false,
         "Warning panel should be hidden after previously accepting dialog " +
         "with a predefined timespan");
      this.selectDuration(Sanitizer.TIMESPAN_HOUR);

      // Remove only form entries, leave history (including downloads).
      this.checkPrefCheckbox("history", false);
      this.checkPrefCheckbox("formdata", true);
      this.acceptDialog();

      intPrefIs("sanitize.timeSpan", Sanitizer.TIMESPAN_HOUR,
                "timeSpan pref should be hour after accepting dialog with " +
                "hour selected");
      boolPrefIs("cpd.history", false,
                 "history pref should be false after accepting dialog with " +
                 "history checkbox unchecked");
      boolPrefIs("cpd.downloads", false,
                 "downloads pref should be false after accepting dialog with " +
                 "history checkbox unchecked");

      // Of the three only form entries should be cleared.
      ensureHistoryClearedState(uris, false);
      ensureDownloadsClearedState(downloadIDs, false);
      ensureFormEntriesClearedState(formEntries, true);

      // OK, done, cleanup after ourselves.
      blankSlate();
      ensureHistoryClearedState(uris, true);
      ensureDownloadsClearedState(downloadIDs, true);
    };
    wh.open();
  },

  /**
   * Ensures that the "Everything" duration option works.
   */
  function () {
    // Add history.
    let uris = [];
    uris.push(addHistoryWithMinutesAgo(10));  // within past hour
    uris.push(addHistoryWithMinutesAgo(70));  // within past two hours
    uris.push(addHistoryWithMinutesAgo(130)); // within past four hours
    uris.push(addHistoryWithMinutesAgo(250)); // outside past four hours

    let wh = new WindowHelper();
    wh.onload = function () {
      is(this.isWarningPanelVisible(), false,
         "Warning panel should be hidden after previously accepting dialog " +
         "with a predefined timespan");
      this.selectDuration(Sanitizer.TIMESPAN_EVERYTHING);
      this.checkPrefCheckbox("history", true);
      this.checkDetails(true);

      // Hide details
      this.toggleDetails();
      this.checkDetails(false);

      // Show details
      this.toggleDetails();
      this.checkDetails(true);

      this.acceptDialog();

      intPrefIs("sanitize.timeSpan", Sanitizer.TIMESPAN_EVERYTHING,
                "timeSpan pref should be everything after accepting dialog " +
                "with everything selected");
      ensureHistoryClearedState(uris, true);
    };
    wh.open();
  },

  /**
   * Ensures that the "Everything" warning is visible on dialog open after
   * the previous test.
   */
  function () {
    // Add history.
    let uris = [];
    uris.push(addHistoryWithMinutesAgo(10));  // within past hour
    uris.push(addHistoryWithMinutesAgo(70));  // within past two hours
    uris.push(addHistoryWithMinutesAgo(130)); // within past four hours
    uris.push(addHistoryWithMinutesAgo(250)); // outside past four hours

    let wh = new WindowHelper();
    wh.onload = function () {
      is(this.isWarningPanelVisible(), true,
         "Warning panel should be visible after previously accepting dialog " +
         "with clearing everything");
      this.selectDuration(Sanitizer.TIMESPAN_EVERYTHING);
      this.checkPrefCheckbox("history", true);
      this.acceptDialog();

      intPrefIs("sanitize.timeSpan", Sanitizer.TIMESPAN_EVERYTHING,
                "timeSpan pref should be everything after accepting dialog " +
                "with everything selected");
      ensureHistoryClearedState(uris, true);
    };
    wh.open();
  },

  /**
   * The next three tests checks that when a certain history item cannot be
   * cleared then the checkbox should be both disabled and unchecked.
   * In addition, we ensure that this behavior does not modify the preferences.
   */
  function () {
    // Add history.
    let uris = [ addHistoryWithMinutesAgo(10) ];
    let formEntries = [ addFormEntryWithMinutesAgo(10) ];

    let wh = new WindowHelper();
    wh.onload = function() {
      // Check that the relevant checkboxes are enabled
      var cb = this.win.document.querySelectorAll(
                 "#itemList > [preference='privacy.cpd.formdata']");
      ok(cb.length == 1 && !cb[0].disabled, "There is formdata, checkbox to " +
         "clear formdata should be enabled.");

      var cb = this.win.document.querySelectorAll(
                 "#itemList > [preference='privacy.cpd.history']");
      ok(cb.length == 1 && !cb[0].disabled, "There is history, checkbox to " +
         "clear history should be enabled.");

      this.checkAllCheckboxes();
      this.acceptDialog();

      ensureHistoryClearedState(uris, true);
      ensureFormEntriesClearedState(formEntries, true);
    };
    wh.open();
  },
  function () {
    let wh = new WindowHelper();
    wh.onload = function() {
      boolPrefIs("cpd.history", true,
                 "history pref should be true after accepting dialog with " +
                 "history checkbox checked");
      boolPrefIs("cpd.formdata", true,
                 "formdata pref should be true after accepting dialog with " +
                 "formdata checkbox checked");


      // Even though the formdata pref is true, because there is no history
      // left to clear, the checkbox will be disabled.
      var cb = this.win.document.querySelectorAll(
                 "#itemList > [preference='privacy.cpd.formdata']");
      ok(cb.length == 1 && cb[0].disabled && !cb[0].checked,
         "There is no formdata history, checkbox should be disabled and be " +
         "cleared to reduce user confusion (bug 497664).");

      var cb = this.win.document.querySelectorAll(
                 "#itemList > [preference='privacy.cpd.history']");
      ok(cb.length == 1 && !cb[0].disabled && cb[0].checked,
         "There is no history, but history checkbox should always be enabled " +
         "and will be checked from previous preference.");

      this.acceptDialog();
    }
    wh.open();
  },
  function () {
    let formEntries = [ addFormEntryWithMinutesAgo(10) ];

    let wh = new WindowHelper();
    wh.onload = function() {
      boolPrefIs("cpd.formdata", true,
                 "formdata pref should persist previous value after accepting " +
                 "dialog where you could not clear formdata.");

      var cb = this.win.document.querySelectorAll(
                 "#itemList > [preference='privacy.cpd.formdata']");
      ok(cb.length == 1 && !cb[0].disabled && cb[0].checked,
         "There exists formEntries so the checkbox should be in sync with " +
         "the pref.");

      this.acceptDialog();
      ensureFormEntriesClearedState(formEntries, true);
    };
    wh.open();
  },


  /**
   * These next six tests together ensure that toggling details persists
   * across dialog openings.
   */
  function () {
    let wh = new WindowHelper();
    wh.onload = function () {
      // Check all items and select "Everything"
      this.checkAllCheckboxes();
      this.selectDuration(Sanitizer.TIMESPAN_EVERYTHING);

      // Hide details
      this.toggleDetails();
      this.checkDetails(false);
      this.acceptDialog();
    };
    wh.open();
  },
  function () {
    let wh = new WindowHelper();
    wh.onload = function () {
      // Details should remain closed because all items are checked.
      this.checkDetails(false);

      // Uncheck history.
      this.checkPrefCheckbox("history", false);
      this.acceptDialog();
    };
    wh.open();
  },
  function () {
    let wh = new WindowHelper();
    wh.onload = function () {
      // Details should be open because not all items are checked.
      this.checkDetails(true);

      // Modify the Site Preferences item state (bug 527820)
      this.checkAllCheckboxes();
      this.checkPrefCheckbox("siteSettings", false);
      this.acceptDialog();
    };
    wh.open();
  },
  function () {
    let wh = new WindowHelper();
    wh.onload = function () {
      // Details should be open because not all items are checked.
      this.checkDetails(true);

      // Hide details
      this.toggleDetails();
      this.checkDetails(false);
      this.cancelDialog();
    };
    wh.open();
  },
  function () {
    let wh = new WindowHelper();
    wh.onload = function () {
      // Details should be open because not all items are checked.
      this.checkDetails(true);

      // Select another duration
      this.selectDuration(Sanitizer.TIMESPAN_HOUR);
      // Hide details
      this.toggleDetails();
      this.checkDetails(false);
      this.acceptDialog();
    };
    wh.open();
  },
  function () {
    let wh = new WindowHelper();
    wh.onload = function () {
      // Details should not be open because "Last Hour" is selected
      this.checkDetails(false);

      this.cancelDialog();
    };
    wh.open();
  },
  function () {
    let wh = new WindowHelper();
    wh.onload = function () {
      // Details should have remained closed
      this.checkDetails(false);

      // Show details
      this.toggleDetails();
      this.checkDetails(true);
      this.cancelDialog();
    };
    wh.open();
  },
  function () {
    // Test for offline apps data and cache deletion

    // Prepare stuff, we will work with www.example.com
    var URL = "http://www.example.com";

    var ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);
    var URI = ios.newURI(URL, null, null);

    var sm = Cc["@mozilla.org/scriptsecuritymanager;1"]
             .getService(Ci.nsIScriptSecurityManager);
    var principal = sm.getNoAppCodebasePrincipal(URI);

    // Give www.example.com privileges to store offline data
    var pm = Cc["@mozilla.org/permissionmanager;1"]
             .getService(Ci.nsIPermissionManager);
    pm.add(URI, "offline-app", Ci.nsIPermissionManager.ALLOW_ACTION);
    pm.add(URI, "offline-app", Ci.nsIOfflineCacheUpdateService.ALLOW_NO_WARN);

    // Store some user data to localStorage
    var dsm = Cc["@mozilla.org/dom/storagemanager;1"]
             .getService(Ci.nsIDOMStorageManager);
    var localStorage = dsm.getLocalStorageForPrincipal(principal, URL);
    localStorage.setItem("test", "value");

    // Store something to the offline cache
    const nsICache = Components.interfaces.nsICache;
    var cs = Components.classes["@mozilla.org/network/cache-service;1"]
             .getService(Components.interfaces.nsICacheService);
    var session = cs.createSession(URL + "/manifest", nsICache.STORE_OFFLINE, nsICache.STREAM_BASED);

    // Open the dialog
    let wh = new WindowHelper();
    wh.onload = function () {
      this.selectDuration(Sanitizer.TIMESPAN_EVERYTHING);
      // Show details
      this.toggleDetails();
      // Clear only offlineApps
      this.uncheckAllCheckboxes();
      this.checkPrefCheckbox("offlineApps", true);
      this.acceptDialog();

      // Check all has been deleted (data, cache)
      is(localStorage.length, 0, "DOM storage cleared");

      var size = -1;
      var visitor = {
        visitDevice: function (deviceID, deviceInfo)
        {
          if (deviceID == "offline")
            size = deviceInfo.totalSize;

          // Do not enumerate entries
          return false;
        },

        visitEntry: function (deviceID, entryInfo)
        {
          // Do not enumerate entries.
          return false;
        }
      };
      cs.visitEntries(visitor);
      is(size, 0, "offline application cache entries evicted");
    };

    var cacheListener = {
      onCacheEntryAvailable: function (entry, access, status) {
        is(status, Cr.NS_OK);
        var stream = entry.openOutputStream(0);
        var content = "content";
        stream.write(content, content.length);
        stream.close();
        entry.close();
        wh.open();
      }
    };

    session.asyncOpenCacheEntry(URL, nsICache.ACCESS_READ_WRITE, cacheListener);
  },
  function () {
    // Test for offline apps permission deletion

    // Prepare stuff, we will work with www.example.com
    var URL = "http://www.example.com";

    var ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);
    var URI = ios.newURI(URL, null, null);

    // Open the dialog
    let wh = new WindowHelper();
    wh.onload = function () {
      this.selectDuration(Sanitizer.TIMESPAN_EVERYTHING);
      // Show details
      this.toggleDetails();
      // Clear only offlineApps
      this.uncheckAllCheckboxes();
      this.checkPrefCheckbox("siteSettings", true);
      this.acceptDialog();

      // Check all has been deleted (privileges, data, cache)
      var pm = Cc["@mozilla.org/permissionmanager;1"]
               .getService(Ci.nsIPermissionManager);
      is(pm.testPermission(URI, "offline-app"), 0, "offline-app permissions removed");
    };
    wh.open();
  }
];

// Used as the download database ID for a new download.  Incremented for each
// new download.  See addDownloadWithMinutesAgo().
var gDownloadId = 5555551;

// Index in gAllTests of the test currently being run.  Incremented for each
// test run.  See doNextTest().
var gCurrTest = 0;

var now_uSec = Date.now() * 1000;

///////////////////////////////////////////////////////////////////////////////

/**
 * This wraps the dialog and provides some convenience methods for interacting
 * with it.
 *
 * @param aWin
 *        The dialog's nsIDOMWindow
 */
function WindowHelper(aWin) {
  this.win = aWin;
}

WindowHelper.prototype = {
  /**
   * "Presses" the dialog's OK button.
   */
  acceptDialog: function () {
    is(this.win.document.documentElement.getButton("accept").disabled, false,
       "Dialog's OK button should not be disabled");
    this.win.document.documentElement.acceptDialog();
  },

  /**
   * "Presses" the dialog's Cancel button.
   */
  cancelDialog: function () {
    this.win.document.documentElement.cancelDialog();
  },

  /**
   * Ensures that the details progressive disclosure button and the item list
   * hidden by it match up.  Also makes sure the height of the dialog is
   * sufficient for the item list and warning panel.
   *
   * @param aShouldBeShown
   *        True if you expect the details to be shown and false if hidden
   */
  checkDetails: function (aShouldBeShown) {
    let button = this.getDetailsButton();
    let list = this.getItemList();
    let hidden = list.hidden || list.collapsed;
    is(hidden, !aShouldBeShown,
       "Details should be " + (aShouldBeShown ? "shown" : "hidden") +
       " but were actually " + (hidden ? "hidden" : "shown"));
    let dir = hidden ? "down" : "up";
    is(button.className, "expander-" + dir,
       "Details button should be " + dir + " because item list is " +
       (hidden ? "" : "not ") + "hidden");
    let height = 0;
    if (!hidden) {
      ok(list.boxObject.height > 30, "listbox has sufficient size")
      height += list.boxObject.height;
    }
    if (this.isWarningPanelVisible())
      height += this.getWarningPanel().boxObject.height;
    ok(height < this.win.innerHeight,
       "Window should be tall enough to fit warning panel and item list");
  },

  /**
   * (Un)checks a history scope checkbox (browser & download history,
   * form history, etc.).
   *
   * @param aPrefName
   *        The final portion of the checkbox's privacy.cpd.* preference name
   * @param aCheckState
   *        True if the checkbox should be checked, false otherwise
   */
  checkPrefCheckbox: function (aPrefName, aCheckState) {
    var pref = "privacy.cpd." + aPrefName;
    var cb = this.win.document.querySelectorAll(
               "#itemList > [preference='" + pref + "']");
    is(cb.length, 1, "found checkbox for " + pref + " preference");
    if (cb[0].checked != aCheckState)
      cb[0].click();
  },

  /**
   * Makes sure all the checkboxes are checked.
   */
  _checkAllCheckboxesCustom: function (check) {
    var cb = this.win.document.querySelectorAll("#itemList > [preference]");
    ok(cb.length > 1, "found checkboxes for preferences");
    for (var i = 0; i < cb.length; ++i) {
      var pref = this.win.document.getElementById(cb[i].getAttribute("preference"));
      if (!!pref.value ^ check)
        cb[i].click();
    }
  },

  checkAllCheckboxes: function () {
    this._checkAllCheckboxesCustom(true);
  },

  uncheckAllCheckboxes: function () {
    this._checkAllCheckboxesCustom(false);
  },

  /**
   * @return The details progressive disclosure button
   */
  getDetailsButton: function () {
    return this.win.document.getElementById("detailsExpander");
  },

  /**
   * @return The dialog's duration dropdown
   */
  getDurationDropdown: function () {
    return this.win.document.getElementById("sanitizeDurationChoice");
  },

  /**
   * @return The item list hidden by the details progressive disclosure button
   */
  getItemList: function () {
    return this.win.document.getElementById("itemList");
  },

  /**
   * @return The clear-everything warning box
   */
  getWarningPanel: function () {
    return this.win.document.getElementById("sanitizeEverythingWarningBox");
  },

  /**
   * @return True if the "Everything" warning panel is visible (as opposed to
   *         the tree)
   */
  isWarningPanelVisible: function () {
    return !this.getWarningPanel().hidden;
  },

  /**
   * Opens the clear recent history dialog.  Before calling this, set
   * this.onload to a function to execute onload.  It should close the dialog
   * when done so that the tests may continue.  Set this.onunload to a function
   * to execute onunload.  this.onunload is optional.
   */
  open: function () {
    let wh = this;

    function windowObserver(aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened")
        return;

      Services.ww.unregisterNotification(windowObserver);

      var loaded = false;
      let win = aSubject.QueryInterface(Ci.nsIDOMWindow);

      win.addEventListener("load", function onload(event) {
        win.removeEventListener("load", onload, false);

        if (win.name !== "SanitizeDialog")
          return;

        wh.win = win;
        loaded = true;

        executeSoon(function () {
          // Some exceptions that reach here don't reach the test harness, but
          // ok()/is() do...
          try {
            wh.onload();
          }
          catch (exc) {
            win.close();
            ok(false, "Unexpected exception: " + exc + "\n" + exc.stack);
            finish();
          }
        });
      }, false);

      win.addEventListener("unload", function onunload(event) {
        if (win.name !== "SanitizeDialog") {
          win.removeEventListener("unload", onunload, false);
          return;
        }

        // Why is unload fired before load?
        if (!loaded)
          return;

        win.removeEventListener("unload", onunload, false);
        wh.win = win;

        executeSoon(function () {
          // Some exceptions that reach here don't reach the test harness, but
          // ok()/is() do...
          try {
            if (wh.onunload)
              wh.onunload();
            waitForAsyncUpdates(doNextTest);
          }
          catch (exc) {
            win.close();
            ok(false, "Unexpected exception: " + exc + "\n" + exc.stack);
            finish();
          }
        });
      }, false);
    }
    Services.ww.registerNotification(windowObserver);
    Services.ww.openWindow(null,
                           "chrome://browser/content/sanitize.xul",
                           "SanitizeDialog",
                           "chrome,titlebar,dialog,centerscreen,modal",
                           null);
  },

  /**
   * Selects a duration in the duration dropdown.
   *
   * @param aDurVal
   *        One of the Sanitizer.TIMESPAN_* values
   */
  selectDuration: function (aDurVal) {
    this.getDurationDropdown().value = aDurVal;
    if (aDurVal === Sanitizer.TIMESPAN_EVERYTHING) {
      is(this.isWarningPanelVisible(), true,
         "Warning panel should be visible for TIMESPAN_EVERYTHING");
    }
    else {
      is(this.isWarningPanelVisible(), false,
         "Warning panel should not be visible for non-TIMESPAN_EVERYTHING");
    }
  },

  /**
   * Toggles the details progressive disclosure button.
   */
  toggleDetails: function () {
    this.getDetailsButton().click();
  }
};

/**
 * Adds a download to history.
 *
 * @param aMinutesAgo
 *        The download will be downloaded this many minutes ago
 */
function addDownloadWithMinutesAgo(aMinutesAgo) {
  let name = "fakefile-" + aMinutesAgo + "-minutes-ago";
  let data = {
    id:        gDownloadId,
    name:      name,
    source:   "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target:    name,
    startTime: now_uSec - (aMinutesAgo * kUsecPerMin),
    endTime:   now_uSec - ((aMinutesAgo + 1) * kUsecPerMin),
    state:     Ci.nsIDownloadManager.DOWNLOAD_FINISHED,
    currBytes: 0, maxBytes: -1, preferredAction: 0, autoResume: 0
  };

  let db = dm.DBConnection;
  let stmt = db.createStatement(
    "INSERT INTO moz_downloads (id, name, source, target, startTime, endTime, " +
      "state, currBytes, maxBytes, preferredAction, autoResume) " +
    "VALUES (:id, :name, :source, :target, :startTime, :endTime, :state, " +
      ":currBytes, :maxBytes, :preferredAction, :autoResume)");
  try {
    for (let prop in data) {
      stmt.params[prop] = data[prop];
    }
    stmt.execute();
  }
  finally {
    stmt.reset();
  }

  is(downloadExists(gDownloadId), true,
     "Sanity check: download " + gDownloadId +
     " should exist after creating it");

  return gDownloadId++;
}

/**
 * Adds a form entry to history.
 *
 * @param aMinutesAgo
 *        The entry will be added this many minutes ago
 */
function addFormEntryWithMinutesAgo(aMinutesAgo) {
  let name = aMinutesAgo + "-minutes-ago";
  formhist.addEntry(name, "dummy");

  // Artifically age the entry to the proper vintage.
  let db = formhist.DBConnection;
  let timestamp = now_uSec - (aMinutesAgo * kUsecPerMin);
  db.executeSimpleSQL("UPDATE moz_formhistory SET firstUsed = " +
                      timestamp +  " WHERE fieldname = '" + name + "'");

  is(formhist.nameExists(name), true,
     "Sanity check: form entry " + name + " should exist after creating it");
  return name;
}

/**
 * Adds a history visit to history.
 *
 * @param aMinutesAgo
 *        The visit will be visited this many minutes ago
 */
function addHistoryWithMinutesAgo(aMinutesAgo) {
  let pURI = makeURI("http://" + aMinutesAgo + "-minutes-ago.com/");
  PlacesUtils.history.addVisit(pURI,
                               now_uSec - aMinutesAgo * kUsecPerMin,
                               null,
                               Ci.nsINavHistoryService.TRANSITION_LINK,
                               false,
                               0);
  is(PlacesUtils.bhistory.isVisited(pURI), true,
     "Sanity check: history visit " + pURI.spec +
     " should exist after creating it");
  return pURI;
}

/**
 * Removes all history visits, downloads, and form entries.
 */
function blankSlate() {
  PlacesUtils.bhistory.removeAllPages();
  dm.cleanUp();
  formhist.removeAllEntries();
}

/**
 * Waits for all pending async statements on the default connection, before
 * proceeding with aCallback.
 *
 * @param aCallback
 *        Function to be called when done.
 * @param aScope
 *        Scope for the callback.
 * @param aArguments
 *        Arguments array for the callback.
 *
 * @note The result is achieved by asynchronously executing a query requiring
 *       a write lock.  Since all statements on the same connection are
 *       serialized, the end of this write operation means that all writes are
 *       complete.  Note that WAL makes so that writers don't block readers, but
 *       this is a problem only across different connections.
 */
function waitForAsyncUpdates(aCallback, aScope, aArguments)
{
  let scope = aScope || this;
  let args = aArguments || [];
  let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                              .DBConnection;
  let begin = db.createAsyncStatement("BEGIN EXCLUSIVE");
  begin.executeAsync();
  begin.finalize();

  let commit = db.createAsyncStatement("COMMIT");
  commit.executeAsync({
    handleResult: function() {},
    handleError: function() {},
    handleCompletion: function(aReason)
    {
      aCallback.apply(scope, args);
    }
  });
  commit.finalize();
}

/**
 * Ensures that the given pref is the expected value.
 *
 * @param aPrefName
 *        The pref's sub-branch under the privacy branch
 * @param aExpectedVal
 *        The pref's expected value
 * @param aMsg
 *        Passed to is()
 */
function boolPrefIs(aPrefName, aExpectedVal, aMsg) {
  is(gPrefService.getBoolPref("privacy." + aPrefName), aExpectedVal, aMsg);
}

/**
 * Checks to see if the download with the specified ID exists.
 *
 * @param  aID
 *         The ID of the download to check
 * @return True if the download exists, false otherwise
 */
function downloadExists(aID)
{
  let db = dm.DBConnection;
  let stmt = db.createStatement(
    "SELECT * " +
    "FROM moz_downloads " +
    "WHERE id = :id"
  );
  stmt.params.id = aID;
  let rows = stmt.executeStep();
  stmt.finalize();
  return !!rows;
}

/**
 * Runs the next test in the gAllTests array.  If all tests have been run,
 * finishes the entire suite.
 */
function doNextTest() {
  if (gAllTests.length <= gCurrTest) {
    blankSlate();
    waitForAsyncUpdates(finish);
  }
  else {
    let ct = gCurrTest;
    gCurrTest++;
    gAllTests[ct]();
  }
}

/**
 * Ensures that the specified downloads are either cleared or not.
 *
 * @param aDownloadIDs
 *        Array of download database IDs
 * @param aShouldBeCleared
 *        True if each download should be cleared, false otherwise
 */
function ensureDownloadsClearedState(aDownloadIDs, aShouldBeCleared) {
  let niceStr = aShouldBeCleared ? "no longer" : "still";
  aDownloadIDs.forEach(function (id) {
    is(downloadExists(id), !aShouldBeCleared,
       "download " + id + " should " + niceStr + " exist");
  });
}

/**
 * Ensures that the specified form entries are either cleared or not.
 *
 * @param aFormEntries
 *        Array of form entry names
 * @param aShouldBeCleared
 *        True if each form entry should be cleared, false otherwise
 */
function ensureFormEntriesClearedState(aFormEntries, aShouldBeCleared) {
  let niceStr = aShouldBeCleared ? "no longer" : "still";
  aFormEntries.forEach(function (entry) {
    is(formhist.nameExists(entry), !aShouldBeCleared,
       "form entry " + entry + " should " + niceStr + " exist");
  });
}

/**
 * Ensures that the specified URIs are either cleared or not.
 *
 * @param aURIs
 *        Array of page URIs
 * @param aShouldBeCleared
 *        True if each visit to the URI should be cleared, false otherwise
 */
function ensureHistoryClearedState(aURIs, aShouldBeCleared) {
  let niceStr = aShouldBeCleared ? "no longer" : "still";
  aURIs.forEach(function (aURI) {
    is(PlacesUtils.bhistory.isVisited(aURI), !aShouldBeCleared,
       "history visit " + aURI.spec + " should " + niceStr + " exist");
  });
}

/**
 * Ensures that the given pref is the expected value.
 *
 * @param aPrefName
 *        The pref's sub-branch under the privacy branch
 * @param aExpectedVal
 *        The pref's expected value
 * @param aMsg
 *        Passed to is()
 */
function intPrefIs(aPrefName, aExpectedVal, aMsg) {
  is(gPrefService.getIntPref("privacy." + aPrefName), aExpectedVal, aMsg);
}

///////////////////////////////////////////////////////////////////////////////

function test() {
  requestLongerTimeout(2);
  blankSlate();
  waitForExplicitFinish();
  // Kick off all the tests in the gAllTests array.
  waitForAsyncUpdates(doNextTest);
}
