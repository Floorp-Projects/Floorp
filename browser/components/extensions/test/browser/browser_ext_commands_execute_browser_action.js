/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function testExecuteBrowserActionWithOptions(options = {}) {
  // Make sure the mouse isn't hovering over the browserAction widget.
  EventUtils.synthesizeMouseAtCenter(gURLBar, {type: "mouseover"}, window);

  let extensionOptions = {};

  extensionOptions.manifest = {
    "commands": {
      "_execute_browser_action": {
        "suggested_key": {
          "default": "Alt+Shift+J",
        },
      },
    },
    "browser_action": {
      "browser_style": true,
    },
  };

  if (options.withPopup) {
    extensionOptions.manifest.browser_action.default_popup = "popup.html";

    extensionOptions.files = {
      "popup.html": `
        <!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="popup.js"></script>
          </head>
        </html>
      `,

      "popup.js": function() {
        browser.runtime.sendMessage("from-browser-action-popup");
      },
    };
  }

  extensionOptions.background = () => {
    browser.test.onMessage.addListener((message, withPopup) => {
      browser.commands.onCommand.addListener((commandName) => {
        if (commandName == "_execute_browser_action") {
          browser.test.fail("The onCommand listener should never fire for _execute_browser_action.");
        }
      });

      browser.browserAction.onClicked.addListener(() => {
        if (withPopup) {
          browser.test.fail("The onClick listener should never fire if the browserAction has a popup.");
          browser.test.notifyFail("execute-browser-action-on-clicked-fired");
        } else {
          browser.test.notifyPass("execute-browser-action-on-clicked-fired");
        }
      });

      browser.runtime.onMessage.addListener(msg => {
        if (msg == "from-browser-action-popup") {
          browser.test.notifyPass("execute-browser-action-popup-opened");
        }
      });

      browser.test.sendMessage("send-keys");
    });
  };

  let extension = ExtensionTestUtils.loadExtension(extensionOptions);

  extension.onMessage("send-keys", () => {
    EventUtils.synthesizeKey("j", {altKey: true, shiftKey: true});
  });

  await extension.startup();

  await SimpleTest.promiseFocus(window);

  if (options.inArea) {
    let widget = getBrowserActionWidget(extension);
    CustomizableUI.addWidgetToArea(widget.id, options.inArea);
  }

  extension.sendMessage("withPopup", options.withPopup);

  if (options.withPopup) {
    await extension.awaitFinish("execute-browser-action-popup-opened");

    if (!getBrowserActionPopup(extension)) {
      await awaitExtensionPanel(extension);
    }
    await closeBrowserAction(extension);
  } else {
    await extension.awaitFinish("execute-browser-action-on-clicked-fired");
  }
  await extension.unload();
}

add_task(async function test_execute_browser_action_with_popup() {
  await testExecuteBrowserActionWithOptions({
    withPopup: true,
  });
});

add_task(async function test_execute_browser_action_without_popup() {
  await testExecuteBrowserActionWithOptions();
});

add_task(async function test_execute_browser_action_in_hamburger_menu_with_popup() {
  await testExecuteBrowserActionWithOptions({
    withPopup: true,
    inArea: getCustomizableUIPanelID(),
  });
});

add_task(async function test_execute_browser_action_in_hamburger_menu_without_popup() {
  await testExecuteBrowserActionWithOptions({
    inArea: getCustomizableUIPanelID(),
  });
});
