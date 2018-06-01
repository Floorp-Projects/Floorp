/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

requestLongerTimeout(2);

// Test that the preview images are updated when the theme changes.

const { getTheme, setTheme } = require("devtools/client/shared/theme");

const TEST_URI = URL_ROOT + "browser_fontinspector.html";
const originalTheme = getTheme();

registerCleanupFunction(() => {
  info(`Restoring theme to '${originalTheme}.`);
  setTheme(originalTheme);
});

add_task(async function() {
  const { inspector, view } = await openFontInspectorForURL(TEST_URI);
  const { document: doc } = view;

  await selectNode(".normal-text", inspector);

  // Store the original preview URI for later comparison.
  const originalURI = doc.querySelector("#font-container .font-preview").src;
  const newTheme = originalTheme === "light" ? "dark" : "light";

  info(`Original theme was '${originalTheme}'.`);

  await setThemeAndWaitForUpdate(newTheme, inspector);
  isnot(doc.querySelector("#font-container .font-preview").src, originalURI,
    "The preview image changed with the theme.");

  await setThemeAndWaitForUpdate(originalTheme, inspector);
  is(doc.querySelector("#font-container .font-preview").src, originalURI,
    "The preview image is correct after the original theme was restored.");
});

/**
 * Sets the current theme and waits for fontinspector-updated event.
 *
 * @param {String} theme - the new theme
 * @param {Object} inspector - the inspector panel
 */
async function setThemeAndWaitForUpdate(theme, inspector) {
  const onUpdated = inspector.once("fontinspector-updated");

  info(`Setting theme to '${theme}'.`);
  setTheme(theme);

  info("Waiting for font-inspector to update.");
  await onUpdated;
}
