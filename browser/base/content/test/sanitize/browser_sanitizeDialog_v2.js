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
  PermissionTestUtils: "resource://testing-common/PermissionTestUtils.sys.mjs",
  FileTestUtils: "resource://testing-common/FileTestUtils.sys.mjs",
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
});

const kMsecPerMin = 60 * 1000;
const kUsecPerMin = 60 * 1000000;
let today = Date.now() - new Date().setHours(0, 0, 0, 0);
let nowMSec = Date.now();
let nowUSec = nowMSec * 1000;
let fileURL;

const TEST_TARGET_FILE_NAME = "test-download.txt";
const TEST_QUOTA_USAGE_HOST = "example.com";
const TEST_QUOTA_USAGE_ORIGIN = "https://" + TEST_QUOTA_USAGE_HOST;
const TEST_QUOTA_USAGE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    TEST_QUOTA_USAGE_ORIGIN
  ) + "site_data_test.html";

const siteOrigins = [
  "https://www.example.com",
  "https://example.org",
  "http://localhost:8000",
  "http://localhost:3000",
];

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
  let timestamp = nowUSec - aMinutesAgo * kUsecPerMin;

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
  download.startTime = new Date(nowMSec - aMinutesAgo * kMsecPerMin);
  download.canceled = true;
  publicList.add(download);

  ok(
    await downloadExists(name),
    "Sanity check: download " + name + " should exist after creating it"
  );

  aExpectedPathList.push(name);
}

/**
 * Adds multiple downloads to the PUBLIC download list
 */
async function addToDownloadList() {
  const url = createFileURL();
  const downloadsList = await Downloads.getList(Downloads.PUBLIC);
  let timeOptions = [1, 2, 4, 24, 128, 128];
  let buffer = 100000;

  for (let i = 0; i < timeOptions.length; i++) {
    let timeDownloaded = 60 * kMsecPerMin * timeOptions[i];
    if (timeOptions[i] === 24) {
      timeDownloaded = today;
    }

    let download = await Downloads.createDownload({
      source: { url: url.spec, isPrivate: false },
      target: { path: FileTestUtils.getTempFile(TEST_TARGET_FILE_NAME).path },
      startTime: {
        getTime: _ => {
          return nowMSec - timeDownloaded + buffer;
        },
      },
    });

    Assert.ok(!!download);
    downloadsList.add(download);
  }
  let items = await downloadsList.getAll();
  Assert.equal(items.length, 6, "Items were added to the list");
}

async function addToSiteUsage() {
  // Fill indexedDB with test data.
  // Don't wait for the page to load, to register the content event handler as quickly as possible.
  // If this test goes intermittent, we might have to tell the page to wait longer before
  // firing the event.
  BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_QUOTA_USAGE_URL, false);
  await BrowserTestUtils.waitForContentEvent(
    gBrowser.selectedBrowser,
    "test-indexedDB-done",
    false,
    null,
    true
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  let siteLastAccessed = [1, 2, 4, 24];

  let staticUsage = 4096 * 6;
  // Add a time buffer so the site access falls within the time range
  const buffer = 10000;

  // Change lastAccessed of sites
  for (let index = 0; index < siteLastAccessed.length; index++) {
    let lastAccessedTime = 60 * kMsecPerMin * siteLastAccessed[index];
    if (siteLastAccessed[index] === 24) {
      lastAccessedTime = today;
    }

    let site = SiteDataManager._testInsertSite(siteOrigins[index], {
      quotaUsage: staticUsage,
      lastAccessed: (nowMSec - lastAccessedTime + buffer) * 1000,
    });
    Assert.ok(site, "Site added successfully");
  }
}

/**
 * Helper function to create file URL to open
 *
 * @returns {Object} a file URL
 */
function createFileURL() {
  if (!fileURL) {
    let file = Services.dirsvc.get("TmpD", Ci.nsIFile);
    file.append("foo.txt");
    file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

    fileURL = Services.io.newFileURI(file);
  }
  return fileURL;
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

  // open preferences to trigger an updateSites()
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
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
 * @param mode (optional)
 *        Open in the clear on shutdown settings context or
 *        Open in the clear site data settings context
 *        null by default
 */
function DialogHelper(browserWin = window, mode = null) {
  this._browserWin = browserWin;
  this.win = null;
  this._mode = mode;
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
    var cb = this.win.document.querySelectorAll(
      "checkbox[id='" + aPrefName + "']"
    );
    is(cb.length, 1, "found checkbox for " + aPrefName + " id");
    if (cb[0].checked != aCheckState) {
      cb[0].click();
    }
  },

  /**
   * @param {String} aCheckboxId
   *        The checkbox id name
   * @param {Boolean} aCheckState
   *        True if the checkbox should be checked, false otherwise
   */
  validateCheckbox(aCheckboxId, aCheckState) {
    let cb = this.win.document.querySelectorAll(
      "checkbox[id='" + aCheckboxId + "']"
    );
    is(cb.length, 1, `found checkbox for id=${aCheckboxId}`);
    is(
      cb[0].checked,
      aCheckState,
      `checkbox for ${aCheckboxId} is ${aCheckState}`
    );
  },

  /**
   * Makes sure all the checkboxes are checked.
   */
  _checkAllCheckboxesCustom(check) {
    var cb = this.win.document.querySelectorAll("checkbox[id]");
    ok(cb.length, "found checkboxes for ids");
    for (var i = 0; i < cb.length; ++i) {
      if (cb[i].checked != check) {
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

  setMode(value) {
    this._mode = value;
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
      Sanitizer.showUI(this._browserWin, this._mode);
    });

    this.win = await dialogPromise;
    this.win.addEventListener(
      "load",
      () => {
        // Run onload on next tick so that gSanitizePromptDialog.init can run first.
        executeSoon(async () => {
          await this.win.gSanitizePromptDialog.dataSizesFinishedUpdatingPromise;
          this.onload();
        });
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
  return nowUSec - aMinutesAgo * kUsecPerMin;
}

function promiseSanitizationComplete() {
  return TestUtils.topicObserved("sanitizer-sanitization-complete");
}

/**
 * Helper function to validate the data sizes shown for each time selection
 *
 * @param {DialogHelper} dh - dialog object to access window and timespan
 */
async function validateDataSizes(dialogHelper) {
  let timespans = [
    "TIMESPAN_HOUR",
    "TIMESPAN_2HOURS",
    "TIMESPAN_4HOURS",
    "TIMESPAN_TODAY",
    "TIMESPAN_EVERYTHING",
  ];

  // get current data sizes from siteDataManager
  let cacheUsage = await SiteDataManager.getCacheSize();
  let quotaUsage = await SiteDataManager.getQuotaUsageForTimeRanges(timespans);

  for (let i = 0; i < timespans.length; i++) {
    // select timespan to check
    dialogHelper.selectDuration(Sanitizer[timespans[i]]);

    // get the elements
    let clearCookiesAndSiteDataCheckbox =
      dialogHelper.win.document.getElementById("cookiesAndStorage");
    let clearCacheCheckbox = dialogHelper.win.document.getElementById("cache");

    let [convertedQuotaUsage] = DownloadUtils.convertByteUnits(
      quotaUsage[timespans[i]]
    );
    let [, convertedCacheUnit] = DownloadUtils.convertByteUnits(cacheUsage);

    // Ensure l10n is finished before inspecting the category labels.
    await dialogHelper.win.document.l10n.translateElements([
      clearCookiesAndSiteDataCheckbox,
      clearCacheCheckbox,
    ]);
    ok(
      clearCacheCheckbox.label.includes(convertedCacheUnit),
      "Should show the cache usage"
    );
    ok(
      clearCookiesAndSiteDataCheckbox.label.includes(convertedQuotaUsage),
      `Should show the quota usage as ${convertedQuotaUsage}`
    );
  }
}

/**
 *
 * Opens dialog in the provided context and selects the checkboxes
 * as sent in the parameters
 *
 * @param {Object} context the dialog is opened in, timespan to select,
 *  if historyFormDataAndDownloads, cookiesAndStorage, cache or siteSettings
 *  are checked
 */
async function performActionsOnDialog({
  context = "browser",
  timespan = Sanitizer.TIMESPAN_HOUR,
  historyFormDataAndDownloads = true,
  cookiesAndStorage = true,
  cache = false,
  siteSettings = false,
}) {
  let dh = new DialogHelper();
  dh.setMode(context);
  dh.onload = function () {
    this.selectDuration(timespan);
    this.checkPrefCheckbox(
      "historyFormDataAndDownloads",
      historyFormDataAndDownloads
    );
    this.checkPrefCheckbox("cookiesAndStorage", cookiesAndStorage);
    this.checkPrefCheckbox("cache", cache);
    this.checkPrefCheckbox("siteSettings", siteSettings);
    this.acceptDialog();
  };
  dh.open();
  await dh.promiseClosed;
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
    this.checkPrefCheckbox("historyFormDataAndDownloads", false);
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
    this.checkPrefCheckbox("historyFormDataAndDownloads", true);
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
    this.checkPrefCheckbox("historyFormDataAndDownloads", true);
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
    this.checkPrefCheckbox("historyFormDataAndDownloads", true);
    this.acceptDialog();
  };
  dh.onunload = async function () {
    intPrefIs(
      "sanitize.timeSpan",
      Sanitizer.TIMESPAN_HOUR,
      "timeSpan pref should be hour after accepting dialog with " +
        "hour selected"
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
      "checkbox[id='historyFormDataAndDownloads']"
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
    var cb = this.win.document.querySelectorAll(
      "checkbox[id='historyFormDataAndDownloads']"
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
      "checkbox[id='historyFormDataAndDownloads']"
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

add_task(async function test_cookie_sizes() {
  await clearAndValidateDataSizes({
    clearCookies: true,
    clearCache: false,
    clearDownloads: false,
    timespan: Sanitizer.TIMESPAN_HOUR,
  });
  await clearAndValidateDataSizes({
    clearCookies: true,
    clearCache: false,
    clearDownloads: false,
    timespan: Sanitizer.TIMESPAN_4HOURS,
  });
  await clearAndValidateDataSizes({
    clearCookies: true,
    clearCache: false,
    clearDownloads: false,
    timespan: Sanitizer.TIMESPAN_EVERYTHING,
  });
});

add_task(async function test_cache_sizes() {
  await clearAndValidateDataSizes({
    clearCookies: false,
    clearCache: true,
    clearDownloads: false,
    timespan: Sanitizer.TIMESPAN_HOUR,
  });
  await clearAndValidateDataSizes({
    clearCookies: false,
    clearCache: true,
    clearDownloads: false,
    timespan: Sanitizer.TIMESPAN_4HOURS,
  });
  await clearAndValidateDataSizes({
    clearCookies: false,
    clearCache: true,
    clearDownloads: false,
    timespan: Sanitizer.TIMESPAN_EVERYTHING,
  });
});

add_task(async function test_downloads_sizes() {
  await clearAndValidateDataSizes({
    clearCookies: false,
    clearCache: false,
    clearDownloads: true,
    timespan: Sanitizer.TIMESPAN_HOUR,
  });
  await clearAndValidateDataSizes({
    clearCookies: false,
    clearCache: false,
    clearDownloads: true,
    timespan: Sanitizer.TIMESPAN_4HOURS,
  });
  await clearAndValidateDataSizes({
    clearCookies: false,
    clearCache: false,
    clearDownloads: true,
    timespan: Sanitizer.TIMESPAN_EVERYTHING,
  });
});

add_task(async function test_all_data_sizes() {
  await clearAndValidateDataSizes({
    clearCookies: true,
    clearCache: true,
    clearDownloads: true,
    timespan: Sanitizer.TIMESPAN_HOUR,
  });
  await clearAndValidateDataSizes({
    clearCookies: true,
    clearCache: true,
    clearDownloads: true,
    timespan: Sanitizer.TIMESPAN_4HOURS,
  });
  await clearAndValidateDataSizes({
    clearCookies: true,
    clearCache: true,
    clearDownloads: true,
    timespan: Sanitizer.TIMESPAN_EVERYTHING,
  });
});

// test the case when we open the dialog through the clear on shutdown settings
add_task(async function test_clear_on_shutdown() {
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  await SpecialPowers.pushPrefEnv({
    set: [["privacy.sanitize.sanitizeOnShutdown", true]],
  });

  let dh = new DialogHelper();
  dh.setMode("clearOnShutdown");
  dh.onload = async function () {
    this.uncheckAllCheckboxes();
    this.checkPrefCheckbox("historyFormDataAndDownloads", false);
    this.checkPrefCheckbox("cookiesAndStorage", true);
    this.acceptDialog();
  };
  dh.open();
  await dh.promiseClosed;

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

  boolPrefIs(
    "clearOnShutdown_v2.historyFormDataAndDownloads",
    false,
    "clearOnShutdown_v2 history should be false"
  );

  boolPrefIs(
    "clearOnShutdown_v2.cookiesAndStorage",
    true,
    "clearOnShutdown_v2 cookies should be true"
  );

  boolPrefIs(
    "clearOnShutdown_v2.cache",
    false,
    "clearOnShutdown_v2 cache should be false"
  );

  await createDummyDataForHost("example.org");
  await createDummyDataForHost("example.com");

  ok(
    await SiteDataTestUtils.hasIndexedDB("https://example.org"),
    "We have indexedDB data for example.org"
  );
  ok(
    await SiteDataTestUtils.hasIndexedDB("https://example.com"),
    "We have indexedDB data for example.com"
  );

  // Cleaning up
  await Sanitizer.runSanitizeOnShutdown();

  // Data for example.org should be cleared
  ok(
    !(await SiteDataTestUtils.hasIndexedDB("https://example.org")),
    "We don't have indexedDB data for example.org"
  );
  // Data for example.com should be cleared
  ok(
    !(await SiteDataTestUtils.hasIndexedDB("https://example.com")),
    "We don't have indexedDB data for example.com"
  );

  // Downloads shouldn't have cleared
  await ensureDownloadsClearedState(downloadIDs, false);
  await ensureDownloadsClearedState(olderDownloadIDs, false);

  dh = new DialogHelper();
  dh.setMode("clearOnShutdown");
  dh.onload = async function () {
    this.uncheckAllCheckboxes();
    this.checkPrefCheckbox("historyFormDataAndDownloads", true);
    this.acceptDialog();
  };
  dh.open();
  await dh.promiseClosed;

  boolPrefIs(
    "clearOnShutdown_v2.historyFormDataAndDownloads",
    true,
    "clearOnShutdown_v2 history should be true"
  );

  boolPrefIs(
    "clearOnShutdown_v2.cookiesAndStorage",
    false,
    "clearOnShutdown_v2 cookies should be false"
  );

  boolPrefIs(
    "clearOnShutdown_v2.cache",
    false,
    "clearOnShutdown_v2 cache should be false"
  );

  ok(
    await SiteDataTestUtils.hasIndexedDB("https://example.org"),
    "We have indexedDB data for example.org"
  );
  ok(
    await SiteDataTestUtils.hasIndexedDB("https://example.com"),
    "We have indexedDB data for example.com"
  );

  // Cleaning up
  await Sanitizer.runSanitizeOnShutdown();

  // Data for example.org should not be cleared
  ok(
    await SiteDataTestUtils.hasIndexedDB("https://example.org"),
    "We have indexedDB data for example.org"
  );
  // Data for example.com should not be cleared
  ok(
    await SiteDataTestUtils.hasIndexedDB("https://example.com"),
    "We have indexedDB data for example.com"
  );

  // downloads should have cleared
  await ensureDownloadsClearedState(downloadIDs, true);
  await ensureDownloadsClearedState(olderDownloadIDs, true);

  // Clean up
  await SiteDataTestUtils.clear();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// test default prefs for entry points
add_task(async function test_defaults_prefs() {
  let dh = new DialogHelper();
  dh.setMode("clearSiteData");

  dh.onload = function () {
    this.validateCheckbox("historyFormDataAndDownloads", false);
    this.validateCheckbox("cache", true);
    this.validateCheckbox("cookiesAndStorage", true);
    this.validateCheckbox("siteSettings", false);

    this.cancelDialog();
  };
  dh.open();
  await dh.promiseClosed;

  // We don't need to specify the mode again,
  // as the default mode is taken (browser, clear history)

  dh = new DialogHelper();
  dh.onload = function () {
    // Default checked for browser and clear history mode
    this.validateCheckbox("historyFormDataAndDownloads", true);
    this.validateCheckbox("cache", true);
    this.validateCheckbox("cookiesAndStorage", true);
    this.validateCheckbox("siteSettings", false);

    this.cancelDialog();
  };
  dh.open();
  await dh.promiseClosed;
});

/**
 * Helper function to simulate switching timespan selections and
 * validate data sizes before and after clearing
 *
 * @param {Object}
 *    clearCookies - boolean
 *    clearDownloads - boolean
 *    clearCaches - boolean
 *    timespan - one of Sanitizer.TIMESPAN_*
 */
async function clearAndValidateDataSizes({
  clearCache,
  clearDownloads,
  clearCookies,
  timespan,
}) {
  await blankSlate();

  await addToDownloadList();
  await addToSiteUsage();
  let promiseSanitized = promiseSanitizationComplete();

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let dh = new DialogHelper();
  dh.onload = async function () {
    await validateDataSizes(this);
    this.checkPrefCheckbox("cache", clearCache);
    this.checkPrefCheckbox("cookiesAndStorage", clearCookies);
    this.checkPrefCheckbox("historyFormDataAndDownloads", clearDownloads);
    this.selectDuration(timespan);
    this.acceptDialog();
  };
  dh.onunload = async function () {
    await promiseSanitized;
  };
  dh.open();
  await dh.promiseClosed;

  let dh2 = new DialogHelper();
  // Check if the newly cleared values are reflected
  dh2.onload = async function () {
    await validateDataSizes(this);
    this.acceptDialog();
  };
  dh2.open();
  await dh2.promiseClosed;

  await SiteDataTestUtils.clear();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

add_task(async function testEntryPointTelemetry() {
  Services.fog.testResetFOG();

  // Telemetry count we expect for each context
  const EXPECTED_CONTEXT_COUNTS = {
    browser: 3,
    history: 2,
    clearData: 1,
  };

  for (let key in EXPECTED_CONTEXT_COUNTS) {
    Services.fog.testResetFOG();
    for (let i = 0; i < EXPECTED_CONTEXT_COUNTS[key]; i++) {
      await performActionsOnDialog({ context: key });
    }
    is(
      Glean.privacySanitize.dialogOpen.testGetValue().length,
      EXPECTED_CONTEXT_COUNTS[key],
      `There should be ${EXPECTED_CONTEXT_COUNTS[key]} opens from ${key} context`
    );
  }
});

add_task(async function testTimespanTelemetry() {
  Services.fog.testResetFOG();

  // Expected timespan selections from telemetry
  const EXPECTED_TIMESPANS = [
    Sanitizer.TIMESPAN_HOUR,
    Sanitizer.TIMESPAN_2HOURS,
    Sanitizer.TIMESPAN_4HOURS,
    Sanitizer.TIMESPAN_EVERYTHING,
  ];

  for (let timespan of EXPECTED_TIMESPANS) {
    await performActionsOnDialog({ timespan });
  }

  for (let index in EXPECTED_TIMESPANS) {
    is(
      Glean.privacySanitize.clearingTimeSpanSelected.testGetValue()[index].extra
        .time_span,
      EXPECTED_TIMESPANS[index].toString(),
      `Selected timespan should be ${EXPECTED_TIMESPANS[index]}`
    );
  }
});

add_task(async function testLoadtimeTelemetry() {
  Services.fog.testResetFOG();

  // loadtime metric is collected everytime that the dialog is opened
  // expected number of times dialog will be opened for the test for each context
  let EXPECTED_CONTEXT_COUNTS = {
    browser: 2,
    history: 3,
    clearData: 2,
  };

  // open dialog based on expected_context_counts
  for (let context in EXPECTED_CONTEXT_COUNTS) {
    for (let i = 0; i < EXPECTED_CONTEXT_COUNTS[context]; i++) {
      await performActionsOnDialog({ context });
    }
  }

  let loadTimeDistribution = Glean.privacySanitize.loadTime.testGetValue();

  let expectedNumberOfCounts = Object.entries(EXPECTED_CONTEXT_COUNTS).reduce(
    (acc, [key, value]) => acc + value,
    0
  );
  // No guarantees from timers means no guarantees on buckets.
  // But we can guarantee it's only two samples.
  is(
    Object.entries(loadTimeDistribution.values).reduce(
      (acc, [bucket, count]) => acc + count,
      0
    ),
    expectedNumberOfCounts,
    `Only ${expectedNumberOfCounts} buckets with samples`
  );
});

add_task(async function testClearingOptionsTelemetry() {
  Services.fog.testResetFOG();

  let expectedObject = {
    context: "clearSiteData",
    history_form_data_downloads: "true",
    cookies_and_storage: "false",
    cache: "true",
    site_settings: "true",
  };

  await performActionsOnDialog({
    context: "clearSiteData",
    historyFormDataAndDownloads: true,
    cookiesAndStorage: false,
    cache: true,
    siteSettings: true,
  });

  let telemetryObject = Glean.privacySanitize.clear.testGetValue();
  Assert.equal(
    telemetryObject.length,
    1,
    "There should be only 1 telemetry object recorded"
  );

  Assert.deepEqual(
    expectedObject,
    telemetryObject[0].extra,
    `Expected ${telemetryObject} to be the same as ${expectedObject}`
  );
});
