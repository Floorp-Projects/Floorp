/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testTabSwitchActionContext() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.manifestV3.enabled", true]],
  });
});

async function testExecuteBrowserActionWithOptions(options = {}) {
  // Make sure the mouse isn't hovering over the browserAction widget.
  EventUtils.synthesizeMouseAtCenter(
    gURLBar.textbox,
    { type: "mouseover" },
    window
  );

  let extensionOptions = {};

  let browser_action =
    options.manifest_version > 2 ? "action" : "browser_action";
  let browser_action_key = options.manifest_version > 2 ? "a" : "j";

  // We accept any command in the manifest, so here we add commands for
  // both V2 and V3, but only the command that matches the manifest version
  // should ever work.
  extensionOptions.manifest = {
    manifest_version: options.manifest_version || 2,
    commands: {
      _execute_browser_action: {
        suggested_key: {
          default: "Alt+Shift+J",
        },
      },
      _execute_action: {
        suggested_key: {
          default: "Alt+Shift+A",
        },
      },
    },
    [browser_action]: {
      browser_style: true,
    },
  };

  if (options.withPopup) {
    extensionOptions.manifest[browser_action].default_popup = "popup.html";

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

      "popup.js": function () {
        browser.runtime.sendMessage("from-browser-action-popup");
      },
    };
  }

  extensionOptions.background = () => {
    let manifest = browser.runtime.getManifest();
    let { manifest_version } = manifest;
    const action = manifest_version < 3 ? "browserAction" : "action";

    browser.test.onMessage.addListener((message, options) => {
      browser.commands.onCommand.addListener(commandName => {
        if (
          ["_execute_browser_action", "_execute_action"].includes(commandName)
        ) {
          browser.test.assertEq(
            commandName,
            options.expectedCommand,
            `The onCommand listener fired for ${commandName}.`
          );
          browser[action].openPopup();
        }
      });

      if (!options.expectedCommand) {
        browser[action].onClicked.addListener(() => {
          if (options.withPopup) {
            browser.test.fail(
              "The onClick listener should never fire if the browserAction has a popup."
            );
            browser.test.notifyFail("execute-browser-action-on-clicked-fired");
          } else {
            browser.test.notifyPass("execute-browser-action-on-clicked-fired");
          }
        });
      }

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
    EventUtils.synthesizeKey(browser_action_key, {
      altKey: true,
      shiftKey: true,
    });
  });

  await extension.startup();

  await SimpleTest.promiseFocus(window);

  if (options.inArea) {
    let widget = getBrowserActionWidget(extension);
    CustomizableUI.addWidgetToArea(widget.id, options.inArea);
  }

  extension.sendMessage("options", options);

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

add_task(async function test_execute_browser_action_command() {
  await testExecuteBrowserActionWithOptions({
    withPopup: true,
    expectedCommand: "_execute_browser_action",
  });
});

add_task(async function test_execute_action_with_popup() {
  await testExecuteBrowserActionWithOptions({
    withPopup: true,
    manifest_version: 3,
  });
});

add_task(async function test_execute_action_without_popup() {
  await testExecuteBrowserActionWithOptions({
    manifest_version: 3,
  });
});

add_task(async function test_execute_action_command() {
  await testExecuteBrowserActionWithOptions({
    withPopup: true,
    expectedCommand: "_execute_action",
  });
});

add_task(
  async function test_execute_browser_action_in_hamburger_menu_with_popup() {
    await testExecuteBrowserActionWithOptions({
      withPopup: true,
      inArea: getCustomizableUIPanelID(),
    });
  }
);

add_task(
  async function test_execute_browser_action_in_hamburger_menu_without_popup() {
    await testExecuteBrowserActionWithOptions({
      inArea: getCustomizableUIPanelID(),
    });
  }
);
