/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testTabEvents() {
  function background() {
    let events = [];
    browser.tabs.onCreated.addListener(tab => {
      events.push({type: "onCreated", tab});
    });

    browser.tabs.onAttached.addListener((tabId, info) => {
      events.push(Object.assign({type: "onAttached", tabId}, info));
    });

    browser.tabs.onDetached.addListener((tabId, info) => {
      events.push(Object.assign({type: "onDetached", tabId}, info));
    });

    browser.tabs.onRemoved.addListener((tabId, info) => {
      events.push(Object.assign({type: "onRemoved", tabId}, info));
    });

    function expectEvents(names) {
      browser.test.log(`Expecting events: ${names.join(", ")}`);

      return new Promise(resolve => {
        setTimeout(resolve, 0);
      }).then(() => {
        browser.test.assertEq(names.length, events.length, "Got expected number of events");
        for (let [i, name] of names.entries()) {
          browser.test.assertEq(name, i in events && events[i].type,
                                `Got expected ${name} event`);
        }
        return events.splice(0);
      });
    }

    browser.test.log("Create second browser window");
    Promise.all([
      browser.windows.getCurrent(),
      browser.windows.create({}),
    ]).then(windows => {
      let windowId = windows[0].id;
      let otherWindowId = windows[1].id;
      let initialTab;

      return expectEvents(["onCreated"]).then(([created]) => {
        initialTab = created.tab;

        browser.test.log("Create tab in window 1");
        return browser.tabs.create({windowId});
      }).then(tab => {
        let oldIndex = tab.index;

        return expectEvents(["onCreated"]).then(([created]) => {
          browser.test.assertEq(tab.id, created.tab.id, "Got expected tab ID");

          browser.test.log("Move tab to window 2");
          return browser.tabs.move([tab.id], {windowId: otherWindowId, index: 0});
        }).then(() => {
          return expectEvents(["onDetached", "onAttached"]);
        }).then(([detached, attached]) => {
          browser.test.assertEq(oldIndex, detached.oldPosition, "Expected old index");
          browser.test.assertEq(windowId, detached.oldWindowId, "Expected old window ID");

          browser.test.assertEq(0, attached.newPosition, "Expected new index");
          browser.test.assertEq(otherWindowId, attached.newWindowId, "Expected new window ID");

          browser.test.log("Remove tab");
          return browser.tabs.remove(tab.id);
        }).then(() => {
          return expectEvents(["onRemoved"]);
        }).then(([removed]) => {
          browser.test.assertEq(tab.id, removed.tabId, "Expected removed tab ID");
          browser.test.assertEq(otherWindowId, removed.windowId, "Expected removed tab window ID");
          // Note: We want to test for the actual boolean value false here.
          browser.test.assertEq(false, removed.isWindowClosing, "Expected isWindowClosing value");

          browser.test.log("Close second window");
          return browser.windows.remove(otherWindowId);
        }).then(() => {
          return expectEvents(["onRemoved"]);
        }).then(([removed]) => {
          browser.test.assertEq(initialTab.id, removed.tabId, "Expected removed tab ID");
          browser.test.assertEq(otherWindowId, removed.windowId, "Expected removed tab window ID");
          browser.test.assertEq(true, removed.isWindowClosing, "Expected isWindowClosing value");
        }).then(() => {
          browser.test.notifyPass("tabs-events");
        });
      });
    }).catch(e => {
      try {
        browser.test.fail(`${e} :: ${e.stack}`);
      } catch (ex) {
        throw e;
      }
      browser.test.notifyFail("tabs-events");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background,
  });

  yield extension.startup();
  yield extension.awaitFinish("tabs-events");
  yield extension.unload();
});
