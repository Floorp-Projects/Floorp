/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test ability to tab to and away from inspector search using keyboard.

const TEST_URL = URL_ROOT + "doc_inspector_search.html";

/**
 * Test data has the format of:
 * {
 *   desc         {String}    description for better logging
 *   focused      {Boolean}   flag, indicating if search box contains focus
 *   keys:        {Array}     list of keys that include key code and optional
 *                            event data (shiftKey, etc)
 * }
 *
 */
const TEST_DATA = [
  {
    desc: "Move focus to a next focusable element",
    focused: false,
    keys: [
      {
        key: "VK_TAB",
        options: { }
      }
    ]
  },
  {
    desc: "Move focus back to searchbox",
    focused: true,
    keys: [
      {
        key: "VK_TAB",
        options: { shiftKey: true }
      }
    ]
  },
  {
    desc: "Open popup and then tab away (2 times) to the a next focusable " +
          "element",
    focused: false,
    keys: [
      {
        key: "d",
        options: { }
      },
      {
        key: "VK_TAB",
        options: { }
      },
      {
        key: "VK_TAB",
        options: { }
      }
    ]
  },
  {
    desc: "Move focus back to searchbox",
    focused: true,
    keys: [
      {
        key: "VK_TAB",
        options: { shiftKey: true }
      }
    ]
  }
];

add_task(function* () {
  let { inspector } = yield openInspectorForURL(TEST_URL);
  let { searchBox } = inspector;
  let doc = inspector.panelDoc;

  yield selectNode("#b1", inspector);
  yield focusSearchBoxUsingShortcut(inspector.panelWin);

  // Ensure a searchbox is focused.
  ok(containsFocus(doc, searchBox), "Focus is in a searchbox");

  for (let { desc, focused, keys } of TEST_DATA) {
    info(desc);
    for (let { key, options } of keys) {
      let done = !focused ?
        inspector.searchSuggestions.once("processing-done") : Promise.resolve();
      EventUtils.synthesizeKey(key, options);
      yield done;
    }
    is(containsFocus(doc, searchBox), focused, "Focus is set correctly");
  }
});
