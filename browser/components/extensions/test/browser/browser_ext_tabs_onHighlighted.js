/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function test_onHighlighted() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    background: async function() {
      async function expectHighlighted(fn, action) {
        let resolve;
        let promise = new Promise(r => {
          resolve = r;
        });
        let expected;
        let events = [];
        let listener = highlightInfo => {
          events.push(highlightInfo);
          if (expected && expected.length >= events.length) {
            resolve();
          }
        };
        browser.tabs.onHighlighted.addListener(listener);
        expected = (await fn()) || [];
        if (events.length < expected.length) {
          await promise;
        }
        let unexpected = events.splice(expected.length);
        browser.test.assertEq(
          JSON.stringify(expected),
          JSON.stringify(events),
          `Should get ${expected.length} expected onHighlighted events when ${action}`
        );
        if (unexpected.length) {
          browser.test.fail(
            `${unexpected.length} unexpected onHighlighted events when ${action}: ` +
              JSON.stringify(unexpected)
          );
        }
        browser.tabs.onHighlighted.removeListener(listener);
      }

      let [{ id, windowId }] = await browser.tabs.query({
        active: true,
        currentWindow: true,
      });
      let windows = [windowId];
      let tabs = [id];

      await expectHighlighted(async () => {
        let tab = await browser.tabs.create({
          active: true,
          url: "about:blank?1",
        });
        tabs.push(tab.id);
        return [{ tabIds: [tabs[1]], windowId: windows[0] }];
      }, "creating a new active tab");

      await expectHighlighted(async () => {
        await browser.tabs.update(tabs[0], { active: true });
        return [{ tabIds: [tabs[0]], windowId: windows[0] }];
      }, "selecting former tab");

      await expectHighlighted(async () => {
        await browser.tabs.highlight({ tabs: [0, 1] });
        return [{ tabIds: [tabs[0], tabs[1]], windowId: windows[0] }];
      }, "highlighting both tabs");

      await expectHighlighted(async () => {
        await browser.tabs.highlight({ tabs: [1, 0] });
        return [{ tabIds: [tabs[0], tabs[1]], windowId: windows[0] }];
      }, "highlighting same tabs but changing selected one");

      await expectHighlighted(async () => {
        let tab = await browser.tabs.create({
          active: false,
          url: "about:blank?2",
        });
        tabs.push(tab.id);
      }, "create a new inactive tab");

      await expectHighlighted(async () => {
        await browser.tabs.highlight({ tabs: [2, 0, 1] });
        return [{ tabIds: [tabs[0], tabs[1], tabs[2]], windowId: windows[0] }];
      }, "highlighting all tabs");

      await expectHighlighted(async () => {
        await browser.tabs.move(tabs[1], { index: 0 });
      }, "reordering tabs");

      await expectHighlighted(async () => {
        await browser.tabs.highlight({ tabs: [0] });
        return [{ tabIds: [tabs[1]], windowId: windows[0] }];
      }, "highlighting moved tab");

      await expectHighlighted(async () => {
        await browser.tabs.highlight({ tabs: [0] });
      }, "highlighting again");

      await expectHighlighted(async () => {
        await browser.tabs.highlight({ tabs: [2, 1, 0] });
        return [{ tabIds: [tabs[1], tabs[0], tabs[2]], windowId: windows[0] }];
      }, "highlighting all tabs");

      await expectHighlighted(async () => {
        await browser.tabs.highlight({ tabs: [2, 0, 1] });
      }, "highlighting same tabs with different order");

      await expectHighlighted(async () => {
        let window = await browser.windows.create({ tabId: tabs[2] });
        windows.push(window.id);
        // Bug 1481185: on Chrome it's [tabs[1], tabs[0]] instead of [tabs[0]]
        return [
          { tabIds: [tabs[0]], windowId: windows[0] },
          { tabIds: [tabs[2]], windowId: windows[1] },
        ];
      }, "moving selected tab into a new window");

      await browser.tabs.remove(tabs.slice(1));
      browser.test.notifyPass("test-finished");
    },
  });

  await extension.startup();
  await extension.awaitFinish("test-finished");
  await extension.unload();
});
