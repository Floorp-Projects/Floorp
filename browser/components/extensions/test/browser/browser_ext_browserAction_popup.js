/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function getBrowserAction(extension) {
  let {GlobalManager, browserActionFor} = Cu.import("resource://gre/modules/Extension.jsm", {});

  let ext = GlobalManager.extensionMap.get(extension.id);
  return browserActionFor(ext);
}

function* testInArea(area) {
  let scriptPage = url => `<html><head><meta charset="utf-8"><script src="${url}"></script></head><body>${url}</body></html>`;

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "background": {
        "page": "data/background.html",
      },
      "browser_action": {
        "default_popup": "popup-a.html",
        "browser_style": true,
      },
    },

    files: {
      "popup-a.html": scriptPage("popup-a.js"),
      "popup-a.js": function() {
        window.onload = () => {
          let color = window.getComputedStyle(document.body).color;
          browser.test.assertEq("rgb(34, 36, 38)", color);
          browser.runtime.sendMessage("from-popup-a");
        };
        browser.runtime.onMessage.addListener(msg => {
          if (msg == "close-popup-using-window.close") {
            window.close();
          }
        });
      },

      "data/popup-b.html": scriptPage("popup-b.js"),
      "data/popup-b.js": function() {
        browser.runtime.sendMessage("from-popup-b");
      },

      "data/background.html": scriptPage("background.js"),

      "data/background.js": function() {
        let sendClick;
        let tests = [
          () => {
            browser.test.log(`Click browser action, expect popup "a".`);
            sendClick({expectEvent: false, expectPopup: "a"});
          },
          () => {
            browser.test.log(`Click browser action again, expect popup "a".`);
            sendClick({expectEvent: false, expectPopup: "a"});
          },
          () => {
            browser.test.log(`Call triggerAction, expect popup "a" again. Leave popup open.`);
            sendClick({expectEvent: false, expectPopup: "a", leaveOpen: true}, "trigger-action");
          },
          () => {
            browser.test.log(`Call triggerAction again. Expect remaining popup closed.`);
            sendClick({expectEvent: false, expectPopup: null}, "trigger-action");
            browser.test.sendMessage("next-test", {waitUntilClosed: true});
          },
          () => {
            browser.test.log(`Call triggerAction again. Expect popup "a" again.`);
            sendClick({expectEvent: false, expectPopup: "a"}, "trigger-action");
          },
          () => {
            browser.test.log(`Change popup URL. Click browser action. Expect popup "b".`);
            browser.browserAction.setPopup({popup: "popup-b.html"});
            sendClick({expectEvent: false, expectPopup: "b"});
          },
          () => {
            browser.test.log(`Click browser action again. Expect popup "b" again.`);
            sendClick({expectEvent: false, expectPopup: "b"});
          },
          () => {
            browser.test.log(`Clear popup URL. Click browser action. Expect click event.`);
            browser.browserAction.setPopup({popup: ""});
            sendClick({expectEvent: true, expectPopup: null});
          },
          () => {
            browser.test.log(`Click browser action again. Expect another click event.`);
            sendClick({expectEvent: true, expectPopup: null});
          },
          () => {
            browser.test.log(`Call triggerAction. Expect click event.`);
            sendClick({expectEvent: true, expectPopup: null}, "trigger-action");
          },
          () => {
            browser.test.log(`Change popup URL. Click browser action. Expect popup "a", and leave open.`);
            browser.browserAction.setPopup({popup: "/popup-a.html"});
            sendClick({expectEvent: false, expectPopup: "a", leaveOpen: true});
          },
          () => {
            browser.test.log(`Call window.close(). Expect popup closed.`);
            browser.test.sendMessage("next-test", {closePopupUsingWindow: true});
          },
        ];

        let expect = {};
        sendClick = ({expectEvent, expectPopup, runNextTest, waitUntilClosed, leaveOpen}, message = "send-click") => {
          expect = {event: expectEvent, popup: expectPopup, runNextTest, waitUntilClosed, leaveOpen};
          browser.test.sendMessage(message);
        };

        browser.runtime.onMessage.addListener(msg => {
          if (msg == "close-popup-using-window.close") {
            return;
          } else if (expect.popup) {
            browser.test.assertEq(msg, `from-popup-${expect.popup}`,
                                  "expected popup opened");
          } else {
            browser.test.fail(`unexpected popup: ${msg}`);
          }

          expect.popup = null;
          browser.test.sendMessage("next-test", expect);
        });

        browser.browserAction.onClicked.addListener(() => {
          if (expect.event) {
            browser.test.succeed("expected click event received");
          } else {
            browser.test.fail("unexpected click event");
          }

          expect.event = false;
          browser.test.sendMessage("next-test", expect);
        });

        browser.test.onMessage.addListener((msg) => {
          if (msg == "close-popup-using-window.close") {
            browser.runtime.sendMessage("close-popup-using-window.close");
            return;
          }

          if (msg != "next-test") {
            browser.test.fail("Expecting 'next-test' message");
          }

          if (tests.length) {
            let test = tests.shift();
            test();
          } else {
            browser.test.notifyPass("browseraction-tests-done");
          }
        });

        browser.test.sendMessage("next-test");
      },
    },
  });

  extension.onMessage("send-click", () => {
    clickBrowserAction(extension);
  });

  extension.onMessage("trigger-action", () => {
    getBrowserAction(extension).triggerAction(window);
  });

  let widget;
  extension.onMessage("next-test", Task.async(function* (expecting = {}) {
    if (!widget) {
      widget = getBrowserActionWidget(extension);
      CustomizableUI.addWidgetToArea(widget.id, area);
    }
    if (expecting.waitUntilClosed) {
      let panel = getBrowserActionPopup(extension);
      if (panel && panel.state != "closed") {
        yield promisePopupHidden(panel);
      }
    } else if (expecting.closePopupUsingWindow) {
      let panel = getBrowserActionPopup(extension);
      ok(panel, "Expect panel to exist");
      yield promisePopupShown(panel);

      extension.sendMessage("close-popup-using-window.close");

      yield promisePopupHidden(panel);
      ok(true, "Panel is closed");
    } else if (!expecting.leaveOpen) {
      yield closeBrowserAction(extension);
    }

    extension.sendMessage("next-test");
  }));

  yield Promise.all([extension.startup(), extension.awaitFinish("browseraction-tests-done")]);

  yield extension.unload();

  let view = document.getElementById(widget.viewId);
  is(view, null, "browserAction view removed from document");
}

add_task(function* testBrowserActionInToolbar() {
  yield testInArea(CustomizableUI.AREA_NAVBAR);
});

add_task(function* testBrowserActionInPanel() {
  yield testInArea(CustomizableUI.AREA_PANEL);
});
