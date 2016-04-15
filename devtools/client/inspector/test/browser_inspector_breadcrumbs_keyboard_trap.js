/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test ability to tab to and away from breadcrumbs using keyboard.

const TEST_URL = URL_ROOT + "doc_inspector_breadcrumbs.html";

/**
 * Test data has the format of:
 * {
 *   desc     {String}   description for better logging
 *   focused  {Boolean}  flag, indicating if breadcrumbs contain focus
 *   key      {String}   key event's key
 *   options  {?Object}  optional event data such as shiftKey, etc
 * }
 */
const TEST_DATA = [
  {
    desc: "Move the focus away from breadcrumbs to a next focusable element",
    focused: false,
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Move the focus back to the breadcrumbs",
    focused: true,
    key: "VK_TAB",
    options: { shiftKey: true }
  },
  {
    desc: "Move the focus back away from breadcrumbs to a previous focusable " +
          "element",
    focused: false,
    key: "VK_TAB",
    options: { shiftKey: true }
  },
  {
    desc: "Move the focus back to the breadcrumbs",
    focused: true,
    key: "VK_TAB",
    options: { }
  }
];

add_task(function* () {
  let { toolbox, inspector } = yield openInspectorForURL(TEST_URL);
  let doc = inspector.panelDoc;
  let {breadcrumbs} = inspector;

  yield selectNode("#i2", inspector);

  info("Clicking on the corresponding breadcrumbs node to focus it");
  let container = doc.getElementById("inspector-breadcrumbs");

  let button = container.querySelector("button[checked]");
  let onHighlight = toolbox.once("node-highlight");
  button.click();
  yield onHighlight;

  // Ensure a breadcrumb is focused.
  is(doc.activeElement, button, "Focus is on selected breadcrumb");

  for (let { desc, focused, key, options } of TEST_DATA) {
    info(desc);

    EventUtils.synthesizeKey(key, options);
    // Wait until the keyPromise promise resolves.
    yield breadcrumbs.keyPromise;

    if (focused) {
      is(doc.activeElement, button, "Focus is on selected breadcrumb");
    } else {
      ok(!containsFocus(doc, container), "Focus is outside of breadcrumbs");
    }
  }
});
