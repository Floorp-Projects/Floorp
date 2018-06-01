/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";


add_task(async function test_manifest_commands() {
  const validShortcuts = ["Ctrl+Y", "MacCtrl+Y", "Command+Y", "Alt+Shift+Y", "Ctrl+Alt+Y", "F1", "MediaNextTrack"];
  const invalidShortcuts = ["Shift+Y", "Y", "Ctrl+Ctrl+Y", "Ctrl+Command+Y"];

  async function validateShortcut(shortcut, isValid) {
    let normalized = await ExtensionTestUtils.normalizeManifest({
      "commands": {
        "toggle-feature": {
          "suggested_key": {"default": shortcut},
          "description": "Send a 'toggle-feature' event to the extension",
        },
      },
    });
    if (isValid) {
      ok(!normalized.error,
         "There should be no manifest errors.");
    } else {
      let expectedError = (
        String.raw`Error processing commands.toggle-feature.suggested_key.default: Error: ` +
          String.raw`Value "${shortcut}" must consist of ` +
          String.raw`either a combination of one or two modifiers, including ` +
          String.raw`a mandatory primary modifier and a key, separated by '+', ` +
          String.raw`or a media key. For details see: ` +
          String.raw`https://developer.mozilla.org/en-US/Add-ons/WebExtensions/manifest.json/commands#Key_combinations`
      );

      ok(normalized.error.includes(expectedError),
         `The manifest error ${JSON.stringify(normalized.error)} must contain ${JSON.stringify(expectedError)}`);
    }
  }

  for (let shortcut of validShortcuts) {
    validateShortcut(shortcut, true);
  }
  for (let shortcut of invalidShortcuts) {
    validateShortcut(shortcut, false);
  }
});
