/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function () {
  let extension = ExtensionTestUtils.loadExtension({
    files: {
      "_locales/en/messages.json": {
        with_translation: {
          message: "The description",
          description: "A description",
        },
      },
    },
    manifest: {
      name: "Commands Extension",
      default_locale: "en",
      commands: {
        "with-desciption": {
          suggested_key: {
            default: "Ctrl+Shift+Y",
          },
          description: "should have a description",
        },
        "without-description": {
          suggested_key: {
            default: "Ctrl+Shift+D",
          },
        },
        "with-platform-info": {
          suggested_key: {
            mac: "Ctrl+Shift+M",
            linux: "Ctrl+Shift+L",
            windows: "Ctrl+Shift+W",
            android: "Ctrl+Shift+A",
          },
        },
        "with-translation": {
          description: "__MSG_with_translation__",
        },
        "without-suggested-key": {
          description: "has no suggested_key",
        },
        "without-suggested-key-nor-description": {},
      },
    },

    background: function () {
      browser.test.onMessage.addListener((message, additionalScope) => {
        browser.commands.getAll(commands => {
          let errorMessage = "getAll should return an array of commands";
          browser.test.assertEq(commands.length, 6, errorMessage);

          let command = commands.find(c => c.name == "with-desciption");

          errorMessage =
            "The description should match what is provided in the manifest";
          browser.test.assertEq(
            "should have a description",
            command.description,
            errorMessage
          );

          errorMessage =
            "The shortcut should match the default shortcut provided in the manifest";
          browser.test.assertEq("Ctrl+Shift+Y", command.shortcut, errorMessage);

          command = commands.find(c => c.name == "without-description");

          errorMessage =
            "The description should be empty when it is not provided";
          browser.test.assertEq(null, command.description, errorMessage);

          errorMessage =
            "The shortcut should match the default shortcut provided in the manifest";
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

          command = commands.find(c => c.name == "with-translation");
          browser.test.assertEq(
            command.description,
            "The description",
            "The description can be localized"
          );

          command = commands.find(c => c.name == "without-suggested-key");

          browser.test.assertEq(
            "has no suggested_key",
            command.description,
            "The description should match what is provided in the manifest"
          );

          browser.test.assertEq(
            "",
            command.shortcut,
            "The shortcut should be empty if not provided"
          );

          command = commands.find(
            c => c.name == "without-suggested-key-nor-description"
          );

          browser.test.assertEq(
            null,
            command.description,
            "The description should be empty when it is not provided"
          );

          browser.test.assertEq(
            "",
            command.shortcut,
            "The shortcut should be empty if not provided"
          );

          browser.test.notifyPass("commands");
        });
      });
      browser.test.sendMessage("ready");
    },
  });

  await extension.startup();
  await extension.awaitMessage("ready");
  extension.sendMessage("additional-scope", {
    platform: AppConstants.platform,
  });
  await extension.awaitFinish("commands");
  await extension.unload();
});
