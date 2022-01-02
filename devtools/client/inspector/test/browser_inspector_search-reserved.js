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
    suggestions: [{ label: "#d1\\.d2" }],
  },
  {
    key: "d",
    suggestions: [{ label: "#d1\\.d2" }],
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [{ label: "#d1\\.d2" }],
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [],
  },
  {
    key: ".",
    suggestions: [{ label: ".c1\\.c2" }],
  },
  {
    key: "c",
    suggestions: [{ label: ".c1\\.c2" }],
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [{ label: ".c1\\.c2" }],
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [],
  },
  {
    key: "d",
    suggestions: [{ label: "div" }, { label: "#d1\\.d2" }],
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [],
  },
  {
    key: "c",
    suggestions: [{ label: ".c1\\.c2" }],
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [],
  },
  {
    key: "b",
    suggestions: [{ label: "body" }],
  },
  {
    key: "o",
    suggestions: [{ label: "body" }],
  },
  {
    key: "d",
    suggestions: [{ label: "body" }],
  },
  {
    key: "y",
    suggestions: [],
  },
  {
    key: " ",
    suggestions: [{ label: "body div" }],
  },
  {
    key: ".",
    suggestions: [{ label: "body .c1\\.c2" }],
  },
  {
    key: "VK_BACK_SPACE",
    suggestions: [{ label: "body div" }],
  },
  {
    key: "#",
    suggestions: [{ label: "body #d1\\.d2" }],
  },
];

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);
  const searchBox = inspector.searchBox;
  const popup = inspector.searchSuggestions.searchPopup;

  await focusSearchBoxUsingShortcut(inspector.panelWin);

  for (const { key, suggestions } of TEST_DATA) {
    info("Pressing " + key + " to get " + formatSuggestions(suggestions));

    const command = once(searchBox, "input");
    const onSearchProcessingDone = inspector.searchSuggestions.once(
      "processing-done"
    );
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
    await command;

    info("Waiting for search query to complete");
    await onSearchProcessingDone;

    info(
      "Query completed. Performing checks for input '" + searchBox.value + "'"
    );
    const actualSuggestions = popup.getItems();

    is(
      popup.isOpen ? actualSuggestions.length : 0,
      suggestions.length,
      "There are expected number of suggestions."
    );

    for (let i = 0; i < suggestions.length; i++) {
      is(
        suggestions[i].label,
        actualSuggestions[i].label,
        "The suggestion at " + i + "th index is correct."
      );
    }
  }
});

function formatSuggestions(suggestions) {
  return "[" + suggestions.map(s => "'" + s.label + "'").join(", ") + "]";
}
