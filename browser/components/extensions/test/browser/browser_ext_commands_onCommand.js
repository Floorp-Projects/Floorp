/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/AppConstants.jsm");

add_task(function* test_user_defined_commands() {
  const testCommands = [
    // Ctrl Shortcuts
    {
      name: "toggle-ctrl-a",
      shortcut: "Ctrl+A",
      key: "A",
      modifiers: {
        accelKey: true,
      },
    },
    {
      name: "toggle-ctrl-up",
      shortcut: "Ctrl+Up",
      key: "VK_UP",
      modifiers: {
        accelKey: true,
      },
    },
    // Alt Shortcuts
    {
      name: "toggle-alt-a",
      shortcut: "Alt+A",
      key: "A",
      modifiers: {
        altKey: true,
      },
    },
    {
      name: "toggle-alt-down",
      shortcut: "Alt+Down",
      key: "VK_DOWN",
      modifiers: {
        altKey: true,
      },
    },
    // Mac Shortcuts
    {
      name: "toggle-command-shift-page-up",
      shortcutMac: "Command+Shift+PageUp",
      key: "VK_PAGE_UP",
      modifiers: {
        accelKey: true,
        shiftKey: true,
      },
    },
    {
      name: "toggle-mac-control-shift+period",
      shortcut: "Ctrl+Shift+Period",
      shortcutMac: "MacCtrl+Shift+Period",
      key: "VK_PERIOD",
      modifiers: {
        ctrlKey: true,
        shiftKey: true,
      },
    },
    // Ctrl+Shift Shortcuts
    {
      name: "toggle-ctrl-shift-left",
      shortcut: "Ctrl+Shift+Left",
      key: "VK_LEFT",
      modifiers: {
        accelKey: true,
        shiftKey: true,
      },
    },
    // Alt+Shift Shortcuts
    {
      name: "toggle-alt-shift-1",
      shortcut: "Alt+Shift+1",
      key: "1",
      modifiers: {
        altKey: true,
        shiftKey: true,
      },
    },
    {
      name: "toggle-alt-shift-a",
      shortcut: "Alt+Shift+A",
      key: "A",
      modifiers: {
        altKey: true,
        shiftKey: true,
      },
    },
    {
      name: "toggle-alt-shift-right",
      shortcut: "Alt+Shift+Right",
      key: "VK_RIGHT",
      modifiers: {
        altKey: true,
        shiftKey: true,
      },
    },
    // Function keys
    {
      name: "function-keys",
      shortcut: "Alt+Shift+F3",
      key: "VK_F3",
      modifiers: {
        altKey: true,
        shiftKey: true,
      },
    },
    {
      name: "function-keys",
      shortcut: "F2",
      key: "VK_F2",
    },
    // Misc Shortcuts
    {
      name: "valid-command-with-unrecognized-property-name",
      shortcut: "Alt+Shift+3",
      key: "3",
      modifiers: {
        altKey: true,
        shiftKey: true,
      },
      unrecognized_property: "with-a-random-value",
    },
    {
      name: "spaces-in-shortcut-name",
      shortcut: "  Alt + Shift + 2  ",
      key: "2",
      modifiers: {
        altKey: true,
        shiftKey: true,
      },
    },
  ];

  // Create a window before the extension is loaded.
  let win1 = yield BrowserTestUtils.openNewBrowserWindow();
  yield BrowserTestUtils.loadURI(win1.gBrowser.selectedBrowser, "about:robots");
  yield BrowserTestUtils.browserLoaded(win1.gBrowser.selectedBrowser);

  let commands = {};
  let isMac = AppConstants.platform == "macosx";
  let totalMacOnlyCommands = 0;

  for (let testCommand of testCommands) {
    let command = {
      suggested_key: {},
    };

    if (testCommand.shortcut) {
      command.suggested_key.default = testCommand.shortcut;
    }

    if (testCommand.shortcutMac) {
      command.suggested_key.mac = testCommand.shortcutMac;
    }

    if (testCommand.shortcutMac && !testCommand.shortcut) {
      totalMacOnlyCommands++;
    }

    if (testCommand.unrecognized_property) {
      command.unrecognized_property = testCommand.unrecognized_property;
    }

    commands[testCommand.name] = command;
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "commands": commands,
    },

    background: function() {
      browser.commands.onCommand.addListener(commandName => {
        browser.test.sendMessage("oncommand", commandName);
      });
      browser.test.sendMessage("ready");
    },
  });

  SimpleTest.waitForExplicitFinish();
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [{
      message: /Reading manifest: Error processing commands.*.unrecognized_property: An unexpected property was found/,
    }]);
  });

  yield extension.startup();
  yield extension.awaitMessage("ready");

  function* runTest(window) {
    for (let testCommand of testCommands) {
      if (testCommand.shortcutMac && !testCommand.shortcut && !isMac) {
        continue;
      }
      EventUtils.synthesizeKey(testCommand.key, testCommand.modifiers, window);
      let message = yield extension.awaitMessage("oncommand");
      is(message, testCommand.name, `Expected onCommand listener to fire with the correct name: ${testCommand.name}`);
    }
  }

  // Create another window after the extension is loaded.
  let win2 = yield BrowserTestUtils.openNewBrowserWindow();
  yield BrowserTestUtils.loadURI(win2.gBrowser.selectedBrowser, "about:robots");
  yield BrowserTestUtils.browserLoaded(win2.gBrowser.selectedBrowser);

  let totalTestCommands = Object.keys(testCommands).length;
  let expectedCommandsRegistered = isMac ? totalTestCommands : totalTestCommands - totalMacOnlyCommands;

  // Confirm the keysets have been added to both windows.
  let keysetID = `ext-keyset-id-${makeWidgetId(extension.id)}`;
  let keyset = win1.document.getElementById(keysetID);
  ok(keyset != null, "Expected keyset to exist");
  is(keyset.childNodes.length, expectedCommandsRegistered, "Expected keyset to have the correct number of children");

  keyset = win2.document.getElementById(keysetID);
  ok(keyset != null, "Expected keyset to exist");
  is(keyset.childNodes.length, expectedCommandsRegistered, "Expected keyset to have the correct number of children");

  // Confirm that the commands are registered to both windows.
  yield focusWindow(win1);
  yield runTest(win1);

  yield focusWindow(win2);
  yield runTest(win2);

  yield extension.unload();

  // Confirm that the keysets have been removed from both windows after the extension is unloaded.
  keyset = win1.document.getElementById(keysetID);
  is(keyset, null, "Expected keyset to be removed from the window");

  keyset = win2.document.getElementById(keysetID);
  is(keyset, null, "Expected keyset to be removed from the window");

  yield BrowserTestUtils.closeWindow(win1);
  yield BrowserTestUtils.closeWindow(win2);

  SimpleTest.endMonitorConsole();
  yield waitForConsole;
});
