/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that searching for elements inside iframes does work.

const IFRAME_SRC = "doc_inspector_search.html";
const TEST_URL = "data:text/html;charset=utf-8," +
                 "<div class=\"c1 c2\">" +
                 "<iframe src=\"" + TEST_URL_ROOT + IFRAME_SRC + "\"></iframe>" +
                 "<iframe src=\"" + TEST_URL_ROOT + IFRAME_SRC + "\"></iframe>";

// An array of (key, suggestions) pairs where key is a key to press and
// suggestions is an array of suggestions that should be shown in the popup.
// Suggestion is an object with label of the entry and optional count
// (defaults to 1)
let TEST_DATA = [
  {
    key: "d",
    suggestions: [
      {label: "div", count: 5},
      {label: "#d1", count: 2},
      {label: "#d2", count: 2}
    ]
  },
  {
    key: "i",
    suggestions: [{label: "div", count: 5}]
  },
  {
    key: "v",
    suggestions: []
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [{label: "div", count: 5}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [
      {label: "div", count: 5},
      {label: "#d1", count: 2},
      {label: "#d2", count: 2}
    ]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: ".",
    suggestions: [
      {label: ".c1", count: 7},
      {label: ".c2", count: 3}
    ]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: "#",
    suggestions: [
      {label: "#b1", count: 2},
      {label: "#d1", count: 2},
      {label: "#d2", count: 2},
      {label: "#p1", count: 2},
      {label: "#p2", count: 2},
      {label: "#p3", count: 2},
      {label: "#s1", count: 2},
      {label: "#s2", count: 2}
    ]
  },
];

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URL);
  let searchBox = inspector.searchBox;
  let popup = inspector.searchSuggestions.searchPopup;

  yield focusSearchBoxUsingShortcut(inspector.panelWin);

  for (let {key, suggestions} of TEST_DATA) {
    info("Pressing " + key + " to get " + formatSuggestions(suggestions));

    let command = once(searchBox, "command");
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
    yield command;

    info("Waiting for search query to complete");
    yield inspector.searchSuggestions._lastQuery;

    info("Query completed. Performing checks for input '" + searchBox.value + "'");
    let actualSuggestions = popup.getItems().reverse();

    is(popup.isOpen ? actualSuggestions.length: 0, suggestions.length,
       "There are expected number of suggestions.");

    for (let i = 0; i < suggestions.length; i++) {
      is(actualSuggestions[i].label, suggestions[i].label,
         "The suggestion at " + i + "th index is correct.");
      is(actualSuggestions[i].count, suggestions[i].count || 1,
         "The count for suggestion at " + i + "th index is correct.");
    }
  }
});

function formatSuggestions(suggestions) {
  return "[" + suggestions
                .map(s => "'" + s.label + "' (" + (s.count || 1) + ")")
                .join(", ") + "]";
}
