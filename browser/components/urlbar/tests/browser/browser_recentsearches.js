/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CONFIG_DEFAULT = [
  {
    webExtension: { id: "basic@search.mozilla.org" },
    appliesTo: [{ included: { everywhere: true } }],
    default: "yes",
  },
];

SearchTestUtils.init(this);

add_setup(async () => {
  // Use engines in test directory
  let searchExtensions = getChromeDir(getResolvedURI(gTestPath));
  searchExtensions.append("search-engines");
  await SearchTestUtils.useMochitestEngines(searchExtensions);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.suggest.recentsearches", true],
      ["browser.urlbar.recentsearches.featureGate", true],
      // Disable UrlbarProviderSearchTips
      [
        "browser.newtabpage.activity-stream.asrouter.userprefs.cfr.features",
        false,
      ],
    ],
  });

  SearchTestUtils.useMockIdleService();
  await SearchTestUtils.updateRemoteSettingsConfig(CONFIG_DEFAULT);
  Services.telemetry.clearScalars();

  registerCleanupFunction(async () => {
    let settingsWritten = SearchTestUtils.promiseSearchNotification(
      "write-settings-to-disk-complete"
    );
    await SearchTestUtils.updateRemoteSettingsConfig();
    await settingsWritten;
    await UrlbarTestUtils.formHistory.clear();
  });
});

add_task(async () => {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "data:text/html,"
  );

  info("Perform a search that will be added to search history.");
  let browserLoaded = BrowserTestUtils.browserLoaded(
    window.gBrowser.selectedBrowser
  );
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "Bob Vylan",
    waitForFocus: SimpleTest.waitForFocus,
  });

  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Enter", {}, window);
  });
  await browserLoaded;

  info("Now check that is shown in search history.");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
    waitForFocus: SimpleTest.waitForFocus,
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "Previous search shown"
  );
  let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.equal(result.providerName, "RecentSearches");

  info("Selecting the recent search should be indicated in telemetry.");
  browserLoaded = BrowserTestUtils.browserLoaded(
    window.gBrowser.selectedBrowser
  );
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
    EventUtils.synthesizeKey("KEY_Enter", {}, window);
  });
  await browserLoaded;

  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    "urlbar.picked.recent_search",
    0,
    1
  );
  await BrowserTestUtils.removeTab(tab);
});
