/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function* testInArea(area) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "background": {
        "page": "data/background.html",
      },
      "browser_action": {
        "default_popup": "popup-a.html",
      },
    },

    files: {
      "popup-a.html": `<script src="popup-a.js"></script>`,
      "popup-a.js": function() {
        browser.runtime.sendMessage("from-popup-a");
      },

      "data/popup-b.html": `<script src="popup-b.js"></script>`,
      "data/popup-b.js": function() {
        browser.runtime.sendMessage("from-popup-b");
      },

      "data/background.html": `<script src="background.js"></script>`,

      "data/background.js": function() {
        let sendClick;
        let tests = [
          () => {
            sendClick({ expectEvent: false, expectPopup: "a" });
          },
          () => {
            sendClick({ expectEvent: false, expectPopup: "a" });
          },
          () => {
            browser.browserAction.setPopup({ popup: "popup-b.html" });
            sendClick({ expectEvent: false, expectPopup: "b" });
          },
          () => {
            sendClick({ expectEvent: false, expectPopup: "b" });
          },
          () => {
            browser.browserAction.setPopup({ popup: "" });
            sendClick({ expectEvent: true, expectPopup: null });
          },
          () => {
            sendClick({ expectEvent: true, expectPopup: null });
          },
          () => {
            browser.browserAction.setPopup({ popup: "/popup-a.html" });
            sendClick({ expectEvent: false, expectPopup: "a" });
          },
        ];

        let expect = {};
        sendClick = ({ expectEvent, expectPopup }) => {
          expect = { event: expectEvent, popup: expectPopup };
          browser.test.sendMessage("send-click");
        };

        browser.runtime.onMessage.addListener(msg => {
          if (expect.popup) {
            browser.test.assertEq(msg, `from-popup-${expect.popup}`,
                                  "expected popup opened");
          } else {
            browser.test.fail("unexpected popup");
          }

          expect.popup = null;
          browser.test.sendMessage("next-test");
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
  extension.onMessage("next-test", Task.async(function* () {
    if (!widget) {
      widget = getBrowserActionWidget(extension);
      CustomizableUI.addWidgetToArea(widget.id, area);
    }

    yield closeBrowserAction(extension);

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
