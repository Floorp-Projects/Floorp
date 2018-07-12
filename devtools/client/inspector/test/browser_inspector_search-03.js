/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that searching for elements using the inspector search field
// produces correct suggestions.

const TEST_URL = URL_ROOT + "doc_inspector_search.html";

// An array of (key, suggestions) pairs where key is a key to press and
// suggestions is an array of suggestions that should be shown in the popup.
// Suggestion is an object with label of the entry and optional count
// (defaults to 1)
var TEST_DATA = [
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
    key: ".",
    suggestions: [{label: "div.c1"}]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: "#",
    suggestions: [
      {label: "div#d1"},
      {label: "div#d2"}
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
    key: ".",
    suggestions: [
      {label: ".c1"},
      {label: ".c2"}
    ]
  },
  {
    key: "c",
    suggestions: [
      {label: ".c1"},
      {label: ".c2"}
    ]
  },
  {
    key: "2",
    suggestions: []
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [
      {label: ".c1"},
      {label: ".c2"}
    ]
  },
  {
    key: "1",
    suggestions: []
  },
  {
    key: "#",
    suggestions: [
      {label: "#d2"},
      {label: "#p1"},
      {label: "#s2"}
    ]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [
      {label: ".c1"},
      {label: ".c2"}
    ]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [
      {label: ".c1"},
      {label: ".c2"}
    ]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: "#",
    suggestions: [
      {label: "#b1"},
      {label: "#d1"},
      {label: "#d2"},
      {label: "#p1"},
      {label: "#p2"},
      {label: "#p3"},
      {label: "#s1"},
      {label: "#s2"}
    ]
  },
  {
    key: "p",
    suggestions: [
      {label: "#p1"},
      {label: "#p2"},
      {label: "#p3"}
    ]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [
      {label: "#b1"},
      {label: "#d1"},
      {label: "#d2"},
      {label: "#p1"},
      {label: "#p2"},
      {label: "#p3"},
      {label: "#s1"},
      {label: "#s2"}
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
      {label: "#p3"}
    ]
  },
  {
    key: "[", suggestions: []
  },
  {
    key: "i", suggestions: []
  },
  {
    key: "d", suggestions: []
  },
  {
    key: "*", suggestions: []
  },
  {
    key: "=", suggestions: []
  },
  {
    key: "p", suggestions: []
  },
  {
    key: "]", suggestions: []
  },
  {
    key: ".",
    suggestions: [
      {label: "p[id*=p].c1"},
      {label: "p[id*=p].c2"}
    ]
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: []
  },
  {
    key: "#",
    suggestions: [
      {label: "p[id*=p]#p1"},
      {label: "p[id*=p]#p2"},
      {label: "p[id*=p]#p3"}
    ]
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

    info("Query completed. Performing checks for input '" +
         searchBox.value + "'");
    const actualSuggestions = popup.getItems();

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
