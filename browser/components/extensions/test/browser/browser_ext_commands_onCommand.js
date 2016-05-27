/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/AppConstants.jsm");

add_task(function* test_user_defined_commands() {
  // Create a window before the extension is loaded.
  let win1 = yield BrowserTestUtils.openNewBrowserWindow();
  yield BrowserTestUtils.loadURI(win1.gBrowser.selectedBrowser, "about:robots");
  yield BrowserTestUtils.browserLoaded(win1.gBrowser.selectedBrowser);

  let commands = {
    "toggle-feature-using-alt-shift-3": {
      "suggested_key": {
        "default": "Alt+Shift+3",
      },
    },
    "toggle-feature-using-control-shift-4": {
      "suggested_key": {
        "default": "Ctrl+Shift+4",
      },
    },
    "toggle-feature-using-control-page-up": {
      "suggested_key": {
        "default": "Ctrl+PageUp",
      },
    },
    "toggle-feature-using-alt-shift-comma": {
      "suggested_key": {
        "default": "Alt+Shift+Comma",
      },
      "unrecognized_property": "with-a-random-value",
    },
    "toggle-feature-with-whitespace-in-suggested-key": {
      "suggested_key": {
        "default": "  Alt + Shift + 2  ",
      },
    },
  };

  // Register the Mac OS-X specific commands.
  if (AppConstants.platform == "macosx") {
    commands["toggle-feature-using-mac-control-5"] = {
      "suggested_key": {
        "default": "MacCtrl+5",
      },
    };
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

  // Create another window after the extension is loaded.
  let win2 = yield BrowserTestUtils.openNewBrowserWindow();
  yield BrowserTestUtils.loadURI(win2.gBrowser.selectedBrowser, "about:config");
  yield BrowserTestUtils.browserLoaded(win2.gBrowser.selectedBrowser);

  // Confirm the keysets have been added to both windows.
  let keysetID = `ext-keyset-id-${makeWidgetId(extension.id)}`;
  let keyset = win1.document.getElementById(keysetID);
  ok(keyset != null, "Expected keyset to exist");
  is(keyset.childNodes.length, 6, "Expected keyset to have the correct number of children");

  keyset = win2.document.getElementById(keysetID);
  ok(keyset != null, "Expected keyset to exist");
  is(keyset.childNodes.length, 6, "Expected keyset to have the correct number of children");

  // Confirm that the commands are registered to both windows.
  yield focusWindow(win1);
  EventUtils.synthesizeKey("3", {altKey: true, shiftKey: true});
  let message = yield extension.awaitMessage("oncommand");
  is(message, "toggle-feature-using-alt-shift-3", "Expected onCommand listener to fire with correct message");

  EventUtils.synthesizeKey("4", {accelKey: true, shiftKey: true});
  message = yield extension.awaitMessage("oncommand");
  is(message, "toggle-feature-using-control-shift-4", "Expected onCommand listener to fire with correct message");

  EventUtils.synthesizeKey("VK_PAGE_UP", {accelKey: true});
  message = yield extension.awaitMessage("oncommand");
  is(message, "toggle-feature-using-control-page-up", "Expected onCommand listener to fire with correct message");

  // Test the Mac OS-X specific shortcut.
  if (AppConstants.platform == "macosx") {
    EventUtils.synthesizeKey("5", {ctrlKey: true});
    message = yield extension.awaitMessage("oncommand");
    is(message, "toggle-feature-using-mac-control-5", "Expected onCommand listener to fire with correct message");
  }

  yield focusWindow(win2);
  EventUtils.synthesizeKey("VK_COMMA", {altKey: true, shiftKey: true});
  message = yield extension.awaitMessage("oncommand");
  is(message, "toggle-feature-using-alt-shift-comma", "Expected onCommand listener to fire with correct message");

  EventUtils.synthesizeKey("2", {altKey: true, shiftKey: true});
  message = yield extension.awaitMessage("oncommand");
  is(message, "toggle-feature-with-whitespace-in-suggested-key", "Expected onCommand listener to fire with correct message");

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


