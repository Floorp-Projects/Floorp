/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

requestLongerTimeout(2);

// Test that searching for nodes in the search field actually selects those
// nodes.

const TEST_URL = URL_ROOT + "doc_inspector_search.html";

// The various states of the inspector: [key, id, isValid]
// [
//  what key to press,
//  what id should be selected after the keypress,
//  is the searched text valid selector
// ]
const KEY_STATES = [
  ["#", "b1", true], // #
  ["d", "b1", true], // #d
  ["1", "b1", true], // #d1
  ["VK_RETURN", "d1", true], // #d1
  ["VK_BACK_SPACE", "d1", true], // #d
  ["2", "d1", true], // #d2
  ["VK_RETURN", "d2", true], // #d2
  ["2", "d2", true], // #d22
  ["VK_RETURN", "d2", false], // #d22
  ["VK_BACK_SPACE", "d2", false], // #d2
  ["VK_RETURN", "d2", true], // #d2
  ["VK_BACK_SPACE", "d2", true], // #d
  ["1", "d2", true], // #d1
  ["VK_RETURN", "d1", true], // #d1
  ["VK_BACK_SPACE", "d1", true], // #d
  ["VK_BACK_SPACE", "d1", true], // #
  ["VK_BACK_SPACE", "d1", true], //
  ["d", "d1", true], // d
  ["i", "d1", true], // di
  ["v", "d1", true], // div
  [".", "d1", true], // div.
  ["c", "d1", true], // div.c
  ["VK_UP", "d1", true], // div.c1
  ["VK_TAB", "d1", true], // div.c1
  ["VK_RETURN", "d2", true], // div.c1
  ["VK_BACK_SPACE", "d2", true], // div.c
  ["VK_BACK_SPACE", "d2", true], // div.
  ["VK_BACK_SPACE", "d2", true], // div
  ["VK_BACK_SPACE", "d2", true], // di
  ["VK_BACK_SPACE", "d2", true], // d
  ["VK_BACK_SPACE", "d2", true], //
  [".", "d2", true], // .
  ["c", "d2", true], // .c
  ["1", "d2", true], // .c1
  ["VK_RETURN", "d2", true], // .c1
  ["VK_RETURN", "s2", true], // .c1
  ["VK_RETURN", "p1", true], // .c1
  ["P", "p1", true], // .c1P
  ["VK_RETURN", "p1", false], // .c1P
];

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);
  const { searchBox } = inspector;

  await selectNode("#b1", inspector);
  await focusSearchBoxUsingShortcut(inspector.panelWin);

  let index = 0;
  for (const [key, id, isValid] of KEY_STATES) {
    const promises = [];
    info(index + ": Pressing key " + key + " to get id " + id + ".");

    info("Waiting for current key press processing to complete");
    promises.push(inspector.searchSuggestions.once("processing-done"));

    if (key === "VK_RETURN") {
      info("Waiting for " + (isValid ? "NO " : "") + "results");
      promises.push(inspector.search.once("search-result"));
    }

    info("Waiting for search query to complete");
    promises.push(inspector.searchSuggestions.once("processing-done"));

    EventUtils.synthesizeKey(key, {}, inspector.panelWin);

    await Promise.all(promises);
    info(
      "The keypress press process, any possible search results and the search query are complete."
    );

    info(
      inspector.selection.nodeFront.id +
        " is selected with text " +
        searchBox.value
    );
    const nodeFront = await getNodeFront("#" + id, inspector);
    is(
      inspector.selection.nodeFront,
      nodeFront,
      "Correct node is selected for state " + index
    );

    is(
      !searchBox.parentNode.classList.contains("devtools-searchbox-no-match"),
      isValid,
      "Correct searchbox result state for state " + index
    );

    index++;
  }
});
