/* -*- indent-tabs-mode: nil; js-indent-level: 2; fill-column: 80 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/locale/sourceeditor.properties");

const FIND_KEY = L10N.getStr("find.commandkey");
const FINDAGAIN_KEY = L10N.getStr("findAgain.commandkey");

const { OS } = Services.appinfo;

// On linux, getting immediately the selection's range here fails, returning
// values like it's not selected – even if the selection is visible.
// For the record, setting the selection's range immediately doesn't have
// any effect.
// It's like the <input> is not ready yet.
// Therefore, we trigger the UI focus event to the <input>, waiting for the
// response.
// Using a timeout could also work, but that is more precise, ensuring also
// the execution of the listeners added to the <input>'s focus.
const dispatchAndWaitForFocus = (target) => new Promise((resolve) => {
  target.addEventListener("focus", function listener() {
    target.removeEventListener("focus", listener);
    resolve(target);
  });

  target.dispatchEvent(new UIEvent("focus"));
});

function openSearchBox(ed) {
  let edDoc = ed.container.contentDocument;
  let edWin = edDoc.defaultView;

  let input = edDoc.querySelector("input[type=search]");
  ok(!input, "search box closed");

  // The editor needs the focus to properly receive the `synthesizeKey`
  ed.focus();

  EventUtils.synthesizeKey(FINDAGAIN_KEY, { accelKey: true }, edWin);

  input = edDoc.querySelector("input[type=search]");
  ok(input, "find again command key opens the search box");
}

function testFindAgain(ed, inputLine, expectCursor, shiftKey = false) {
  let edDoc = ed.container.contentDocument;
  let edWin = edDoc.defaultView;

  let input = edDoc.querySelector("input[type=search]");
  input.value = inputLine;

  // Ensure the input has the focus before send the key – necessary on Linux,
  // it seems that during the tests can be lost
  input.focus();

  EventUtils.synthesizeKey(FINDAGAIN_KEY, { accelKey: true, shiftKey }, edWin);

  ch(ed.getCursor(), expectCursor,
    "find: " + inputLine + " expects cursor: " + expectCursor.toSource());
}

const testSearchBoxTextIsSelected = Task.async(function* (ed) {
  let edDoc = ed.container.contentDocument;
  let edWin = edDoc.defaultView;

  let input = edDoc.querySelector("input[type=search]");
  ok(input, "search box is opened");

  // Ensure the input has the focus before send the key – necessary on Linux,
  // it seems that during the tests can be lost
  input.focus();

  // Close search box
  EventUtils.synthesizeKey("VK_ESCAPE", {}, edWin);

  input = edDoc.querySelector("input[type=search]");
  ok(!input, "search box is closed");

  // Re-open the search box
  EventUtils.synthesizeKey(FIND_KEY, { accelKey: true }, edWin);

  input = edDoc.querySelector("input[type=search]");
  ok(input, "find command key opens the search box");

  yield dispatchAndWaitForFocus(input);

  let { selectionStart, selectionEnd, value } = input;

  ok(selectionStart === 0 && selectionEnd === value.length,
    "search box's text is selected when re-opened");

  // Removing selection
  input.setSelectionRange(0, 0);

  EventUtils.synthesizeKey(FIND_KEY, { accelKey: true }, edWin);

  ({ selectionStart, selectionEnd } = input);

  ok(selectionStart === 0 && selectionEnd === value.length,
    "search box's text is selected when find key is pressed");

  // Close search box
  EventUtils.synthesizeKey("VK_ESCAPE", {}, edWin);
});

const testReplaceBoxTextIsSelected = Task.async(function* (ed) {
  let edDoc = ed.container.contentDocument;
  let edWin = edDoc.defaultView;

  let input = edDoc.querySelector(".CodeMirror-dialog > input");
  ok(!input, "dialog box with replace is closed");

  // The editor needs the focus to properly receive the `synthesizeKey`
  ed.focus();

  // Send the replace's key with the appropriate modifiers based on OS
  let [altKey, shiftKey] = OS === "Darwin" ? [true, false] : [false, true];

  EventUtils.synthesizeKey(FIND_KEY,
    { accelKey: true, altKey, shiftKey }, edWin);

  input = edDoc.querySelector(".CodeMirror-dialog > input");
  ok(input, "dialog box with replace is opened");

  input.value = "line 5";

  // Ensure the input has the focus before send the key – necessary on Linux,
  // it seems that during the tests can be lost
  input.focus();

  yield dispatchAndWaitForFocus(input);

  let { selectionStart, selectionEnd, value } = input;

  ok(!(selectionStart === 0 && selectionEnd === value.length),
    "Text in dialog box is not selected");

  EventUtils.synthesizeKey(FIND_KEY,
    { accelKey: true, altKey, shiftKey }, edWin);

  ({ selectionStart, selectionEnd } = input);

  ok(selectionStart === 0 && selectionEnd === value.length,
    "dialog box's text is selected when replace key is pressed");

  // Close dialog box
  EventUtils.synthesizeKey("VK_ESCAPE", {}, edWin);
});

add_task(function* () {
  let { ed, win } = yield setup();

  ed.setText([
    "// line 1",
    "//  line 2",
    "//   line 3",
    "//    line 4",
    "//     line 5"
  ].join("\n"));

  yield promiseWaitForFocus();

  openSearchBox(ed);

  let testVectors = [
    // Starting here expect data needs to get updated for length changes to
    // "textLines" above.
    ["line",
     {line: 0, ch: 7}],
    ["line",
     {line: 1, ch: 8}],
    ["line",
     {line: 2, ch: 9}],
    ["line",
     {line: 3, ch: 10}],
    ["line",
     {line: 4, ch: 11}],
    ["ne 3",
     {line: 2, ch: 11}],
    ["line 1",
      {line: 0, ch: 9}],
    // Testing find prev
    ["line",
      {line: 4, ch: 11},
      true],
    ["line",
      {line: 3, ch: 10},
      true],
    ["line",
      {line: 2, ch: 9},
      true],
    ["line",
      {line: 1, ch: 8},
      true],
    ["line",
      {line: 0, ch: 7},
      true]
  ];

  for (let v of testVectors) {
    yield testFindAgain(ed, ...v);
  }

  yield testSearchBoxTextIsSelected(ed);

  yield testReplaceBoxTextIsSelected(ed);

  teardown(ed, win);
});
