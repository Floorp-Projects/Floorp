/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const scriptPage = url =>
  `<html><head><meta charset="utf-8"><script src="${url}"></script></head><body>Test Popup</body></html>`;

add_task(async function test_execute_page_action_without_popup() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      commands: {
        _execute_page_action: {
          suggested_key: {
            default: "Alt+Shift+J",
          },
        },
        "send-keys-command": {
          suggested_key: {
            default: "Alt+Shift+3",
          },
        },
      },
      page_action: {},
    },

    background: function() {
      let isShown = false;

      browser.commands.onCommand.addListener(commandName => {
        if (commandName == "_execute_page_action") {
          browser.test.fail(
            `The onCommand listener should never fire for ${commandName}.`
          );
        } else if (commandName == "send-keys-command") {
          if (!isShown) {
            isShown = true;
            browser.tabs.query({ currentWindow: true, active: true }, tabs => {
              tabs.forEach(tab => {
                browser.pageAction.show(tab.id);
              });
              browser.test.sendMessage("send-keys");
            });
          }
        }
      });

      browser.pageAction.onClicked.addListener(() => {
        browser.test.assertTrue(
          isShown,
          "The onClicked event should fire if the page action is shown."
        );
        browser.test.notifyPass("page-action-without-popup");
      });

      browser.test.sendMessage("send-keys");
    },
  });

  extension.onMessage("send-keys", () => {
    EventUtils.synthesizeKey("j", { altKey: true, shiftKey: true });
    EventUtils.synthesizeKey("3", { altKey: true, shiftKey: true });
  });

  await extension.startup();
  await extension.awaitFinish("page-action-without-popup");
  await extension.unload();
});

add_task(async function test_execute_page_action_with_popup() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      commands: {
        _execute_page_action: {
          suggested_key: {
            default: "Alt+Shift+J",
          },
        },
        "send-keys-command": {
          suggested_key: {
            default: "Alt+Shift+3",
          },
        },
      },
      page_action: {
        default_popup: "popup.html",
        browser_style: true,
      },
    },

    files: {
      "popup.html": scriptPage("popup.js"),
      "popup.js": function() {
        browser.runtime.sendMessage("popup-opened");
      },
    },

    background: function() {
      let isShown = false;

      browser.commands.onCommand.addListener(message => {
        if (message == "_execute_page_action") {
          browser.test.fail(
            `The onCommand listener should never fire for ${message}.`
          );
        }

        if (message == "send-keys-command") {
          if (!isShown) {
            isShown = true;
            browser.tabs.query({ currentWindow: true, active: true }, tabs => {
              tabs.forEach(tab => {
                browser.pageAction.show(tab.id);
              });
              browser.test.sendMessage("send-keys");
            });
          }
        }
      });

      browser.pageAction.onClicked.addListener(() => {
        browser.test.fail(
          `The onClicked listener should never fire when the pageAction has a popup.`
        );
      });

      browser.runtime.onMessage.addListener(msg => {
        browser.test.assertEq(msg, "popup-opened", "expected popup opened");
        browser.test.assertTrue(
          isShown,
          "The onClicked event should fire if the page action is shown."
        );
        browser.test.notifyPass("page-action-with-popup");
      });

      browser.test.sendMessage("send-keys");
    },
  });

  extension.onMessage("send-keys", () => {
    EventUtils.synthesizeKey("j", { altKey: true, shiftKey: true });
    EventUtils.synthesizeKey("3", { altKey: true, shiftKey: true });
  });

  await extension.startup();
  await extension.awaitFinish("page-action-with-popup");
  await extension.unload();
});

add_task(async function test_execute_page_action_with_matching() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      commands: {
        _execute_page_action: {
          suggested_key: {
            default: "Alt+Shift+J",
          },
        },
      },
      page_action: {
        default_popup: "popup.html",
        show_matches: ["<all_urls>"],
        browser_style: true,
      },
    },

    files: {
      "popup.html": scriptPage("popup.js"),
      "popup.js": function() {
        window.addEventListener(
          "load",
          () => {
            browser.test.notifyPass("page-action-with-popup");
          },
          { once: true }
        );
      },
    },
  });

  await extension.startup();
  let tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    "http://example.com/"
  );
  EventUtils.synthesizeKey("j", { altKey: true, shiftKey: true });
  info("Waiting for pageAction open.");
  await extension.awaitFinish("page-action-with-popup");

  // Bug 1447796 make sure the key command can close the page action
  let panel = document.getElementById(`${makeWidgetId(extension.id)}-panel`);
  let hidden = promisePopupHidden(panel);
  EventUtils.synthesizeKey("j", { altKey: true, shiftKey: true });
  info("Waiting for pageAction close.");
  await hidden;

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});
