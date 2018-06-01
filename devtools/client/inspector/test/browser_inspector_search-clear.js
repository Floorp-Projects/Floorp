/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Bug 1295081 Test searchbox clear button's display behavior is correct

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

// Type "d" in inspector-searchbox, Enter [Back space] key and check if the
// clear button is shown correctly
add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URI);
  const {searchBox, searchClearButton} = inspector;

  await focusSearchBoxUsingShortcut(inspector.panelWin);

  info("Type d and the clear button will be shown");

  const command = once(searchBox, "input");
  EventUtils.synthesizeKey("c", {}, inspector.panelWin);
  await command;

  info("Waiting for search query to complete and getting the suggestions");
  await inspector.searchSuggestions._lastQuery;

  ok(!searchClearButton.hidden,
    "The clear button is shown when some word is in searchBox");

  EventUtils.synthesizeKey("VK_BACK_SPACE", {}, inspector.panelWin);
  await command;

  info("Waiting for search query to complete and getting the suggestions");
  await inspector.searchSuggestions._lastQuery;

  ok(searchClearButton.hidden, "The clear button is hidden when no word is in searchBox");
});
