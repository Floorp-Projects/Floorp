ChromeUtils.defineESModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  PermissionTestUtils: "resource://testing-common/PermissionTestUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
  FileTestUtils: "resource://testing-common/FileTestUtils.sys.mjs",
  Sanitizer: "resource:///modules/Sanitizer.sys.mjs",
  SiteDataTestUtils: "resource://testing-common/SiteDataTestUtils.sys.mjs",
});

const kMsecPerMin = 60 * 1000;
const kUsecPerMin = kMsecPerMin * 1000;
let today = Date.now() - new Date().setHours(0, 0, 0, 0);
let nowMSec = Date.now();
let nowUSec = nowMSec * 1000;
const TEST_TARGET_FILE_NAME = "test-download.txt";
const TEST_QUOTA_USAGE_HOST = "example.com";
const TEST_QUOTA_USAGE_ORIGIN = "https://" + TEST_QUOTA_USAGE_HOST;
const TEST_QUOTA_USAGE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    TEST_QUOTA_USAGE_ORIGIN
  ) + "site_data_test.html";
const SITE_ORIGINS = [
  "https://www.example.com",
  "https://example.org",
  "http://localhost:8000",
  "http://localhost:3000",
];

let fileURL;

function createIndexedDB(host, originAttributes) {
  let uri = Services.io.newURI("https://" + host);
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    originAttributes
  );
  return SiteDataTestUtils.addToIndexedDB(principal.origin);
}

function checkIndexedDB(host, originAttributes) {
  return new Promise(resolve => {
    let data = true;
    let uri = Services.io.newURI("https://" + host);
    let principal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      originAttributes
    );
    let request = indexedDB.openForPrincipal(principal, "TestDatabase", 1);
    request.onupgradeneeded = function () {
      data = false;
    };
    request.onsuccess = function () {
      resolve(data);
    };
  });
}

function createHostCookie(host, originAttributes) {
  Services.cookies.add(
    host,
    "/test",
    "foo",
    "bar",
    false,
    false,
    false,
    Date.now() + 24000 * 60 * 60,
    originAttributes,
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTPS
  );
}

function createDomainCookie(host, originAttributes) {
  Services.cookies.add(
    "." + host,
    "/test",
    "foo",
    "bar",
    false,
    false,
    false,
    Date.now() + 24000 * 60 * 60,
    originAttributes,
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTPS
  );
}

function checkCookie(host, originAttributes) {
  for (let cookie of Services.cookies.cookies) {
    if (
      ChromeUtils.isOriginAttributesEqual(
        originAttributes,
        cookie.originAttributes
      ) &&
      cookie.host.includes(host)
    ) {
      return true;
    }
  }
  return false;
}

async function deleteOnShutdown(opt) {
  // Let's clean up all the data.
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.sanitize.sanitizeOnShutdown", opt.sanitize],
      ["privacy.clearOnShutdown.cookies", opt.sanitize],
      ["privacy.clearOnShutdown.offlineApps", opt.sanitize],
      ["browser.sanitizer.loglevel", "All"],
    ],
  });

  // Custom permission without considering OriginAttributes
  if (opt.cookiePermission !== undefined) {
    let uri = Services.io.newURI("https://www.example.com");
    PermissionTestUtils.add(uri, "cookie", opt.cookiePermission);
  }

  // Let's create a tab with some data.
  await opt.createData(
    (opt.fullHost ? "www." : "") + "example.org",
    opt.originAttributes
  );
  ok(
    await opt.checkData(
      (opt.fullHost ? "www." : "") + "example.org",
      opt.originAttributes
    ),
    "We have data for www.example.org"
  );
  await opt.createData(
    (opt.fullHost ? "www." : "") + "example.com",
    opt.originAttributes
  );
  ok(
    await opt.checkData(
      (opt.fullHost ? "www." : "") + "example.com",
      opt.originAttributes
    ),
    "We have data for www.example.com"
  );

  // Cleaning up.
  await Sanitizer.runSanitizeOnShutdown();

  // All gone!
  is(
    !!(await opt.checkData(
      (opt.fullHost ? "www." : "") + "example.org",
      opt.originAttributes
    )),
    opt.expectedForOrg,
    "Do we have data for www.example.org?"
  );
  is(
    !!(await opt.checkData(
      (opt.fullHost ? "www." : "") + "example.com",
      opt.originAttributes
    )),
    opt.expectedForCom,
    "Do we have data for www.example.com?"
  );

  // Clean up.
  await Sanitizer.sanitize(["cookies", "offlineApps"]);

  if (opt.cookiePermission !== undefined) {
    let uri = Services.io.newURI("https://www.example.com");
    PermissionTestUtils.remove(uri, "cookie");
  }
}

function runAllCookiePermissionTests(originAttributes) {
  let tests = [
    { name: "IDB", createData: createIndexedDB, checkData: checkIndexedDB },
    {
      name: "Host Cookie",
      createData: createHostCookie,
      checkData: checkCookie,
    },
    {
      name: "Domain Cookie",
      createData: createDomainCookie,
      checkData: checkCookie,
    },
  ];

  // Delete all, no custom permission, data in example.com, cookie permission set
  // for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageOnShutdown() {
      info(
        methods.name +
          ": Delete all, no custom permission, data in example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        sanitize: true,
        createData: methods.createData,
        checkData: methods.checkData,
        originAttributes: originAttributes.oa,
        cookiePermission: undefined,
        expectedForOrg: false,
        expectedForCom: false,
        fullHost: false,
      });
    });
  });

  // Delete all, no custom permission, data in www.example.com, cookie permission
  // set for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageOnShutdown() {
      info(
        methods.name +
          ": Delete all, no custom permission, data in www.example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        sanitize: true,
        createData: methods.createData,
        checkData: methods.checkData,
        originAttributes: originAttributes.oa,
        cookiePermission: undefined,
        expectedForOrg: false,
        expectedForCom: false,
        fullHost: true,
      });
    });
  });

  // All is session, but with ALLOW custom permission, data in example.com,
  // cookie permission set for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageWithCustomPermission() {
      info(
        methods.name +
          ": All is session, but with ALLOW custom permission, data in example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        sanitize: true,
        createData: methods.createData,
        checkData: methods.checkData,
        originAttributes: originAttributes.oa,
        cookiePermission: Ci.nsICookiePermission.ACCESS_ALLOW,
        expectedForOrg: false,
        expectedForCom: true,
        fullHost: false,
      });
    });
  });

  // All is session, but with ALLOW custom permission, data in www.example.com,
  // cookie permission set for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageWithCustomPermission() {
      info(
        methods.name +
          ": All is session, but with ALLOW custom permission, data in www.example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        sanitize: true,
        createData: methods.createData,
        checkData: methods.checkData,
        originAttributes: originAttributes.oa,
        cookiePermission: Ci.nsICookiePermission.ACCESS_ALLOW,
        expectedForOrg: false,
        expectedForCom: true,
        fullHost: true,
      });
    });
  });

  // All is default, but with SESSION custom permission, data in example.com,
  // cookie permission set for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageOnlyCustomPermission() {
      info(
        methods.name +
          ": All is default, but with SESSION custom permission, data in example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        sanitize: false,
        createData: methods.createData,
        checkData: methods.checkData,
        originAttributes: originAttributes.oa,
        cookiePermission: Ci.nsICookiePermission.ACCESS_SESSION,
        expectedForOrg: true,
        // expected data just for example.com when using indexedDB because
        // QuotaManager deletes for principal.
        expectedForCom: false,
        fullHost: false,
      });
    });
  });

  // All is default, but with SESSION custom permission, data in www.example.com,
  // cookie permission set for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageOnlyCustomPermission() {
      info(
        methods.name +
          ": All is default, but with SESSION custom permission, data in www.example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        sanitize: false,
        createData: methods.createData,
        checkData: methods.checkData,
        originAttributes: originAttributes.oa,
        cookiePermission: Ci.nsICookiePermission.ACCESS_SESSION,
        expectedForOrg: true,
        expectedForCom: false,
        fullHost: true,
      });
    });
  });

  // Session mode, but with unsupported custom permission, data in
  // www.example.com, cookie permission set for www.example.com
  tests.forEach(methods => {
    add_task(async function deleteStorageOnlyCustomPermission() {
      info(
        methods.name +
          ": All is session only, but with unsupported custom custom permission, data in www.example.com, cookie permission set for www.example.com - OA: " +
          originAttributes.name
      );
      await deleteOnShutdown({
        sanitize: true,
        createData: methods.createData,
        checkData: methods.checkData,
        originAttributes: originAttributes.oa,
        cookiePermission: 123, // invalid cookie permission
        expectedForOrg: false,
        expectedForCom: false,
        fullHost: true,
      });
    });
  });
}

function openPreferencesViaOpenPreferencesAPI(aPane, aOptions) {
  return new Promise(resolve => {
    let finalPrefPaneLoaded = TestUtils.topicObserved(
      "sync-pane-loaded",
      () => true
    );
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
    openPreferences(aPane);
    let newTabBrowser = gBrowser.selectedBrowser;

    newTabBrowser.addEventListener(
      "Initialized",
      function () {
        newTabBrowser.contentWindow.addEventListener(
          "load",
          async function () {
            let win = gBrowser.contentWindow;
            let selectedPane = win.history.state;
            await finalPrefPaneLoaded;
            if (!aOptions || !aOptions.leaveOpen) {
              gBrowser.removeCurrentTab();
            }
            resolve({ selectedPane });
          },
          { once: true }
        );
      },
      { capture: true, once: true }
    );
  });
}

async function createDummyDataForHost(host) {
  let origin = "https://" + host;
  let dummySWURL =
    getRootDirectory(gTestPath).replace("chrome://mochitests/content", origin) +
    "dummy.js";

  await SiteDataTestUtils.addToIndexedDB(origin);
  await SiteDataTestUtils.addServiceWorker(dummySWURL);
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

    let site = SiteDataManager._testInsertSite(SITE_ORIGINS[index], {
      quotaUsage: staticUsage,
      lastAccessed: (nowMSec - lastAccessedTime + buffer) * 1000,
    });
    Assert.ok(site, "Site added successfully");
  }
}

function promiseSanitizationComplete() {
  return TestUtils.topicObserved("sanitizer-sanitization-complete");
}

/**
 * This wraps the dialog and provides some convenience methods for interacting
 * with it.
 *
 * @param {Window} browserWin (optional)
 *        The browser window that the dialog is expected to open in. If not
 *        supplied, the initial browser window of the test run is used.
 * @param {Object} {mode, checkingDataSizes}
 *        mode: context to open the dialog in
 *          One of
 *            clear on shutdown settings context ("clearOnShutdown"),
 *            clear site data settings context ("clearSiteData"),
 *            clear history context ("clearHistory"),
 *            browser context ("browser")
 *          "browser" by default
 *        checkingDataSizes: boolean check if we should wait for the data sizes
 *          to load
 *
 */
function ClearHistoryDialogHelper({
  mode = "browser",
  checkingDataSizes = false,
} = {}) {
  this._browserWin = window;
  this.win = null;
  this._mode = mode;
  this._checkingDataSizes = checkingDataSizes;
  this.promiseClosed = new Promise(resolve => {
    this._resolveClosed = resolve;
  });
}

ClearHistoryDialogHelper.prototype = {
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
    var cb = this.win.document.querySelectorAll(".clearingItemCheckbox");
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

    // We want to simulate opening the dialog inside preferences for clear history
    // and clear site data
    if (this._mode != "browser") {
      await openPreferencesViaOpenPreferencesAPI("privacy", {
        leaveOpen: true,
      });
      let tabWindow = gBrowser.selectedBrowser.contentWindow;
      let clearDialogOpenButtonId = this._mode + "Button";
      // the id for clear on shutdown is of a different format
      if (this._mode == "clearOnShutdown") {
        // set always clear to true to enable the clear on shutdown dialog
        let enableSettingsCheckbox =
          tabWindow.document.getElementById("alwaysClear");
        if (!enableSettingsCheckbox.checked) {
          enableSettingsCheckbox.click();
        }
        clearDialogOpenButtonId = "clearDataSettings";
      }
      // open dialog
      tabWindow.document.getElementById(clearDialogOpenButtonId).click();
    }
    // We open the dialog in the chrome context in other cases
    else {
      executeSoon(() => {
        Sanitizer.showUI(this._browserWin, this._mode);
      });
    }

    this.win = await dialogPromise;
    this.win.addEventListener(
      "load",
      () => {
        // Run onload on next tick so that gSanitizePromptDialog.init can run first.
        executeSoon(async () => {
          if (this._checkingDataSizes) {
            // we wait for the data sizes to load to avoid async errors when validating sizes
            await this.win.gSanitizePromptDialog
              .dataSizesFinishedUpdatingPromise;
          }
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
          if (this._mode != "browser") {
            BrowserTestUtils.removeTab(gBrowser.selectedTab);
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
