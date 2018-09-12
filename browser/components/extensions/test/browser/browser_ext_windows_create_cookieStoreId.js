/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function no_cookies_permission() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true],
  ]});

  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      await browser.test.assertRejects(
        browser.windows.create({cookieStoreId: "firefox-container-1"}),
        /No permission for cookieStoreId/,
        "cookieStoreId requires cookies permission");
      browser.test.sendMessage("done");
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});

add_task(async function invalid_cookieStoreId() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true],
  ]});

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["cookies"],
    },
    async background() {
      await browser.test.assertRejects(
        browser.windows.create({cookieStoreId: "not-firefox-container-1"}),
        /Illegal cookieStoreId/,
        "cookieStoreId must be valid");

      await browser.test.assertRejects(
        browser.windows.create({cookieStoreId: "firefox-private"}),
        /Illegal to set private cookieStoreId in a non-private window/,
        "cookieStoreId cannot be private in a non-private window");
      await browser.test.assertRejects(
        browser.windows.create({cookieStoreId: "firefox-default", incognito: true}),
        /Illegal to set non-private cookieStoreId in a private window/,
        "cookieStoreId cannot be non-private in an private window");

      await browser.test.assertRejects(
        browser.windows.create({cookieStoreId: "firefox-container-1", incognito: true}),
        /Illegal to set non-private cookieStoreId in a private window/,
        "cookieStoreId cannot be a container tab ID in a private window");

      browser.test.sendMessage("done");
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});

add_task(async function valid_cookieStoreId() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true],
  ]});

  const testCases = [{
    description: "no explicit URL",
    createParams: {
      cookieStoreId: "firefox-container-1",
    },
    expectedCookieStoreIds: [
      "firefox-container-1",
    ],
    expectedExecuteScriptResult: [
      // Default URL is about:newtab, and extensions cannot run scripts in it.
      "Missing host permission for the tab",
    ],
  }, {
    description: "one URL",
    createParams: {
      url: "about:blank",
      cookieStoreId: "firefox-container-1",
    },
    expectedCookieStoreIds: [
      "firefox-container-1",
    ],
    expectedExecuteScriptResult: [
      "about:blank - null",
    ],
  }, {
    description: "one URL in an array",
    createParams: {
      url: ["about:blank"],
      cookieStoreId: "firefox-container-1",
    },
    expectedCookieStoreIds: [
      "firefox-container-1",
    ],
    expectedExecuteScriptResult: [
      "about:blank - null",
    ],
  }, {
    description: "two URLs in an array",
    createParams: {
      url: ["about:blank", "about:blank"],
      cookieStoreId: "firefox-container-1",
    },
    expectedCookieStoreIds: [
      "firefox-container-1",
      "firefox-container-1",
    ],
    expectedExecuteScriptResult: [
      "about:blank - null",
      "about:blank - null",
    ],
  }];

  async function background(testCases) {
    async function executeScriptAndGetResult(tabId, attempts = 0) {
      try {
        return (await browser.tabs.executeScript(tabId, {
          matchAboutBlank: true,
          code: "`${document.URL} - ${origin}`",
        }))[0];
      } catch (e) {
        if (e.message === "No matching message handler") {
          if (++attempts < 10) {
            // The tabs.executeScript API does not wait for the tab to load
            // if the tab is "about:blank" ( bug 1397667 ):
            // https://searchfox.org/mozilla-central/rev/c3fef66a5b211ea8038c1c132706d02db408093a/browser/components/extensions/parent/ext-tabs.js#112
            // If the test called tabs.executeScript too soon, wait a little bit
            // and retry a couple of times before giving up.
            // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
            await new Promise(resolve => setTimeout(resolve, 100 * attempts));
            return executeScriptAndGetResult(tabId, attempts);
          }
          browser.test.log(`Warning: Cannot execute script after ${attempts} attempts.`);
        }
        return e.message;
      }
    }
    for (let {description, createParams, expectedCookieStoreIds, expectedExecuteScriptResult} of testCases) {
      let win = await browser.windows.create(createParams);

      browser.test.assertEq(expectedCookieStoreIds.length, win.tabs.length, "Expected number of tabs");

      for (let [i, expectedCookieStoreId] of Object.entries(expectedCookieStoreIds)) {
        browser.test.assertEq(expectedCookieStoreId, win.tabs[i].cookieStoreId, `expected cookieStoreId for tab ${i} (${description})`);
      }

      for (let [i, expectedResult] of Object.entries(expectedExecuteScriptResult)) {
        let result = await executeScriptAndGetResult(win.tabs[i].id);
        browser.test.assertEq(expectedResult, result, `expected executeScript result for tab ${i} (${description})`);
      }

      await browser.windows.remove(win.id);
    }
    browser.test.sendMessage("done");
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["cookies"],
    },
    background: `(${background})(${JSON.stringify(testCases)})`,
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});

add_task(async function cookieStoreId_and_tabId() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["privacy.userContext.enabled", true],
  ]});

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["cookies"],
    },
    async background() {
      for (let cookieStoreId of ["firefox-default", "firefox-container-1"]) {
        let {id: normalTabId} = await browser.tabs.create({cookieStoreId});

        await browser.test.assertRejects(
          browser.windows.create({cookieStoreId: "firefox-private", tabId: normalTabId}),
          /`cookieStoreId` must match the tab's cookieStoreId/,
          "Cannot use cookieStoreId for pre-existing tabs with a different cookieStoreId");

        let win = await browser.windows.create({cookieStoreId, tabId: normalTabId});
        browser.test.assertEq(cookieStoreId, win.tabs[0].cookieStoreId, "Adopted tab");
        await browser.windows.remove(win.id);
      }

      {
        let privateWindow = await browser.windows.create({incognito: true});
        let privateTabId = privateWindow.tabs[0].id;

        await browser.test.assertRejects(
          browser.windows.create({cookieStoreId: "firefox-default", tabId: privateTabId}),
          /`cookieStoreId` must match the tab's cookieStoreId/,
          "Cannot use cookieStoreId for pre-existing tab in a private window");
        let win = await browser.windows.create({cookieStoreId: "firefox-private", tabId: privateTabId});
        browser.test.assertEq("firefox-private", win.tabs[0].cookieStoreId, "Adopted private tab");
        await browser.windows.remove(win.id);

        await browser.test.assertRejects(
          browser.windows.remove(privateWindow.id),
          /Invalid window ID:/,
          "The original private window should have been closed when its only tab was adopted.");
      }

      browser.test.sendMessage("done");
    },
  });

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});
