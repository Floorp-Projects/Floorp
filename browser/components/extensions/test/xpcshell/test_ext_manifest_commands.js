/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";


add_task(function* test_manifest_commands() {
  let normalized = yield ExtensionTestUtils.normalizeManifest({
    "commands": {
      "toggle-feature": {
        "suggested_key": {"default": "Shifty+Y"},
        "description": "Send a 'toggle-feature' event to the extension",
      },
    },
  });

  let expectedError = (
    String.raw`commands.toggle-feature.suggested_key.default: Value must either: ` +
    String.raw`match the pattern /^\s*(Alt|Ctrl|Command|MacCtrl)\s*\+\s*(Shift\s*\+\s*)?([A-Z0-9]|Comma|Period|Home|End|PageUp|PageDown|Space|Insert|Delete|Up|Down|Left|Right)\s*$/, or ` +
    String.raw`match the pattern /^\s*((Alt|Ctrl|Command|MacCtrl)\s*\+\s*)?(Shift\s*\+\s*)?(F[1-9]|F1[0-2])\s*$/, or ` +
    String.raw`match the pattern /^(MediaNextTrack|MediaPlayPause|MediaPrevTrack|MediaStop)$/`
  );

  ok(normalized.error.includes(expectedError),
     `The manifest error ${JSON.stringify(normalized.error)} must contain ${JSON.stringify(expectedError)}`);
});
