/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderInterventions:
    "resource:///modules/UrlbarProviderInterventions.sys.mjs",
});

add_task(async function init() {
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
  let enginesReloaded = SearchTestUtils.promiseSearchNotification(
    "engines-reloaded"
  );

  const originalAvailable = Services.locale.availableLocales;
  const originalRequested = Services.locale.requestedLocales;
  Services.locale.availableLocales = ["en-US", "de"];
  Services.locale.requestedLocales = ["de"];

  registerCleanupFunction(async () => {
    let enginesReloaded2 = SearchTestUtils.promiseSearchNotification(
      "engines-reloaded"
    );
    Services.locale.requestedLocales = originalRequested;
    Services.locale.availableLocales = originalAvailable;
    await enginesReloaded2;
  });

  let appLocales = Services.locale.appLocalesAsBCP47;
  Assert.equal(appLocales[0], "de");

  await enginesReloaded;

  // Interventions should no longer work in the new locale.
  await awaitNoTip(SEARCH_STRINGS.CLEAR, window);
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
});

/**
 * Picks the help button from an Intervention. We spoof the Intervention in this
 * test because our withDNSRedirect helper cannot handle the HTTPS SUMO links.
 */
add_task(async function pickHelpButton() {
  const helpUrl = "http://example.com/";
  let results = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/a" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TIP,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        type: UrlbarProviderInterventions.TIP_TYPE.CLEAR,
        text: "This is a test tip.",
        buttonText: "Done",
        helpUrl,
      }
    ),
  ];
  let interventionProvider = new UrlbarTestUtils.TestProvider({
    results,
    priority: 2,
  });
  UrlbarProvidersManager.registerProvider(interventionProvider);

  registerCleanupFunction(() => {
    UrlbarProvidersManager.unregisterProvider(interventionProvider);
  });

  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let [result, element] = await awaitTip(SEARCH_STRINGS.CLEAR);
    Assert.strictEqual(
      result.payload.type,
      UrlbarProviderInterventions.TIP_TYPE.CLEAR
    );

    let helpButton = element._elements.get("helpButton");
    Assert.ok(BrowserTestUtils.is_visible(helpButton));
    EventUtils.synthesizeMouseAtCenter(helpButton, {});

    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, helpUrl);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "urlbar.tips",
      `${UrlbarProviderInterventions.TIP_TYPE.CLEAR}-help`,
      1
    );
  });
});
