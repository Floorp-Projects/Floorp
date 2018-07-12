/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that searching for classes on SVG elements does work (see bug 1219920).

const TEST_URL = URL_ROOT + "doc_inspector_search-svg.html";

// An array of (key, suggestions) pairs where key is a key to press and
// suggestions is an array of suggestions that should be shown in the popup.
const TEST_DATA = [{
  key: "c",
  suggestions: ["circle", "clipPath", ".class1", ".class2"]
}, {
  key: "VK_BACK_SPACE",
  suggestions: []
}, {
  key: ".",
  suggestions: [".class1", ".class2"]
}];

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);
  const {searchBox} = inspector;
  const popup = inspector.searchSuggestions.searchPopup;

  await focusSearchBoxUsingShortcut(inspector.panelWin);

  for (const {key, suggestions} of TEST_DATA) {
    info("Pressing " + key + " to get " + suggestions);

    const command = once(searchBox, "input");
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
    await command;

    info("Waiting for search query to complete and getting the suggestions");
    await inspector.searchSuggestions._lastQuery;
    const actualSuggestions = popup.getItems();

    is(popup.isOpen ? actualSuggestions.length : 0, suggestions.length,
       "There are expected number of suggestions.");

    for (let i = 0; i < suggestions.length; i++) {
      is(actualSuggestions[i].label, suggestions[i],
         "The suggestion at " + i + "th index is correct.");
    }
  }
});
