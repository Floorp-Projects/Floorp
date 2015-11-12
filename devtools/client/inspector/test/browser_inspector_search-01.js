/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that searching for nodes in the search field actually selects those
// nodes.

const TEST_URL = TEST_URL_ROOT + "doc_inspector_search.html";

// The various states of the inspector: [key, id, isValid]
// [
//  what key to press,
//  what id should be selected after the keypress,
//  is the searched text valid selector
// ]
const KEY_STATES = [
  ["#", "b1", true],                 // #
  ["d", "b1", true],                 // #d
  ["1", "b1", true],                 // #d1
  ["VK_RETURN", "d1", true],         // #d1
  ["VK_BACK_SPACE", "d1", true],     // #d
  ["2", "d1", true],                 // #d2
  ["VK_RETURN", "d2", true],         // #d2
  ["2", "d2", true],                 // #d22
  ["VK_RETURN", "d2", false],        // #d22
  ["VK_BACK_SPACE", "d2", false],    // #d2
  ["VK_RETURN", "d2", true],         // #d2
  ["VK_BACK_SPACE", "d2", true],     // #d
  ["1", "d2", true],                 // #d1
  ["VK_RETURN", "d1", true],         // #d1
  ["VK_BACK_SPACE", "d1", true],     // #d
  ["VK_BACK_SPACE", "d1", true],     // #
  ["VK_BACK_SPACE", "d1", true],     //
  ["d", "d1", true],                 // d
  ["i", "d1", true],                 // di
  ["v", "d1", true],                 // div
  [".", "d1", true],                 // div.
  ["c", "d1", true],                 // div.c
  ["VK_UP", "d1", true],             // div.c1
  ["VK_TAB", "d1", true],            // div.c1
  ["VK_RETURN", "d2", true],         // div.c1
  ["VK_BACK_SPACE", "d2", true],     // div.c
  ["VK_BACK_SPACE", "d2", true],     // div.
  ["VK_BACK_SPACE", "d2", true],     // div
  ["VK_BACK_SPACE", "d2", true],     // di
  ["VK_BACK_SPACE", "d2", true],     // d
  ["VK_BACK_SPACE", "d2", true],     //
  [".", "d2", true],                 // .
  ["c", "d2", true],                 // .c
  ["1", "d2", true],                 // .c1
  ["VK_RETURN", "d2", true],         // .c1
  ["VK_RETURN", "s2", true],         // .c1
  ["VK_RETURN", "p1", true],         // .c1
  ["P", "p1", true],                 // .c1P
  ["VK_RETURN", "p1", false],        // .c1P
];

add_task(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URL);
  let { searchBox } = inspector;

  yield selectNode("#b1", inspector);
  yield focusSearchBoxUsingShortcut(inspector.panelWin);

  let index = 0;
  for (let [ key, id, isValid ] of KEY_STATES) {
    info(index + ": Pressing key " + key + " to get id " + id + ".");
    let done = inspector.searchSuggestions.once("processing-done");
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
    yield done;
    info("Got processing-done event");

    if (key === "VK_RETURN") {
      info ("Waiting for " + (isValid ? "NO " : "") + "results");
      yield inspector.search.once("search-result");
    }

    info("Waiting for search query to complete");
    yield inspector.searchSuggestions._lastQuery;

    info(inspector.selection.nodeFront.id + " is selected with text " +
         searchBox.value);
    let nodeFront = yield getNodeFront("#" + id, inspector);
    is(inspector.selection.nodeFront, nodeFront,
       "Correct node is selected for state " + index);

    is(!searchBox.classList.contains("devtools-no-search-result"), isValid,
       "Correct searchbox result state for state " + index);

    index++;
  }
});
