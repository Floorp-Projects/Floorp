/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(this, "Ci", "chrome", true);
loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(
  this,
  "loadSheet",
  "devtools/shared/layout/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "removeSheet",
  "devtools/shared/layout/utils",
  true
);

// Highlighter style used for preventing transitions and applying transparency
// when calculating colour contrast.
const HIGHLIGHTER_STYLES_SHEET = `data:text/css;charset=utf-8,
* {
  transition: none !important;
}

:-moz-devtools-highlighted {
  color: transparent !important;
  text-shadow: none !important;
}`;

/**
 * Helper function that determines if nsIAccessible object is in defunct state.
 *
 * @param  {nsIAccessible}  accessible
 *         object to be tested.
 * @return {Boolean}
 *         True if accessible object is defunct, false otherwise.
 */
function isDefunct(accessible) {
  // If accessibility is disabled, safely assume that the accessible object is
  // now dead.
  if (!Services.appinfo.accessibilityEnabled) {
    return true;
  }

  let defunct = false;

  try {
    const extraState = {};
    accessible.getState({}, extraState);
    // extraState.value is a bitmask. We are applying bitwise AND to mask out
    // irrelevant states.
    defunct = !!(extraState.value & Ci.nsIAccessibleStates.EXT_STATE_DEFUNCT);
  } catch (e) {
    defunct = true;
  }

  return defunct;
}

/**
 * Load highlighter style sheet used for preventing transitions and
 * applying transparency when calculating colour contrast.
 *
 * @param  {Window} win
 *         Window where highlighting happens.
 */
function loadSheetForBackgroundCalculation(win) {
  loadSheet(win, HIGHLIGHTER_STYLES_SHEET);
}

/**
 * Unload highlighter style sheet used for preventing transitions
 * and applying transparency when calculating colour contrast.
 *
 * @param  {Window} win
 *         Window where highlighting was happenning.
 */
function removeSheetForBackgroundCalculation(win) {
  removeSheet(win, HIGHLIGHTER_STYLES_SHEET);
}

exports.isDefunct = isDefunct;
exports.loadSheetForBackgroundCalculation = loadSheetForBackgroundCalculation;
exports.removeSheetForBackgroundCalculation = removeSheetForBackgroundCalculation;
