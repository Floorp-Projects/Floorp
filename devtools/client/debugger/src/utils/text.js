/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Utils for keyboard command strings
 * @module utils/text
 */

const isMacOS = Services.appinfo.OS === "Darwin";

/**
 * Formats key for use in tooltips
 * For macOS we use the following unicode
 *
 * cmd ⌘ = \u2318
 * shift ⇧ – \u21E7
 * option (alt) ⌥ \u2325
 *
 * For Win/Lin this replaces CommandOrControl or CmdOrCtrl with Ctrl
 *
 * @memberof utils/text
 * @static
 */
export function formatKeyShortcut(shortcut) {
  if (isMacOS) {
    return shortcut
      .replace(/Shift\+/g, "\u21E7")
      .replace(/Command\+|Cmd\+/g, "\u2318")
      .replace(/CommandOrControl\+|CmdOrCtrl\+/g, "\u2318")
      .replace(/Alt\+/g, "\u2325");
  }
  return shortcut
    .replace(/CommandOrControl\+|CmdOrCtrl\+/g, `${L10N.getStr("ctrl")}+`)
    .replace(/Shift\+/g, "Shift+");
}

/**
 * Truncates the received text to the maxLength in the format:
 * Original: 'this is a very long text and ends here'
 * Truncated: 'this is a ver...and ends here'
 * @param {String} sourceText - Source text
 * @param {Number} maxLength - Max allowed length
 * @memberof utils/text
 * @static
 */
export function truncateMiddleText(sourceText, maxLength) {
  let truncatedText = sourceText;
  if (sourceText.length > maxLength) {
    truncatedText = `${sourceText.substring(
      0,
      Math.round(maxLength / 2) - 2
    )}…${sourceText.substring(
      sourceText.length - Math.round(maxLength / 2 - 1)
    )}`;
  }
  return truncatedText;
}
