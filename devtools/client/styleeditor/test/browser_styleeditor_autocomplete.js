/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that autocompletion works as expected.

const TESTCASE_URI = TEST_BASE_HTTP + "autocomplete.html";
const MAX_SUGGESTIONS = 15;

// Test cases to test that autocompletion works correctly when enabled.
// Format:
// [
//   key,
//   {
//     total: Number of suggestions in the popup (-1 if popup is closed),
//     current: Index of selected suggestion,
//     inserted: 1 to check whether the selected suggestion is inserted into the
//               editor or not,
//     entered: 1 if the suggestion is inserted and finalized
//   }
// ]

function getTestCases(cssProperties) {
  const keywords = getCSSKeywords(cssProperties);
  const getSuggestionNumberFor = suggestionNumberGetter(keywords);

  return [
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["Ctrl+Space", { total: 1, current: 0 }],
    ["VK_LEFT"],
    ["VK_RIGHT"],
    ["VK_DOWN"],
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["Ctrl+Space", { total: getSuggestionNumberFor("font"), current: 0 }],
    ["VK_END"],
    ["VK_RETURN"],
    ["b", { total: getSuggestionNumberFor("b"), current: 0 }],
    ["a", { total: getSuggestionNumberFor("ba"), current: 0 }],
    [
      "VK_DOWN",
      { total: getSuggestionNumberFor("ba"), current: 0, inserted: 1 },
    ],
    [
      "VK_DOWN",
      { total: getSuggestionNumberFor("ba"), current: 1, inserted: 1 },
    ],
    [
      "VK_TAB",
      { total: getSuggestionNumberFor("ba"), current: 2, inserted: 1 },
    ],
    ["VK_RETURN", { current: 2, inserted: 1, entered: 1 }],
    ["b", { total: getSuggestionNumberFor("background", "b"), current: 0 }],
    ["l", { total: getSuggestionNumberFor("background", "bl"), current: 0 }],
    [
      "VK_TAB",
      {
        total: getSuggestionNumberFor("background", "bl"),
        current: 0,
        inserted: 1,
      },
    ],
    [
      "VK_DOWN",
      {
        total: getSuggestionNumberFor("background", "bl"),
        current: 1,
        inserted: 1,
      },
    ],
    [
      "VK_UP",
      {
        total: getSuggestionNumberFor("background", "bl"),
        current: 0,
        inserted: 1,
      },
    ],
    [
      "VK_TAB",
      {
        total: getSuggestionNumberFor("background", "bl"),
        current: 1,
        inserted: 1,
      },
    ],
    [
      "VK_TAB",
      {
        total: getSuggestionNumberFor("background", "bl"),
        current: 2,
        inserted: 1,
      },
    ],
    [";"],
    ["VK_RETURN"],
    ["c", { total: getSuggestionNumberFor("c"), current: 0 }],
    ["o", { total: getSuggestionNumberFor("co"), current: 0 }],
    ["VK_RETURN", { current: 0, inserted: 1 }],
    ["r", { total: getSuggestionNumberFor("color", "r"), current: 0 }],
    ["VK_RETURN", { current: 0, inserted: 1 }],
    [";"],
    ["VK_LEFT"],
    ["VK_RIGHT"],
    ["VK_DOWN"],
    ["VK_RETURN"],
    ["b", { total: 2, current: 0 }],
    ["u", { total: 1, current: 0 }],
    ["VK_RETURN", { current: 0, inserted: 1 }],
    ["{"],
    ["VK_HOME"],
    ["VK_DOWN"],
    ["VK_DOWN"],
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["VK_RIGHT"],
    ["Ctrl+Space", { total: 1, current: 0 }],
  ];
}

add_task(async function() {
  // We try to type "background" above, so backdrop-filter enabledness affects
  // the expectations. Instead of branching on the test set the pref to true
  // here as that is the end state, and it doesn't interact with the test in
  // other ways.
  await SpecialPowers.pushPrefEnv({
    set: [["layout.css.backdrop-filter.enabled", true]],
  });
  const { panel, ui } = await openStyleEditorForURL(TESTCASE_URI);
  const { cssProperties } = ui;
  const testCases = getTestCases(cssProperties);

  await ui.selectStyleSheet(ui.editors[1].styleSheet);
  const editor = await ui.editors[1].getSourceEditor();

  const sourceEditor = editor.sourceEditor;
  const popup = sourceEditor.getAutocompletionPopup();

  await SimpleTest.promiseFocus(panel.panelWindow);

  for (const index in testCases) {
    await testState(testCases, index, sourceEditor, popup, panel.panelWindow);
    await checkState(testCases, index, sourceEditor, popup);
  }
});

function testState(testCases, index, sourceEditor, popup, panelWindow) {
  let [key, details] = testCases[index];
  let entered;
  if (details) {
    entered = details.entered;
  }
  const mods = {};

  info(
    "pressing key " +
      key +
      " to get result: " +
      JSON.stringify(testCases[index]) +
      " for index " +
      index
  );

  let evt = "after-suggest";

  if (key == "Ctrl+Space") {
    key = " ";
    mods.ctrlKey = true;
  } else if (key == "VK_RETURN" && entered) {
    evt = "popup-hidden";
  } else if (
    /(left|right|return|home|end)/gi.test(key) ||
    (key == "VK_DOWN" && !popup.isOpen)
  ) {
    evt = "cursorActivity";
  } else if (key == "VK_TAB" || key == "VK_UP" || key == "VK_DOWN") {
    evt = "suggestion-entered";
  }

  const ready = sourceEditor.once(evt);
  EventUtils.synthesizeKey(key, mods, panelWindow);

  return ready;
}

function checkState(testCases, index, sourceEditor, popup) {
  return new Promise(resolve => {
    executeSoon(() => {
      let [, details] = testCases[index];
      details = details || {};
      const { total, current, inserted } = details;

      if (total != undefined) {
        ok(popup.isOpen, "Popup is open for index " + index);
        is(
          total,
          popup.itemCount,
          "Correct total suggestions for index " + index
        );
        is(
          current,
          popup.selectedIndex,
          "Correct index is selected for index " + index
        );
        if (inserted) {
          const { text } = popup.getItemAtIndex(current);
          const { line, ch } = sourceEditor.getCursor();
          const lineText = sourceEditor.getText(line);
          is(
            lineText.substring(ch - text.length, ch),
            text,
            "Current suggestion from the popup is inserted into the editor."
          );
        }
      } else {
        ok(!popup.isOpen, "Popup is closed for index " + index);
        if (inserted) {
          const { text } = popup.getItemAtIndex(current);
          const { line, ch } = sourceEditor.getCursor();
          const lineText = sourceEditor.getText(line);
          is(
            lineText.substring(ch - text.length, ch),
            text,
            "Current suggestion from the popup is inserted into the editor."
          );
        }
      }
      resolve();
    });
  });
}

/**
 * Returns a list of all property names and a map of property name vs possible
 * CSS values provided by the Gecko engine.
 *
 * @return {Object} An object with following properties:
 *         - CSSProperties {Array} Array of string containing all the possible
 *                         CSS property names.
 *         - CSSValues {Object|Map} A map where key is the property name and
 *                     value is an array of string containing all the possible
 *                     CSS values the property can have.
 */
function getCSSKeywords(cssProperties) {
  const props = {};
  const propNames = cssProperties.getNames();
  propNames.forEach(prop => {
    props[prop] = cssProperties.getValues(prop).sort();
  });
  return {
    CSSValues: props,
    CSSProperties: propNames.sort(),
  };
}

/**
 * Returns a function that returns the number of expected suggestions for the given
 * property and value. If the value is not null, returns the number of values starting
 * with `value`. Returns the number of properties starting with `property` otherwise.
 */
function suggestionNumberGetter({ CSSProperties, CSSValues }) {
  return (property, value) => {
    if (value == null) {
      return CSSProperties.filter(prop => prop.startsWith(property)).slice(
        0,
        MAX_SUGGESTIONS
      ).length;
    }
    return CSSValues[property]
      .filter(val => val.startsWith(value))
      .slice(0, MAX_SUGGESTIONS).length;
  };
}
