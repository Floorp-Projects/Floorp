/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests the sanitize dialog (a.k.a. the clear recent history dialog).
 * See bug 480169.
 *
 * The purpose of this test is not to fully flex the sanitize timespan code;
 * browser/base/content/test/sanitize/browser_sanitize-timespans.js does that.  This
 * test checks the UI of the dialog and makes sure it's correctly connected to
 * the sanitize timespan code.
 *
 * Some of this code, especially the history creation parts, was taken from
 * browser/base/content/test/sanitize/browser_sanitize-timespans.js.
 */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Timer",
                               "resource://gre/modules/Timer.jsm");
ChromeUtils.defineModuleGetter(this, "PlacesTestUtils",
                               "resource://testing-common/PlacesTestUtils.jsm");

const kMsecPerMin = 60 * 1000;
const kUsecPerMin = 60 * 1000000;

/**
 * Ensures that the specified URIs are either cleared or not.
 *
 * @param aURIs
 *        Array of page URIs
 * @param aShouldBeCleared
 *        True if each visit to the URI should be cleared, false otherwise
 */
async function promiseHistoryClearedState(aURIs, aShouldBeCleared) {
  for (let uri of aURIs) {
    let visited = await PlacesUtils.history.hasVisits(uri);
    Assert.equal(visited, !aShouldBeCleared,
      `history visit ${uri.spec} should ${aShouldBeCleared ? "no longer" : "still"} exist`);
  }
}

add_task(async function init() {
  requestLongerTimeout(3);
  await blankSlate();
  registerCleanupFunction(async function() {
    await blankSlate();
    await PlacesTestUtils.promiseAsyncUpdates();
  });
});

/**
 * Initializes the dialog to its default state.
 */
add_task(async function default_state() {
  let wh = new WindowHelper();
  wh.onload = function() {
    // Select "Last Hour"
    this.selectDuration(Sanitizer.TIMESPAN_HOUR);
    // Hide details
    if (!this.getItemList().collapsed)
      this.toggleDetails();
    this.acceptDialog();
  };
  wh.open();
  await wh.promiseClosed;
});

/**
 * Cancels the dialog, makes sure history not cleared.
 */
add_task(async function test_cancel() {
  // Add history (within the past hour)
  let uris = [];
  let places = [];
  let pURI;
  for (let i = 0; i < 30; i++) {
    pURI = makeURI("http://" + i + "-minutes-ago.com/");
    places.push({uri: pURI, visitDate: visitTimeForMinutesAgo(i)});
    uris.push(pURI);
  }
  await PlacesTestUtils.addVisits(places);

  let wh = new WindowHelper();
  wh.onload = function() {
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
  wh.onunload = async function() {
    await promiseHistoryClearedState(uris, false);
    await blankSlate();
    await promiseHistoryClearedState(uris, true);
  };
  wh.open();
  await wh.promiseClosed;
});

/**
 * Ensures that the combined history-downloads checkbox clears both history
 * visits and downloads when checked; the dialog respects simple timespan.
 */
add_task(async function test_history_downloads_checked() {
  // Add downloads (within the past hour).
  let downloadIDs = [];
  for (let i = 0; i < 5; i++) {
    await addDownloadWithMinutesAgo(downloadIDs, i);
  }
  // Add downloads (over an hour ago).
  let olderDownloadIDs = [];
  for (let i = 0; i < 5; i++) {
    await addDownloadWithMinutesAgo(olderDownloadIDs, 61 + i);
  }

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
  let promiseSanitized = promiseSanitizationComplete();

  await PlacesTestUtils.addVisits(places);

  let wh = new WindowHelper();
  wh.onload = function() {
    this.selectDuration(Sanitizer.TIMESPAN_HOUR);
    this.checkPrefCheckbox("history", true);
    this.acceptDialog();
  };
  wh.onunload = async function() {
    intPrefIs("sanitize.timeSpan", Sanitizer.TIMESPAN_HOUR,
              "timeSpan pref should be hour after accepting dialog with " +
              "hour selected");
    boolPrefIs("cpd.history", true,
               "history pref should be true after accepting dialog with " +
               "history checkbox checked");
    boolPrefIs("cpd.downloads", true,
               "downloads pref should be true after accepting dialog with " +
               "history checkbox checked");

    await promiseSanitized;

    // History visits and downloads within one hour should be cleared.
    await promiseHistoryClearedState(uris, true);
    await ensureDownloadsClearedState(downloadIDs, true);

    // Visits and downloads > 1 hour should still exist.
    await promiseHistoryClearedState(olderURIs, false);
    await ensureDownloadsClearedState(olderDownloadIDs, false);

    // OK, done, cleanup after ourselves.
    await blankSlate();
    await promiseHistoryClearedState(olderURIs, true);
    await ensureDownloadsClearedState(olderDownloadIDs, true);
  };
  wh.open();
  await wh.promiseClosed;
});

/**
 * Ensures that the combined history-downloads checkbox removes neither
 * history visits nor downloads when not checked.
 */
add_task(async function test_history_downloads_unchecked() {
  // Add form entries
  let formEntries = [];

  for (let i = 0; i < 5; i++) {
    formEntries.push((await promiseAddFormEntryWithMinutesAgo(i)));
  }


  // Add downloads (within the past hour).
  let downloadIDs = [];
  for (let i = 0; i < 5; i++) {
    await addDownloadWithMinutesAgo(downloadIDs, i);
  }

  // Add history, downloads, form entries (within the past hour).
  let uris = [];
  let places = [];
  let pURI;
  for (let i = 0; i < 5; i++) {
    pURI = makeURI("http://" + i + "-minutes-ago.com/");
    places.push({uri: pURI, visitDate: visitTimeForMinutesAgo(i)});
    uris.push(pURI);
  }

  await PlacesTestUtils.addVisits(places);
  let wh = new WindowHelper();
  wh.onload = function() {
    is(this.isWarningPanelVisible(), false,
       "Warning panel should be hidden after previously accepting dialog " +
       "with a predefined timespan");
    this.selectDuration(Sanitizer.TIMESPAN_HOUR);

    // Remove only form entries, leave history (including downloads).
    this.checkPrefCheckbox("history", false);
    this.checkPrefCheckbox("formdata", true);
    this.acceptDialog();
  };
  wh.onunload = async function() {
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
    await promiseHistoryClearedState(uris, false);
    await ensureDownloadsClearedState(downloadIDs, false);

    for (let entry of formEntries) {
      let exists = await formNameExists(entry);
      is(exists, false, "form entry " + entry + " should no longer exist");
    }

    // OK, done, cleanup after ourselves.
    await blankSlate();
    await promiseHistoryClearedState(uris, true);
    await ensureDownloadsClearedState(downloadIDs, true);
  };
  wh.open();
  await wh.promiseClosed;
});

/**
 * Ensures that the "Everything" duration option works.
 */
add_task(async function test_everything() {
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

  let promiseSanitized = promiseSanitizationComplete();

  await PlacesTestUtils.addVisits(places);
  let wh = new WindowHelper();
  wh.onload = function() {
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
  };
  wh.onunload = async function() {
    await promiseSanitized;
    intPrefIs("sanitize.timeSpan", Sanitizer.TIMESPAN_EVERYTHING,
              "timeSpan pref should be everything after accepting dialog " +
              "with everything selected");

    await promiseHistoryClearedState(uris, true);
  };
  wh.open();
  await wh.promiseClosed;
});

/**
 * Ensures that the "Everything" warning is visible on dialog open after
 * the previous test.
 */
add_task(async function test_everything_warning() {
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

  let promiseSanitized = promiseSanitizationComplete();

  await PlacesTestUtils.addVisits(places);
  let wh = new WindowHelper();
  wh.onload = function() {
    is(this.isWarningPanelVisible(), true,
       "Warning panel should be visible after previously accepting dialog " +
       "with clearing everything");
    this.selectDuration(Sanitizer.TIMESPAN_EVERYTHING);
    this.checkPrefCheckbox("history", true);
    this.acceptDialog();
  };
  wh.onunload = async function() {
    intPrefIs("sanitize.timeSpan", Sanitizer.TIMESPAN_EVERYTHING,
              "timeSpan pref should be everything after accepting dialog " +
              "with everything selected");

    await promiseSanitized;

    await promiseHistoryClearedState(uris, true);
  };
  wh.open();
 await wh.promiseClosed;
});

/**
 * The next three tests checks that when a certain history item cannot be
 * cleared then the checkbox should be both disabled and unchecked.
 * In addition, we ensure that this behavior does not modify the preferences.
 */
add_task(async function test_cannot_clear_history() {
  // Add form entries
  let formEntries = [ (await promiseAddFormEntryWithMinutesAgo(10)) ];

  let promiseSanitized = promiseSanitizationComplete();

  // Add history.
  let pURI = makeURI("http://" + 10 + "-minutes-ago.com/");
  await PlacesTestUtils.addVisits({uri: pURI, visitDate: visitTimeForMinutesAgo(10)});
  let uris = [ pURI ];

  let wh = new WindowHelper();
  wh.onload = function() {
    // Check that the relevant checkboxes are enabled
    var cb = this.win.document.querySelectorAll(
               "#itemList > [preference='privacy.cpd.formdata']");
    ok(cb.length == 1 && !cb[0].disabled, "There is formdata, checkbox to " +
       "clear formdata should be enabled.");

    cb = this.win.document.querySelectorAll(
               "#itemList > [preference='privacy.cpd.history']");
    ok(cb.length == 1 && !cb[0].disabled, "There is history, checkbox to " +
       "clear history should be enabled.");

    this.checkAllCheckboxes();
    this.acceptDialog();
  };
  wh.onunload = async function() {
    await promiseSanitized;

    await promiseHistoryClearedState(uris, true);

    let exists = await formNameExists(formEntries[0]);
    is(exists, false, "form entry " + formEntries[0] + " should no longer exist");
  };
  wh.open();
  await wh.promiseClosed;
});

add_task(async function test_no_formdata_history_to_clear() {
  let promiseSanitized = promiseSanitizationComplete();
  let wh = new WindowHelper();
  wh.onload = function() {
    boolPrefIs("cpd.history", true,
               "history pref should be true after accepting dialog with " +
               "history checkbox checked");
    boolPrefIs("cpd.formdata", true,
               "formdata pref should be true after accepting dialog with " +
               "formdata checkbox checked");

    var cb = this.win.document.querySelectorAll(
               "#itemList > [preference='privacy.cpd.history']");
    ok(cb.length == 1 && !cb[0].disabled && cb[0].checked,
       "There is no history, but history checkbox should always be enabled " +
       "and will be checked from previous preference.");

    this.acceptDialog();
  };
  wh.open();
  await wh.promiseClosed;
  await promiseSanitized;
});

add_task(async function test_form_entries() {
  let formEntry = (await promiseAddFormEntryWithMinutesAgo(10));

  let promiseSanitized = promiseSanitizationComplete();

  let wh = new WindowHelper();
  wh.onload = function() {
    boolPrefIs("cpd.formdata", true,
               "formdata pref should persist previous value after accepting " +
               "dialog where you could not clear formdata.");

    var cb = this.win.document.querySelectorAll(
               "#itemList > [preference='privacy.cpd.formdata']");

    info("There exists formEntries so the checkbox should be in sync with the pref.");
    is(cb.length, 1, "There is only one checkbox for form data");
    ok(!cb[0].disabled, "The checkbox is enabled");
    ok(cb[0].checked, "The checkbox is checked");

    this.acceptDialog();
  };
  wh.onunload = async function() {
    await promiseSanitized;
    let exists = await formNameExists(formEntry);
    is(exists, false, "form entry " + formEntry + " should no longer exist");
  };
  wh.open();
  await wh.promiseClosed;
});


/**
 * Ensure that toggling details persists
 * across dialog openings.
 */
add_task(async function test_toggling_details_persists() {
  {
    let wh = new WindowHelper();
    wh.onload = function() {
      // Check all items and select "Everything"
      this.checkAllCheckboxes();
      this.selectDuration(Sanitizer.TIMESPAN_EVERYTHING);

      // Hide details
      this.toggleDetails();
      this.checkDetails(false);
      this.acceptDialog();
    };
    wh.open();
    await wh.promiseClosed;
  }
  {
    let wh = new WindowHelper();
    wh.onload = function() {
      // Details should remain closed because all items are checked.
      this.checkDetails(false);

      // Uncheck history.
      this.checkPrefCheckbox("history", false);
      this.acceptDialog();
    };
    wh.open();
    await wh.promiseClosed;
  }
  {
    let wh = new WindowHelper();
    wh.onload = function() {
      // Details should be open because not all items are checked.
      this.checkDetails(true);

      // Modify the Site Preferences item state (bug 527820)
      this.checkAllCheckboxes();
      this.checkPrefCheckbox("siteSettings", false);
      this.acceptDialog();
    };
    wh.open();
    await wh.promiseClosed;
  }
  {
    let wh = new WindowHelper();
    wh.onload = function() {
      // Details should be open because not all items are checked.
      this.checkDetails(true);

      // Hide details
      this.toggleDetails();
      this.checkDetails(false);
      this.cancelDialog();
    };
    wh.open();
    await wh.promiseClosed;
  }
  {
    let wh = new WindowHelper();
    wh.onload = function() {
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
    await wh.promiseClosed;
  }
  {
    let wh = new WindowHelper();
    wh.onload = function() {
      // Details should not be open because "Last Hour" is selected
      this.checkDetails(false);

      this.cancelDialog();
    };
    wh.open();
    await wh.promiseClosed;
  }
  {
    let wh = new WindowHelper();
    wh.onload = function() {
      // Details should have remained closed
      this.checkDetails(false);

      // Show details
      this.toggleDetails();
      this.checkDetails(true);
      this.cancelDialog();
    };
    wh.open();
    await wh.promiseClosed;
  }
});

// Test for offline cache deletion
add_task(async function test_offline_cache() {
  // Prepare stuff, we will work with www.example.com
  var URL = "http://www.example.com";
  var URI = makeURI(URL);
  var principal = Services.scriptSecurityManager.createCodebasePrincipal(URI, {});

  // Give www.example.com privileges to store offline data
  Services.perms.addFromPrincipal(principal, "offline-app", Ci.nsIPermissionManager.ALLOW_ACTION);
  Services.perms.addFromPrincipal(principal, "offline-app", Ci.nsIOfflineCacheUpdateService.ALLOW_NO_WARN);

  // Store something to the offline cache
  var appcacheserv = Cc["@mozilla.org/network/application-cache-service;1"]
                     .getService(Ci.nsIApplicationCacheService);
  var appcachegroupid = appcacheserv.buildGroupIDForInfo(makeURI(URL + "/manifest"), Services.loadContextInfo.default);
  var appcache = appcacheserv.createApplicationCache(appcachegroupid);
  var storage = Services.cache2.appCacheStorage(Services.loadContextInfo.default, appcache);

  // Open the dialog
  let wh = new WindowHelper();
  wh.onload = function() {
    this.selectDuration(Sanitizer.TIMESPAN_EVERYTHING);
    // Show details
    this.toggleDetails();
    // Clear only offlineApps
    this.uncheckAllCheckboxes();
    this.checkPrefCheckbox("offlineApps", true);
    this.acceptDialog();
  };
  wh.onunload = function() {
    // Check if the cache has been deleted
    var size = -1;
    var visitor = {
      onCacheStorageInfo(aEntryCount, aConsumption, aCapacity, aDiskDirectory) {
        size = aConsumption;
      }
    };
    storage.asyncVisitStorage(visitor, false);
    // Offline cache visit happens synchronously, since it's forwarded to the old code
    is(size, 0, "offline application cache entries evicted");
  };

  var cacheListener = {
    onCacheEntryCheck() { return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED; },
    onCacheEntryAvailable(entry, isnew, unused, status) {
      is(status, Cr.NS_OK);
      var stream = entry.openOutputStream(0, -1);
      var content = "content";
      stream.write(content, content.length);
      stream.close();
      entry.close();
      wh.open();
    }
  };

  storage.asyncOpenURI(makeURI(URL), "", Ci.nsICacheStorage.OPEN_TRUNCATE, cacheListener);
  await wh.promiseClosed;
});

// Test for offline apps permission deletion
add_task(async function test_offline_apps_permissions() {
  // Prepare stuff, we will work with www.example.com
  var URL = "http://www.example.com";
  var URI = makeURI(URL);
  var principal = Services.scriptSecurityManager.createCodebasePrincipal(URI, {});

  let promiseSanitized = promiseSanitizationComplete();

  // Open the dialog
  let wh = new WindowHelper();
  wh.onload = function() {
    this.selectDuration(Sanitizer.TIMESPAN_EVERYTHING);
    // Show details
    this.toggleDetails();
    // Clear only offlineApps
    this.uncheckAllCheckboxes();
    this.checkPrefCheckbox("siteSettings", true);
    this.acceptDialog();
  };
  wh.onunload = async function() {
    await promiseSanitized;

    // Check all has been deleted (privileges, data, cache)
    is(Services.perms.testPermissionFromPrincipal(principal, "offline-app"), 0, "offline-app permissions removed");
  };
  wh.open();
  await wh.promiseClosed;
});

var now_mSec = Date.now();
var now_uSec = now_mSec * 1000;

/**
 * This wraps the dialog and provides some convenience methods for interacting
 * with it.
 *
 * @param aWin
 *        The dialog's nsIDOMWindow
 */
function WindowHelper(aWin) {
  this.win = aWin;
  this.promiseClosed = new Promise(resolve => { this._resolveClosed = resolve; });
}

WindowHelper.prototype = {
  /**
   * "Presses" the dialog's OK button.
   */
  acceptDialog() {
    is(this.win.document.documentElement.getButton("accept").disabled, false,
       "Dialog's OK button should not be disabled");
    this.win.document.documentElement.acceptDialog();
  },

  /**
   * "Presses" the dialog's Cancel button.
   */
  cancelDialog() {
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
  checkDetails(aShouldBeShown) {
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
      ok(list.boxObject.height > 30, "listbox has sufficient size");
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
  checkPrefCheckbox(aPrefName, aCheckState) {
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
  _checkAllCheckboxesCustom(check) {
    var cb = this.win.document.querySelectorAll("#itemList > [preference]");
    ok(cb.length > 1, "found checkboxes for preferences");
    for (var i = 0; i < cb.length; ++i) {
      var pref = this.win.Preferences.get(cb[i].getAttribute("preference"));
      if (!!pref.value ^ check)
        cb[i].click();
    }
  },

  checkAllCheckboxes() {
    this._checkAllCheckboxesCustom(true);
  },

  uncheckAllCheckboxes() {
    this._checkAllCheckboxesCustom(false);
  },

  /**
   * @return The details progressive disclosure button
   */
  getDetailsButton() {
    return this.win.document.getElementById("detailsExpander");
  },

  /**
   * @return The dialog's duration dropdown
   */
  getDurationDropdown() {
    return this.win.document.getElementById("sanitizeDurationChoice");
  },

  /**
   * @return The item list hidden by the details progressive disclosure button
   */
  getItemList() {
    return this.win.document.getElementById("itemList");
  },

  /**
   * @return The clear-everything warning box
   */
  getWarningPanel() {
    return this.win.document.getElementById("sanitizeEverythingWarningBox");
  },

  /**
   * @return True if the "Everything" warning panel is visible (as opposed to
   *         the tree)
   */
  isWarningPanelVisible() {
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
  open() {
    let wh = this;

    function windowObserver(aSubject, aTopic, aData) {
      if (aTopic != "domwindowopened")
        return;

      Services.ww.unregisterNotification(windowObserver);

      var loaded = false;
      let win = aSubject.QueryInterface(Ci.nsIDOMWindow);

      win.addEventListener("load", function onload(event) {
        if (win.name !== "SanitizeDialog")
          return;

        wh.win = win;
        loaded = true;
        executeSoon(() => wh.onload());
      }, {once: true});

      win.addEventListener("unload", function onunload(event) {
        if (win.name !== "SanitizeDialog") {
          win.removeEventListener("unload", onunload);
          return;
        }

        // Why is unload fired before load?
        if (!loaded)
          return;

        win.removeEventListener("unload", onunload);
        wh.win = win;

        // Some exceptions that reach here don't reach the test harness, but
        // ok()/is() do...
        (async function() {
          if (wh.onunload) {
            await wh.onunload();
          }
          await PlacesTestUtils.promiseAsyncUpdates();
          wh._resolveClosed();
        })();
      });
    }
    Services.ww.registerNotification(windowObserver);

    let browserWin = null;
    if (Services.appinfo.OS !== "Darwin") {
      // Retrieve the browser window so we can specify it as the parent
      // of the dialog to simulate the way the user opens the dialog
      // on Windows and Linux.
      browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    }

    Services.ww.openWindow(browserWin,
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
  selectDuration(aDurVal) {
    this.getDurationDropdown().value = aDurVal;
    if (aDurVal === Sanitizer.TIMESPAN_EVERYTHING) {
      is(this.isWarningPanelVisible(), true,
         "Warning panel should be visible for TIMESPAN_EVERYTHING");
    } else {
      is(this.isWarningPanelVisible(), false,
         "Warning panel should not be visible for non-TIMESPAN_EVERYTHING");
    }
  },

  /**
   * Toggles the details progressive disclosure button.
   */
  toggleDetails() {
    this.getDetailsButton().click();
  }
};

function promiseSanitizationComplete() {
  return TestUtils.topicObserved("sanitizer-sanitization-complete");
}

/**
 * Adds a download to history.
 *
 * @param aMinutesAgo
 *        The download will be downloaded this many minutes ago
 */
async function addDownloadWithMinutesAgo(aExpectedPathList, aMinutesAgo) {
  let publicList = await Downloads.getList(Downloads.PUBLIC);

  let name = "fakefile-" + aMinutesAgo + "-minutes-ago";
  let download = await Downloads.createDownload({
    source: "https://bugzilla.mozilla.org/show_bug.cgi?id=480169",
    target: name
  });
  download.startTime = new Date(now_mSec - (aMinutesAgo * kMsecPerMin));
  download.canceled = true;
  publicList.add(download);

  ok((await downloadExists(name)),
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
function promiseAddFormEntryWithMinutesAgo(aMinutesAgo) {
  let name = aMinutesAgo + "-minutes-ago";

  // Artifically age the entry to the proper vintage.
  let timestamp = now_uSec - (aMinutesAgo * kUsecPerMin);

  return new Promise((resolve, reject) =>
    FormHistory.update({ op: "add", fieldname: name, value: "dummy", firstUsed: timestamp },
                       { handleError(error) {
                           reject();
                           throw new Error("Error occurred updating form history: " + error);
                         },
                         handleCompletion(reason) {
                           resolve(name);
                         }
                       })
   );
}

/**
 * Checks if a form entry exists.
 */
function formNameExists(name) {
  return new Promise((resolve, reject) => {
    let count = 0;
    FormHistory.count({ fieldname: name },
                      { handleResult: result => count = result,
                        handleError(error) {
                          reject(error);
                          throw new Error("Error occurred searching form history: " + error);
                        },
                        handleCompletion(reason) {
                          if (!reason) {
                            resolve(count);
                          }
                        }
                      });
  });
}

/**
 * Removes all history visits, downloads, and form entries.
 */
async function blankSlate() {
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let downloads = await publicList.getAll();
  for (let download of downloads) {
    await publicList.remove(download);
    await download.finalize(true);
  }

  await new Promise((resolve, reject) => {
    FormHistory.update({op: "remove"}, {
      handleCompletion(reason) {
        if (!reason) {
          resolve();
        }
      },
      handleError(error) {
        reject(error);
        throw new Error("Error occurred updating form history: " + error);
      }
    });
  });

  await PlacesUtils.history.clear();
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
  is(Services.prefs.getBoolPref("privacy." + aPrefName), aExpectedVal, aMsg);
}

/**
 * Checks to see if the download with the specified path exists.
 *
 * @param  aPath
 *         The path of the download to check
 * @return True if the download exists, false otherwise
 */
async function downloadExists(aPath) {
  let publicList = await Downloads.getList(Downloads.PUBLIC);
  let listArray = await publicList.getAll();
  return listArray.some(i => i.target.path == aPath);
}

/**
 * Ensures that the specified downloads are either cleared or not.
 *
 * @param aDownloadIDs
 *        Array of download database IDs
 * @param aShouldBeCleared
 *        True if each download should be cleared, false otherwise
 */
async function ensureDownloadsClearedState(aDownloadIDs, aShouldBeCleared) {
  let niceStr = aShouldBeCleared ? "no longer" : "still";
  for (let id of aDownloadIDs) {
    is((await downloadExists(id)), !aShouldBeCleared,
       "download " + id + " should " + niceStr + " exist");
  }
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
  is(Services.prefs.getIntPref("privacy." + aPrefName), aExpectedVal, aMsg);
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
