/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);

SearchTestUtils.init(this);

add_setup(async function() {
  await SearchTestUtils.installSearchExtension({
    name: "engine1",
    search_url: "https://example.com/engine1",
    search_url_get_params: "search={searchTerms}",
  });
  await SearchTestUtils.installSearchExtension({
    name: "engine2",
    search_url: "https://example.com/engine2",
    search_url_get_params: "search={searchTerms}",
  });

  const defaultEngine = await Services.search.getDefault();
  const defaultPrivateEngine = await Services.search.getDefaultPrivate();

  registerCleanupFunction(async () => {
    await Services.search.setDefault(defaultEngine);
    await Services.search.setDefaultPrivate(defaultPrivateEngine);
  });
});

add_task(async function test_openWithPrivateDefaultNotEnabledFirst() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", false],
      ["browser.search.separatePrivateDefault", false],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  const doc = gBrowser.selectedBrowser.contentDocument;
  const separateEngineCheckbox = doc.getElementById(
    "browserSeparateDefaultEngine"
  );
  const privateDefaultVbox = doc.getElementById(
    "browserPrivateEngineSelection"
  );

  Assert.ok(
    separateEngineCheckbox.hidden,
    "Should have hidden the separate search engine checkbox"
  );
  Assert.ok(
    privateDefaultVbox.hidden,
    "Should have hidden the private engine selection box"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.separatePrivateDefault.ui.enabled", true]],
  });

  Assert.ok(
    !separateEngineCheckbox.hidden,
    "Should have displayed the separate search engine checkbox"
  );
  Assert.ok(
    privateDefaultVbox.hidden,
    "Should not have displayed the private engine selection box"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.separatePrivateDefault", true]],
  });

  Assert.ok(
    !separateEngineCheckbox.hidden,
    "Should still be displaying the separate search engine checkbox"
  );
  Assert.ok(
    !privateDefaultVbox.hidden,
    "Should have displayed the private engine selection box"
  );

  gBrowser.removeCurrentTab();
});

add_task(async function test_openWithPrivateDefaultEnabledFirst() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", true],
      ["browser.search.separatePrivateDefault", true],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  const doc = gBrowser.selectedBrowser.contentDocument;
  const separateEngineCheckbox = doc.getElementById(
    "browserSeparateDefaultEngine"
  );
  const privateDefaultVbox = doc.getElementById(
    "browserPrivateEngineSelection"
  );

  Assert.ok(
    !separateEngineCheckbox.hidden,
    "Should not have hidden the separate search engine checkbox"
  );
  Assert.ok(
    !privateDefaultVbox.hidden,
    "Should not have hidden the private engine selection box"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.separatePrivateDefault", false]],
  });

  Assert.ok(
    !separateEngineCheckbox.hidden,
    "Should not have hidden the separate search engine checkbox"
  );
  Assert.ok(
    privateDefaultVbox.hidden,
    "Should have hidden the private engine selection box"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.separatePrivateDefault.ui.enabled", false]],
  });

  Assert.ok(
    separateEngineCheckbox.hidden,
    "Should have hidden the separate private engine checkbox"
  );
  Assert.ok(
    privateDefaultVbox.hidden,
    "Should still be hiding the private engine selection box"
  );

  gBrowser.removeCurrentTab();
});

add_task(async function test_separatePrivateDefault() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", true],
      ["browser.search.separatePrivateDefault", false],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  const doc = gBrowser.selectedBrowser.contentDocument;
  const separateEngineCheckbox = doc.getElementById(
    "browserSeparateDefaultEngine"
  );
  const privateDefaultVbox = doc.getElementById(
    "browserPrivateEngineSelection"
  );

  Assert.ok(
    privateDefaultVbox.hidden,
    "Should not be displaying the private engine selection box"
  );

  separateEngineCheckbox.checked = false;
  separateEngineCheckbox.doCommand();

  Assert.ok(
    Services.prefs.getBoolPref("browser.search.separatePrivateDefault"),
    "Should have correctly set the pref"
  );

  Assert.ok(
    !privateDefaultVbox.hidden,
    "Should be displaying the private engine selection box"
  );

  separateEngineCheckbox.checked = true;
  separateEngineCheckbox.doCommand();

  Assert.ok(
    !Services.prefs.getBoolPref("browser.search.separatePrivateDefault"),
    "Should have correctly turned the pref off"
  );

  Assert.ok(
    privateDefaultVbox.hidden,
    "Should have hidden the private engine selection box"
  );

  gBrowser.removeCurrentTab();
});

async function setDefaultEngine(
  testPrivate,
  currentEngineName,
  expectedEngineName
) {
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  const doc = gBrowser.selectedBrowser.contentDocument;
  const defaultEngineSelector = doc.getElementById(
    testPrivate ? "defaultPrivateEngine" : "defaultEngine"
  );

  Assert.equal(
    defaultEngineSelector.selectedItem.engine.name,
    currentEngineName,
    "Should have the correct engine as default on first open"
  );

  const popup = defaultEngineSelector.menupopup;
  const popupShown = BrowserTestUtils.waitForEvent(popup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(
    defaultEngineSelector,
    {},
    defaultEngineSelector.ownerGlobal
  );
  await popupShown;

  const items = Array.from(popup.children);
  const engine2Item = items.find(
    item => item.engine.name == expectedEngineName
  );

  const defaultChanged = SearchTestUtils.promiseSearchNotification(
    testPrivate ? "engine-default-private" : "engine-default",
    "browser-search-engine-modified"
  );
  // Waiting for popupHiding here seemed to cause a race condition, however
  // as we're really just interested in the notification, we'll just use
  // that here.
  EventUtils.synthesizeMouseAtCenter(engine2Item, {}, engine2Item.ownerGlobal);
  await defaultChanged;

  const newDefault = testPrivate
    ? await Services.search.getDefaultPrivate()
    : await Services.search.getDefault();
  Assert.equal(
    newDefault.name,
    expectedEngineName,
    "Should have changed the default engine to engine2"
  );
}

add_task(async function test_setDefaultEngine() {
  const engine1 = Services.search.getEngineByName("engine1");

  // Set an initial default so we have a known engine.
  await Services.search.setDefault(engine1);

  await setDefaultEngine(false, "engine1", "engine2");

  gBrowser.removeCurrentTab();
});

add_task(async function test_setPrivateDefaultEngine() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", true],
      ["browser.search.separatePrivateDefault", true],
    ],
  });

  const engine2 = Services.search.getEngineByName("engine2");

  // Set an initial default so we have a known engine.
  await Services.search.setDefaultPrivate(engine2);

  await setDefaultEngine(true, "engine2", "engine1");

  gBrowser.removeCurrentTab();
});
