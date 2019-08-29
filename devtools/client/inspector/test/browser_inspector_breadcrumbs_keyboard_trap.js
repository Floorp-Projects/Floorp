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
    options: {},
  },
  {
    desc: "Move the focus back to the breadcrumbs",
    focused: true,
    key: "VK_TAB",
    options: { shiftKey: true },
  },
  {
    desc:
      "Move the focus back away from breadcrumbs to a previous focusable " +
      "element",
    focused: false,
    key: "VK_TAB",
    options: { shiftKey: true },
  },
  {
    desc: "Move the focus back to the breadcrumbs",
    focused: true,
    key: "VK_TAB",
    options: {},
  },
];

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);
  const doc = inspector.panelDoc;
  const { breadcrumbs } = inspector;

  await selectNode("#i2", inspector);

  info("Clicking on the corresponding breadcrumbs node to focus it");
  const container = doc.getElementById("inspector-breadcrumbs");

  const button = container.querySelector("button[checked]");
  const onHighlight = inspector.highlighter.once("node-highlight");
  button.click();
  await onHighlight;

  // Ensure a breadcrumb is focused.
  is(doc.activeElement, container, "Focus is on selected breadcrumb");
  is(
    container.getAttribute("aria-activedescendant"),
    button.id,
    "aria-activedescendant is set correctly"
  );

  for (const { desc, focused, key, options } of TEST_DATA) {
    info(desc);

    EventUtils.synthesizeKey(key, options);
    // Wait until the keyPromise promise resolves.
    await breadcrumbs.keyPromise;

    if (focused) {
      is(doc.activeElement, container, "Focus is on selected breadcrumb");
    } else {
      ok(!containsFocus(doc, container), "Focus is outside of breadcrumbs");
    }
    is(
      container.getAttribute("aria-activedescendant"),
      button.id,
      "aria-activedescendant is set correctly"
    );
  }
});
