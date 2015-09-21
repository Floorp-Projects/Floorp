/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the preview images are updated when the theme changes.

const { getTheme, setTheme } = require("devtools/shared/theme");

const TEST_URI = BASE_URI + "browser_fontinspector.html";
const originalTheme = getTheme();

registerCleanupFunction(() => {
  info(`Restoring theme to '${originalTheme}.`);
  setTheme(originalTheme);
});

add_task(function* () {
  let { inspector, fontInspector } = yield openFontInspectorForURL(TEST_URI);
  let { chromeDoc: doc } = fontInspector;

  yield selectNode(".normal-text", inspector);

  // Store the original preview URI for later comparison.
  let originalURI = doc.querySelector("#all-fonts .font-preview").src;
  let newTheme = originalTheme === "light" ? "dark" : "light";

  info(`Original theme was '${originalTheme}'.`);

  yield setThemeAndWaitForUpdate(newTheme, inspector);
  isnot(doc.querySelector("#all-fonts .font-preview").src, originalURI,
    "The preview image changed with the theme.");

  yield setThemeAndWaitForUpdate(originalTheme, inspector);
  is(doc.querySelector("#all-fonts .font-preview").src, originalURI,
    "The preview image is correct after the original theme was restored.");
});

/**
 * Sets the current theme and waits for fontinspector-updated event.
 *
 * @param {String} theme - the new theme
 * @param {Object} inspector - the inspector panel
 */
function* setThemeAndWaitForUpdate(theme, inspector) {
  let onUpdated = inspector.once("fontinspector-updated");

  info(`Setting theme to '${theme}'.`);
  setTheme(theme);

  info("Waiting for font-inspector to update.");
  yield onUpdated;
}
