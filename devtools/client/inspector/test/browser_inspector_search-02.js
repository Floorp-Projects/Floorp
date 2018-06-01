/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that searching for combining selectors using the inspector search
// field produces correct suggestions.

const TEST_URL = URL_ROOT + "doc_inspector_search-suggestions.html";

// An array of (key, suggestions) pairs where key is a key to press and
// suggestions is an array of suggestions that should be shown in the popup.
// Suggestion is an object with label of the entry and optional count
// (defaults to 1)
const TEST_DATA = [
  {
    key: "d",
    suggestions: [
      {label: "div"},
      {label: "#d1"},
      {label: "#d2"}
    ]
  },
  {
    key: "i",
    suggestions: [{label: "div"}]
  },
  {
    key: "v",
    suggestions: []
  },
  {
    key: " ",
    suggestions: [
      {label: "div div"},
      {label: "div span"}
    ]
  },
  {
    key: ">",
    suggestions: [
      {label: "div >div"},
      {label: "div >span"}
    ]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [
      {label: "div div"},
      {label: "div span"}
    ]
  },
  {
    key: "+",
    suggestions: [{label: "div +span"}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [
      {label: "div div"},
      {label: "div span"}
    ]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [{label: "div"}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [
      {label: "div"},
      {label: "#d1"},
      {label: "#d2"}
    ]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: "p",
    suggestions: [
      {label: "p"},
      {label: "#p1"},
      {label: "#p2"},
      {label: "#p3"},
    ]
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

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);
  const searchBox = inspector.searchBox;
  const popup = inspector.searchSuggestions.searchPopup;

  await focusSearchBoxUsingShortcut(inspector.panelWin);

  for (const { key, suggestions } of TEST_DATA) {
    info("Pressing " + key + " to get " + formatSuggestions(suggestions));

    const command = once(searchBox, "input");
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
    await command;

    info("Waiting for search query to complete");
    await inspector.searchSuggestions._lastQuery;

    info("Query completed. Performing checks for input '" + searchBox.value +
      "' - key pressed: " + key);
    const actualSuggestions = popup.getItems().reverse();

    is(popup.isOpen ? actualSuggestions.length : 0, suggestions.length,
       "There are expected number of suggestions.");

    for (let i = 0; i < suggestions.length; i++) {
      is(actualSuggestions[i].label, suggestions[i].label,
         "The suggestion at " + i + "th index is correct.");
    }
  }
});

function formatSuggestions(suggestions) {
  return "[" + suggestions
                .map(s => "'" + s.label + "'")
                .join(", ") + "]";
}
