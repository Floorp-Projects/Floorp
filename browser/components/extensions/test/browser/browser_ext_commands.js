/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

var {AppConstants} = Cu.import("resource://gre/modules/AppConstants.jsm");

add_task(function* () {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "name": "Commands Extension",
      "commands": {
        "with-desciption": {
          "suggested_key": {
            "default": "Ctrl+Shift+Y",
          },
          "description": "should have a description",
        },
        "without-description": {
          "suggested_key": {
            "default": "Ctrl+Shift+D",
          },
        },
        "with-platform-info": {
          "suggested_key": {
            "mac": "Ctrl+Shift+M",
            "linux": "Ctrl+Shift+L",
            "windows": "Ctrl+Shift+W",
            "android": "Ctrl+Shift+A",
          },
        },
      },
    },

    background: function() {
      browser.test.onMessage.addListener((message, additionalScope) => {
        browser.commands.getAll((commands) => {
          let errorMessage = "getAll should return an array of commands";
          browser.test.assertEq(commands.length, 3, errorMessage);

          let command = commands.find(c => c.name == "with-desciption");

          errorMessage = "The description should match what is provided in the manifest";
          browser.test.assertEq("should have a description", command.description, errorMessage);

          errorMessage = "The shortcut should match the default shortcut provided in the manifest";
          browser.test.assertEq("Ctrl+Shift+Y", command.shortcut, errorMessage);

          command = commands.find(c => c.name == "without-description");

          errorMessage = "The description should be empty when it is not provided";
          browser.test.assertEq(null, command.description, errorMessage);

          errorMessage = "The shortcut should match the default shortcut provided in the manifest";
          browser.test.assertEq("Ctrl+Shift+D", command.shortcut, errorMessage);

          let platformKeys = {
            macosx: "M",
            linux: "L",
            win: "W",
            android: "A",
          };

          command = commands.find(c => c.name == "with-platform-info");
          let platformKey = platformKeys[additionalScope.platform];
          let shortcut = `Ctrl+Shift+${platformKey}`;
          errorMessage = `The shortcut should match the one provided in the manifest for OS='${additionalScope.platform}'`;
          browser.test.assertEq(shortcut, command.shortcut, errorMessage);

          browser.test.notifyPass("commands");
        });
      });
      browser.test.sendMessage("ready");
    },
  });

  yield extension.startup();
  yield extension.awaitMessage("ready");
  extension.sendMessage("additional-scope", {platform: AppConstants.platform});
  yield extension.awaitFinish("commands");
  yield extension.unload();
});
