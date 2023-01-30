/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */

"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this
);

const FLEXBOX_OPENED_PREF = "devtools.layout.flexbox.opened";
const FLEX_CONTAINER_OPENED_PREF = "devtools.layout.flex-container.opened";
const FLEX_ITEM_OPENED_PREF = "devtools.layout.flex-item.opened";
const GRID_OPENED_PREF = "devtools.layout.grid.opened";
const BOXMODEL_OPENED_PREF = "devtools.layout.boxmodel.opened";

// Make sure only the flexbox layout accordions are opened, and the others are closed.
Services.prefs.setBoolPref(FLEXBOX_OPENED_PREF, true);
Services.prefs.setBoolPref(FLEX_CONTAINER_OPENED_PREF, true);
Services.prefs.setBoolPref(FLEX_ITEM_OPENED_PREF, true);
Services.prefs.setBoolPref(BOXMODEL_OPENED_PREF, false);
Services.prefs.setBoolPref(GRID_OPENED_PREF, false);

// Clear all set prefs.
registerCleanupFunction(() => {
  Services.prefs.clearUserPref(FLEXBOX_OPENED_PREF);
  Services.prefs.clearUserPref(FLEX_CONTAINER_OPENED_PREF);
  Services.prefs.clearUserPref(FLEX_ITEM_OPENED_PREF);
  Services.prefs.clearUserPref(BOXMODEL_OPENED_PREF);
  Services.prefs.clearUserPref(GRID_OPENED_PREF);
});

/**
 * Toggles ON the flexbox highlighter given the flexbox highlighter button from the
 * layout panel.
 *
 * @param  {DOMNode} button
 *         The flexbox highlighter toggle button in the flex container panel.
 * @param  {Inspector} inspector
 *         Inspector panel instance.
 */
async function toggleHighlighterON(button, inspector) {
  info("Toggling ON the flexbox highlighter from the layout panel.");
  const { waitForHighlighterTypeShown } = getHighlighterTestHelpers(inspector);
  const onHighlighterShown = waitForHighlighterTypeShown(
    inspector.highlighters.TYPES.FLEXBOX
  );
  const { store } = inspector;
  const onToggleChange = waitUntilState(
    store,
    state => state.flexbox.highlighted
  );
  button.click();
  await Promise.all([onHighlighterShown, onToggleChange]);
}

/**
 * Toggles OFF the flexbox highlighter given the flexbox highlighter button from the
 * layout panel.
 *
 * @param  {DOMNode} button
 *         The flexbox highlighter toggle button in the flex container panel.
 * @param  {Inspector} inspector
 *         Inspector panel instance.
 */
async function toggleHighlighterOFF(button, inspector) {
  info("Toggling OFF the flexbox highlighter from the layout panel.");
  const { waitForHighlighterTypeHidden } = getHighlighterTestHelpers(inspector);
  const onHighlighterHidden = waitForHighlighterTypeHidden(
    inspector.highlighters.TYPES.FLEXBOX
  );
  const { store } = inspector;
  const onToggleChange = waitUntilState(
    store,
    state => !state.flexbox.highlighted
  );
  button.click();
  await Promise.all([onHighlighterHidden, onToggleChange]);
}
