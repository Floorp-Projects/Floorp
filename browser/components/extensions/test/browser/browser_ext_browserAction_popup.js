/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function getBrowserAction(extension) {
  const {GlobalManager, Management: {global: {browserActionFor}}} = Cu.import("resource://gre/modules/Extension.jsm", {});

  let ext = GlobalManager.extensionMap.get(extension.id);
  return browserActionFor(ext);
}

let scriptPage = url => `<html><head><meta charset="utf-8"><script src="${url}"></script></head><body>${url}</body></html>`;

async function testInArea(area) {
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
        window.onload = () => {
          browser.runtime.sendMessage("from-popup-b");
        };
      },

      "data/popup-c.html": scriptPage("popup-c.js"),
      "data/popup-c.js": function() {
        // Close the popup before the document is fully-loaded to make sure that
        // we handle this case sanely.
        browser.runtime.sendMessage("from-popup-c");
        window.close();
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
            sendClick({expectEvent: false, expectPopup: "a",
                       closePopup: false, containingPopupShouldClose: false}, "trigger-action");
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
            browser.test.log(`Set popup to "c" and click browser action. Expect popup "c".`);
            browser.browserAction.setPopup({popup: "popup-c.html"});
            sendClick({expectEvent: false, expectPopup: "c", waitUntilClosed: true});
          },
          () => {
            browser.test.log(`Set popup to "b" and click browser action. Expect popup "b".`);
            browser.browserAction.setPopup({popup: "popup-b.html"});
            sendClick({expectEvent: false, expectPopup: "b"});
          },
          () => {
            browser.test.log(`Click browser action again, expect popup "b".`);
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
            browser.test.log(`Set popup to "a" and click browser action. Expect popup "a", and leave open.`);
            browser.browserAction.setPopup({popup: "/popup-a.html"});
            sendClick({expectEvent: false, expectPopup: "a", closePopup: false,
                       containingPopupShouldClose: false});
          },
          () => {
            browser.test.log(`Tell popup "a" to call window.close(). Expect popup closed.`);
            browser.test.sendMessage("next-test", {closePopupUsingWindow: true});
          },
        ];

        let expect = {};
        sendClick = ({expectEvent, expectPopup, runNextTest, waitUntilClosed,
                      closePopup, containingPopupShouldClose = true},
                     message = "send-click") => {
          if (closePopup == undefined) {
            closePopup = !expectEvent;
          }

          expect = {
            event: expectEvent, popup: expectPopup, runNextTest,
            waitUntilClosed, closePopup, containingPopupShouldClose,
          };
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
  extension.onMessage("next-test", async function(expecting = {}) {
    if (!widget) {
      widget = getBrowserActionWidget(extension);
      CustomizableUI.addWidgetToArea(widget.id, area);
    }
    if (expecting.waitUntilClosed) {
      await new Promise(resolve => setTimeout(resolve, 0));

      let panel = getBrowserActionPopup(extension);
      if (panel && panel.state != "closed") {
        info("Popup is open. Waiting for close");
        await promisePopupHidden(panel);
      }
    } else if (expecting.closePopupUsingWindow) {
      let panel = getBrowserActionPopup(extension);
      ok(panel, "Expect panel to exist");
      await promisePopupShown(panel);

      extension.sendMessage("close-popup-using-window.close");

      await promisePopupHidden(panel);
      ok(true, "Panel is closed");
    } else if (expecting.closePopup) {
      if (!getBrowserActionPopup(extension)) {
        info("Waiting for panel");
        await awaitExtensionPanel(extension);
      }

      info("Closing for panel");
      await closeBrowserAction(extension);
    }

    if (area == getCustomizableUIPanelID() && expecting.containingPopupShouldClose) {
      let {node} = getBrowserActionWidget(extension).forWindow(window);
      let panel = node.closest("panel");
      info(`State of panel ${panel.id} is: ${panel.state}`);
      ok(!["open", "showing"].includes(panel.state),
         "Panel containing the action should be closed");
    }

    info("Starting next test");
    extension.sendMessage("next-test");
  });

  await Promise.all([extension.startup(), extension.awaitFinish("browseraction-tests-done")]);

  await extension.unload();

  let view = document.getElementById(widget.viewId);
  is(view, null, "browserAction view removed from document");
}

add_task(async function testBrowserActionInToolbar() {
  await testInArea(CustomizableUI.AREA_NAVBAR);
});

add_task(async function testBrowserActionInPanel() {
  await testInArea(getCustomizableUIPanelID());
});
