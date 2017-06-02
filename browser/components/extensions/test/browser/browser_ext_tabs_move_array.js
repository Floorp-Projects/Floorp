/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function moveMultiple() {
  let tabs = [];
  for (let k of [1, 2, 3, 4]) {
    let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser, `http://example.com/?${k}`);
    tabs.push(tab);
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {permissions: ["tabs"]},
    background: async function() {
      function num(url) {
        return parseInt(url.slice(-1), 10);
      }

      async function check(expected) {
        let tabs = await browser.tabs.query({url: "http://example.com/*"});
        let endings = tabs.map(tab => num(tab.url));
        browser.test.assertTrue(
          expected.every((v, i) => v === endings[i]),
          `Tab order should be ${expected}, got ${endings}.`
        );
      }

      async function reset() {
        let tabs = await browser.tabs.query({url: "http://example.com/*"});
        await browser.tabs.move(
            tabs.sort((a, b) => (num(a.url) - num(b.url))).map(tab => tab.id),
            {index: 0}
        );
      }

      async function move(moveIndexes, moveTo) {
        let tabs = await browser.tabs.query({url: "http://example.com/*"});
        await browser.tabs.move(
          moveIndexes.map(e => tabs[e - 1].id),
          {index: moveTo});
      }

      let tests = [
        {"move": [2], "index": 0, "result": [2, 1, 3, 4]},
        {"move": [2], "index": -1, "result": [1, 3, 4, 2]},
        // Start -> After first tab  -> After second tab
        {"move": [4, 3], "index":  0, "result": [4, 3, 1, 2]},
        // [1, 2, 3, 4] -> [1, 4, 2, 3] -> [1, 4, 3, 2]
        {"move": [4, 3], "index":  1, "result": [1, 4, 3, 2]},
        // [1, 2, 3, 4] -> [2, 3, 1, 4] -> [3, 1, 2, 4]
        {"move": [1, 2], "index":  2, "result": [3, 1, 2, 4]},
        // [1, 2, 3, 4] -> [1, 2, 4, 3] -> [2, 4, 1, 3]
        {"move": [4, 1], "index":  2, "result": [2, 4, 1, 3]},
        // [1, 2, 3, 4] -> [2, 3, 1, 4] -> [2, 3, 1, 4]
        {"move": [1, 4], "index":  2, "result": [2, 3, 1, 4]},
      ];

      for (let test of tests) {
        await reset();
        await move(test.move, test.index);
        await check(test.result);
      }

      let firstId = (await browser.tabs.query({url: "http://example.com/*"}))[0].id;
      // Assuming that tab.id of 12345 does not exist.
      await browser.test.assertRejects(
        browser.tabs.move([firstId, 12345], {index: -1}),
        /Invalid tab/,
        "Should receive invalid tab error");
      // The first argument got moved, the second on failed.
      await check([2, 3, 1, 4]);

      browser.test.notifyPass("tabs.move");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.move");
  await extension.unload();

  for (let tab of tabs) {
    await BrowserTestUtils.removeTab(tab);
  }
});
