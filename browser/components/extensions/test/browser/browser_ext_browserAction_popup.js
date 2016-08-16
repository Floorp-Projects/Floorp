/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

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
          if (msg == "close-popup") {
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
            sendClick({expectEvent: false, expectPopup: "a"});
          },
          () => {
            sendClick({expectEvent: false, expectPopup: "a"});
          },
          () => {
            browser.browserAction.setPopup({popup: "popup-b.html"});
            sendClick({expectEvent: false, expectPopup: "b"});
          },
          () => {
            sendClick({expectEvent: false, expectPopup: "b"});
          },
          () => {
            browser.browserAction.setPopup({popup: ""});
            sendClick({expectEvent: true, expectPopup: null});
          },
          () => {
            sendClick({expectEvent: true, expectPopup: null});
          },
          () => {
            browser.browserAction.setPopup({popup: "/popup-a.html"});
            sendClick({expectEvent: false, expectPopup: "a", runNextTest: true});
          },
          () => {
            browser.test.sendMessage("next-test", {expectClosed: true});
          },
        ];

        let expect = {};
        sendClick = ({expectEvent, expectPopup, runNextTest}) => {
          expect = {event: expectEvent, popup: expectPopup, runNextTest};
          browser.test.sendMessage("send-click");
        };

        browser.runtime.onMessage.addListener(msg => {
          if (msg == "close-popup") {
            return;
          } else if (expect.popup) {
            browser.test.assertEq(msg, `from-popup-${expect.popup}`,
                                  "expected popup opened");
          } else {
            browser.test.fail(`unexpected popup: ${msg}`);
          }

          expect.popup = null;
          if (expect.runNextTest) {
            expect.runNextTest = false;
            tests.shift()();
          } else {
            browser.test.sendMessage("next-test");
          }
        });

        browser.browserAction.onClicked.addListener(() => {
          if (expect.event) {
            browser.test.succeed("expected click event received");
          } else {
            browser.test.fail("unexpected click event");
          }

          expect.event = false;
          browser.test.sendMessage("next-test");
        });

        browser.test.onMessage.addListener((msg) => {
          if (msg == "close-popup") {
            browser.runtime.sendMessage("close-popup");
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

  let widget;
  extension.onMessage("next-test", Task.async(function* (expecting = {}) {
    if (!widget) {
      widget = getBrowserActionWidget(extension);
      CustomizableUI.addWidgetToArea(widget.id, area);
    }
    if (expecting.expectClosed) {
      let panel = getBrowserActionPopup(extension);
      ok(panel, "Expect panel to exist");
      yield promisePopupShown(panel);

      extension.sendMessage("close-popup");

      yield promisePopupHidden(panel);
      ok(true, "Panel is closed");
    } else {
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
