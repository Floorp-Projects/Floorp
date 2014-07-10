/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that searching for nodes in the search field actually selects those
// nodes.

const TEST_URL = TEST_URL_ROOT + "doc_inspector_search.html";

// Indexes of the keys in the KEY_STATES array that should listen to "keypress"
// event instead of "command". These are keys that don't change the content of
// the search field and thus don't trigger command event.
const LISTEN_KEYPRESS = [3,4,8,18,19,20,21,22];

// The various states of the inspector: [key, id, isValid]
// [
//  what key to press,
//  what id should be selected after the keypress,
//  is the searched text valid selector
// ]
const KEY_STATES = [
  ["d", "b1", false],
  ["i", "b1", false],
  ["v", "d1", true],
  ["VK_DOWN", "d2", true], // keypress
  ["VK_RETURN", "d1", true], //keypress
  [".", "d1", false],
  ["c", "d1", false],
  ["1", "d2", true],
  ["VK_DOWN", "d2", true], // keypress
  ["VK_BACK_SPACE", "d2", false],
  ["VK_BACK_SPACE", "d2", false],
  ["VK_BACK_SPACE", "d1", true],
  ["VK_BACK_SPACE", "d1", false],
  ["VK_BACK_SPACE", "d1", false],
  ["VK_BACK_SPACE", "d1", true],
  [".", "d1", false],
  ["c", "d1", false],
  ["1", "d2", true],
  ["VK_DOWN", "s2", true], // keypress
  ["VK_DOWN", "p1", true], // kepress
  ["VK_UP", "s2", true], // keypress
  ["VK_UP", "d2", true], // keypress
  ["VK_UP", "p1", true],
  ["VK_BACK_SPACE", "p1", false],
  ["2", "p3", true],
  ["VK_BACK_SPACE", "p3", false],
  ["VK_BACK_SPACE", "p3", false],
  ["VK_BACK_SPACE", "p3", true],
  ["r", "p3", false],
];

let test = asyncTest(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URL);
  let { searchBox } = inspector;

  yield selectNode("#b1", inspector);
  yield focusSearchBoxUsingShortcut(inspector.panelWin);

  let index = 0;
  for (let [ key, id, isValid ] of KEY_STATES) {
    let event = (LISTEN_KEYPRESS.indexOf(index) !== -1) ? "keypress" : "command";
    let eventHandled = once(searchBox, event, true);

    info(index + ": Pressing key " + key + " to get id " + id);
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
    yield eventHandled;

    info("Got " + event + " event. Waiting for search query to complete");
    yield inspector.searchSuggestions._lastQuery;

    info(inspector.selection.node.id + " is selected with text " +
         searchBox.value);
    is(inspector.selection.node, content.document.getElementById(id),
       "Correct node is selected for state " + index);
    is(!searchBox.classList.contains("devtools-no-search-result"), isValid,
       "Correct searchbox result state for state " + index);

    index++;
  }
});
