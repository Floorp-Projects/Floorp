/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_user_defined_commands() {
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
    {
      name: "toggle-ctrl-shift-1",
      shortcut: "Ctrl+Shift+1",
      key: "1",
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
      name: "function-keys-Alt+Shift+F3",
      shortcut: "Alt+Shift+F3",
      key: "VK_F3",
      modifiers: {
        altKey: true,
        shiftKey: true,
      },
    },
    {
      name: "function-keys-F2",
      shortcut: "F2",
      key: "VK_F2",
      modifiers: {
        altKey: false,
        shiftKey: false,
      },
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
    {
      name: "toggle-ctrl-space",
      shortcut: "Ctrl+Space",
      key: "VK_SPACE",
      modifiers: {
        accelKey: true,
      },
    },
    {
      name: "toggle-ctrl-comma",
      shortcut: "Ctrl+Comma",
      key: "VK_COMMA",
      modifiers: {
        accelKey: true,
      },
    },
    {
      name: "toggle-ctrl-period",
      shortcut: "Ctrl+Period",
      key: "VK_PERIOD",
      modifiers: {
        accelKey: true,
      },
    },
    {
      name: "toggle-ctrl-alt-v",
      shortcut: "Ctrl+Alt+V",
      key: "V",
      modifiers: {
        accelKey: true,
        altKey: true,
      },
    },
  ];

  // Create a window before the extension is loaded.
  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  BrowserTestUtils.startLoadingURIString(
    win1.gBrowser.selectedBrowser,
    "about:robots"
  );
  await BrowserTestUtils.browserLoaded(win1.gBrowser.selectedBrowser);

  // We would have previously focused the window's content area after the
  // navigation from about:blank to about:robots, but bug 1596738 changed this
  // to prevent the browser element from stealing focus from the urlbar.
  //
  // Some of these command tests (specifically alt-a on linux) were designed
  // based on focus being in the browser content, so we need to manually focus
  // the browser here to preserve that assumption.
  win1.gBrowser.selectedBrowser.focus();

  let commands = {};
  let isMac = AppConstants.platform == "macosx";
  let totalMacOnlyCommands = 0;
  let numberNumericCommands = 4;

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

  function background() {
    browser.commands.onCommand.addListener(commandName => {
      browser.test.sendMessage("oncommand", commandName);
    });
    browser.test.sendMessage("ready");
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      commands: commands,
    },
    background,
  });

  SimpleTest.waitForExplicitFinish();
  let waitForConsole = new Promise(resolve => {
    SimpleTest.monitorConsole(resolve, [
      {
        message:
          /Reading manifest: Warning processing commands.*.unrecognized_property: An unexpected property was found/,
      },
    ]);
  });
  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.startup();
  ExtensionTestUtils.failOnSchemaWarnings(true);
  await extension.awaitMessage("ready");

  async function runTest(window) {
    for (let testCommand of testCommands) {
      if (testCommand.shortcutMac && !testCommand.shortcut && !isMac) {
        continue;
      }
      EventUtils.synthesizeKey(testCommand.key, testCommand.modifiers, window);
      let message = await extension.awaitMessage("oncommand");
      is(
        message,
        testCommand.name,
        `Expected onCommand listener to fire with the correct name: ${testCommand.name}`
      );
    }
  }

  // Create another window after the extension is loaded.
  let win2 = await BrowserTestUtils.openNewBrowserWindow();
  BrowserTestUtils.startLoadingURIString(
    win2.gBrowser.selectedBrowser,
    "about:robots"
  );
  await BrowserTestUtils.browserLoaded(win2.gBrowser.selectedBrowser);

  // See comment above.
  win2.gBrowser.selectedBrowser.focus();

  let totalTestCommands =
    Object.keys(testCommands).length + numberNumericCommands;
  let expectedCommandsRegistered = isMac
    ? totalTestCommands
    : totalTestCommands - totalMacOnlyCommands;

  // Confirm the keysets have been added to both windows.
  let keysetID = `ext-keyset-id-${makeWidgetId(extension.id)}`;
  let keyset = win1.document.getElementById(keysetID);
  Assert.notEqual(keyset, null, "Expected keyset to exist");
  is(
    keyset.children.length,
    expectedCommandsRegistered,
    "Expected keyset to have the correct number of children"
  );

  keyset = win2.document.getElementById(keysetID);
  Assert.notEqual(keyset, null, "Expected keyset to exist");
  is(
    keyset.children.length,
    expectedCommandsRegistered,
    "Expected keyset to have the correct number of children"
  );

  // Confirm that the commands are registered to both windows.
  await focusWindow(win1);
  await runTest(win1);

  await focusWindow(win2);
  await runTest(win2);

  let privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  BrowserTestUtils.startLoadingURIString(
    privateWin.gBrowser.selectedBrowser,
    "about:robots"
  );
  await BrowserTestUtils.browserLoaded(privateWin.gBrowser.selectedBrowser);

  // See comment above.
  privateWin.gBrowser.selectedBrowser.focus();

  keyset = privateWin.document.getElementById(keysetID);
  is(keyset, null, "Expected keyset is not added to private windows");

  await extension.unload();

  // Confirm that the keysets have been removed from both windows after the extension is unloaded.
  keyset = win1.document.getElementById(keysetID);
  is(keyset, null, "Expected keyset to be removed from the window");

  keyset = win2.document.getElementById(keysetID);
  is(keyset, null, "Expected keyset to be removed from the window");

  // Test that given permission the keyset is added to the private window.
  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      commands: commands,
    },
    incognitoOverride: "spanning",
    background,
  });

  // unrecognized_property in manifest triggers warning.
  ExtensionTestUtils.failOnSchemaWarnings(false);
  await extension.startup();
  ExtensionTestUtils.failOnSchemaWarnings(true);
  await extension.awaitMessage("ready");
  keysetID = `ext-keyset-id-${makeWidgetId(extension.id)}`;

  keyset = win1.document.getElementById(keysetID);
  Assert.notEqual(keyset, null, "Expected keyset to exist on win1");
  is(
    keyset.children.length,
    expectedCommandsRegistered,
    "Expected keyset to have the correct number of children"
  );

  keyset = win2.document.getElementById(keysetID);
  Assert.notEqual(keyset, null, "Expected keyset to exist on win2");
  is(
    keyset.children.length,
    expectedCommandsRegistered,
    "Expected keyset to have the correct number of children"
  );

  keyset = privateWin.document.getElementById(keysetID);
  Assert.notEqual(keyset, null, "Expected keyset was added to private windows");
  is(
    keyset.children.length,
    expectedCommandsRegistered,
    "Expected keyset to have the correct number of children"
  );

  await focusWindow(privateWin);
  await runTest(privateWin);

  await extension.unload();

  keyset = privateWin.document.getElementById(keysetID);
  is(keyset, null, "Expected keyset to be removed from the private window");

  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
  await BrowserTestUtils.closeWindow(privateWin);

  SimpleTest.endMonitorConsole();
  await waitForConsole;
});

add_task(async function test_commands_event_page() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.eventPages.enabled", true]],
  });

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      browser_specific_settings: { gecko: { id: "eventpage@commands" } },
      background: { persistent: false },
      commands: {
        "toggle-feature": {
          suggested_key: {
            default: "Alt+Shift+J",
          },
        },
      },
    },
    background() {
      browser.commands.onCommand.addListener(name => {
        browser.test.assertEq(name, "toggle-feature", "command received");
        browser.test.sendMessage("onCommand");
      });
      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");
  assertPersistentListeners(extension, "commands", "onCommand", {
    primed: false,
  });

  // test events waken background
  await extension.terminateBackground();
  assertPersistentListeners(extension, "commands", "onCommand", {
    primed: true,
  });

  EventUtils.synthesizeKey("j", { altKey: true, shiftKey: true });

  await extension.awaitMessage("ready");
  await extension.awaitMessage("onCommand");
  ok(true, "persistent event woke background");
  assertPersistentListeners(extension, "commands", "onCommand", {
    primed: false,
  });

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});
