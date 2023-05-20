/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  SearchTestUtils: "resource://testing-common/SearchTestUtils.sys.mjs",
});

const CONFIG_DEFAULT = [
  {
    webExtension: { id: "basic@search.mozilla.org" },
    appliesTo: [{ included: { everywhere: true } }],
    default: "yes",
  },
  {
    webExtension: { id: "private@search.mozilla.org" },
    appliesTo: [
      {
        experiment: "testing",
        included: { everywhere: true },
      },
    ],
    defaultPrivate: "yes",
  },
];

SearchTestUtils.init(this);

add_setup(async () => {
  // Use engines in test directory
  let searchExtensions = getChromeDir(getResolvedURI(gTestPath));
  searchExtensions.append("search-engines");
  await SearchTestUtils.useMochitestEngines(searchExtensions);

  // Current default values.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", false],
      ["browser.search.separatePrivateDefault.urlbarResult.enabled", false],
      ["browser.search.separatePrivateDefault", true],
      ["browser.urlbar.suggest.searches", true],
    ],
  });

  SearchTestUtils.useMockIdleService();
  await SearchTestUtils.updateRemoteSettingsConfig(CONFIG_DEFAULT);

  registerCleanupFunction(async () => {
    let settingsWritten = SearchTestUtils.promiseSearchNotification(
      "write-settings-to-disk-complete"
    );
    await SearchTestUtils.updateRemoteSettingsConfig();
    await settingsWritten;
  });
});

add_task(async function test_nimbus_experiment() {
  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "basic",
    "Should have basic as private default while not in experiment"
  );
  await ExperimentAPI.ready();

  let reloadObserved =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "searchConfiguration",
    value: {
      seperatePrivateDefaultUIEnabled: true,
      seperatePrivateDefaultUrlbarResultEnabled: false,
      experiment: "testing",
    },
  });
  await reloadObserved;
  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "private",
    "Should have private as private default while in experiment"
  );
  reloadObserved =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");
  await doExperimentCleanup();
  await reloadObserved;
  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "basic",
    "Should turn off private default and restore default engine after experiment"
  );
});

add_task(async function test_nimbus_experiment_urlbar_result_enabled() {
  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "basic",
    "Should have basic as private default while not in experiment"
  );
  await ExperimentAPI.ready();

  let reloadObserved =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "searchConfiguration",
    value: {
      seperatePrivateDefaultUIEnabled: true,
      seperatePrivateDefaultUrlbarResultEnabled: true,
      experiment: "testing",
    },
  });
  await reloadObserved;
  Assert.equal(
    Services.search.separatePrivateDefaultUrlbarResultEnabled,
    true,
    "Should have set the urlbar result enabled value to true"
  );
  reloadObserved =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");
  await doExperimentCleanup();
  await reloadObserved;
  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "basic",
    "Should turn off private default and restore default engine after experiment"
  );
});

add_task(async function test_non_experiment_prefs() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.separatePrivateDefault.ui.enabled", false]],
  });
  let uiPref = () =>
    Services.prefs.getBoolPref(
      "browser.search.separatePrivateDefault.ui.enabled"
    );
  Assert.equal(uiPref(), false, "defaulted false");
  await ExperimentAPI.ready();
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "privatesearch",
    value: {
      seperatePrivateDefaultUIEnabled: true,
    },
  });
  Assert.equal(uiPref(), false, "Pref did not change without experiment");
  await doExperimentCleanup();
  await SpecialPowers.popPrefEnv();
});
