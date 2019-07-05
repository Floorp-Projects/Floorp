/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function getExtension(page_action) {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      page_action,
    },
    background: function() {
      browser.test.onMessage.addListener(
        async ({ method, param, expect, msg }) => {
          let result = await browser.pageAction[method](param);
          if (expect !== undefined) {
            browser.test.assertEq(expect, result, msg);
          }
          browser.test.sendMessage("done");
        }
      );
    },
  });
}

async function sendMessage(ext, method, param, expect, msg) {
  ext.sendMessage({ method, param, expect, msg });
  await ext.awaitMessage("done");
}

let tests = [
  {
    name: "Test shown for all_urls",
    page_action: {
      show_matches: ["<all_urls>"],
    },
    shown: [true, true, false, false],
  },
  {
    name: "Test hide_matches overrides all_urls.",
    page_action: {
      show_matches: ["<all_urls>"],
      hide_matches: ["*://mochi.test/*"],
    },
    shown: [true, false, false, false],
  },
  {
    name: "Test shown only for show_matches.",
    page_action: {
      show_matches: ["*://mochi.test/*"],
    },
    shown: [false, true, false, false],
  },
];

// For some reason about:rights and about:about used to behave differently (maybe
// because only the latter is privileged?) so both should be tested.
let urls = [
  "http://example.com/",
  "http://mochi.test:8888/",
  "about:rights",
  "about:about",
];

function getId(tab) {
  let {
    Management: {
      global: { tabTracker },
    },
  } = ChromeUtils.import("resource://gre/modules/Extension.jsm", null);
  getId = tabTracker.getId.bind(tabTracker); // eslint-disable-line no-func-assign
  return getId(tab);
}

async function check(extension, tab, expected, msg) {
  await promiseAnimationFrame();
  let widgetId = makeWidgetId(extension.id);
  let pageActionId = BrowserPageActions.urlbarButtonNodeIDForActionID(widgetId);
  is(
    gBrowser.selectedTab,
    tab,
    `tab ${tab.linkedBrowser.currentURI.spec} is selected`
  );
  let button = document.getElementById(pageActionId);
  // Sometimes we're hidden, sometimes a parent is hidden via css (e.g. about pages)
  let hidden =
    button === null ||
    button.hidden ||
    window.getComputedStyle(button).display == "none";
  is(!hidden, expected, msg + " (computed)");
  await sendMessage(
    extension,
    "isShown",
    { tabId: getId(tab) },
    expected,
    msg + " (isShown)"
  );
}

add_task(async function test_pageAction_default_show_tabs() {
  info(
    "Check show_matches and hide_matches are respected when opening a new tab or switching to an existing tab."
  );
  let switchTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank",
    true,
    true
  );
  for (let [i, test] of tests.entries()) {
    info(`test ${i}: ${test.name}`);
    let extension = getExtension(test.page_action);
    await extension.startup();
    for (let [j, url] of urls.entries()) {
      let expected = test.shown[j];
      let msg = `test ${i} url ${j}: page action is ${
        expected ? "shown" : "hidden"
      } for ${url}`;

      info("Check new tab.");
      let tab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        url,
        true,
        true
      );
      await check(extension, tab, expected, msg + " (new)");

      info("Check switched tab.");
      await BrowserTestUtils.switchTab(gBrowser, switchTab);
      await BrowserTestUtils.switchTab(gBrowser, tab);
      await check(extension, tab, expected, msg + " (switched)");

      BrowserTestUtils.removeTab(tab);
    }
    await extension.unload();
  }
  BrowserTestUtils.removeTab(switchTab);
});

add_task(async function test_pageAction_default_show_install() {
  info(
    "Check show_matches and hide_matches are respected when installing the extension"
  );
  for (let [i, test] of tests.entries()) {
    info(`test ${i}: ${test.name}`);
    for (let expected of [true, false]) {
      let j = test.shown.indexOf(expected);
      if (j === -1) {
        continue;
      }
      let initialTab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        urls[j],
        true,
        true
      );
      let installTab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        urls[j],
        true,
        true
      );
      let extension = getExtension(test.page_action);
      await extension.startup();
      let msg = `test ${i} url ${j}: page action is ${
        expected ? "shown" : "hidden"
      } for ${urls[j]}`;
      await check(extension, installTab, expected, msg + " (active)");

      // initialTab has not been activated after installation, so we have not evaluated whether the page
      // action should be shown in it. Check that pageAction.isShown works anyways.
      await sendMessage(
        extension,
        "isShown",
        { tabId: getId(initialTab) },
        expected,
        msg + " (inactive)"
      );

      BrowserTestUtils.removeTab(initialTab);
      BrowserTestUtils.removeTab(installTab);
      await extension.unload();
    }
  }
});

add_task(async function test_pageAction_history() {
  info(
    "Check match patterns are reevaluated when using history.pushState or navigating"
  );
  let url1 = "http://example.com/";
  let url2 = url1 + "path/";
  let extension = getExtension({
    show_matches: [url1],
    hide_matches: [url2],
  });
  await extension.startup();
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    url1,
    true,
    true
  );
  await check(extension, tab, true, "page action is shown for " + url1);

  info("Use history.pushState to change the URL without navigating");
  await historyPushState(tab, url2);
  await check(extension, tab, false, "page action is hidden for " + url2);

  info("Use hide()");
  await sendMessage(extension, "hide", getId(tab));
  await check(extension, tab, false, "page action is still hidden");

  info("Use history.pushState to revert to first url");
  await historyPushState(tab, url1);
  await check(
    extension,
    tab,
    false,
    "hide() has more precedence than pattern matching"
  );

  info("Select another tab");
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    url1,
    true,
    true
  );

  info("Perform navigation in the old tab");
  await navigateTab(tab, url1);
  await sendMessage(
    extension,
    "isShown",
    { tabId: getId(tab) },
    true,
    "Navigating undoes hide(), even when the tab is not selected."
  );

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
  await extension.unload();
});

add_task(async function test_pageAction_all_urls() {
  info("Check <all_urls> is not allowed in hide_matches");
  let extension = getExtension({
    show_matches: ["*://mochi.test/*"],
    hide_matches: ["<all_urls>"],
  });
  let rejects = await extension.startup().then(() => false, () => true);
  is(rejects, true, "startup failed");
});

add_task(async function test_pageAction_restrictScheme_false() {
  info(
    "Check restricted origins are allowed in show_matches for privileged extensions"
  );
  let extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,
    manifest: {
      permissions: ["mozillaAddons", "tabs"],
      page_action: {
        show_matches: ["about:reader*"],
        hide_matches: ["*://*/*"],
      },
    },
    background: function() {
      browser.tabs.onUpdated.addListener(async (tabId, changeInfo) => {
        if (changeInfo.url && changeInfo.url.startsWith("about:reader")) {
          browser.test.sendMessage("readerModeEntered");
        }
      });

      browser.test.onMessage.addListener(async msg => {
        if (msg !== "enterReaderMode") {
          browser.test.fail(`Received unexpected test message: ${msg}`);
          return;
        }

        browser.tabs.toggleReaderMode();
      });
    },
  });

  async function expectPageAction(extension, tab, isShown) {
    await promiseAnimationFrame();
    let widgetId = makeWidgetId(extension.id);
    let pageActionId = BrowserPageActions.urlbarButtonNodeIDForActionID(
      widgetId
    );
    let iconEl = document.getElementById(pageActionId);

    if (isShown) {
      ok(iconEl && !iconEl.hasAttribute("disabled"), "pageAction is shown");
    } else {
      ok(
        iconEl == null || iconEl.getAttribute("disabled") == "true",
        "pageAction is hidden"
      );
    }
  }

  const baseUrl = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://example.com"
  );
  const url = `${baseUrl}/readerModeArticle.html`;
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    url,
    true,
    true
  );

  await extension.startup();

  await expectPageAction(extension, tab, false);

  extension.sendMessage("enterReaderMode");
  await extension.awaitMessage("readerModeEntered");

  await expectPageAction(extension, tab, true);

  BrowserTestUtils.removeTab(tab);

  await extension.unload();
});
