/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing searching for ids and classes that contain reserved characters.
const TEST_URL = URL_ROOT + "doc_inspector_search-reserved.html";

// An array of (key, suggestions) pairs where key is a key to press and
// suggestions is an array of suggestions that should be shown in the popup.
// Suggestion is an object with label of the entry and optional count
// (defaults to 1)
const TEST_DATA = [
  {
    key: "#",
    suggestions: [{label: "#d1\\.d2"}]
  },
  {
    key: "d",
    suggestions: [{label: "#d1\\.d2"}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [{label: "#d1\\.d2"}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: ".",
    suggestions: [{label: ".c1\\.c2"}]
  },
  {
    key: "c",
    suggestions: [{label: ".c1\\.c2"}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [{label: ".c1\\.c2"}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: "d",
    suggestions: [{label: "div"},
                  {label: "#d1\\.d2"}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: "c",
    suggestions: [{label: ".c1\\.c2"}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: "b",
    suggestions: [{label: "body"}]
  },
  {
    key: "o",
    suggestions: [{label: "body"}]
  },
  {
    key: "d",
    suggestions: [{label: "body"}]
  },
  {
    key: "y",
    suggestions: []
  },
  {
    key: " ",
    suggestions: [{label: "body div"}]
  },
  {
    key: ".",
    suggestions: [{label: "body .c1\\.c2"}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [{label: "body div"}]
  },
  {
    key: "#",
    suggestions: [{label: "body #d1\\.d2"}]
  }
];

add_task(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URL);
  let searchBox = inspector.searchBox;
  let popup = inspector.searchSuggestions.searchPopup;

  yield focusSearchBoxUsingShortcut(inspector.panelWin);

  for (let { key, suggestions } of TEST_DATA) {
    info("Pressing " + key + " to get " + formatSuggestions(suggestions));

    let command = once(searchBox, "input");
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
    yield command;

    info("Waiting for search query to complete");
    yield inspector.searchSuggestions._lastQuery;

    info("Query completed. Performing checks for input '" +
         searchBox.value + "'");
    let actualSuggestions = popup.getItems().reverse();

    is(popup.isOpen ? actualSuggestions.length : 0, suggestions.length,
       "There are expected number of suggestions.");

    for (let i = 0; i < suggestions.length; i++) {
      is(suggestions[i].label, actualSuggestions[i].label,
         "The suggestion at " + i + "th index is correct.");
    }
  }
});

function formatSuggestions(suggestions) {
  return "[" + suggestions
                .map(s => "'" + s.label + "'")
                .join(", ") + "]";
}
