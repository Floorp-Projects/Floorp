/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_create_options() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:robots"
  );
  gBrowser.selectedTab = tab;

  // TODO: Multiple windows.

  await SpecialPowers.pushPrefEnv({
    set: [
      // Using pre-loaded new tab pages interferes with onUpdated events.
      // It probably shouldn't.
      ["browser.newtab.preload", false],
      // Some test cases below load http and check the behavior of https-first.
      ["dom.security.https_first", true],
    ],
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],

      background: { page: "bg/background.html" },
    },

    files: {
      "bg/blank.html": `<html><head><meta charset="utf-8"></head></html>`,

      "bg/background.html": `<html><head>
        <meta charset="utf-8">
        <script src="background.js"></script>
      </head></html>`,

      "bg/background.js": function () {
        let activeTab;
        let activeWindow;

        function runTests() {
          const DEFAULTS = {
            index: 2,
            windowId: activeWindow,
            active: true,
            pinned: false,
            url: "about:newtab",
            // 'selected' is marked as unsupported in schema, so we've removed it.
            // For more details, see bug 1337509
            selected: undefined,
            mutedInfo: {
              muted: false,
              extensionId: undefined,
              reason: undefined,
            },
          };

          let tests = [
            {
              create: { url: "https://example.com/" },
              result: { url: "https://example.com/" },
            },
            {
              create: { url: "view-source:https://example.com/" },
              result: { url: "view-source:https://example.com/" },
            },
            {
              create: { url: "blank.html" },
              result: { url: browser.runtime.getURL("bg/blank.html") },
            },
            {
              create: { url: "https://example.com/", openInReaderMode: true },
              result: {
                url: `about:reader?url=${encodeURIComponent(
                  "https://example.com/"
                )}`,
              },
            },
            {
              create: {},
              result: { url: "about:newtab" },
            },
            {
              create: { active: false },
              result: { active: false },
            },
            {
              create: { active: true },
              result: { active: true },
            },
            {
              create: { pinned: true },
              result: { pinned: true, index: 0 },
            },
            {
              create: { pinned: true, active: true },
              result: { pinned: true, active: true, index: 0 },
            },
            {
              create: { pinned: true, active: false },
              result: { pinned: true, active: false, index: 0 },
            },
            {
              create: { index: 1 },
              result: { index: 1 },
            },
            {
              create: { index: 1, active: false },
              result: { index: 1, active: false },
            },
            {
              create: { windowId: activeWindow },
              result: { windowId: activeWindow },
            },
            {
              create: { index: 9999 },
              result: { index: 2 },
            },
            {
              // https-first redirects http to https.
              create: { url: "http://example.com/" },
              result: { url: "https://example.com/" },
            },
            {
              // https-first redirects http to https.
              create: { url: "view-source:http://example.com/" },
              result: { url: "view-source:https://example.com/" },
            },
            {
              // Despite https-first, the about:reader URL does not change.
              create: { url: "http://example.com/", openInReaderMode: true },
              result: {
                url: `about:reader?url=${encodeURIComponent(
                  "http://example.com/"
                )}`,
              },
            },
            {
              create: { muted: true },
              result: {
                mutedInfo: {
                  muted: true,
                  extensionId: browser.runtime.id,
                  reason: "extension",
                },
              },
            },
            {
              create: { muted: false },
              result: {
                mutedInfo: {
                  muted: false,
                  extensionId: undefined,
                  reason: undefined,
                },
              },
            },
          ];

          async function nextTest() {
            if (!tests.length) {
              browser.test.notifyPass("tabs.create");
              return;
            }

            let test = tests.shift();
            let expected = Object.assign({}, DEFAULTS, test.result);

            browser.test.log(
              `Testing tabs.create(${JSON.stringify(
                test.create
              )}), expecting ${JSON.stringify(test.result)}`
            );

            let updatedPromise = new Promise(resolve => {
              let onUpdated = (changedTabId, changed) => {
                if (changed.url) {
                  browser.tabs.onUpdated.removeListener(onUpdated);
                  resolve({ tabId: changedTabId, url: changed.url });
                }
              };
              browser.tabs.onUpdated.addListener(onUpdated);
            });

            let createdPromise = new Promise(resolve => {
              let onCreated = tab => {
                browser.test.assertTrue(
                  "id" in tab,
                  `Expected tabs.onCreated callback to receive tab object`
                );
                resolve();
              };
              browser.tabs.onCreated.addListener(onCreated);
            });

            let [tab] = await Promise.all([
              browser.tabs.create(test.create),
              createdPromise,
            ]);
            let tabId = tab.id;

            for (let key of Object.keys(expected)) {
              if (key === "url") {
                // FIXME: This doesn't get updated until later in the load cycle.
                continue;
              }

              if (key === "mutedInfo") {
                for (let key of Object.keys(expected.mutedInfo)) {
                  browser.test.assertEq(
                    expected.mutedInfo[key],
                    tab.mutedInfo[key],
                    `Expected value for tab.mutedInfo.${key}`
                  );
                }
              } else {
                browser.test.assertEq(
                  expected[key],
                  tab[key],
                  `Expected value for tab.${key}`
                );
              }
            }

            let updated = await updatedPromise;
            browser.test.assertEq(
              tabId,
              updated.tabId,
              `Expected value for tab.id`
            );
            browser.test.assertEq(
              expected.url,
              updated.url,
              `Expected value for tab.url`
            );

            await browser.tabs.remove(tabId);
            await browser.tabs.update(activeTab, { active: true });

            nextTest();
          }

          nextTest();
        }

        browser.tabs.query({ active: true, currentWindow: true }, tabs => {
          activeTab = tabs[0].id;
          activeWindow = tabs[0].windowId;

          runTests();
        });
      },
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.create");
  await extension.unload();

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_create_with_popup() {
  const extension = ExtensionTestUtils.loadExtension({
    async background() {
      let normalWin = await browser.windows.create();
      let lastFocusedNormalWin = await browser.windows.getLastFocused({});
      browser.test.assertEq(
        lastFocusedNormalWin.id,
        normalWin.id,
        "The normal window is the last focused window."
      );
      let popupWin = await browser.windows.create({ type: "popup" });
      let lastFocusedPopupWin = await browser.windows.getLastFocused({});
      browser.test.assertEq(
        lastFocusedPopupWin.id,
        popupWin.id,
        "The popup window is the last focused window."
      );
      let newtab = await browser.tabs.create({});
      browser.test.assertEq(
        normalWin.id,
        newtab.windowId,
        "New tab was created in last focused normal window."
      );
      await Promise.all([
        browser.windows.remove(normalWin.id),
        browser.windows.remove(popupWin.id),
      ]);
      browser.test.sendMessage("complete");
    },
  });

  await extension.startup();
  await extension.awaitMessage("complete");
  await extension.unload();
});
