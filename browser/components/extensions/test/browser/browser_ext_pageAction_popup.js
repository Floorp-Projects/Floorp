/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function promisePopupShown(popup) {
  return new Promise(resolve => {
    if (popup.popupOpen) {
      resolve();
    } else {
      let onPopupShown = event => {
        popup.removeEventListener("popupshown", onPopupShown);
        resolve();
      };
      popup.addEventListener("popupshown", onPopupShown);
    }
  });
}

add_task(function* testPageActionPopup() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "background": {
        "page": "data/background.html"
      },
      "page_action": {
        "default_popup": "popup-a.html"
      }
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
        var tabId;

        var tests = [
          () => {
            sendClick({ expectEvent: false, expectPopup: "a" });
          },
          () => {
            sendClick({ expectEvent: false, expectPopup: "a" });
          },
          () => {
            browser.pageAction.setPopup({ tabId, popup: "popup-b.html" });
            sendClick({ expectEvent: false, expectPopup: "b" });
          },
          () => {
            sendClick({ expectEvent: false, expectPopup: "b" });
          },
          () => {
            browser.pageAction.setPopup({ tabId, popup: "" });
            sendClick({ expectEvent: true, expectPopup: null });
          },
          () => {
            sendClick({ expectEvent: true, expectPopup: null });
          },
          () => {
            browser.pageAction.setPopup({ tabId, popup: "/popup-a.html" });
            sendClick({ expectEvent: false, expectPopup: "a" });
          },
        ];

        var expect = {};
        function sendClick({ expectEvent, expectPopup }) {
          expect = { event: expectEvent, popup: expectPopup };
          browser.test.sendMessage("send-click");
        }

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

        browser.pageAction.onClicked.addListener(() => {
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
            var test = tests.shift();
            test();
          } else {
            browser.test.notifyPass("pageaction-tests-done");
          }
        });

        browser.tabs.query({ active: true, currentWindow: true }, tabs => {
          tabId = tabs[0].id;

          browser.pageAction.show(tabId);
          browser.test.sendMessage("next-test");
        });
      },
    },
  });

  let pageActionId = makeWidgetId(extension.id) + "-page-action";
  let panelId = makeWidgetId(extension.id) + "-panel";

  extension.onMessage("send-click", () => {
    let image = document.getElementById(pageActionId);

    let evt = new MouseEvent("click", {});
    image.dispatchEvent(evt);
  });

  extension.onMessage("next-test", Task.async(function* () {
    let panel = document.getElementById(panelId);
    if (panel) {
      yield promisePopupShown(panel);
      panel.hidePopup();

      panel = document.getElementById(panelId);
      is(panel, undefined, "panel successfully removed from document after hiding");
    }

    extension.sendMessage("next-test");
  }));


  yield Promise.all([extension.startup(), extension.awaitFinish("pageaction-tests-done")]);

  yield extension.unload();

  let node = document.getElementById(pageActionId);
  is(node, undefined, "pageAction image removed from document");

  let panel = document.getElementById(panelId);
  is(panel, undefined, "pageAction panel removed from document");
});


add_task(function* testPageActionSecurity() {
  const URL = "chrome://browser/content/browser.xul";

  let matchURLForbidden = url => ({
    message: new RegExp(`Loading extension.*Access to.*'${URL}' denied`),
  });

  let messages = [/Access to restricted URI denied/,
                  /Access to restricted URI denied/];

  let waitForConsole = new Promise(resolve => {
    // Not necessary in browser-chrome tests, but monitorConsole gripes
    // if we don't call it.
    SimpleTest.waitForExplicitFinish();

    SimpleTest.monitorConsole(resolve, messages);
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": { "default_popup": URL },
      "page_action": { "default_popup": URL },
    },

    background: function () {
      browser.tabs.query({ active: true, currentWindow: true }, tabs => {
        var tabId = tabs[0].id;

        browser.pageAction.show(tabId);
        browser.test.sendMessage("ready");
      });
    },
  });

  yield Promise.all([extension.startup(), extension.awaitMessage("ready")]);

  let browserActionId = makeWidgetId(extension.id) + "-browser-action";
  let pageActionId = makeWidgetId(extension.id) + "-page-action";

  let browserAction = document.getElementById(browserActionId);
  let evt = new CustomEvent("command", {});
  browserAction.dispatchEvent(evt);

  let pageAction = document.getElementById(pageActionId);
  evt = new MouseEvent("click", {});
  pageAction.dispatchEvent(evt);

  yield extension.unload();

  let node = document.getElementById(pageActionId);
  is(node, undefined, "pageAction image removed from document");

  SimpleTest.endMonitorConsole();
  yield waitForConsole;
});
