/* -*- indent-tabs-mode: nil; js-indent-level: 2; fill-column: 80 -*- */
/* vim:set ts=2 sw=2 sts=2 et tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const Services = require("Services");

const EXPAND_TAB = "devtools.editor.expandtab";
const TAB_SIZE = "devtools.editor.tabsize";
const DETECT_INDENT = "devtools.editor.detectindentation";
const DETECT_INDENT_MAX_LINES = 500;

/**
 * Get the indentation to use in an editor, or return false if the user has
 * asked for the indentation to be guessed from some text.
 *
 * @return {false | Object}
 *        Returns false if the "detect indentation" pref is set.
 *        an object of the form {indentUnit, indentWithTabs}.
 *        |indentUnit| is the number of indentation units to use
 *        to indent a "block".
 *        |indentWithTabs| is a boolean which is true if indentation
 *        should be done using tabs.
 */
function getIndentationFromPrefs() {
  let shouldDetect = Services.prefs.getBoolPref(DETECT_INDENT);
  if (shouldDetect) {
    return false;
  }

  let indentWithTabs = !Services.prefs.getBoolPref(EXPAND_TAB);
  let indentUnit = Services.prefs.getIntPref(TAB_SIZE);
  return {indentUnit, indentWithTabs};
}

/**
 * Given a function that can iterate over some text, compute the indentation to
 * use.  This consults various prefs to arrive at a decision.
 *
 * @param {Function} iterFunc A function of three arguments:
 *        (start, end, callback); where |start| and |end| describe
 *        the range of text lines to examine, and |callback| is a function
 *        to be called with the text of each line.
 *
 * @return {Object} an object of the form {indentUnit, indentWithTabs}.
 *        |indentUnit| is the number of indentation units to use
 *        to indent a "block".
 *        |indentWithTabs| is a boolean which is true if indentation
 *        should be done using tabs.
 */
function getIndentationFromIteration(iterFunc) {
  let indentWithTabs = !Services.prefs.getBoolPref(EXPAND_TAB);
  let indentUnit = Services.prefs.getIntPref(TAB_SIZE);
  let shouldDetect = Services.prefs.getBoolPref(DETECT_INDENT);

  if (shouldDetect) {
    let indent = detectIndentation(iterFunc);
    if (indent != null) {
      indentWithTabs = indent.tabs;
      indentUnit = indent.spaces ? indent.spaces : indentUnit;
    }
  }

  return {indentUnit, indentWithTabs};
}

/**
 * A wrapper for @see getIndentationFromIteration which computes the
 * indentation of a given string.
 *
 * @param {String} string the input text
 * @return {Object} an object of the same form as returned by
 *                  getIndentationFromIteration
 */
function getIndentationFromString(string) {
  let iteratorFn = function (start, end, callback) {
    let split = string.split(/\r\n|\r|\n|\f/);
    split.slice(start, end).forEach(callback);
  };
  return getIndentationFromIteration(iteratorFn);
}

/**
 * Detect the indentation used in an editor. Returns an object
 * with 'tabs' - whether this is tab-indented and 'spaces' - the
 * width of one indent in spaces. Or `null` if it's inconclusive.
 */
function detectIndentation(textIteratorFn) {
  // # spaces indent -> # lines with that indent
  let spaces = {};
  // indentation width of the last line we saw
  let last = 0;
  // # of lines that start with a tab
  let tabs = 0;
  // # of indented lines (non-zero indent)
  let total = 0;

  textIteratorFn(0, DETECT_INDENT_MAX_LINES, (text) => {
    if (text.startsWith("\t")) {
      tabs++;
      total++;
      return;
    }
    let width = 0;
    while (text[width] === " ") {
      width++;
    }
    // don't count lines that are all spaces
    if (width == text.length) {
      last = 0;
      return;
    }
    if (width > 1) {
      total++;
    }

    // see how much this line is offset from the line above it
    let indent = Math.abs(width - last);
    if (indent > 1 && indent <= 8) {
      spaces[indent] = (spaces[indent] || 0) + 1;
    }
    last = width;
  });

  // this file is not indented at all
  if (total == 0) {
    return null;
  }

  // mark as tabs if they start more than half the lines
  if (tabs >= total / 2) {
    return { tabs: true };
  }

  // find most frequent non-zero width difference between adjacent lines
  let freqIndent = null, max = 1;
  for (let width in spaces) {
    width = parseInt(width, 10);
    let tally = spaces[width];
    if (tally > max) {
      max = tally;
      freqIndent = width;
    }
  }
  if (!freqIndent) {
    return null;
  }

  return { tabs: false, spaces: freqIndent };
}

exports.EXPAND_TAB = EXPAND_TAB;
exports.TAB_SIZE = TAB_SIZE;
exports.DETECT_INDENT = DETECT_INDENT;
exports.getIndentationFromPrefs = getIndentationFromPrefs;
exports.getIndentationFromIteration = getIndentationFromIteration;
exports.getIndentationFromString = getIndentationFromString;
