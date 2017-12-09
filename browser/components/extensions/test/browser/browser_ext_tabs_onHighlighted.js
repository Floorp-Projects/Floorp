/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

add_task(async function testTabEvents() {
  async function background() {
    /** The list of active tab ID's */
    let tabIds = [];

    /**
     * Stores the events that fire for each tab.
     *
     * events {
     *   tabId1: [event1, event2, ...],
     *   tabId2: [event1, event2, ...],
     * }
     */
    let events = {};

    browser.tabs.onActivated.addListener((info) => {
      if (info.tabId in events) {
        events[info.tabId].push("onActivated");
      } else {
        events[info.tabId] = ["onActivated"];
      }
    });

    browser.tabs.onCreated.addListener((info) => {
      if (info.id in events) {
        events[info.id].push("onCreated");
      } else {
        events[info.id] = ["onCreated"];
      }
    });

    browser.tabs.onHighlighted.addListener((info) => {
      if (info.tabIds[0] in events) {
        events[info.tabIds[0]].push("onHighlighted");
      } else {
        events[info.tabIds[0]] = ["onHighlighted"];
      }
    });

    /**
     * Asserts that the expected events are fired for the tab with id = tabId.
     * The events associated to the specified tab are removed after this check is made.
     *
     * @param {number} tabId
     * @param {Array<string>} expectedEvents
     */
    async function expectEvents(tabId, expectedEvents) {
      browser.test.log(`Expecting events: ${expectedEvents.join(", ")}`);

      // Wait up to 5000 ms for the expected number of events.
      for (let i = 0; i < 50 && (!events[tabId] || events[tabId].length < expectedEvents.length); i++) {
        await new Promise(resolve => setTimeout(resolve, 100));
      }

      browser.test.assertEq(expectedEvents.length, events[tabId].length,
                            `Got expected number of events for ${tabId}`);

      for (let [i, name] of expectedEvents.entries()) {
        browser.test.assertEq(name, i in events[tabId] && events[tabId][i],
                              `Got expected ${name} event`);
      }
      delete events[tabId];
    }

    /**
     * Opens a new tab and asserts that the correct events are fired.
     *
     * @param {number} windowId
     */
    async function openTab(windowId) {
      browser.test.assertEq(0, Object.keys(events).length,
                            "No events remaining before testing openTab.");

      let tab = await browser.tabs.create({windowId});

      tabIds.push(tab.id);
      browser.test.log(`Opened tab ${tab.id}`);

      await expectEvents(tab.id, [
        "onCreated",
        "onActivated",
        "onHighlighted",
      ]);
    }

    /**
     * Opens a new window and asserts that the correct events are fired.
     *
     * @param {Array} urls A list of urls for which to open tabs in the new window.
     */
    async function openWindow(urls) {
      browser.test.assertEq(0, Object.keys(events).length,
                            "No events remaining before testing openWindow.");

      let window = await browser.windows.create({url: urls});
      browser.test.log(`Opened new window ${window.id}`);

      for (let [i] of urls.entries()) {
        let tab = window.tabs[i];
        tabIds.push(tab.id);

        let expectedEvents = [
          "onCreated",
          "onActivated",
          "onHighlighted",
        ];
        if (i !== 0) {
          expectedEvents.splice(1);
        }
        await expectEvents(window.tabs[i].id, expectedEvents);
      }
    }

    /**
     * Highlights an existing tab and asserts that the correct events are fired.
     *
     * @param {number} tabId
     */
    async function highlightTab(tabId) {
      browser.test.assertEq(0, Object.keys(events).length,
                            "No events remaining before testing highlightTab.");

      browser.test.log(`Highlighting tab ${tabId}`);
      let tab = await browser.tabs.update(tabId, {active: true});

      browser.test.assertEq(tab.id, tabId, `Tab ${tab.id} highlighted`);

      await expectEvents(tab.id, [
        "onActivated",
        "onHighlighted",
      ]);
    }

    /**
     * The main entry point to the tests.
     */
    let tabs = await browser.tabs.query({active: true, currentWindow: true});

    let activeWindow = tabs[0].windowId;
    await Promise.all([
      openTab(activeWindow),
      openTab(activeWindow),
      openTab(activeWindow),
    ]);

    await Promise.all([
      highlightTab(tabIds[0]),
      highlightTab(tabIds[1]),
      highlightTab(tabIds[2]),
    ]);

    await Promise.all([
      openWindow(["http://example.com"]),
      openWindow(["http://example.com", "http://example.org"]),
      openWindow(["http://example.com", "http://example.org", "http://example.net"]),
    ]);

    browser.test.assertEq(0, Object.keys(events).length,
                          "No events remaining after tests.");

    await Promise.all(tabIds.map(id => browser.tabs.remove(id)));

    browser.test.notifyPass("tabs.highlight");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background,
  });

  await extension.startup();
  await extension.awaitFinish("tabs.highlight");
  await extension.unload();
});
