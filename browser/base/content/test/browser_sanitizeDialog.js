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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
                                  "resource://gre/modules/FormHistory.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");

let tempScope = {};
Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader)
                                           .loadSubScript("chrome://browser/content/sanitize.js", tempScope);
let Sanitizer = tempScope.Sanitizer;

const kMsecPerMin = 60 * 1000;
const kUsecPerMin = 60 * 1000000;

let formEntries, downloadIDs, olderDownloadIDs;

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
    let places = [];
    let pURI;
    for (let i = 0; i < 30; i++) {
      pURI = makeURI("http://" + i + "-minutes-ago.com/");
      places.push({uri: pURI, visitDate: visitTimeForMinutesAgo(i)});
      uris.push(pURI);
    }

    addVisits(places, function() {
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
      };
      wh.onunload = function () {
        yield promiseHistoryClearedState(uris, false);
        yield blankSlate();
        yield promiseHistoryClearedState(uris, true);
      };
      wh.open();
    });
  },

  function () {
    // Add downloads (within the past hour).
    Task.spawn(function () {
      downloadIDs = [];
      for (let i = 0; i < 5; i++) {
        yield addDownloadWithMinutesAgo(downloadIDs, i);
      }
      // Add downloads (over an hour ago).
      olderDownloadIDs = [];
      for (let i = 0; i < 5; i++) {
        yield addDownloadWithMinutesAgo(olderDownloadIDs, 61 + i);
      }

      doNextTest();
    }).then(null, Components.utils.reportError);
  },

  /**
   * Ensures that the combined history-downloads checkbox clears both history
   * visits and downloads when checked; the dialog respects simple timespan.
   */
  function () {
    // Add history (within the past hour).
    let uris = [];
    let places = [];
    let pURI;
    for (let i = 0; i < 30; i++) {
      pURI = makeURI("http://" + i + "-minutes-ago.com/");
      places.push({uri: pURI, visitDate: visitTimeForMinutesAgo(i)});
      uris.push(pURI);
    }
    // Add history (over an hour ago).
    let olderURIs = [];
    for (let i = 0; i < 5; i++) {
      pURI = makeURI("http://" + (61 + i) + "-minutes-ago.com/");
      places.push({uri: pURI, visitDate: visitTimeForMinutesAgo(61 + i)});
      olderURIs.push(pURI);
    }

    addVisits(places, function() {
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
      };
      wh.onunload = function () {
        // History visits and downloads within one hour should be cleared.
        yield promiseHistoryClearedState(uris, true);
        yield ensureDownloadsClearedState(downloadIDs, true);

        // Visits and downloads > 1 hour should still exist.
        yield promiseHistoryClearedState(olderURIs, false);
        yield ensureDownloadsClearedState(olderDownloadIDs, false);

        // OK, done, cleanup after ourselves.
        yield blankSlate();
        yield promiseHistoryClearedState(olderURIs, true);
        yield ensureDownloadsClearedState(olderDownloadIDs, true);
      };
      wh.open();
    });
  },

  /**
   * Add form history entries for the next test.
   */
  function () {
    formEntries = [];

    let iter = function() {
      for (let i = 0; i < 5; i++) {
        formEntries.push(addFormEntryWithMinutesAgo(iter, i));
        yield undefined;
      }
      doNextTest();
    }();

    iter.next();
  },

  function () {
    // Add downloads (within the past hour).
    Task.spawn(function () {
      downloadIDs = [];
      for (let i = 0; i < 5; i++) {
        yield addDownloadWithMinutesAgo(downloadIDs, i);
      }

      doNextTest();
    }).then(null, Components.utils.reportError);
  },

  /**
   * Ensures that the combined history-downloads checkbox removes neither
   * history visits nor downloads when not checked.
   */
  function () {
    // Add history, downloads, form entries (within the past hour).
    let uris = [];
    let places = [];
    let pURI;
    for (let i = 0; i < 5; i++) {
      pURI = makeURI("http://" + i + "-minutes-ago.com/");
      places.push({uri: pURI, visitDate: visitTimeForMinutesAgo(i)});
      uris.push(pURI);
    }

    addVisits(places, function() {
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
      };
      wh.onunload = function () {
        // Of the three only form entries should be cleared.
        yield promiseHistoryClearedState(uris, false);
        yield ensureDownloadsClearedState(downloadIDs, false);

        formEntries.forEach(function (entry) {
          let exists = yield formNameExists(entry);
          is(exists, false, "form entry " + entry + " should no longer exist");
        });

        // OK, done, cleanup after ourselves.
        yield blankSlate();
        yield promiseHistoryClearedState(uris, true);
        yield ensureDownloadsClearedState(downloadIDs, true);
      };
      wh.open();
    });
  },

  /**
   * Ensures that the "Everything" duration option works.
   */
  function () {
    // Add history.
    let uris = [];
    let places = [];
    let pURI;
    // within past hour, within past two hours, within past four hours and 
    // outside past four hours
    [10, 70, 130, 250].forEach(function(aValue) {
      pURI = makeURI("http://" + aValue + "-minutes-ago.com/");
      places.push({uri: pURI, visitDate: visitTimeForMinutesAgo(aValue)});
      uris.push(pURI);
    });
    addVisits(places, function() {
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
      };
      wh.onunload = function () {
        yield promiseHistoryClearedState(uris, true);
      };
      wh.open();
    });
  },

  /**
   * Ensures that the "Everything" warning is visible on dialog open after
   * the previous test.
   */
  function () {
    // Add history.
    let uris = [];
    let places = [];
    let pURI;
    // within past hour, within past two hours, within past four hours and 
    // outside past four hours
    [10, 70, 130, 250].forEach(function(aValue) {
      pURI = makeURI("http://" + aValue + "-minutes-ago.com/");
      places.push({uri: pURI, visitDate: visitTimeForMinutesAgo(aValue)});
      uris.push(pURI);
    });
    addVisits(places, function() {
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
      };
      wh.onunload = function () {
        yield promiseHistoryClearedState(uris, true);
      };
      wh.open();
    });
  },

  /**
   * Add form history entry for the next test.
   */
  function () {
    let iter = function() {
      formEntries = [ addFormEntryWithMinutesAgo(iter, 10) ];
      yield undefined;
      doNextTest();
    }();

    iter.next();
  },

  /**
   * The next three tests checks that when a certain history item cannot be
   * cleared then the checkbox should be both disabled and unchecked.
   * In addition, we ensure that this behavior does not modify the preferences.
   */
  function () {
    // Add history.
    let pURI = makeURI("http://" + 10 + "-minutes-ago.com/");
    addVisits({uri: pURI, visitDate: visitTimeForMinutesAgo(10)}, function() {
      let uris = [ pURI ];

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
      };
      wh.onunload = function () {
        yield promiseHistoryClearedState(uris, true);

        let exists = yield formNameExists(formEntries[0]);
        is(exists, false, "form entry " + formEntries[0] + " should no longer exist");
      };
      wh.open();
    });
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

  /**
   * Add form history entry for the next test.
   */
  function () {
    let iter = function() {
      formEntries = [ addFormEntryWithMinutesAgo(iter, 10) ];
      yield undefined;
      doNextTest();
    }();

    iter.next();
  },

  function () {
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
    };
    wh.onunload = function () {
      let exists = yield formNameExists(formEntries[0]);
      is(exists, false, "form entry " + formEntries[0] + " should no longer exist");
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
    // Test for offline cache deletion

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
    pm.addFromPrincipal(principal, "offline-app", Ci.nsIPermissionManager.ALLOW_ACTION);
    pm.addFromPrincipal(principal, "offline-app", Ci.nsIOfflineCacheUpdateService.ALLOW_NO_WARN);

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

      // Check if the cache has been deleted
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

    var sm = Cc["@mozilla.org/scriptsecuritymanager;1"]
             .getService(Ci.nsIScriptSecurityManager);
    var principal = sm.getNoAppCodebasePrincipal(URI);

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
      is(pm.testPermissionFromPrincipal(principal, "offline-app"), 0, "offline-app permissions removed");
    };
    wh.open();
  }
];

// Index in gAllTests of the test currently being run.  Incremented for each
// test run.  See doNextTest().
var gCurrTest = 0;

let now_mSec = Date.now();
let now_uSec = now_mSec * 1000;

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
   * to execute onunload.  this.onunload is optional. If it returns true, the
   * caller is expected to call waitForAsyncUpdates at some point; if false is
   * returned, waitForAsyncUpdates is called automatically.
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
            if (wh.onunload) {
              Task.spawn(wh.onunload).then(function() {
                waitForAsyncUpdates(doNextTest);
              }).then(null, Components.utils.reportError);
            } else {
              waitForAsyncUpdates(doNextTest);
            }
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
function addDownloadWithMinutesAgo(aExpectedPathList, aMinutesAgo) {
  let publicList = yield Downloads.getList(Downloads.PUBLIC);

  let name = "fakefile-" + aMinutesAgo + "-minutes-ago";
  let download = yield Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: name
  });
  download.startTime = new Date(now_mSec - (aMinutesAgo * kMsecPerMin));
  download.canceled = true;
  publicList.add(download);

  ok((yield downloadExists(name)),
     "Sanity check: download " + name +
     " should exist after creating it");

  aExpectedPathList.push(name);
}

/**
 * Adds a form entry to history.
 *
 * @param aMinutesAgo
 *        The entry will be added this many minutes ago
 */
function addFormEntryWithMinutesAgo(then, aMinutesAgo) {
  let name = aMinutesAgo + "-minutes-ago";

  // Artifically age the entry to the proper vintage.
  let timestamp = now_uSec - (aMinutesAgo * kUsecPerMin);

  FormHistory.update({ op: "add", fieldname: name, value: "dummy", firstUsed: timestamp },
                     { handleError: function (error) {
                         do_throw("Error occurred updating form history: " + error);
                       },
                       handleCompletion: function (reason) { then.next(); }
                     });
  return name;
}

/**
 * Checks if a form entry exists.
 */
function formNameExists(name)
{
  let deferred = Promise.defer();

  let count = 0;
  FormHistory.count({ fieldname: name },
                    { handleResult: function (result) count = result,
                      handleError: function (error) {
                        do_throw("Error occurred searching form history: " + error);
                        deferred.reject(error);
                      },
                      handleCompletion: function (reason) {
                          if (!reason) deferred.resolve(count);
                      }
                    });

  return deferred.promise;
}

/**
 * Removes all history visits, downloads, and form entries.
 */
function blankSlate() {
  PlacesUtils.bhistory.removeAllPages();

  // The promise is resolved only when removing both downloads and form history are done.
  let deferred = Promise.defer();
  let formHistoryDone = false, downloadsDone = false;

  Task.spawn(function deleteAllDownloads() {
    let publicList = yield Downloads.getList(Downloads.PUBLIC);
    let downloads = yield publicList.getAll();
    for (let download of downloads) {
      publicList.remove(download);
      yield download.finalize(true);
    }
    downloadsDone = true;
    if (formHistoryDone) {
      deferred.resolve();
    }
  }).then(null, Components.utils.reportError);

  FormHistory.update({ op: "remove" },
                     { handleError: function (error) {
                         do_throw("Error occurred updating form history: " + error);
                         deferred.reject(error);
                       },
                       handleCompletion: function (reason) {
                         if (!reason) {
                           formHistoryDone = true;
                           if (downloadsDone) {
                             deferred.resolve();
                           }
                         }
                       }
                     });
  return deferred.promise;
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
 * Checks to see if the download with the specified path exists.
 *
 * @param  aPath
 *         The path of the download to check
 * @return True if the download exists, false otherwise
 */
function downloadExists(aPath)
{
  return Task.spawn(function() {
    let publicList = yield Downloads.getList(Downloads.PUBLIC);
    let listArray = yield publicList.getAll();
    throw new Task.Result(listArray.some(i => i.target.path == aPath));
  });
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
    is((yield downloadExists(id)), !aShouldBeCleared,
       "download " + id + " should " + niceStr + " exist");
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

/**
 * Creates a visit time.
 *
 * @param aMinutesAgo
 *        The visit will be visited this many minutes ago
 */
function visitTimeForMinutesAgo(aMinutesAgo) {
  return now_uSec - aMinutesAgo * kUsecPerMin;
}

///////////////////////////////////////////////////////////////////////////////

function test() {
  requestLongerTimeout(2);
  waitForExplicitFinish();
  blankSlate();
  // Kick off all the tests in the gAllTests array.
  waitForAsyncUpdates(doNextTest);
}
