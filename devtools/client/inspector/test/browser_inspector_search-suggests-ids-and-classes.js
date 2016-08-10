/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the selector-search input proposes ids and classes even when . and
// # is missing, but that this only occurs when the query is one word (no
// selector combination)

// The various states of the inspector: [key, suggestions array]
// [
//  what key to press,
//  suggestions array with count [
//    [suggestion1, count1], [suggestion2] ...
//  ] count can be left to represent 1
// ]
const KEY_STATES = [
  ["s", [["span", 1], [".span", 1], ["#span", 1]]],
  ["p", [["span", 1], [".span", 1], ["#span", 1]]],
  ["a", [["span", 1], [".span", 1], ["#span", 1]]],
  ["n", []],
  [" ", [["span div", 1]]],
  // mixed tag/class/id suggestions only work for the first word
  ["d", [["span div", 1]]],
  ["VK_BACK_SPACE", [["span div", 1]]],
  ["VK_BACK_SPACE", []],
  ["VK_BACK_SPACE", [["span", 1], [".span", 1], ["#span", 1]]],
  ["VK_BACK_SPACE", [["span", 1], [".span", 1], ["#span", 1]]],
  ["VK_BACK_SPACE", [["span", 1], [".span", 1], ["#span", 1]]],
  ["VK_BACK_SPACE", []],
  // Test that mixed tags, classes and ids are grouped by types, sorted by
  // count and alphabetical order
  ["b", [
    ["button", 3],
    ["body", 1],
    [".bc", 3],
    [".ba", 1],
    [".bb", 1],
    ["#ba", 1],
    ["#bb", 1],
    ["#bc", 1]
  ]],
];

const TEST_URL = `<span class="span" id="span">
                    <div class="div" id="div"></div>
                  </span>
                  <button class="ba bc" id="bc"></button>
                  <button class="bb bc" id="bb"></button>
                  <button class="bc" id="ba"></button>`;

add_task(function* () {
  let {inspector} = yield openInspectorForURL("data:text/html;charset=utf-8," +
                                              encodeURI(TEST_URL));

  let searchBox = inspector.panelWin.document.getElementById(
    "inspector-searchbox");
  let popup = inspector.searchSuggestions.searchPopup;

  yield focusSearchBoxUsingShortcut(inspector.panelWin);

  for (let [key, expectedSuggestions] of KEY_STATES) {
    info("pressing key " + key + " to get suggestions " +
         JSON.stringify(expectedSuggestions));

    let onCommand = once(searchBox, "input", true);
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
    yield onCommand;

    info("Waiting for the suggestions to be retrieved");
    yield inspector.searchSuggestions._lastQuery;

    let actualSuggestions = popup.getItems();
    is(popup.isOpen ? actualSuggestions.length : 0, expectedSuggestions.length,
       "There are expected number of suggestions");
    actualSuggestions.reverse();

    for (let i = 0; i < expectedSuggestions.length; i++) {
      is(expectedSuggestions[i][0], actualSuggestions[i].label,
         "The suggestion at " + i + "th index is correct.");
      is(expectedSuggestions[i][1] || 1, actualSuggestions[i].count,
         "The count for suggestion at " + i + "th index is correct.");
    }
  }
});
