/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

add_task(async function test_switch_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.userContext.enabled", true],
      ["browser.urlbar.switchTabs.searchAllContainers", true],
    ],
  });

  let urlA = "https://example.com/";
  let urlB = "https://www.mozilla.org/";
  let contextIdTabA = 1;
  let contextIdTabB = 2;
  let contextIdTabC = 3;

  let tabA = await openTabInUserContext(urlA, contextIdTabA);
  let tabB = await openTabInUserContext(urlA, contextIdTabB);
  let tabC = await openTabInUserContext(urlB, contextIdTabC);

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
    element.querySelectorAll(".urlbarView-action").length,
    2,
    "Has switch to tab and user-context chiclet"
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
