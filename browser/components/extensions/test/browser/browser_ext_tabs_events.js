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

    browser.tabs.onMoved.addListener((tabId, info) => {
      events.push(Object.assign({type: "onMoved", tabId}, info));
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
    let windowId;
    Promise.all([
      browser.windows.getCurrent(),
      browser.windows.create({url: "about:blank"}),
    ]).then(windows => {
      windowId = windows[0].id;
      let otherWindowId = windows[1].id;
      let initialTab;

      return expectEvents(["onCreated"]).then(([created]) => {
        initialTab = created.tab;

        browser.test.log("Create tab in window 1");
        return browser.tabs.create({windowId, index: 0, url: "about:blank"});
      }).then(tab => {
        let oldIndex = tab.index;
        browser.test.assertEq(0, oldIndex, "Tab has the expected index");

        return expectEvents(["onCreated"]).then(([created]) => {
          browser.test.assertEq(tab.id, created.tab.id, "Got expected tab ID");
          browser.test.assertEq(oldIndex, created.tab.index, "Got expected tab index");

          browser.test.log("Move tab to window 2");
          return browser.tabs.move([tab.id], {windowId: otherWindowId, index: 0});
        }).then(() => {
          return expectEvents(["onDetached", "onAttached"]);
        }).then(([detached, attached]) => {
          browser.test.assertEq(oldIndex, detached.oldPosition, "Expected old index");
          browser.test.assertEq(windowId, detached.oldWindowId, "Expected old window ID");

          browser.test.assertEq(0, attached.newPosition, "Expected new index");
          browser.test.assertEq(otherWindowId, attached.newWindowId, "Expected new window ID");

          browser.test.log("Move tab within the same window");
          return browser.tabs.move([tab.id], {index: 1});
        }).then(([moved]) => {
          browser.test.assertEq(1, moved.index, "Expected new index");

          return expectEvents(["onMoved"]);
        }).then(([moved]) => {
          browser.test.assertEq(tab.id, moved.tabId, "Expected tab ID");
          browser.test.assertEq(0, moved.fromIndex, "Expected old index");
          browser.test.assertEq(1, moved.toIndex, "Expected new index");
          browser.test.assertEq(otherWindowId, moved.windowId, "Expected window ID");

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
        });
      });
    }).then(() => {
      browser.test.log("Create additional tab in window 1");
      return browser.tabs.create({windowId, url: "about:blank"});
    }).then(tab => {
      return expectEvents(["onCreated"]).then(() => {
        browser.test.log("Create a new window, adopting the new tab");

        // We have to explicitly wait for the event here, since its timing is
        // not predictable.
        let promiseAttached = new Promise(resolve => {
          browser.tabs.onAttached.addListener(function listener(tabId) {
            browser.tabs.onAttached.removeListener(listener);
            resolve();
          });
        });

        return Promise.all([
          browser.windows.create({tabId: tab.id}),
          promiseAttached,
        ]);
      }).then(([window]) => {
        return expectEvents(["onDetached", "onAttached"]).then(([detached, attached]) => {
          browser.test.assertEq(tab.id, detached.tabId, "Expected onDetached tab ID");

          browser.test.assertEq(tab.id, attached.tabId, "Expected onAttached tab ID");
          browser.test.assertEq(0, attached.newPosition, "Expected onAttached new index");
          browser.test.assertEq(window.id, attached.newWindowId,
                                "Expected onAttached new window id");

          browser.test.log("Close the new window");
          return browser.windows.remove(window.id);
        });
      });
    }).then(() => {
      browser.test.notifyPass("tabs-events");
    }).catch(e => {
      browser.test.fail(`${e} :: ${e.stack}`);
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

add_task(function* testTabEventsSize() {
  function background() {
    function sendSizeMessages(tab, type) {
      browser.test.sendMessage(`${type}-dims`, {width: tab.width, height: tab.height});
    }

    browser.tabs.onCreated.addListener(tab => {
      sendSizeMessages(tab, "on-created");
    });

    browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
      if (tab.status == "complete") {
        sendSizeMessages(tab, "on-updated");
      }
    });

    browser.test.onMessage.addListener((msg, arg) => {
      if (msg === "create-tab") {
        browser.tabs.create({url: "http://example.com/"}).then(tab => {
          sendSizeMessages(tab, "create");
          browser.test.sendMessage("created-tab-id", tab.id);
        });
      } else if (msg === "update-tab") {
        browser.tabs.update(arg, {url: "http://example.org/"}).then(tab => {
          sendSizeMessages(tab, "update");
        });
      } else if (msg === "remove-tab") {
        browser.tabs.remove(arg);
        browser.test.sendMessage("tab-removed");
      }
    });

    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },
    background,
  });

  const RESOLUTION_PREF = "layout.css.devPixelsPerPx";
  registerCleanupFunction(() => {
    SpecialPowers.clearUserPref(RESOLUTION_PREF);
  });

  function checkDimensions(dims, type) {
    is(dims.width, gBrowser.selectedBrowser.clientWidth, `tab from ${type} reports expected width`);
    is(dims.height, gBrowser.selectedBrowser.clientHeight, `tab from ${type} reports expected height`);
  }

  yield Promise.all([extension.startup(), extension.awaitMessage("ready")]);

  for (let resolution of [2, 1]) {
    SpecialPowers.setCharPref(RESOLUTION_PREF, String(resolution));
    is(window.devicePixelRatio, resolution, "window has the required resolution");

    extension.sendMessage("create-tab");
    let tabId = yield extension.awaitMessage("created-tab-id");

    checkDimensions(yield extension.awaitMessage("create-dims"), "create");
    checkDimensions(yield extension.awaitMessage("on-created-dims"), "onCreated");

    extension.sendMessage("update-tab", tabId);

    checkDimensions(yield extension.awaitMessage("update-dims"), "update");
    checkDimensions(yield extension.awaitMessage("on-updated-dims"), "onUpdated");

    extension.sendMessage("remove-tab", tabId);
    yield extension.awaitMessage("tab-removed");
  }

  yield extension.unload();
  SpecialPowers.clearUserPref(RESOLUTION_PREF);
});

add_task(function* testTabRemovalEvent() {
  function background() {
    let removalTabId;

    function awaitLoad(tabId) {
      return new Promise(resolve => {
        browser.tabs.onUpdated.addListener(function listener(tabId_, changed, tab) {
          if (tabId == tabId_ && changed.status == "complete") {
            browser.tabs.onUpdated.removeListener(listener);
            resolve();
          }
        });
      });
    }

    chrome.tabs.onRemoved.addListener((tabId, info) => {
      browser.test.log("Make sure the removed tab is not available in the tabs.query callback.");
      chrome.tabs.query({}, tabs => {
        for (let tab of tabs) {
          browser.test.assertTrue(tab.id != tabId, "Tab query should not include removed tabId");
        }
        browser.test.notifyPass("tabs-events");
      });
    });

    let url = "http://example.com/browser/browser/components/extensions/test/browser/context.html";
    browser.tabs.create({url: url})
    .then(tab => {
      removalTabId = tab.id;
      return awaitLoad(tab.id);
    }).then(() => {
      return browser.tabs.remove(removalTabId);
    }).catch(e => {
      browser.test.fail(`${e} :: ${e.stack}`);
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
