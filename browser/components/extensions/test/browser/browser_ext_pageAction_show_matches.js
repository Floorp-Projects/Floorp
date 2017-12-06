/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function getExtension(page_action) {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      page_action,
    },
  });
}

add_task(async function test_pageAction_default_show() {
  let tests = [
    {
      "name": "Test shown for all_urls",
      "page_action": {
        "show_matches": ["<all_urls>"],
      },
      "shown": [true, true, false],
    },
    {
      "name": "Test hide_matches overrides all_urls.",
      "page_action": {
        "show_matches": ["<all_urls>"],
        "hide_matches": ["*://mochi.test/*"],
      },
      "shown": [true, false, false],
    },
    {
      "name": "Test shown only for show_matches.",
      "page_action": {
        "show_matches": ["*://mochi.test/*"],
      },
      "shown": [false, true, false],
    },
  ];

  let tabs = [
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/", true, true),
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://mochi.test:8888/", true, true),
    await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:rights", true, true),
  ];

  for (let [i, test] of tests.entries()) {
    info(`test ${i}: ${test.name}`);
    let extension = getExtension(test.page_action);
    await extension.startup();

    let widgetId = makeWidgetId(extension.id);
    let pageActionId = BrowserPageActions.urlbarButtonNodeIDForActionID(widgetId);

    for (let [t, tab] of tabs.entries()) {
      await BrowserTestUtils.switchTab(gBrowser, tab);
      is(gBrowser.selectedTab, tab, `tab ${t} selected`);
      let button = document.getElementById(pageActionId);
      // Sometimes we're hidden, sometimes a parent is hidden via css (e.g. about pages)
      let hidden = button === null || button.hidden ||
        window.getComputedStyle(button).display == "none";
      is(!hidden, test.shown[t], `test ${i} tab ${t}: page action is ${test.shown[t] ? "shown" : "hidden"} for ${tab.linkedBrowser.currentURI.spec}`);
    }

    await extension.unload();
  }
  for (let tab of tabs) {
    await BrowserTestUtils.removeTab(tab);
  }
});
