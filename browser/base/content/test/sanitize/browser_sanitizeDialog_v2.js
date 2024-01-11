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

ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
  Timer: "resource://gre/modules/Timer.sys.mjs",
});

const kMsecPerMin = 60 * 1000;
const kUsecPerMin = 60 * 1000000;
let nowMsec = Date.now();
let nowUsec = nowMsec * 1000;

const EXAMPLE_ORIGIN = "https://www.example.com";
const siteUsage = 4096;
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
    Assert.equal(
      visited,
      !aShouldBeCleared,
      `history visit ${uri.spec} should ${
        aShouldBeCleared ? "no longer" : "still"
      } exist`
    );
  }
}

/**
 * Ensures that the given pref is the expected value.
 *
 * @param {String} aPrefName
 *        The pref's sub-branch under the privacy branch
 * @param {Boolean} aExpectedVal
 *        The pref's expected value
 * @param {String} aMsg
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
    is(
      await downloadExists(id),
      !aShouldBeCleared,
      "download " + id + " should " + niceStr + " exist"
    );
  }
}

/**
 * Checks if a form entry exists.
 */
async function formNameExists(name) {
  return !!(await FormHistory.count({ fieldname: name }));
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
  let timestamp = nowUsec - aMinutesAgo * kUsecPerMin;

  return FormHistory.update({
    op: "add",
    fieldname: name,
    value: "dummy",
    firstUsed: timestamp,
  });
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
    target: name,
  });
  download.startTime = new Date(nowMsec - aMinutesAgo * kMsecPerMin);
  download.canceled = true;
  publicList.add(download);

  ok(
    await downloadExists(name),
    "Sanity check: download " + name + " should exist after creating it"
  );

  aExpectedPathList.push(name);
}

add_setup(async function () {
  requestLongerTimeout(3);
  await blankSlate();
  registerCleanupFunction(async function () {
    await blankSlate();
    await PlacesTestUtils.promiseAsyncUpdates();
  });
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.sanitize.useOldClearHistoryDialog", false]],
  });
});

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

  await FormHistory.update({ op: "remove" });
  await PlacesUtils.history.clear();
}

/**
 * This wraps the dialog and provides some convenience methods for interacting
 * with it.
 *
 * @param browserWin (optional)
 *        The browser window that the dialog is expected to open in. If not
 *        supplied, the initial browser window of the test run is used.
 */
function DialogHelper(browserWin = window) {
  this._browserWin = browserWin;
  this.win = null;
  this.promiseClosed = new Promise(resolve => {
    this._resolveClosed = resolve;
  });
}

DialogHelper.prototype = {
  /**
   * "Presses" the dialog's OK button.
   */
  acceptDialog() {
    let dialogEl = this.win.document.querySelector("dialog");
    is(
      dialogEl.getButton("accept").disabled,
      false,
      "Dialog's OK button should not be disabled"
    );
    dialogEl.acceptDialog();
  },

  /**
   * "Presses" the dialog's Cancel button.
   */
  cancelDialog() {
    this.win.document.querySelector("dialog").cancelDialog();
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
      "checkbox[preference='" + pref + "']"
    );
    is(cb.length, 1, "found checkbox for " + pref + " preference");
    if (cb[0].checked != aCheckState) {
      cb[0].click();
    }
  },

  /**
   * Makes sure all the checkboxes are checked.
   */
  _checkAllCheckboxesCustom(check) {
    var cb = this.win.document.querySelectorAll("checkbox[preference]");
    ok(cb.length > 1, "found checkboxes for preferences");
    for (var i = 0; i < cb.length; ++i) {
      var pref = this.win.Preferences.get(cb[i].getAttribute("preference"));
      if (!!pref.value ^ check) {
        cb[i].click();
      }
    }
  },

  checkAllCheckboxes() {
    this._checkAllCheckboxesCustom(true);
  },

  uncheckAllCheckboxes() {
    this._checkAllCheckboxesCustom(false);
  },

  /**
   * @return The dialog's duration dropdown
   */
  getDurationDropdown() {
    return this.win.document.getElementById("sanitizeDurationChoice");
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
   * caller is expected to call promiseAsyncUpdates at some point; if false is
   * returned, promiseAsyncUpdates is called automatically.
   */
  async open() {
    let dialogPromise = BrowserTestUtils.promiseAlertDialogOpen(
      null,
      "chrome://browser/content/sanitize_v2.xhtml",
      {
        isSubDialog: true,
      }
    );

    executeSoon(() => {
      Sanitizer.showUI(this._browserWin);
    });

    this.win = await dialogPromise;
    this.win.addEventListener(
      "load",
      () => {
        // Run onload on next tick so that gSanitizePromptDialog.init can run first.
        executeSoon(() => this.onload());
      },
      { once: true }
    );

    this.win.addEventListener(
      "unload",
      () => {
        // Some exceptions that reach here don't reach the test harness, but
        // ok()/is() do...
        (async () => {
          if (this.onunload) {
            await this.onunload();
          }
          await PlacesTestUtils.promiseAsyncUpdates();
          this._resolveClosed();
          this.win = null;
        })();
      },
      { once: true }
    );
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
      is(
        this.isWarningPanelVisible(),
        true,
        "Warning panel should be visible for TIMESPAN_EVERYTHING"
      );
    } else {
      is(
        this.isWarningPanelVisible(),
        false,
        "Warning panel should not be visible for non-TIMESPAN_EVERYTHING"
      );
    }
  },
};

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
  return nowUsec - aMinutesAgo * kUsecPerMin;
}

function promiseSanitizationComplete() {
  return TestUtils.topicObserved("sanitizer-sanitization-complete");
}

/**
 * Initializes the dialog to its default state.
 */
add_task(async function default_state() {
  let dh = new DialogHelper();
  dh.onload = function () {
    // Select "Last Hour"
    this.selectDuration(Sanitizer.TIMESPAN_HOUR);
    this.acceptDialog();
  };
  dh.open();
  await dh.promiseClosed;
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
    pURI = makeURI("https://" + i + "-minutes-ago.com/");
    places.push({ uri: pURI, visitDate: visitTimeForMinutesAgo(i) });
    uris.push(pURI);
  }
  await PlacesTestUtils.addVisits(places);

  let dh = new DialogHelper();
  dh.onload = function () {
    this.selectDuration(Sanitizer.TIMESPAN_HOUR);
    this.checkPrefCheckbox("history", false);
    this.cancelDialog();
  };
  dh.onunload = async function () {
    await promiseHistoryClearedState(uris, false);
    await blankSlate();
    await promiseHistoryClearedState(uris, true);
  };
  dh.open();
  await dh.promiseClosed;
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
  [10, 70, 130, 250].forEach(function (aValue) {
    pURI = makeURI("https://" + aValue + "-minutes-ago.com/");
    places.push({ uri: pURI, visitDate: visitTimeForMinutesAgo(aValue) });
    uris.push(pURI);
  });

  let promiseSanitized = promiseSanitizationComplete();

  await PlacesTestUtils.addVisits(places);
  let dh = new DialogHelper();
  dh.onload = function () {
    is(
      this.isWarningPanelVisible(),
      false,
      "Warning panel should be hidden after previously accepting dialog " +
        "with a predefined timespan"
    );
    this.selectDuration(Sanitizer.TIMESPAN_EVERYTHING);
    this.checkPrefCheckbox("history", true);
    this.acceptDialog();
  };
  dh.onunload = async function () {
    await promiseSanitized;
    intPrefIs(
      "sanitize.timeSpan",
      Sanitizer.TIMESPAN_EVERYTHING,
      "timeSpan pref should be everything after accepting dialog " +
        "with everything selected"
    );

    await promiseHistoryClearedState(uris, true);
  };
  dh.open();
  await dh.promiseClosed;
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
  [10, 70, 130, 250].forEach(function (aValue) {
    pURI = makeURI("https://" + aValue + "-minutes-ago.com/");
    places.push({ uri: pURI, visitDate: visitTimeForMinutesAgo(aValue) });
    uris.push(pURI);
  });

  let promiseSanitized = promiseSanitizationComplete();

  await PlacesTestUtils.addVisits(places);
  let dh = new DialogHelper();
  dh.onload = function () {
    is(
      this.isWarningPanelVisible(),
      true,
      "Warning panel should be visible after previously accepting dialog " +
        "with clearing everything"
    );
    this.selectDuration(Sanitizer.TIMESPAN_EVERYTHING);
    this.checkPrefCheckbox("history", true);
    this.acceptDialog();
  };
  dh.onunload = async function () {
    intPrefIs(
      "sanitize.timeSpan",
      Sanitizer.TIMESPAN_EVERYTHING,
      "timeSpan pref should be everything after accepting dialog " +
        "with everything selected"
    );

    await promiseSanitized;

    await promiseHistoryClearedState(uris, true);
  };
  dh.open();
  await dh.promiseClosed;
});

/**
 * Checks if clearing history and downloads for the simple timespan
 * behaves as expected
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
    pURI = makeURI("https://" + i + "-minutes-ago.com/");
    places.push({ uri: pURI, visitDate: visitTimeForMinutesAgo(i) });
    uris.push(pURI);
  }
  // Add history (over an hour ago).
  let olderURIs = [];
  for (let i = 0; i < 5; i++) {
    pURI = makeURI("https://" + (61 + i) + "-minutes-ago.com/");
    places.push({ uri: pURI, visitDate: visitTimeForMinutesAgo(61 + i) });
    olderURIs.push(pURI);
  }
  let promiseSanitized = promiseSanitizationComplete();

  await PlacesTestUtils.addVisits(places);

  let dh = new DialogHelper();
  dh.onload = function () {
    this.selectDuration(Sanitizer.TIMESPAN_HOUR);
    this.checkPrefCheckbox("downloads", true);
    this.checkPrefCheckbox("history", true);
    this.acceptDialog();
  };
  dh.onunload = async function () {
    intPrefIs(
      "sanitize.timeSpan",
      Sanitizer.TIMESPAN_HOUR,
      "timeSpan pref should be hour after accepting dialog with " +
        "hour selected"
    );
    boolPrefIs(
      "cpd.history",
      true,
      "history pref should be true after accepting dialog with " +
        "history checkbox checked"
    );
    boolPrefIs(
      "cpd.downloads",
      true,
      "downloads pref should be true after accepting dialog with " +
        "history checkbox checked"
    );

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
  dh.open();
  await dh.promiseClosed;
});

/**
 * The next three tests checks that when a certain history item cannot be
 * cleared then the checkbox should be both disabled and unchecked.
 * In addition, we ensure that this behavior does not modify the preferences.
 */
add_task(async function test_cannot_clear_history() {
  // Add form entries
  let formEntries = [await promiseAddFormEntryWithMinutesAgo(10)];

  let promiseSanitized = promiseSanitizationComplete();

  // Add history.
  let pURI = makeURI("https://" + 10 + "-minutes-ago.com/");
  await PlacesTestUtils.addVisits({
    uri: pURI,
    visitDate: visitTimeForMinutesAgo(10),
  });
  let uris = [pURI];

  let dh = new DialogHelper();
  dh.onload = function () {
    var cb = this.win.document.querySelectorAll(
      "checkbox[preference='privacy.cpd.history']"
    );
    ok(
      cb.length == 1 && !cb[0].disabled,
      "There is history, checkbox to clear history should be enabled."
    );

    this.checkAllCheckboxes();
    this.acceptDialog();
  };
  dh.onunload = async function () {
    await promiseSanitized;

    await promiseHistoryClearedState(uris, true);

    let exists = await formNameExists(formEntries[0]);
    ok(!exists, "form entry " + formEntries[0] + " should no longer exist");
  };
  dh.open();
  await dh.promiseClosed;
});

add_task(async function test_no_formdata_history_to_clear() {
  let promiseSanitized = promiseSanitizationComplete();
  let dh = new DialogHelper();
  dh.onload = function () {
    boolPrefIs(
      "cpd.history",
      true,
      "history pref should be true after accepting dialog with " +
        "history checkbox checked"
    );

    var cb = this.win.document.querySelectorAll(
      "checkbox[preference='privacy.cpd.history']"
    );
    ok(
      cb.length == 1 && !cb[0].disabled && cb[0].checked,
      "There is no history, but history checkbox should always be enabled " +
        "and will be checked from previous preference."
    );

    this.acceptDialog();
  };
  dh.open();
  await dh.promiseClosed;
  await promiseSanitized;
});

add_task(async function test_form_entries() {
  let formEntry = await promiseAddFormEntryWithMinutesAgo(10);

  let promiseSanitized = promiseSanitizationComplete();

  let dh = new DialogHelper();
  dh.onload = function () {
    var cb = this.win.document.querySelectorAll(
      "checkbox[preference='privacy.cpd.history']"
    );
    is(cb.length, 1, "There is only one checkbox for history and form data");
    ok(!cb[0].disabled, "The checkbox is enabled");
    ok(cb[0].checked, "The checkbox is checked");

    this.acceptDialog();
  };
  dh.onunload = async function () {
    await promiseSanitized;
    let exists = await formNameExists(formEntry);
    ok(!exists, "form entry " + formEntry + " should no longer exist");
  };
  dh.open();
  await dh.promiseClosed;
});

add_task(async function test_history_formdata() {
  let promiseSanitized = promiseSanitizationComplete();

  let dh = new DialogHelper();
  dh.onload = function () {
    this.uncheckAllCheckboxes();
    this.checkPrefCheckbox("history", true);
    this.acceptDialog();
  };
  dh.onunload = async function () {
    await promiseSanitized;
    // Check if form data follows history
    boolPrefIs("cpd.formdata", true, `form data pref follows history pref`);

    boolPrefIs("cpd.sessions", false, `sessions pref follows cookies prefs`);

    boolPrefIs(
      "cpd.offlineApps",
      false,
      `offlineapps pref follows cookies prefs`
    );
  };
  dh.open();
  await dh.promiseClosed;
});

add_task(async function test_cookies_sessions_offlineApps() {
  let promiseSanitized = promiseSanitizationComplete();

  let dh = new DialogHelper();
  dh.onload = function () {
    this.uncheckAllCheckboxes();
    this.checkPrefCheckbox("cookies", true);
    this.acceptDialog();
  };
  dh.onunload = async function () {
    await promiseSanitized;
    // Check if form data follows history
    boolPrefIs("cpd.formdata", false, `form data pref follows history pref`);

    boolPrefIs("cpd.sessions", true, `sessions pref follows cookies prefs`);

    boolPrefIs(
      "cpd.offlineApps",
      true,
      `offlineapps pref follows cookies prefs`
    );
  };
  dh.open();
  await dh.promiseClosed;
});
