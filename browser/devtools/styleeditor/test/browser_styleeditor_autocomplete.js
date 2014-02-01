/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "autocomplete.html";
const MAX_SUGGESTIONS = 15;

// Pref which decides if CSS autocompletion is enabled in Style Editor or not.
const AUTOCOMPLETION_PREF = "devtools.styleeditor.autocompletion-enabled";

// Test cases to test that autocompletion works correctly when enabled.
// Format:
// [
//   -1 for pressing Ctrl + Space or the particular key to press,
//   Number of suggestions in the popup (-1 if popup is closed),
//   Index of selected suggestion,
//   1 to check whether the selected suggestion is inserted into the editor or not
// ]
let TEST_CASES = [
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  [-1, 1, 0],
  ['VK_LEFT', -1],
  ['VK_RIGHT', -1],
  ['VK_DOWN', -1],
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  [-1, MAX_SUGGESTIONS, 0],
  ['VK_END', -1],
  ['VK_RETURN', -1],
  ['b', MAX_SUGGESTIONS, 0],
  ['a', 11, 0],
  ['VK_DOWN', 11, 0, 1],
  ['VK_TAB', 11, 1, 1],
  [':', -1],
  ['b', 9, 0],
  ['l', 4, 0],
  ['VK_TAB', 4, 0, 1],
  ['VK_DOWN', 4, 1, 1],
  ['VK_UP', 4, 0, 1],
  ['VK_TAB', 4, 1, 1],
  ['VK_TAB', 4, 2, 1],
  ['VK_LEFT', -1],
  ['VK_RIGHT', -1],
  ['VK_DOWN', -1],
  ['VK_RETURN', -1],
  ['b', 2, 0],
  ['u', 1, 0],
  ['VK_TAB', -1],
  ['{', -1],
  ['VK_HOME', -1],
  ['VK_DOWN', -1],
  ['VK_DOWN', -1],
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  ['VK_RIGHT', -1],
  [-1, 1, 0],
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

  let [key] = TEST_CASES[index];
  let mods = {};

  if (key == -1) {
    info("pressing Ctrl + Space to get result: [" + TEST_CASES[index] +
         "] for index " + index);
    gEditor.once("after-suggest", checkState);
    key = " ";
    mods.accelKey = true;
  }
  else if (/(left|right|return|home|end)/ig.test(key) ||
           (key == "VK_DOWN" && !gPopup.isOpen)) {
    info("pressing key " + key + " to get result: [" + TEST_CASES[index] +
         "] for index " + index);
    gEditor.once("cursorActivity", checkState);
  }
  else if (key == "VK_TAB" || key == "VK_UP" || key == "VK_DOWN") {
    info("pressing key " + key + " to get result: [" + TEST_CASES[index] +
         "] for index " + index);
    gEditor.once("suggestion-entered", checkState);
  }
  else {
    info("pressing key " + key + " to get result: [" + TEST_CASES[index] +
         "] for index " + index);
    gEditor.once("after-suggest", checkState);
  }
  EventUtils.synthesizeKey(key, mods, gPanelWindow);
}

function checkState() {
  executeSoon(() => {
    info("After keypress for index " + index);
    let [key, total, current, inserted] = TEST_CASES[index];
    if (total != -1) {
      ok(gPopup.isOpen, "Popup is open for index " + index);
      is(total, gPopup.itemCount,
         "Correct total suggestions for index " + index);
      is(current, gPopup.selectedIndex,
         "Correct index is selected for index " + index);
      if (inserted) {
        let { preLabel, label } = gPopup.getItemAtIndex(current);
        let { line, ch } = gEditor.getCursor();
        let lineText = gEditor.getText(line);
        is(lineText.substring(ch - label.length, ch), label,
           "Current suggestion from the popup is inserted into the editor.");
      }
    }
    else {
      ok(!gPopup.isOpen, "Popup is closed for index " + index);
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
