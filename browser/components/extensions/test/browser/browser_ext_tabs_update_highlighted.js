/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_update_highlighted() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs"],
    },

    background: async function() {
      const trackedEvents = ["onActivated", "onHighlighted"];
      async function expectResults(fn, action) {
        let resolve;
        let promise = new Promise(r => {
          resolve = r;
        });
        let expected;
        let events = [];
        let listeners = {};
        for (let trackedEvent of trackedEvents) {
          listeners[trackedEvent] = data => {
            events.push([trackedEvent, data]);
            if (expected && expected.length >= events.length) {
              resolve();
            }
          };
          browser.tabs[trackedEvent].addListener(listeners[trackedEvent]);
        }
        let {
          events: expectedEvents,
          highlighted: expectedHighlighted,
          active: expectedActive,
        } = await fn();
        if (events.length < expectedEvents.length) {
          await promise;
        }
        let [{ id: active }] = await browser.tabs.query({ active: true });
        browser.test.assertEq(
          expectedActive,
          active,
          `The expected tab is active when ${action}`
        );
        let highlighted = (await browser.tabs.query({ highlighted: true })).map(
          ({ id }) => id
        );
        browser.test.assertEq(
          JSON.stringify(expectedHighlighted),
          JSON.stringify(highlighted),
          `The expected tabs are highlighted when ${action}`
        );
        let unexpectedEvents = events.splice(expectedEvents.length);
        browser.test.assertEq(
          JSON.stringify(expectedEvents),
          JSON.stringify(events),
          `Should get expected events when ${action}`
        );
        if (unexpectedEvents.length) {
          browser.test.fail(
            `${unexpectedEvents.length} unexpected events when ${action}: ` +
              JSON.stringify(unexpectedEvents)
          );
        }
        for (let trackedEvent of trackedEvents) {
          browser.tabs[trackedEvent].removeListener(listeners[trackedEvent]);
        }
      }

      let { id: windowId } = await browser.windows.getCurrent();
      let { id: tab1 } = await browser.tabs.create({ url: "about:blank?1" });
      let { id: tab2 } = await browser.tabs.create({
        url: "about:blank?2",
        active: true,
      });

      await expectResults(async () => {
        await browser.tabs.update(tab2, { highlighted: true });
        return { active: tab2, highlighted: [tab2], events: [] };
      }, "highlighting active tab");

      await expectResults(async () => {
        await browser.tabs.update(tab2, { highlighted: false });
        return { active: tab2, highlighted: [tab2], events: [] };
      }, "unhighlighting active tab with no multiselection");

      await expectResults(async () => {
        await browser.tabs.update(tab1, { highlighted: true });
        return {
          active: tab1,
          highlighted: [tab1, tab2],
          events: [
            ["onActivated", { tabId: tab1, previousTabId: tab2, windowId }],
            ["onHighlighted", { tabIds: [tab1, tab2], windowId }],
          ],
        };
      }, "highlighting non-highlighted tab");

      await expectResults(async () => {
        await browser.tabs.update(tab2, { highlighted: true });
        return { active: tab1, highlighted: [tab1, tab2], events: [] };
      }, "highlighting inactive highlighted tab");

      await expectResults(async () => {
        await browser.tabs.update(tab1, { highlighted: false });
        return {
          active: tab2,
          highlighted: [tab2],
          events: [
            ["onActivated", { tabId: tab2, previousTabId: tab1, windowId }],
            ["onHighlighted", { tabIds: [tab2], windowId }],
          ],
        };
      }, "unhighlighting active tab with multiselection");

      await expectResults(async () => {
        await browser.tabs.update(tab1, { highlighted: true });
        return {
          active: tab1,
          highlighted: [tab1, tab2],
          events: [
            ["onActivated", { tabId: tab1, previousTabId: tab2, windowId }],
            ["onHighlighted", { tabIds: [tab1, tab2], windowId }],
          ],
        };
      }, "highlighting non-highlighted tab");

      await expectResults(async () => {
        await browser.tabs.update(tab2, { highlighted: false });
        return {
          active: tab1,
          highlighted: [tab1],
          events: [["onHighlighted", { tabIds: [tab1], windowId }]],
        };
      }, "unhighlighting inactive highlighted tab");

      await expectResults(async () => {
        await browser.tabs.update(tab2, { highlighted: true, active: false });
        return {
          active: tab1,
          highlighted: [tab1, tab2],
          events: [["onHighlighted", { tabIds: [tab1, tab2], windowId }]],
        };
      }, "highlighting without activating non-highlighted tab");

      await expectResults(async () => {
        await browser.tabs.update(tab2, { highlighted: true, active: true });
        return {
          active: tab2,
          highlighted: [tab2],
          events: [
            ["onActivated", { tabId: tab2, previousTabId: tab1, windowId }],
            ["onHighlighted", { tabIds: [tab2], windowId }],
          ],
        };
      }, "highlighting and activating inactive highlighted tab");

      await expectResults(async () => {
        await browser.tabs.update(tab1, { active: true, highlighted: true });
        return {
          active: tab1,
          highlighted: [tab1],
          events: [
            ["onActivated", { tabId: tab1, previousTabId: tab2, windowId }],
            ["onHighlighted", { tabIds: [tab1], windowId }],
          ],
        };
      }, "highlighting and activating non-highlighted tab");

      await browser.tabs.remove([tab1, tab2]);
      browser.test.notifyPass("test-finished");
    },
  });

  await extension.startup();
  await extension.awaitFinish("test-finished");
  await extension.unload();
});
