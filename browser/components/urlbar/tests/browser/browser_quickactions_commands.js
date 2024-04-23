/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test QuickActions.
 */

"use strict";

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quickactions.enabled", true],
      ["browser.urlbar.secondaryActions.featureGate", true],
    ],
  });
});

let COMMANDS_TESTS = [
  {
    cmd: "add-ons",
    uri: "about:addons",
    testFun: async () => isSelected("button[name=discover]"),
  },
  {
    cmd: "plugins",
    uri: "about:addons",
    testFun: async () => isSelected("button[name=plugin]"),
  },
  {
    cmd: "extensions",
    uri: "about:addons",
    testFun: async () => isSelected("button[name=extension]"),
  },
  {
    cmd: "themes",
    uri: "about:addons",
    testFun: async () => isSelected("button[name=theme]"),
  },
  {
    cmd: "add-ons",
    setup: async () => {
      const onLoad = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        "https://example.com/"
      );
      BrowserTestUtils.startLoadingURIString(
        gBrowser.selectedBrowser,
        "https://example.com/"
      );
      await onLoad;
    },
    uri: "about:addons",
    isNewTab: true,
    testFun: async () => isSelected("button[name=discover]"),
  },
  {
    cmd: "plugins",
    setup: async () => {
      const onLoad = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        "https://example.com/"
      );
      BrowserTestUtils.startLoadingURIString(
        gBrowser.selectedBrowser,
        "https://example.com/"
      );
      await onLoad;
    },
    uri: "about:addons",
    isNewTab: true,
    testFun: async () => isSelected("button[name=plugin]"),
  },
  {
    cmd: "extensions",
    setup: async () => {
      const onLoad = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        "https://example.com/"
      );
      BrowserTestUtils.startLoadingURIString(
        gBrowser.selectedBrowser,
        "https://example.com/"
      );
      await onLoad;
    },
    uri: "about:addons",
    isNewTab: true,
    testFun: async () => isSelected("button[name=extension]"),
  },
  {
    cmd: "themes",
    setup: async () => {
      const onLoad = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser,
        false,
        "https://example.com/"
      );
      BrowserTestUtils.startLoadingURIString(
        gBrowser.selectedBrowser,
        "https://example.com/"
      );
      await onLoad;
    },
    uri: "about:addons",
    isNewTab: true,
    testFun: async () => isSelected("button[name=theme]"),
  },
];

let isSelected = async selector =>
  SpecialPowers.spawn(gBrowser.selectedBrowser, [selector], arg => {
    return ContentTaskUtils.waitForCondition(() =>
      content.document.querySelector(arg)?.hasAttribute("selected")
    );
  });

add_task(async function test_pages() {
  for (const { cmd, uri, setup, isNewTab, testFun } of COMMANDS_TESTS) {
    info(`Testing ${cmd} command is triggered`);
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

    if (setup) {
      info("Setup");
      await setup();
    }

    let onLoad = isNewTab
      ? BrowserTestUtils.waitForNewTab(gBrowser, uri, true)
      : BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, uri);

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: cmd,
    });
    EventUtils.synthesizeKey("KEY_Tab", {}, window);
    EventUtils.synthesizeKey("KEY_Enter", {}, window);

    const newTab = await onLoad;

    Assert.ok(
      await testFun(),
      `The command "${cmd}" passed completed its test`
    );

    if (isNewTab) {
      await BrowserTestUtils.removeTab(newTab);
    }
    await BrowserTestUtils.removeTab(tab);
  }
});
