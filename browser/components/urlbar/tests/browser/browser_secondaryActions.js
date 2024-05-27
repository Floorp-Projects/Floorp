/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Basic tests for secondary Actions.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ActionsProviderQuickActions:
    "resource:///modules/ActionsProviderQuickActions.sys.mjs",
});

let testActionCalled = 0;

let loadURI = async (browser, uri) => {
  let onLoaded = BrowserTestUtils.browserLoaded(browser, false, uri);
  BrowserTestUtils.startLoadingURIString(browser, uri);
  return onLoaded;
};

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.scotchBonnet.enableOverride", true]],
  });

  ActionsProviderQuickActions.addAction("testaction", {
    commands: ["testaction"],
    actionKey: "testaction",
    label: "quickactions-downloads2",
    onPick: () => testActionCalled++,
  });

  registerCleanupFunction(() => {
    ActionsProviderQuickActions.removeAction("testaction");
  });
});

add_task(async function test_quickaction() {
  info("Match an installed quickaction and trigger it via tab");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "testact",
  });

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "We matched the action"
  );

  info("The callback of the action is fired when selected");
  EventUtils.synthesizeKey("KEY_Tab", {}, window);
  EventUtils.synthesizeKey("KEY_Enter", {}, window);
  Assert.equal(testActionCalled, 1, "Test action was called");
});

add_task(async function test_switchtab() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await loadURI(win.gBrowser, "https://example.com/");

  info("Open a new tab, type in the urlbar and switch to previous tab");
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "example",
  });

  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  EventUtils.synthesizeKey("KEY_Tab", {}, win);
  EventUtils.synthesizeKey("KEY_Enter", {}, win);

  is(win.gBrowser.tabs.length, 1, "We switched to previous tab");
  is(
    win.gBrowser.currentURI.spec,
    "https://example.com/",
    "We switched to previous tab"
  );

  info(
    "Open a new tab, type in the urlbar, select result and open url in current tab"
  );
  await BrowserTestUtils.openNewForegroundTab(win.gBrowser);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "example",
  });

  EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await BrowserTestUtils.browserLoaded(win.gBrowser);
  is(win.gBrowser.tabs.length, 2, "We switched to previous tab");
  is(
    win.gBrowser.currentURI.spec,
    "https://example.com/",
    "We opened in current tab"
  );

  BrowserTestUtils.closeWindow(win);
});

add_task(async function test_sitesearch() {
  await SearchTestUtils.installSearchExtension({
    name: "Contextual",
    search_url: "https://example.com/browser",
  });

  let ENGINE_TEST_URL = "https://example.com/";
  await loadURI(gBrowser.selectedBrowser, ENGINE_TEST_URL);

  const query = "search";
  let engine = Services.search.getEngineByName("Contextual");
  const [expectedUrl] = UrlbarUtils.getSearchQueryUrl(engine, query);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "sea",
  });

  let onLoad = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    expectedUrl
  );
  gURLBar.value = query;
  UrlbarTestUtils.fireInputEvent(window);
  EventUtils.synthesizeKey("KEY_Tab");
  EventUtils.synthesizeKey("KEY_Enter");
  await onLoad;

  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    expectedUrl,
    "Selecting the contextual search result opens the search URL"
  );
});
