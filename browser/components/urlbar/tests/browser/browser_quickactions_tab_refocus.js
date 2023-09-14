/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for QuickActions that re-focus tab..
 */

"use strict";

requestLongerTimeout(3);

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quickactions.enabled", true],
      ["browser.urlbar.suggest.quickactions", true],
      ["browser.urlbar.shortcuts.quickactions", true],
    ],
  });
});

let isSelected = async selector =>
  SpecialPowers.spawn(gBrowser.selectedBrowser, [selector], arg => {
    return ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(arg)?.hasAttribute("selected")
    );
  });

add_task(async function test_about_pages() {
  const testData = [
    {
      firstInput: "downloads",
      uri: "about:downloads",
    },
    {
      firstInput: "logins",
      uri: "about:logins",
    },
    {
      firstInput: "settings",
      uri: "about:preferences",
    },
    {
      firstInput: "add-ons",
      uri: "about:addons",
      component: "button[name=discover]",
    },
    {
      firstInput: "extensions",
      uri: "about:addons",
      component: "button[name=extension]",
    },
    {
      firstInput: "plugins",
      uri: "about:addons",
      component: "button[name=plugin]",
    },
    {
      firstInput: "themes",
      uri: "about:addons",
      component: "button[name=theme]",
    },
    {
      firstLoad: "about:preferences#home",
      secondInput: "settings",
      uri: "about:preferences#home",
    },
  ];

  for (const {
    firstInput,
    firstLoad,
    secondInput,
    uri,
    component,
  } of testData) {
    info("Setup initial state");
    let firstTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    let onLoad = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      uri
    );
    if (firstLoad) {
      info("Load initial URI");
      BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, uri);
    } else {
      info("Open about page by quick action");
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: firstInput,
      });
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
      EventUtils.synthesizeKey("KEY_Enter", {}, window);
    }
    await onLoad;

    if (component) {
      info("Check whether the component is in the page");
      Assert.ok(await isSelected(component), "There is expected component");
    }

    info("Do the second quick action in second tab");
    let secondTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: secondInput || firstInput,
    });
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
    EventUtils.synthesizeKey("KEY_Enter", {}, window);
    Assert.equal(
      gBrowser.selectedTab,
      firstTab,
      "Switched to the tab that is opening the about page"
    );
    Assert.equal(
      gBrowser.selectedBrowser.currentURI.spec,
      uri,
      "URI is not changed"
    );
    Assert.equal(gBrowser.tabs.length, 3, "Not opened a new tab");

    if (component) {
      info("Check whether the component is still in the page");
      Assert.ok(await isSelected(component), "There is expected component");
    }

    BrowserTestUtils.removeTab(secondTab);
    BrowserTestUtils.removeTab(firstTab);
  }
});

add_task(async function test_about_addons_pages() {
  let testData = [
    {
      cmd: "add-ons",
      testFun: async () => isSelected("button[name=discover]"),
    },
    {
      cmd: "plugins",
      testFun: async () => isSelected("button[name=plugin]"),
    },
    {
      cmd: "extensions",
      testFun: async () => isSelected("button[name=extension]"),
    },
    {
      cmd: "themes",
      testFun: async () => isSelected("button[name=theme]"),
    },
  ];

  info("Pick all actions related about:addons");
  let originalTab = gBrowser.selectedTab;
  for (const { cmd, testFun } of testData) {
    await BrowserTestUtils.openNewForegroundTab(gBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: cmd,
    });
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
    EventUtils.synthesizeKey("KEY_Enter", {}, window);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    Assert.ok(await testFun(), "The page content is correct");
  }
  Assert.equal(
    gBrowser.tabs.length,
    testData.length + 1,
    "Tab length is correct"
  );

  info("Pick all again");
  for (const { cmd, testFun } of testData) {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: cmd,
    });
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);
    EventUtils.synthesizeKey("KEY_Enter", {}, window);
    await BrowserTestUtils.waitForCondition(() => testFun());
    Assert.ok(true, "The tab correspondent action is selected");
  }
  Assert.equal(
    gBrowser.tabs.length,
    testData.length + 1,
    "Tab length is not changed"
  );

  for (const tab of gBrowser.tabs) {
    if (tab !== originalTab) {
      BrowserTestUtils.removeTab(tab);
    }
  }
});
