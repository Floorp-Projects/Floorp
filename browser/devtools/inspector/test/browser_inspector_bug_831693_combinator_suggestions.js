/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that searching for combining selectors using the inspector search
// field produces correct suggestions.

const TEST_URL = TEST_URL_ROOT + "browser_inspector_bug_831693_search_suggestions.html";

// An array of (key, suggestions) pairs where key is a key to press and
// suggestions is an array of suggestions that should be shown in the popup.
// Suggestion is an object with label of the entry and optional count
// (defaults to 1)
const TEST_DATA = [
  {
    key: "d",
    suggestions: [{label: "div", count: 4}]
  },
  {
    key: "i",
    suggestions: [{label: "div", count: 4}]
  },
  {
    key: "v",
    suggestions: []
  },
  {
    key: " ",
    suggestions: [
      {label: "div div", count: 2},
      {label: "div span", count: 2}
    ]
  },
  {
    key: ">",
    suggestions: [
      {label: "div >div", count: 2},
      {label: "div >span", count: 2}
    ]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [
      {label: "div div", count: 2 },
      {label: "div span", count: 2}
    ]
  },
  {
    key: "+",
    suggestions: [{label: "div +span"}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [
      {label: "div div", count: 2 },
      {label: "div span", count: 2}
    ]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [{label: "div", count: 4}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [{label: "div", count: 4}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: "p",
    suggestions: []
  },
  {
    key: " ",
    suggestions: [{label: "p strong"}]
  },
  {
    key: "+",
    suggestions: [
      {label: "p +button" },
      {label: "p +p"}
    ]
  },
  {
    key: "b",
    suggestions: [{label: "p +button"}]
  },
  {
    key: "u",
    suggestions: [{label: "p +button"}]
  },
  {
    key: "t",
    suggestions: [{label: "p +button"}]
  },
  {
    key: "t",
    suggestions: [{label: "p +button"}]
  },
  {
    key: "o",
    suggestions: [{label: "p +button"}]
  },
  {
    key: "n",
    suggestions: []
  },
  {
    key: "+",
    suggestions: [{label: "p +button+p"}]
  }
];

let test = asyncTest(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URL);
  let searchBox = inspector.searchBox;
  let popup = inspector.searchSuggestions.searchPopup;

  yield focusSearchBoxUsingShortcut(inspector.panelWin);

  for (let { key, suggestions } of TEST_DATA) {
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
      is(suggestions[i].label, actualSuggestions[i].label,
         "The suggestion at " + i + "th index is correct.");
      is(suggestions[i].count || 1, actualSuggestions[i].count,
         "The count for suggestion at " + i + "th index is correct.");
    }
  }
});

function formatSuggestions(suggestions) {
  return "[" + suggestions
                .map(s => "'" + s.label + "' (" + s.count || 1 + ")")
                .join(", ") + "]";
}
