/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testTabEvents() {
  function background() {
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
     * @returns {Promise}
     */
    function expectEvents(tabId, expectedEvents) {
      browser.test.log(`Expecting events: ${expectedEvents.join(", ")}`);

      return new Promise(resolve => {
        setTimeout(resolve, 0);
      }).then(() => {
        browser.test.assertEq(expectedEvents.length, events[tabId].length,
         `Got expected number of events for ${tabId}`);
        for (let [i, name] of expectedEvents.entries()) {
          browser.test.assertEq(name, i in events[tabId] && events[tabId][i],
                                `Got expected ${name} event`);
        }
        delete events[tabId];
      });
    }

    /**
     * Opens a new tab and asserts that the correct events are fired.
     *
     * @param {number} windowId
     * @returns {Promise}
     */
    function openTab(windowId) {
      return browser.tabs.create({windowId}).then(tab => {
        tabIds.push(tab.id);
        browser.test.log(`Opened tab ${tab.id}`);
        return expectEvents(tab.id, [
          "onActivated",
          "onHighlighted",
        ]);
      });
    }

    /**
     * Highlights an existing tab and asserts that the correct events are fired.
     *
     * @param {number} tabId
     * @returns {Promise}
     */
    function highlightTab(tabId) {
      browser.test.log(`Highlighting tab ${tabId}`);
      return browser.tabs.update(tabId, {active: true}).then(tab => {
        browser.test.assertEq(tab.id, tabId, `Tab ${tab.id} highlighted`);
        return expectEvents(tab.id, [
          "onActivated",
          "onHighlighted",
        ]);
      });
    }

    /**
     * The main entry point to the tests.
     */
    browser.tabs.query({active: true, currentWindow: true}, tabs => {
      let activeWindow = tabs[0].windowId;
      Promise.all([
        openTab(activeWindow),
        openTab(activeWindow),
        openTab(activeWindow),
      ]).then(() => {
        return Promise.all([
          highlightTab(tabIds[0]),
          highlightTab(tabIds[1]),
          highlightTab(tabIds[2]),
        ]);
      }).then(() => {
        return Promise.all(tabIds.map(id => browser.tabs.remove(id)));
      }).then(() => {
        browser.test.notifyPass("tabs.highlight");
      });
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background,
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs.highlight");
  yield extension.unload();
});
