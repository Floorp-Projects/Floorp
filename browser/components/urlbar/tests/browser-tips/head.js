/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This directory contains tests that check tips and interventions, and in
// particular the update-related interventions.
// We mock updates by using the test helpers in
// toolkit/mozapps/update/tests/browser.

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/mozapps/update/tests/browser/head.js",
  this
);

ChromeUtils.defineESModuleGetters(this, {
  HttpServer: "resource://testing-common/httpd.sys.mjs",
  ResetProfile: "resource://gre/modules/ResetProfile.sys.mjs",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
  UrlbarProviderInterventions:
    "resource:///modules/UrlbarProviderInterventions.sys.mjs",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
});

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

ChromeUtils.defineLazyGetter(this, "SearchTestUtils", () => {
  const { SearchTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/SearchTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

// For each intervention type, a search string that trigger the intervention.
const SEARCH_STRINGS = {
  CLEAR: "firefox history",
  REFRESH: "firefox slow",
  UPDATE: "firefox update",
};

registerCleanupFunction(() => {
  // We need to reset the provider's appUpdater.status between tests so that
  // each test doesn't interfere with the next.
  UrlbarProviderInterventions.resetAppUpdater();
});

/**
 * Override our binary path so that the update lock doesn't think more than one
 * instance of this test is running.
 * This is a heavily pared down copy of the function in xpcshellUtilsAUS.js.
 */
function adjustGeneralPaths() {
  let dirProvider = {
    getFile(aProp, aPersistent) {
      // Set the value of persistent to false so when this directory provider is
      // unregistered it will revert back to the original provider.
      aPersistent.value = false;
      // The sync manager only uses XRE_EXECUTABLE_FILE, so that's all we need
      // to override, we won't bother handling anything else.
      if (aProp == XRE_EXECUTABLE_FILE) {
        // The temp directory that the mochitest runner creates is unique per
        // test, so its path can serve to provide the unique key that the update
        // sync manager requires (it doesn't need for this to be the actual
        // path to any real file, it's only used as an opaque string).
        let tempPath = Services.env.get("MOZ_PROCESS_LOG");
        let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        file.initWithPath(tempPath);
        return file;
      }
      return null;
    },
    QueryInterface: ChromeUtils.generateQI(["nsIDirectoryServiceProvider"]),
  };

  let ds = Services.dirsvc.QueryInterface(Ci.nsIDirectoryService);
  try {
    ds.QueryInterface(Ci.nsIProperties).undefine(XRE_EXECUTABLE_FILE);
  } catch (_ex) {
    // We only override one property, so we have nothing to do if that fails.
    return;
  }
  ds.registerProvider(dirProvider);
  registerCleanupFunction(() => {
    ds.unregisterProvider(dirProvider);
    // Reset the update lock once again so that we know the lock we're
    // interested in here will be closed properly (normally that happens during
    // XPCOM shutdown, but that isn't consistent during tests).
    let syncManager = Cc[
      "@mozilla.org/updates/update-sync-manager;1"
    ].getService(Ci.nsIUpdateSyncManager);
    syncManager.resetLock();
  });

  // Now that we've overridden the directory provider, the name of the update
  // lock needs to be changed to match the overridden path.
  let syncManager = Cc["@mozilla.org/updates/update-sync-manager;1"].getService(
    Ci.nsIUpdateSyncManager
  );
  syncManager.resetLock();
}

/**
 * Initializes a mock app update.  Adapted from runAboutDialogUpdateTest:
 * https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/head.js
 *
 * @param {object} params
 *   See the files in toolkit/mozapps/update/tests/browser.
 */
async function initUpdate(params) {
  Services.env.set("MOZ_TEST_SLOW_SKIP_UPDATE_STAGE", "1");
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_DISABLEDFORTESTING, false],
      [PREF_APP_UPDATE_URL_MANUAL, gDetailsURL],
    ],
  });

  adjustGeneralPaths();
  await setupTestUpdater();

  let queryString = params.queryString ? params.queryString : "";
  let updateURL =
    URL_HTTP_UPDATE_SJS +
    "?detailsURL=" +
    gDetailsURL +
    queryString +
    getVersionParams();
  if (params.backgroundUpdate) {
    setUpdateURL(updateURL);
    gAUS.checkForBackgroundUpdates();
    if (params.continueFile) {
      await continueFileHandler(params.continueFile);
    }
    if (params.waitForUpdateState) {
      let whichUpdate =
        params.waitForUpdateState == STATE_DOWNLOADING
          ? "downloadingUpdate"
          : "readyUpdate";
      await TestUtils.waitForCondition(
        () =>
          gUpdateManager[whichUpdate] &&
          gUpdateManager[whichUpdate].state == params.waitForUpdateState,
        "Waiting for update state: " + params.waitForUpdateState,
        undefined,
        200
      ).catch(e => {
        // Instead of throwing let the check below fail the test so the panel
        // ID and the expected panel ID is printed in the log.
        logTestInfo(e);
      });
      // Display the UI after the update state equals the expected value.
      Assert.equal(
        gUpdateManager[whichUpdate].state,
        params.waitForUpdateState,
        "The update state value should equal " + params.waitForUpdateState
      );
    }
  } else {
    updateURL += "&slowUpdateCheck=1&useSlowDownloadMar=1";
    setUpdateURL(updateURL);
  }
}

/**
 * Performs steps in a mock update.  Adapted from runAboutDialogUpdateTest:
 * https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/head.js
 *
 * @param {Array} steps
 *   See the files in toolkit/mozapps/update/tests/browser.
 */
async function processUpdateSteps(steps) {
  for (let step of steps) {
    await processUpdateStep(step);
  }
}

/**
 * Performs a step in a mock update.  Adapted from runAboutDialogUpdateTest:
 * https://searchfox.org/mozilla-central/source/toolkit/mozapps/update/tests/browser/head.js
 *
 * @param {object} step
 *   See the files in toolkit/mozapps/update/tests/browser.
 */
async function processUpdateStep(step) {
  if (typeof step == "function") {
    step();
    return;
  }

  const { panelId, checkActiveUpdate, continueFile, downloadInfo } = step;

  if (
    panelId == "downloading" &&
    gAUS.currentState == Ci.nsIApplicationUpdateService.STATE_IDLE
  ) {
    // Now that `AUS.downloadUpdate` is async, we start showing the
    // downloading panel while `AUS.downloadUpdate` is still resolving.
    // But the below checks assume that this resolution has already
    // happened. So we need to wait for things to actually resolve.
    await gAUS.stateTransition;
  }

  if (checkActiveUpdate) {
    let whichUpdate =
      checkActiveUpdate.state == STATE_DOWNLOADING
        ? "downloadingUpdate"
        : "readyUpdate";
    await TestUtils.waitForCondition(
      () => gUpdateManager[whichUpdate],
      "Waiting for active update"
    );
    Assert.ok(
      !!gUpdateManager[whichUpdate],
      "There should be an active update"
    );
    Assert.equal(
      gUpdateManager[whichUpdate].state,
      checkActiveUpdate.state,
      "The active update state should equal " + checkActiveUpdate.state
    );
  } else {
    Assert.ok(
      !gUpdateManager.readyUpdate,
      "There should not be a ready update"
    );
    Assert.ok(
      !gUpdateManager.downloadingUpdate,
      "There should not be a downloadingUpdate update"
    );
  }

  if (panelId == "downloading") {
    for (let i = 0; i < downloadInfo.length; ++i) {
      let data = downloadInfo[i];
      // The About Dialog tests always specify a continue file.
      await continueFileHandler(continueFile);
      let patch = getPatchOfType(
        data.patchType,
        gUpdateManager.downloadingUpdate
      );
      // The update is removed early when the last download fails so check
      // that there is a patch before proceeding.
      let isLastPatch = i == downloadInfo.length - 1;
      if (!isLastPatch || patch) {
        let resultName = data.bitsResult ? "bitsResult" : "internalResult";
        patch.QueryInterface(Ci.nsIWritablePropertyBag);
        await TestUtils.waitForCondition(
          () => patch.getProperty(resultName) == data[resultName],
          "Waiting for expected patch property " +
            resultName +
            " value: " +
            data[resultName],
          undefined,
          200
        ).catch(e => {
          // Instead of throwing let the check below fail the test so the
          // property value and the expected property value is printed in
          // the log.
          logTestInfo(e);
        });
        Assert.equal(
          patch.getProperty(resultName),
          data[resultName],
          "The patch property " +
            resultName +
            " value should equal " +
            data[resultName]
        );
      }
    }
  } else if (continueFile) {
    await continueFileHandler(continueFile);
  }
}

/**
 * Checks an intervention tip.  This works by starting a search that should
 * trigger a tip, picks the tip, and waits for the tip's action to happen.
 *
 * @param {object} options
 *   Options for the test
 * @param {string} options.searchString
 *   The search string.
 * @param {string} options.tip
 *   The expected tip type.
 * @param {string | RegExp} options.title
 *   The expected tip title.
 * @param {string | RegExp} options.button
 *   The expected button title.
 * @param {Function} options.awaitCallback
 *   A function that checks the tip's action.  Should return a promise (or be
 *   async).
 * @returns {object}
 *   The value returned from `awaitCallback`.
 */
async function doUpdateTest({
  searchString,
  tip,
  title,
  button,
  awaitCallback,
} = {}) {
  // Do a search that triggers the tip.
  let [result, element] = await awaitTip(searchString);
  Assert.strictEqual(result.payload.type, tip, "Tip type");
  await element.ownerDocument.l10n.translateFragment(element);

  let actualTitle = element._elements.get("title").textContent;
  if (typeof title == "string") {
    Assert.equal(actualTitle, title, "Title string");
  } else {
    // regexp
    Assert.ok(title.test(actualTitle), "Title regexp");
  }

  let actualButton = element._buttons.get("0").textContent;
  if (typeof button == "string") {
    Assert.equal(actualButton, button, "Button string");
  } else {
    // regexp
    Assert.ok(button.test(actualButton), "Button regexp");
  }

  Assert.ok(element._buttons.has("menu"), "Tip has a menu button");

  // Pick the tip and wait for the action.
  let values = await Promise.all([awaitCallback(), pickTip()]);

  // Check telemetry.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${tip}-shown`,
    1
  );
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${tip}-picked`,
    1
  );

  return values[0] || null;
}

/**
 * Starts a search and asserts that the second result is a tip.
 *
 * @param {string} searchString
 *   The search string.
 * @param {window} win
 *   The window.
 * @returns {(result| element)[]}
 *   The result and its element in the DOM.
 */
async function awaitTip(searchString, win = window) {
  let context = await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: searchString,
    waitForFocus,
    fireInputEvent: true,
  });
  Assert.ok(
    context.results.length >= 2,
    "Number of results is greater than or equal to 2"
  );
  let result = context.results[1];
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.TIP, "Result type");
  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(win, 1);
  return [result, element];
}

/**
 * Picks the current tip's button.  The view should be open and the second
 * result should be a tip.
 */
async function pickTip() {
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  let button = result.element.row._buttons.get("0");
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeMouseAtCenter(button, {});
  });
}

/**
 * Waits for the quit-application-requested notification and cancels it (so that
 * the app isn't actually restarted).
 */
async function awaitAppRestartRequest() {
  await TestUtils.topicObserved(
    "quit-application-requested",
    (cancelQuit, data) => {
      if (data == "restart") {
        cancelQuit.QueryInterface(Ci.nsISupportsPRBool).data = true;
        return true;
      }
      return false;
    }
  );
}

/**
 * Sets up the profile so that it can be reset.
 */
function makeProfileResettable() {
  // Make reset possible.
  let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
    Ci.nsIToolkitProfileService
  );
  let currentProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let profileName = "mochitest-test-profile-temp-" + Date.now();
  let tempProfile = profileService.createProfile(
    currentProfileDir,
    profileName
  );
  Assert.ok(
    ResetProfile.resetSupported(),
    "Should be able to reset from mochitest's temporary profile once it's in the profile manager."
  );

  registerCleanupFunction(() => {
    tempProfile.remove(false);
    Assert.ok(
      !ResetProfile.resetSupported(),
      "Shouldn't be able to reset from mochitest's temporary profile once removed from the profile manager."
    );
  });
}

/**
 * Starts a search that should trigger a tip, picks the tip, and waits for the
 * tip's action to happen.
 *
 * @param {object} options
 *   Options for the test
 * @param {string} options.searchString
 *   The search string.
 * @param {TIPS} options.tip
 *   The expected tip type.
 * @param {string} options.title
 *   The expected tip title.
 * @param {string} options.button
 *   The expected button title.
 * @param {Function} options.awaitCallback
 *   A function that checks the tip's action.  Should return a promise (or be
 *   async).
 * @returns {*}
 *   The value returned from `awaitCallback`.
 */
function checkIntervention({
  searchString,
  tip,
  title,
  button,
  awaitCallback,
} = {}) {
  // Opening modal dialogs confuses focus on Linux just after them, thus run
  // these checks in separate tabs to better isolate them.
  return BrowserTestUtils.withNewTab("about:blank", async () => {
    // Do a search that triggers the tip.
    let [result, element] = await awaitTip(searchString);
    Assert.strictEqual(result.payload.type, tip);
    await element.ownerDocument.l10n.translateFragment(element);

    let actualTitle = element._elements.get("title").textContent;
    if (typeof title == "string") {
      Assert.equal(actualTitle, title, "Title string");
    } else {
      // regexp
      Assert.ok(title.test(actualTitle), "Title regexp");
    }

    let actualButton = element._buttons.get("0").textContent;
    if (typeof button == "string") {
      Assert.equal(actualButton, button, "Button string");
    } else {
      // regexp
      Assert.ok(button.test(actualButton), "Button regexp");
    }

    let menuButton = element._buttons.get("menu");
    Assert.ok(menuButton, "Menu button exists");
    Assert.ok(BrowserTestUtils.isVisible(menuButton), "Menu button is visible");

    let values = await Promise.all([awaitCallback(), pickTip()]);
    Assert.ok(true, "Refresh dialog opened");

    // Ensure the urlbar is closed so that the engagement is ended.
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());

    const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "urlbar.tips",
      `${tip}-shown`,
      1
    );
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "urlbar.tips",
      `${tip}-picked`,
      1
    );

    return values[0] || null;
  });
}

/**
 * Starts a search and asserts that there are no tips.
 *
 * @param {string} searchString
 *   The search string.
 * @param {Window} win
 *   The host window.
 */
async function awaitNoTip(searchString, win = window) {
  let context = await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: searchString,
    waitForFocus,
    fireInputEvent: true,
  });
  for (let result of context.results) {
    Assert.notEqual(result.type, UrlbarUtils.RESULT_TYPE.TIP);
  }
}

/**
 * Search tips helper.  Asserts that a particular search tip is shown or that no
 * search tip is shown.
 *
 * @param {window} win
 *   A browser window.
 * @param {UrlbarProviderSearchTips.TIP_TYPE} expectedTip
 *   The expected search tip.  Pass a falsey value (like zero) for none.
 * @param {boolean} closeView
 *   If true, this function closes the urlbar view before returning.
 */
async function checkTip(win, expectedTip, closeView = true) {
  if (!expectedTip) {
    // Wait a bit for the tip to not show up.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 100));
    Assert.ok(!win.gURLBar.view.isOpen, "View is not open");
    return;
  }

  // Wait for the view to open, and then check the tip result.
  await UrlbarTestUtils.promisePopupOpen(win, () => {});
  Assert.ok(true, "View opened");
  Assert.equal(UrlbarTestUtils.getResultCount(win), 1, "Number of results");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(win, 0);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.TIP, "Result type");
  let heuristic;
  let title;
  let name = Services.search.defaultEngine.name;
  switch (expectedTip) {
    case UrlbarProviderSearchTips.TIP_TYPE.ONBOARD:
      heuristic = true;
      title =
        `Type less, find more: Search ${name} right from your ` +
        `address bar.`;
      break;
    case UrlbarProviderSearchTips.TIP_TYPE.REDIRECT:
      heuristic = false;
      title =
        `Start your search in the address bar to see suggestions from ` +
        `${name} and your browsing history.`;
      break;
    case UrlbarProviderSearchTips.TIP_TYPE.PERSIST:
      heuristic = false;
      title =
        "Searching just got simpler." +
        " Try making your search more specific here in the address bar." +
        " To show the URL instead, visit Search, in settings.";
      break;
  }
  Assert.equal(result.heuristic, heuristic, "Result is heuristic");
  Assert.equal(result.displayed.title, title, "Title");
  Assert.equal(
    result.element.row._buttons.get("0").textContent,
    expectedTip == UrlbarProviderSearchTips.TIP_TYPE.PERSIST
      ? `Got it`
      : `Okay, Got It`,
    "Button text"
  );
  Assert.ok(
    !result.element.row._buttons.has("help"),
    "Buttons in row does not include help"
  );

  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${expectedTip}-shown`,
    1
  );

  Assert.ok(
    !UrlbarTestUtils.getOneOffSearchButtonsVisible(window),
    "One-offs should be hidden when showing a search tip"
  );

  if (closeView) {
    await UrlbarTestUtils.promisePopupClose(win);
  }
}

function makeTipResult({ buttonUrl, helpUrl = undefined }) {
  return new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.TIP,
    UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
    {
      helpUrl,
      type: "test",
      titleL10n: { id: "urlbar-search-tips-confirm" },
      buttons: [
        {
          url: buttonUrl,
          l10n: { id: "urlbar-search-tips-confirm" },
        },
      ],
    }
  );
}

/**
 * Search tips helper.  Opens a foreground tab and asserts that a particular
 * search tip is shown or that no search tip is shown.
 *
 * @param {window} win
 *   A browser window.
 * @param {string} url
 *   The URL to load in a new foreground tab.
 * @param {UrlbarProviderSearchTips.TIP_TYPE} expectedTip
 *   The expected search tip.  Pass a falsey value (like zero) for none.
 * @param {boolean} reset
 *   If true, the search tips provider will be reset before this function
 *   returns.  See resetSearchTipsProvider.
 */
async function checkTab(win, url, expectedTip, reset = true) {
  // BrowserTestUtils.withNewTab always waits for tab load, which hangs on
  // about:newtab for some reason, so don't use it.
  let shownCount;
  if (expectedTip) {
    shownCount = UrlbarPrefs.get(`tipShownCount.${expectedTip}`);
  }

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    url,
    waitForLoad: url != "about:newtab",
  });

  await checkTip(win, expectedTip, true);
  if (expectedTip) {
    Assert.equal(
      UrlbarPrefs.get(`tipShownCount.${expectedTip}`),
      shownCount + 1,
      "The shownCount pref should have been incremented by one."
    );
  }

  if (reset) {
    resetSearchTipsProvider();
  }

  BrowserTestUtils.removeTab(tab);
}

/**
 * This lets us visit www.google.com (for example) and have it redirect to
 * our test HTTP server instead of visiting the actual site.
 *
 * @param {string} domain
 *   The domain to which we are redirecting.
 * @param {string} path
 *   The pathname on the domain.
 * @param {Function} callback
 *   Executed when the test suite thinks `domain` is loaded.
 */
async function withDNSRedirect(domain, path, callback) {
  // Some domains have special security requirements, like www.bing.com.  We
  // need to override them to successfully load them.  This part is adapted from
  // testing/marionette/cert.js.
  const certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);
  Services.prefs.setBoolPref(
    "network.stricttransportsecurity.preloadlist",
    false
  );
  Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 0);
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  // Now set network.dns.localDomains to redirect the domain to localhost and
  // set up an HTTP server.
  Services.prefs.setCharPref("network.dns.localDomains", domain);

  let server = new HttpServer();
  server.registerPathHandler(path, (req, resp) => {
    resp.write(`Test! http://${domain}${path}`);
  });
  server.start(-1);
  server.identity.setPrimary("http", domain, server.identity.primaryPort);
  let url = `http://${domain}:${server.identity.primaryPort}${path}`;

  await callback(url);

  // Reset network.dns.localDomains and stop the server.
  Services.prefs.clearUserPref("network.dns.localDomains");
  await new Promise(resolve => server.stop(resolve));

  // Reset the security stuff.
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
  Services.prefs.clearUserPref("network.stricttransportsecurity.preloadlist");
  Services.prefs.clearUserPref("security.cert_pinning.enforcement_level");
  const sss = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  sss.clearAll();
}

function resetSearchTipsProvider() {
  Services.prefs.clearUserPref(
    `browser.urlbar.tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.ONBOARD}`
  );
  Services.prefs.clearUserPref(
    `browser.urlbar.tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.PERSIST}`
  );
  Services.prefs.clearUserPref(
    `browser.urlbar.tipShownCount.${UrlbarProviderSearchTips.TIP_TYPE.REDIRECT}`
  );
  UrlbarProviderSearchTips.disableTipsForCurrentSession = false;
}

async function setDefaultEngine(name) {
  let engine = (await Services.search.getEngines()).find(e => e.name == name);
  Assert.ok(engine);
  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
}
