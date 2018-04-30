/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N =
  new LocalizationHelper("devtools/client/locales/animationinspector.properties");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

/**
 * Get a formatted title for this animation. This will be either:
 * "%S", "%S : CSS Transition", "%S : CSS Animation",
 * "%S : Script Animation", or "Script Animation", depending
 * if the server provides the type, what type it is and if the animation
 * has a name.
 *
 * @param {Object} state
 */
function getFormattedTitle(state) {
  // Older servers don't send a type, and only know about
  // CSSAnimations and CSSTransitions, so it's safe to use
  // just the name.
  if (!state.type) {
    return state.name;
  }

  // Script-generated animations may not have a name.
  if (state.type === "scriptanimation" && !state.name) {
    return L10N.getStr("timeline.scriptanimation.unnamedLabel");
  }

  return L10N.getFormatStr(`timeline.${state.type}.nameLabel`, state.name);
}

module.exports = {
  getFormatStr: (...args) => L10N.getFormatStr(...args),
  getFormattedTitle,
  getInspectorStr: (...args) => INSPECTOR_L10N.getStr(...args),
  getStr: (...args) => L10N.getStr(...args),
  numberWithDecimals: (...args) => L10N.numberWithDecimals(...args),
};
