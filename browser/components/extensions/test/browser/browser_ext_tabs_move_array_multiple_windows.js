/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function moveMultipleWindows() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { permissions: ["tabs"] },
    background: async function () {
      let numToId = new Map();
      let idToNum = new Map();
      let windowToInitialTabs = new Map();

      async function createWindow(nums) {
        let window = await browser.windows.create({
          url: nums.map(k => `https://example.com/?${k}`),
        });
        let tabIds = window.tabs.map(tab => tab.id);
        windowToInitialTabs.set(window.id, tabIds);
        for (let i = 0; i < nums.length; ++i) {
          numToId.set(nums[i], tabIds[i]);
          idToNum.set(tabIds[i], nums[i]);
        }
        return window.id;
      }

      let win1 = await createWindow([0, 1, 2, 3, 4]);
      let win2 = await createWindow([5, 6, 7, 8, 9]);

      async function getNums(windowId) {
        let tabs = await browser.tabs.query({ windowId });
        return tabs.map(tab => idToNum.get(tab.id));
      }

      async function check(msg, expected) {
        let nums1 = getNums(win1);
        let nums2 = getNums(win2);
        browser.test.assertEq(
          JSON.stringify(expected),
          JSON.stringify({ win1: await nums1, win2: await nums2 }),
          `Check ${msg}`
        );
      }

      async function reset() {
        for (let [windowId, tabIds] of windowToInitialTabs) {
          await browser.tabs.move(tabIds, { index: 0, windowId });
        }
      }

      async function move(nums, params) {
        await browser.tabs.move(
          nums.map(k => numToId.get(k)),
          params
        );
      }

      let tests = [
        {
          move: [1, 6],
          params: { index: 0 },
          result: { win1: [1, 0, 2, 3, 4], win2: [6, 5, 7, 8, 9] },
        },
        {
          move: [6, 1],
          params: { index: 0 },
          result: { win1: [1, 0, 2, 3, 4], win2: [6, 5, 7, 8, 9] },
        },
        {
          move: [1, 6],
          params: { index: 0, windowId: win2 },
          result: { win1: [0, 2, 3, 4], win2: [1, 6, 5, 7, 8, 9] },
        },
        {
          move: [6, 1],
          params: { index: 0, windowId: win2 },
          result: { win1: [0, 2, 3, 4], win2: [6, 1, 5, 7, 8, 9] },
        },
        {
          move: [1, 6],
          params: { index: -1 },
          result: { win1: [0, 2, 3, 4, 1], win2: [5, 7, 8, 9, 6] },
        },
        {
          move: [6, 1],
          params: { index: -1 },
          result: { win1: [0, 2, 3, 4, 1], win2: [5, 7, 8, 9, 6] },
        },
        {
          move: [1, 6],
          params: { index: -1, windowId: win2 },
          result: { win1: [0, 2, 3, 4], win2: [5, 7, 8, 9, 1, 6] },
        },
        {
          move: [6, 1],
          params: { index: -1, windowId: win2 },
          result: { win1: [0, 2, 3, 4], win2: [5, 7, 8, 9, 6, 1] },
        },
        {
          move: [2, 1, 7, 6],
          params: { index: 3 },
          result: { win1: [0, 3, 2, 1, 4], win2: [5, 8, 7, 6, 9] },
        },
        {
          move: [1, 2, 3, 4],
          params: { index: 0, windowId: win2 },
          result: { win1: [0], win2: [1, 2, 3, 4, 5, 6, 7, 8, 9] },
        },
        {
          move: [0, 1, 2, 3],
          params: { index: 5, windowId: win2 },
          result: { win1: [4], win2: [5, 6, 7, 8, 9, 0, 1, 2, 3] },
        },
        {
          move: [1, 2, 3, 4, 5, 6, 7, 8, 9],
          params: { index: 0, windowId: win2 },
          result: { win1: [0], win2: [1, 2, 3, 4, 5, 6, 7, 8, 9] },
        },
        {
          move: [5, 6, 7, 8, 9, 0, 1, 2, 3],
          params: { index: 0, windowId: win2 },
          result: { win1: [4], win2: [5, 6, 7, 8, 9, 0, 1, 2, 3] },
        },
        {
          move: [5, 1, 6, 2, 7, 3, 8, 4, 9],
          params: { index: 0, windowId: win2 },
          result: { win1: [0], win2: [5, 1, 6, 2, 7, 3, 8, 4, 9] },
        },
        {
          move: [5, 1, 6, 2, 7, 3, 8, 4, 9],
          params: { index: 1, windowId: win2 },
          result: { win1: [0], win2: [5, 1, 6, 2, 7, 3, 8, 4, 9] },
        },
        {
          move: [5, 1, 6, 2, 7, 3, 8, 4, 9],
          params: { index: 999, windowId: win2 },
          result: { win1: [0], win2: [5, 1, 6, 2, 7, 3, 8, 4, 9] },
        },
      ];

      const initial = { win1: [0, 1, 2, 3, 4], win2: [5, 6, 7, 8, 9] };
      await check("initial", initial);
      for (let test of tests) {
        browser.test.log(JSON.stringify(test));
        await move(test.move, test.params);
        await check("move", test.result);
        await reset();
        await check("reset", initial);
      }

      await browser.windows.remove(win1);
      await browser.windows.remove(win2);
      browser.test.notifyPass("tabs.move");
    },
  });

  await extension.startup();
  await extension.awaitFinish("tabs.move");
  await extension.unload();
});
