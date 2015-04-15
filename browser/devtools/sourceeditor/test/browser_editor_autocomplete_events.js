/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {InspectorFront} = require("devtools/server/actors/inspector");
const AUTOCOMPLETION_PREF = "devtools.editor.autocomplete";
const TEST_URI = "data:text/html;charset=UTF-8,<html><body><bar></bar><div id='baz'></div><body></html>";

add_task(function*() {
  yield promiseTab(TEST_URI);
  yield runTests();
});

function* runTests() {
  let target = devtools.TargetFactory.forTab(gBrowser.selectedTab);
  yield target.makeRemote();
  let inspector = InspectorFront(target.client, target.form);
  let walker = yield inspector.getWalker();
  let {ed, win, edWin} = yield setup(null, {
    autocomplete: true,
    mode: Editor.modes.css,
    autocompleteOpts: {walker: walker}
  });
  yield testMouse(ed, edWin);
  yield testKeyboard(ed, edWin);
  yield testKeyboardCycle(ed, edWin);
  yield testKeyboardCycleForPrefixedString(ed, edWin);
  teardown(ed, win);
}

function* testKeyboard(ed, win) {
  ed.focus();
  ed.setText("b");
  ed.setCursor({line: 1, ch: 1});

  let popupOpened = ed.getAutocompletionPopup().once("popup-opened");

  let autocompleteKey = Editor.keyFor("autocompletion", { noaccel: true }).toUpperCase();
  EventUtils.synthesizeKey("VK_" + autocompleteKey, { ctrlKey: true }, win);

  info ("Waiting for popup to be opened");
  yield popupOpened;

  EventUtils.synthesizeKey("VK_RETURN", { }, win);
  is (ed.getText(), "bar", "Editor text has been updated");
}

function* testKeyboardCycle(ed, win) {
  ed.focus();
  ed.setText("b");
  ed.setCursor({line: 1, ch: 1});

  let popupOpened = ed.getAutocompletionPopup().once("popup-opened");

  let autocompleteKey = Editor.keyFor("autocompletion", { noaccel: true }).toUpperCase();
  EventUtils.synthesizeKey("VK_" + autocompleteKey, { ctrlKey: true }, win);

  info ("Waiting for popup to be opened");
  yield popupOpened;

  EventUtils.synthesizeKey("VK_DOWN", { }, win);
  is (ed.getText(), "bar", "Editor text has been updated");

  EventUtils.synthesizeKey("VK_DOWN", { }, win);
  is (ed.getText(), "body", "Editor text has been updated");

  EventUtils.synthesizeKey("VK_DOWN", { }, win);
  is (ed.getText(), "#baz", "Editor text has been updated");
}

function* testKeyboardCycleForPrefixedString(ed, win) {
  ed.focus();
  ed.setText("#b");
  ed.setCursor({line: 1, ch: 2});

  let popupOpened = ed.getAutocompletionPopup().once("popup-opened");

  let autocompleteKey = Editor.keyFor("autocompletion", { noaccel: true }).toUpperCase();
  EventUtils.synthesizeKey("VK_" + autocompleteKey, { ctrlKey: true }, win);

  info ("Waiting for popup to be opened");
  yield popupOpened;

  EventUtils.synthesizeKey("VK_DOWN", { }, win);
  is (ed.getText(), "#baz", "Editor text has been updated");
}

function* testMouse(ed, win) {
  ed.focus();
  ed.setText("b");
  ed.setCursor({line: 1, ch: 1});

  let popupOpened = ed.getAutocompletionPopup().once("popup-opened");

  let autocompleteKey = Editor.keyFor("autocompletion", { noaccel: true }).toUpperCase();
  EventUtils.synthesizeKey("VK_" + autocompleteKey, { ctrlKey: true }, win);

  info ("Waiting for popup to be opened");
  yield popupOpened;
  ed.getAutocompletionPopup()._list.children[2].click();
  is (ed.getText(), "#baz", "Editor text has been updated");
}
