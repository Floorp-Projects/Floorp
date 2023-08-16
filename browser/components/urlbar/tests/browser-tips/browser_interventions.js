/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderInterventions:
    "resource:///modules/UrlbarProviderInterventions.sys.mjs",
});

add_setup(async function () {
  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
  makeProfileResettable();
});

// Tests the refresh tip.
add_task(async function refresh() {
  // Pick the tip, which should open the refresh dialog.  Click its cancel
  // button.
  await checkIntervention({
    searchString: SEARCH_STRINGS.REFRESH,
    tip: UrlbarProviderInterventions.TIP_TYPE.REFRESH,
    title:
      "Restore default settings and remove old add-ons for optimal performance.",
    button: /^Refresh .+…$/,
    awaitCallback() {
      return BrowserTestUtils.promiseAlertDialog(
        "cancel",
        "chrome://global/content/resetProfile.xhtml",
        { isSubDialog: true }
      );
    },
  });
});

// Tests the clear tip.
add_task(async function clear() {
  // Pick the tip, which should open the refresh dialog.  Click its cancel
  // button.
  await checkIntervention({
    searchString: SEARCH_STRINGS.CLEAR,
    tip: UrlbarProviderInterventions.TIP_TYPE.CLEAR,
    title: "Clear your cache, cookies, history and more.",
    button: "Choose What to Clear…",
    awaitCallback() {
      return BrowserTestUtils.promiseAlertDialog(
        "cancel",
        "chrome://browser/content/sanitize.xhtml",
        {
          isSubDialog: true,
        }
      );
    },
  });
});

// Tests the clear tip in a private window. The clear tip shouldn't appear in
// private windows.
add_task(async function clear_private() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  // First, make sure the extension works in PBM by triggering a non-clear
  // tip.
  let result = (await awaitTip(SEARCH_STRINGS.REFRESH, win))[0];
  Assert.strictEqual(
    result.payload.type,
    UrlbarProviderInterventions.TIP_TYPE.REFRESH
  );

  // Blur the urlbar so that the engagement is ended.
  await UrlbarTestUtils.promisePopupClose(win, () => win.gURLBar.blur());

  // Now do a search that would trigger the clear tip.
  await awaitNoTip(SEARCH_STRINGS.CLEAR, win);

  // Blur the urlbar so that the engagement is ended.
  await UrlbarTestUtils.promisePopupClose(win, () => win.gURLBar.blur());

  await BrowserTestUtils.closeWindow(win);
});

// Tests that if multiple interventions of the same type are seen in the same
// engagement, only one instance is recorded in Telemetry.
add_task(async function multipleInterventionsInOneEngagement() {
  Services.telemetry.clearScalars();
  let result = (await awaitTip(SEARCH_STRINGS.REFRESH, window))[0];
  Assert.strictEqual(
    result.payload.type,
    UrlbarProviderInterventions.TIP_TYPE.REFRESH
  );
  result = (await awaitTip(SEARCH_STRINGS.CLEAR, window))[0];
  Assert.strictEqual(
    result.payload.type,
    UrlbarProviderInterventions.TIP_TYPE.CLEAR
  );
  result = (await awaitTip(SEARCH_STRINGS.REFRESH, window))[0];
  Assert.strictEqual(
    result.payload.type,
    UrlbarProviderInterventions.TIP_TYPE.REFRESH
  );

  // Blur the urlbar so that the engagement is ended.
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());

  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  // We should only record one impression for the Refresh tip. Although it was
  // seen twice, it was in the same engagement.
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${UrlbarProviderInterventions.TIP_TYPE.REFRESH}-shown`,
    1
  );
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.tips",
    `${UrlbarProviderInterventions.TIP_TYPE.CLEAR}-shown`,
    1
  );
});

// Test the result of UrlbarProviderInterventions.isActive()
// and whether or not the function calucates the score.
add_task(async function testIsActive() {
  const testData = [
    {
      description: "Test for search string that activates the intervention",
      searchString: "firefox slow",
      expectedActive: true,
      expectedScoreCalculated: true,
    },
    {
      description:
        "Test for search string that does not activate the intervention",
      searchString: "example slow",
      expectedActive: false,
      expectedScoreCalculated: true,
    },
    {
      description: "Test for empty search string",
      searchString: "",
      expectedActive: false,
      expectedScoreCalculated: false,
    },
    {
      description: "Test for an URL",
      searchString: "https://firefox/slow",
      expectedActive: false,
      expectedScoreCalculated: false,
    },
    {
      description: "Test for a data URL",
      searchString: "data:text/html,<div>firefox slow</div>",
      expectedActive: false,
      expectedScoreCalculated: false,
    },
    {
      description: "Test for string like URL",
      searchString: "firefox://slow",
      expectedActive: false,
      expectedScoreCalculated: false,
    },
  ];

  for (const {
    description,
    searchString,
    expectedActive,
    expectedScoreCalculated,
  } of testData) {
    info(description);

    // Set null to currentTip to know whether or not UrlbarProviderInterventions
    // calculated the score.
    UrlbarProviderInterventions.currentTip = null;

    const isActive = UrlbarProviderInterventions.isActive({ searchString });
    Assert.equal(isActive, expectedActive, "Result of isAcitive is correct");
    const isScoreCalculated = UrlbarProviderInterventions.currentTip !== null;
    Assert.equal(
      isScoreCalculated,
      expectedScoreCalculated,
      "The score is calculated correctly"
    );
  }
});

add_task(async function tipsAreEnglishOnly() {
  // Test that Interventions are working in en-US.
  let result = (await awaitTip(SEARCH_STRINGS.REFRESH, window))[0];
  Assert.strictEqual(
    result.payload.type,
    UrlbarProviderInterventions.TIP_TYPE.REFRESH
  );
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());

  // We will need to fetch new engines when we switch locales.
  let enginesReloaded =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");

  const originalAvailable = Services.locale.availableLocales;
  const originalRequested = Services.locale.requestedLocales;
  Services.locale.availableLocales = ["en-US", "de"];
  Services.locale.requestedLocales = ["de"];

  let cleanup = async () => {
    let reloadPromise =
      SearchTestUtils.promiseSearchNotification("engines-reloaded");
    Services.locale.requestedLocales = originalRequested;
    Services.locale.availableLocales = originalAvailable;
    await reloadPromise;
    cleanup = null;
  };
  registerCleanupFunction(() => cleanup?.());

  let appLocales = Services.locale.appLocalesAsBCP47;
  Assert.equal(appLocales[0], "de");

  await enginesReloaded;

  // Interventions should no longer work in the new locale.
  await awaitNoTip(SEARCH_STRINGS.CLEAR, window);
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());

  await cleanup();
});

// Tests the help command (using the clear intervention). It should open the
// help page and it should not trigger the primary intervention behavior.
add_task(async function pickHelp() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    // Do a search that triggers the clear tip.
    let [result] = await awaitTip(SEARCH_STRINGS.CLEAR);
    Assert.strictEqual(
      result.payload.type,
      UrlbarProviderInterventions.TIP_TYPE.CLEAR
    );

    // Click the help command and wait for the help page to load.
    Assert.ok(
      !!result.payload.helpUrl,
      "The result's helpUrl should be defined and non-empty: " +
        JSON.stringify(result.payload.helpUrl)
    );
    let loadPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      result.payload.helpUrl
    );
    await UrlbarTestUtils.openResultMenuAndPressAccesskey(window, "h", {
      openByMouse: true,
      resultIndex: 1,
    });
    info("Waiting for help URL to load in the current tab");
    await loadPromise;

    // Wait a bit and make sure the clear recent history dialog did not open.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 2000));
    Assert.strictEqual(gDialogBox.isOpen, false, "No dialog should be open");

    // Check telemetry.
    const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "urlbar.tips",
      `${UrlbarProviderInterventions.TIP_TYPE.CLEAR}-help`,
      1
    );
  });
});
