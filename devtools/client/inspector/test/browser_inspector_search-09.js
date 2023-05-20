/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

requestLongerTimeout(2);

// Test that searching for XPaths via the search field actually selects the
// matching nodes.

const TEST_URL = URL_ROOT + "doc_inspector_search.html";

// The various states of the inspector: [key, id, isTextNode, isValid]
// [
//  what key to press,
//  what id should be selected after the keypress,
//  is the selected node a text node,
//  is the searched text valid selector
// ]
const KEY_STATES = [
  ["/", "b1", false, true], // /
  ["*", "b1", false, true], // /*
  ["VK_RETURN", "root", false, true], // /*
  ["VK_BACK_SPACE", "root", false, true], // /
  ["/", "root", false, true], // //
  ["d", "root", false, true], // //d
  ["i", "root", false, true], // //di
  ["v", "root", false, true], // //div
  ["VK_RETURN", "d1", false, true], // //div
  ["VK_RETURN", "d2", false, true], // //div
  ["VK_RETURN", "d1", false, true], // //div
  ["VK_BACK_SPACE", "d1", false, true], // //di
  ["VK_BACK_SPACE", "d1", false, true], // //d
  ["VK_BACK_SPACE", "d1", false, true], // //
  ["s", "d1", false, true], // //s
  ["p", "d1", false, true], // //sp
  ["a", "d1", false, true], // //spa
  ["n", "d1", false, true], // //span
  ["/", "d1", false, true], // //span/
  ["t", "d1", false, true], // //span/t
  ["e", "d1", false, true], // //span/te
  ["x", "d1", false, true], // //span/tex
  ["t", "d1", false, true], // //span/text
  ["(", "d1", false, true], // //span/text(
  [")", "d1", false, true], // //span/text()
  ["VK_RETURN", "s1", false, true], // //span/text()
  ["VK_RETURN", "s2", true, true], // //span/text()
];

add_task(async function () {
  const { inspector } = await openInspectorForURL(TEST_URL);
  const { searchBox } = inspector;

  await selectNode("#b1", inspector);
  await focusSearchBoxUsingShortcut(inspector.panelWin);

  let index = 0;
  for (const [key, id, isTextNode, isValid] of KEY_STATES) {
    info(index + ": Pressing key " + key + " to get id " + id + ".");
    const onSearchProcessingDone =
      inspector.searchSuggestions.once("processing-done");
    const onSearchResult = inspector.search.once("search-result");
    EventUtils.synthesizeKey(
      key,
      { shiftKey: key === "*" },
      inspector.panelWin
    );

    if (key === "VK_RETURN") {
      info("Waiting for " + (isValid ? "NO " : "") + "results");
      await onSearchResult;
    }

    info("Waiting for search query to complete");
    await onSearchProcessingDone;

    if (isTextNode) {
      info(
        "Text node of " +
          inspector.selection.nodeFront.parentNode.id +
          " is selected with text " +
          searchBox.value
      );

      is(
        inspector.selection.nodeFront.nodeType,
        Node.TEXT_NODE,
        "Correct node is selected for state " + index
      );
    } else {
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
    }

    is(
      !searchBox.parentNode.classList.contains("devtools-searchbox-no-match"),
      isValid,
      "Correct searchbox result state for state " + index
    );

    index++;
  }
});
