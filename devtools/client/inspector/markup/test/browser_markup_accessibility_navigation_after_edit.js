/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from helper_markup_accessibility_navigation.js */

"use strict";

// Test keyboard navigation accessibility is preserved after editing attributes.

loadHelperScript("helper_markup_accessibility_navigation.js");

const TEST_URI = '<div id="some-id" class="some-class"></div>';

/**
 * Test data has the format of:
 * {
 *   desc              {String}   description for better logging
 *   key               {String}   key event's key
 *   options           {?Object}  optional event data such as shiftKey, etc
 *   focused           {String}   path to expected focused element relative to
 *                                its container
 *   activedescendant  {String}   path to expected aria-activedescendant element
 *                                relative to its container
 *   waitFor           {String}   optional event to wait for if keyboard actions
 *                                result in asynchronous updates
 * }
 */
const TESTS = [
  {
    desc: "Select header container",
    focused: "root.elt",
    activedescendant: "div.tagLine",
    key: "VK_DOWN",
    options: { },
    waitFor: "inspector-updated"
  },
  {
    desc: "Focus on header tag",
    focused: "div.focusableElms.0",
    activedescendant: "div.tagLine",
    key: "VK_RETURN",
    options: { }
  },
  {
    desc: "Activate header tag editor",
    focused: "div.editor.tag.inplaceEditor.input",
    activedescendant: "div.tagLine",
    key: "VK_RETURN",
    options: { }
  },
  {
    desc: "Activate header id attribute editor",
    focused: "div.editor.attrList.children.0.children.1.inplaceEditor.input",
    activedescendant: "div.tagLine",
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Deselect text in header id attribute editor",
    focused: "div.editor.attrList.children.0.children.1.inplaceEditor.input",
    activedescendant: "div.tagLine",
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Move the cursor to the left",
    focused: "div.editor.attrList.children.0.children.1.inplaceEditor.input",
    activedescendant: "div.tagLine",
    key: "VK_LEFT",
    options: { }
  },
  {
    desc: "Modify the attribute",
    focused: "div.editor.attrList.children.0.children.1.inplaceEditor.input",
    activedescendant: "div.tagLine",
    key: "A",
    options: { }
  },
  {
    desc: "Commit the attribute change",
    focused: "div.focusableElms.1",
    activedescendant: "div.tagLine",
    key: "VK_RETURN",
    options: { },
    waitFor: "inspector-updated"
  },
  {
    desc: "Tab and focus on header class attribute",
    focused: "div.focusableElms.2",
    activedescendant: "div.tagLine",
    key: "VK_TAB",
    options: { }
  },
  {
    desc: "Tab and focus on header new attribute node",
    focused: "div.focusableElms.3",
    activedescendant: "div.tagLine",
    key: "VK_TAB",
    options: { }
  },
];

let elms = {};

add_task(async function() {
  const url = `data:text/html;charset=utf-8,${TEST_URI}`;
  const { inspector } = await openInspectorForURL(url);

  elms.docBody = inspector.markup.doc.body;
  elms.root = inspector.markup.getContainer(inspector.markup._rootNode);
  elms.div = await getContainerForSelector("div", inspector);
  elms.body = await getContainerForSelector("body", inspector);

  // Initial focus is on root element and active descendant should be set on
  // body tag line.
  testNavigationState(inspector, elms, elms.docBody, elms.body.tagLine);

  // Focus on the tree element.
  elms.root.elt.focus();

  for (const testData of TESTS) {
    await runAccessibilityNavigationTest(inspector, elms, testData);
  }

  elms = null;
});
