/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.userContext.enabled", true],
      ["browser.urlbar.switchTabs.searchAllContainers", true],
    ],
  });
});

add_task(async function test_switch_tab() {
  let urlA = "https://example.com/";
  let urlB = "https://www.mozilla.org/";

  let contextIdTabA = 1;
  let contextIdTabB = 2;
  let contextIdTabC = 3;

  let { tab: tabA } = await openTabInUserContext(urlA, contextIdTabA);
  let { tab: tabB } = await openTabInUserContext(urlA, contextIdTabB);
  let { tab: tabC } = await openTabInUserContext(urlB, contextIdTabC);

  let searchContext = await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "exa",
  });
  // Check if results contain correct tab switch rows
  ok(
    searchContext.results.find(result => {
      return (
        result.type == UrlbarUtils.RESULT_TYPE.TAB_SWITCH &&
        result.payload.url == "https://example.com/" &&
        result.payload.userContextId == contextIdTabA
      );
    }),
    "Switch tab row for user context A is present in results."
  );

  let resultIndex = -1;
  let tabSwitchRow = searchContext.results.find(result => {
    resultIndex += 1;
    return (
      result.type == UrlbarUtils.RESULT_TYPE.TAB_SWITCH &&
      result.payload.url == "https://example.com/" &&
      result.payload.userContextId == contextIdTabB
    );
  });

  ok(
    tabSwitchRow != undefined,
    "Urlbar results contain the switch to tab from another container."
  );
  let element = UrlbarTestUtils.getRowAt(window, resultIndex);
  is(
    element.querySelectorAll(".urlbarView-action.urlbarView-userContext")
      .length,
    1,
    "Has switch to tab with user-context chiclet"
  );
  let tabSwitchDonePromise = BrowserTestUtils.waitForEvent(
    window,
    "TabSwitchDone"
  );
  EventUtils.synthesizeMouseAtCenter(element, {});
  await tabSwitchDonePromise;

  is(gBrowser.selectedTab, tabB, "Correct tab is selected after switch.");
  is(
    gBrowser.selectedTab.userContextId,
    contextIdTabB,
    "Tab has correct user context id."
  );

  window.gBrowser.removeTab(tabA);
  window.gBrowser.removeTab(tabB);
  window.gBrowser.removeTab(tabC);
});

add_task(async function test_chiclet_disabled_on_update() {
  // When result rows of tab switches across containers are reused for
  // other result types, make sure that the user-context chiclet is removed.

  let urlA = "https://example.com/";

  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "https://exomple.com/",
    title: "Exomple",
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: "https://exooomple.com/",
    title: "Exooomple",
  });

  let contextIdTabA = 2;
  let contextIdTabB = 3;

  let initialTab = gBrowser.selectedTab;
  let { tab: tabA } = await openTabInUserContext(urlA, contextIdTabA);
  let { tab: tabB } = await openTabInUserContext(urlA, contextIdTabB);

  await BrowserTestUtils.switchTab(gBrowser, initialTab);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "exa",
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "exo",
  });

  let row = UrlbarTestUtils.getRowAt(window, 1);
  ok(
    row._elements["user-context"] == undefined,
    "Row doesnt contain user-context chiclet."
  );

  window.gBrowser.removeTab(tabA);
  window.gBrowser.removeTab(tabB);
});
