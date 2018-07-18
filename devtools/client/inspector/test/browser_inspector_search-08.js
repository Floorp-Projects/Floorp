/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that searching for namespaced elements does work.

const XHTML = `
  <!DOCTYPE html>
  <html xmlns="http://www.w3.org/1999/xhtml"
        xmlns:svg="http://www.w3.org/2000/svg">
    <body>
      <svg:svg width="100" height="100">
        <svg:clipPath>
          <svg:rect x="0" y="0" width="10" height="5"></svg:rect>
        </svg:clipPath>
        <svg:circle cx="0" cy="0" r="5"></svg:circle>
      </svg:svg>
    </body>
  </html>
`;

const TEST_URI = "data:application/xhtml+xml;charset=utf-8," + encodeURI(XHTML);

// An array of (key, suggestions) pairs where key is a key to press and
// suggestions is an array of suggestions that should be shown in the popup.
const TEST_DATA = [{
  key: "c",
  suggestions: ["circle", "clipPath"]
}, {
  key: "VK_BACK_SPACE",
  suggestions: []
}, {
  key: "s",
  suggestions: ["svg"]
}];

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URI);
  const {searchBox} = inspector;
  const popup = inspector.searchSuggestions.searchPopup;

  await focusSearchBoxUsingShortcut(inspector.panelWin);

  for (const {key, suggestions} of TEST_DATA) {
    info("Pressing " + key + " to get " + suggestions.join(", "));

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
