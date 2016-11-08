/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const SITE_SPECIFIC_PREF = "browser.zoom.siteSpecific";

add_task(function* () {
  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");
  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.net/");

  gBrowser.selectedTab = tab1;

  function background() {
    function promiseUpdated(tabId, attr) {
      return new Promise(resolve => {
        let onUpdated = (tabId_, changeInfo, tab) => {
          if (tabId == tabId_ && attr in changeInfo) {
            browser.tabs.onUpdated.removeListener(onUpdated);

            resolve({changeInfo, tab});
          }
        };
        browser.tabs.onUpdated.addListener(onUpdated);
      });
    }

    let deferred = {};
    browser.test.onMessage.addListener((message, msg, result) => {
      if (message == "msg-done" && deferred[msg]) {
        deferred[msg].resolve(result);
      }
    });

    let _id = 0;
    function msg(...args) {
      return new Promise((resolve, reject) => {
        let id = ++_id;
        deferred[id] = {resolve, reject};
        browser.test.sendMessage("msg", id, ...args);
      });
    }


    let zoomEvents = [];
    let eventPromises = [];
    browser.tabs.onZoomChange.addListener(info => {
      zoomEvents.push(info);
      if (eventPromises.length) {
        eventPromises.shift().resolve();
      }
    });

    let awaitZoom = (tabId, newValue) => {
      let listener;

      return new Promise(resolve => {
        listener = info => {
          if (info.tabId == tabId && info.newZoomFactor == newValue) {
            resolve();
          }
        };
        browser.tabs.onZoomChange.addListener(listener);

        browser.tabs.getZoom(tabId).then(zoomFactor => {
          if (zoomFactor == newValue) {
            resolve();
          }
        });
      }).then(() => {
        browser.tabs.onZoomChange.removeListener(listener);
      });
    };

    let checkZoom = (tabId, newValue, oldValue = null) => {
      let awaitEvent;
      if (oldValue != null && !zoomEvents.length) {
        awaitEvent = new Promise(resolve => {
          eventPromises.push({resolve});
        });
      }

      return Promise.all([
        browser.tabs.getZoom(tabId),
        msg("get-zoom", tabId),
        awaitEvent,
      ]).then(([apiZoom, realZoom]) => {
        browser.test.assertEq(newValue, apiZoom, `Got expected zoom value from API`);
        browser.test.assertEq(newValue, realZoom, `Got expected zoom value from parent`);

        if (oldValue != null) {
          let event = zoomEvents.shift();
          browser.test.assertEq(tabId, event.tabId, `Got expected zoom event tab ID`);
          browser.test.assertEq(newValue, event.newZoomFactor, `Got expected zoom event zoom factor`);
          browser.test.assertEq(oldValue, event.oldZoomFactor, `Got expected zoom event old zoom factor`);

          browser.test.assertEq(3, Object.keys(event.zoomSettings).length, `Zoom settings should have 3 keys`);
          browser.test.assertEq("automatic", event.zoomSettings.mode, `Mode should be "automatic"`);
          browser.test.assertEq("per-origin", event.zoomSettings.scope, `Scope should be "per-origin"`);
          browser.test.assertEq(1, event.zoomSettings.defaultZoomFactor, `Default zoom should be 1`);
        }
      });
    };

    let tabIds;

    browser.tabs.query({lastFocusedWindow: true}).then(tabs => {
      browser.test.assertEq(tabs.length, 3, "We have three tabs");

      tabIds = [tabs[1].id, tabs[2].id];

      return checkZoom(tabIds[0], 1);
    }).then(() => {
      return browser.tabs.setZoom(tabIds[0], 2);
    }).then(() => {
      return checkZoom(tabIds[0], 2, 1);
    }).then(() => {
      return browser.tabs.getZoomSettings(tabIds[0]);
    }).then(zoomSettings => {
      browser.test.assertEq(3, Object.keys(zoomSettings).length, `Zoom settings should have 3 keys`);
      browser.test.assertEq("automatic", zoomSettings.mode, `Mode should be "automatic"`);
      browser.test.assertEq("per-origin", zoomSettings.scope, `Scope should be "per-origin"`);
      browser.test.assertEq(1, zoomSettings.defaultZoomFactor, `Default zoom should be 1`);

      browser.test.log(`Switch to tab 2`);
      return browser.tabs.update(tabIds[1], {active: true});
    }).then(() => {
      return checkZoom(tabIds[1], 1);
    }).then(() => {
      browser.test.log(`Navigate tab 2 to origin of tab 1`);
      browser.tabs.update(tabIds[1], {url: "http://example.com"});

      return promiseUpdated(tabIds[1], "url");
    }).then(() => {
      return checkZoom(tabIds[1], 2, 1);
    }).then(() => {
      browser.test.log(`Update zoom in tab 2, expect changes in both tabs`);
      return browser.tabs.setZoom(tabIds[1], 1.5);
    }).then(() => {
      return checkZoom(tabIds[1], 1.5, 2);
    }).then(() => {
      browser.test.log(`Switch to tab 1, expect asynchronous zoom change just after the switch`);
      return Promise.all([
        awaitZoom(tabIds[0], 1.5),
        browser.tabs.update(tabIds[0], {active: true}),
      ]);
    }).then(() => {
      return checkZoom(tabIds[0], 1.5, 2);
    }).then(() => {
      browser.test.log("Set zoom to 0, expect it set to 1");
      return browser.tabs.setZoom(tabIds[0], 0);
    }).then(() => {
      return checkZoom(tabIds[0], 1, 1.5);
    }).then(() => {
      browser.test.log("Change zoom externally, expect changes reflected");
      return msg("enlarge");
    }).then(() => {
      return checkZoom(tabIds[0], 1.1, 1);
    }).then(() => {
      return Promise.all([
        browser.tabs.setZoom(tabIds[0], 0),
        browser.tabs.setZoom(tabIds[1], 0),
      ]);
    }).then(() => {
      return Promise.all([
        checkZoom(tabIds[0], 1, 1.1),
        checkZoom(tabIds[1], 1, 1.5),
      ]);
    }).then(() => {
      browser.test.log("Check that invalid zoom values throw an error");
      return browser.tabs.setZoom(tabIds[0], 42).then(
        () => {
          browser.test.fail("Expected an error");
        },
        error => {
          browser.test.assertTrue(error.message.includes("Zoom value 42 out of range"),
                                  "Got expected error");
        });
    }).then(() => {
      browser.test.log("Disable site-specific zoom, expect correct scope");
      return msg("site-specific", false);
    }).then(() => {
      return browser.tabs.getZoomSettings(tabIds[0]);
    }).then(zoomSettings => {
      browser.test.assertEq("per-tab", zoomSettings.scope, `Scope should be "per-tab"`);
    }).then(() => {
      return msg("site-specific", null);
    }).then(() => {
      browser.test.notifyPass("tab-zoom");
    }).catch(e => {
      browser.test.fail(`Error: ${e} :: ${e.stack}`);
      browser.test.notifyFail("tab-zoom");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background,
  });

  extension.onMessage("msg", (id, msg, ...args) => {
    let {Management: {global: {TabManager}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

    let resp;
    if (msg == "get-zoom") {
      let tab = TabManager.getTab(args[0]);
      resp = ZoomManager.getZoomForBrowser(tab.linkedBrowser);
    } else if (msg == "set-zoom") {
      let tab = TabManager.getTab(args[0]);
      ZoomManager.setZoomForBrowser(tab.linkedBrowser);
    } else if (msg == "enlarge") {
      FullZoom.enlarge();
    } else if (msg == "site-specific") {
      if (args[0] == null) {
        SpecialPowers.clearUserPref(SITE_SPECIFIC_PREF);
      } else {
        SpecialPowers.setBoolPref(SITE_SPECIFIC_PREF, args[0]);
      }
    }

    extension.sendMessage("msg-done", id, resp);
  });

  yield extension.startup();

  yield extension.awaitFinish("tab-zoom");

  yield extension.unload();

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
});
