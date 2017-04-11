/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* moveMultiple() {
  let tabs = [];
  for (let k of [1, 2, 3, 4]) {
    let tab = yield BrowserTestUtils.openNewForegroundTab(window.gBrowser, `http://example.com/?${k}`);
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
        // Start -> After first tab  -> After second tab
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

      browser.test.notifyPass("tabs.move");
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.move");
  yield extension.unload();

  for (let tab of tabs) {
    yield BrowserTestUtils.removeTab(tab);
  }
});
