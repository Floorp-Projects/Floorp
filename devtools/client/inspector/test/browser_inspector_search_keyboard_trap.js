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

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);
  const { searchBox } = inspector;
  const doc = inspector.panelDoc;

  await selectNode("#b1", inspector);
  await focusSearchBoxUsingShortcut(inspector.panelWin);

  // Ensure a searchbox is focused.
  ok(containsFocus(doc, searchBox), "Focus is in a searchbox");

  for (const { desc, focused, keys } of TEST_DATA) {
    info(desc);
    for (const { key, options } of keys) {
      const done = !focused ?
        inspector.searchSuggestions.once("processing-done") : Promise.resolve();
      EventUtils.synthesizeKey(key, options);
      await done;
    }
    is(containsFocus(doc, searchBox), focused, "Focus is set correctly");
  }
});
