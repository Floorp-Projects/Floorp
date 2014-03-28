/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "autocomplete.html";
const MAX_SUGGESTIONS = 15;

// Pref which decides if CSS autocompletion is enabled in Style Editor or not.
const AUTOCOMPLETION_PREF = "devtools.styleeditor.autocompletion-enabled";

const {CSSProperties, CSSValues} = getCSSKeywords();

// Test cases to test that autocompletion works correctly when enabled.
// Format:
// [
//   key,
//   {
//     total: Number of suggestions in the popup (-1 if popup is closed),
//     current: Index of selected suggestion,
//     inserted: 1 to check whether the selected suggestion is inserted into the editor or not,
//     entered: 1 if the suggestion is inserted and finalized
//   }
// ]
let TEST_CASES = [
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['Ctrl+Space', {total: 1, current: 0}],
  ['VK_LEFT'],
  ['VK_RIGHT'],
  ['VK_DOWN'],
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['Ctrl+Space', { total: getSuggestionNumberFor("font"), current: 0}],
  ['VK_END'],
  ['VK_RETURN'],
  ['b', {total: getSuggestionNumberFor("b"), current: 0}],
  ['a', {total: getSuggestionNumberFor("ba"), current: 0}],
  ['VK_DOWN', {total: getSuggestionNumberFor("ba"), current: 0, inserted: 1}],
  ['VK_TAB', {total: getSuggestionNumberFor("ba"), current: 1, inserted: 1}],
  ['VK_RETURN', {current: 1, inserted: 1, entered: 1}],
  ['b', {total: getSuggestionNumberFor("background", "b"), current: 0}],
  ['l', {total: getSuggestionNumberFor("background", "bl"), current: 0}],
  ['VK_TAB', {total: getSuggestionNumberFor("background", "bl"), current: 0, inserted: 1}],
  ['VK_DOWN', {total: getSuggestionNumberFor("background", "bl"), current: 1, inserted: 1}],
  ['VK_UP', {total: getSuggestionNumberFor("background", "bl"), current: 0, inserted: 1}],
  ['VK_TAB', {total: getSuggestionNumberFor("background", "bl"), current: 1, inserted: 1}],
  ['VK_TAB', {total: getSuggestionNumberFor("background", "bl"), current: 2, inserted: 1}],
  [';'],
  ['VK_RETURN'],
  ['c', {total: getSuggestionNumberFor("c"), current: 0}],
  ['o', {total: getSuggestionNumberFor("co"), current: 0}],
  ['VK_RETURN', {current: 0, inserted: 1}],
  ['r', {total: getSuggestionNumberFor("color", "r"), current: 0}],
  ['VK_RETURN', {current: 0, inserted: 1}],
  [';'],
  ['VK_LEFT'],
  ['VK_RIGHT'],
  ['VK_DOWN'],
  ['VK_RETURN'],
  ['b', {total: 2, current: 0}],
  ['u', {total: 1, current: 0}],
  ['VK_RETURN', {current: 0, inserted: 1}],
  ['{'],
  ['VK_HOME'],
  ['VK_DOWN'],
  ['VK_DOWN'],
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['VK_RIGHT'],
  ['Ctrl+Space', {total: 1, current: 0}],
];

let gEditor;
let gPopup;
let index = 0;

function test()
{
  waitForExplicitFinish();

  addTabAndOpenStyleEditor(function(panel) {
    panel.UI.on("editor-added", testEditorAdded);
  });

  content.location = TESTCASE_URI;
}

function testEditorAdded(aEvent, aEditor) {
  info("Editor added, getting the source editor and starting tests");
  aEditor.getSourceEditor().then(editor => {
    info("source editor found, starting tests.");
    gEditor = editor.sourceEditor;
    gPopup = gEditor.getAutocompletionPopup();
    waitForFocus(testState, gPanelWindow);
  });
}

function testState() {
  if (index == TEST_CASES.length) {
    testAutocompletionDisabled();
    return;
  }

  let [key, details] = TEST_CASES[index];
  let entered;
  if (details) {
    entered = details.entered;
  }
  let mods = {};

  info("pressing key " + key + " to get result: " +
                JSON.stringify(TEST_CASES[index]) + " for index " + index);

  let evt = "after-suggest";

  if (key == 'Ctrl+Space') {
    key = " ";
    mods.accelKey = true;
  }
  else if (key == "VK_RETURN" && entered) {
    evt = "popup-hidden";
  }
  else if (/(left|right|return|home|end)/ig.test(key) ||
           (key == "VK_DOWN" && !gPopup.isOpen)) {
    evt = "cursorActivity";
  }
  else if (key == "VK_TAB" || key == "VK_UP" || key == "VK_DOWN") {
    evt = "suggestion-entered";
  }

  gEditor.once(evt, checkState);
  EventUtils.synthesizeKey(key, mods, gPanelWindow);
}

function checkState() {
  executeSoon(() => {
    let [key, details] = TEST_CASES[index];
    details = details || {};
    let {total, current, inserted} = details;

    if (total != undefined) {
      ok(gPopup.isOpen, "Popup is open for index " + index);
      is(total, gPopup.itemCount,
         "Correct total suggestions for index " + index);
      is(current, gPopup.selectedIndex,
         "Correct index is selected for index " + index);
      if (inserted) {
        let { preLabel, label, text } = gPopup.getItemAtIndex(current);
        let { line, ch } = gEditor.getCursor();
        let lineText = gEditor.getText(line);
        is(lineText.substring(ch - text.length, ch), text,
           "Current suggestion from the popup is inserted into the editor.");
      }
    }
    else {
      ok(!gPopup.isOpen, "Popup is closed for index " + index);
      if (inserted) {
        let { preLabel, label, text } = gPopup.getItemAtIndex(current);
        let { line, ch } = gEditor.getCursor();
        let lineText = gEditor.getText(line);
        is(lineText.substring(ch - text.length, ch), text,
           "Current suggestion from the popup is inserted into the editor.");
      }
    }
    index++;
    testState();
  });
}

function testAutocompletionDisabled() {
  gBrowser.removeCurrentTab();

  index = 0;
  info("Starting test to check if autocompletion is disabled correctly.")
  Services.prefs.setBoolPref(AUTOCOMPLETION_PREF, false);

  addTabAndOpenStyleEditor(function(panel) {
    panel.UI.on("editor-added", testEditorAddedDisabled);
  });

  content.location = TESTCASE_URI;
}

function testEditorAddedDisabled(aEvent, aEditor) {
  info("Editor added, getting the source editor and starting tests");
  aEditor.getSourceEditor().then(editor => {
    ok(!editor.sourceEditor.getAutocompletionPopup,
       "Autocompletion popup does not exist");
    cleanup();
  });
}

function cleanup() {
  Services.prefs.clearUserPref(AUTOCOMPLETION_PREF);
  gEditor = null;
  gPopup = null;
  finish();
}

/**
 * Returns a list of all property names and a map of property name vs possible
 * CSS values provided by the Gecko engine.
 *
 * @return {Object} An object with following properties:
 *         - CSSProperties {Array} Array of string containing all the possible
 *                         CSS property names.
 *         - CSSValues {Object|Map} A map where key is the property name and
 *                     value is an array of string containing all the possible
 *                     CSS values the property can have.
 */
function getCSSKeywords() {
  let domUtils = Cc["@mozilla.org/inspector/dom-utils;1"]
                   .getService(Ci.inIDOMUtils);
  let props = {};
  let propNames = domUtils.getCSSPropertyNames(domUtils.INCLUDE_ALIASES);
  propNames.forEach(prop => {
    props[prop] = domUtils.getCSSValuesForProperty(prop).sort();
  });
  return {
    CSSValues: props,
    CSSProperties: propNames.sort()
  };
}

/**
 * Returns the number of expected suggestions for the given property and value.
 * If the value is not null, returns the number of values starting with `value`.
 * Returns the number of properties starting with `property` otherwise.
 */
function getSuggestionNumberFor(property, value) {
  if (value == null) {
    return CSSProperties.filter(prop => prop.startsWith(property))
                        .slice(0, MAX_SUGGESTIONS).length;
  }
  return CSSValues[property].filter(val => val.startsWith(value))
                            .slice(0, MAX_SUGGESTIONS).length;
}
