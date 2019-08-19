/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../test/head.js */
/* import-globals-from ../../../inspector/rules/test/head.js */
/* import-globals-from ../../../inspector/test/shared-head.js */
/* import-globals-from ../../../shared/test/shared-redux-head.js */
"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this
);

// Load the shared Redux helpers into this compartment.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-redux-head.js",
  this
);

// Make sure only the flexbox layout accordion is opened, and the others are closed.
Services.prefs.setBoolPref("devtools.layout.flexbox.opened", true);
Services.prefs.setBoolPref("devtools.layout.boxmodel.opened", false);
Services.prefs.setBoolPref("devtools.layout.grid.opened", false);

// Clear all set prefs.
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.layout.flexbox.opened");
  Services.prefs.clearUserPref("devtools.layout.boxmodel.opened");
  Services.prefs.clearUserPref("devtools.layout.grid.opened");
});

/**
 * Toggles ON the flexbox highlighter given the flexbox highlighter button from the
 * layout panel.
 *
 * @param  {DOMNode} button
 *         The flexbox highlighter toggle button in the flex container panel.
 * @param  {HighlightersOverlay} highlighters
 *         The HighlightersOverlay instance.
 * @param  {Store} store
 *         The Redux store instance.
 */
async function toggleHighlighterON(button, highlighters, store) {
  info("Toggling ON the flexbox highlighter from the layout panel.");
  const onHighlighterShown = highlighters.once("flexbox-highlighter-shown");
  const onToggleChange = waitUntilState(
    store,
    state => state.flexbox.highlighted
  );
  button.click();
  await onHighlighterShown;
  await onToggleChange;
}

/**
 * Toggles OFF the flexbox highlighter given the flexbox highlighter button from the
 * layout panel.
 *
 * @param  {DOMNode} button
 *         The flexbox highlighter toggle button in the flex container panel.
 * @param  {HighlightersOverlay} highlighters
 *         The HighlightersOverlay instance.
 * @param  {Store} store
 *         The Redux store instance.
 */
async function toggleHighlighterOFF(button, highlighters, store) {
  info("Toggling OFF the flexbox highlighter from the layout panel.");
  const onHighlighterHidden = highlighters.once("flexbox-highlighter-hidden");
  const onToggleChange = waitUntilState(
    store,
    state => !state.flexbox.highlighted
  );
  button.click();
  await onHighlighterHidden;
  await onToggleChange;
}
