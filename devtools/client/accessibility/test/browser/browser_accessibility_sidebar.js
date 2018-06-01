/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Test</title>
  </head>
  <body></body>
</html>`;

/**
 * Test data has the format of:
 * {
 *   desc     {String}    description for better logging
 *   action   {Function}  An optional action that needs to be performed before
 *                        the state of the tree and the sidebar can be checked.
 *   expected {JSON}      An expected states for the tree and the sidebar.
 * }
 */
const tests = [{
  desc: "Test the initial accessibility sidebar state.",
  expected: {
    sidebar: {
      name: "Accessibility Panel Test",
      role: "document",
      actions: [],
      value: "",
      description: "",
      help: "",
      keyboardShortcut: "",
      childCount: 0,
      indexInParent: 0,
      states: ["readonly", "focusable", "opaque", "enabled", "sensitive"]
    }
  }
}, {
  desc: "Mark document as disabled for accessibility.",
  action: async ({ browser }) => ContentTask.spawn(browser, {}, () =>
    content.document.body.setAttribute("aria-disabled", true)),
  expected: {
    sidebar: {
      states: ["unavailable", "readonly", "focusable", "opaque"]
    }
  }
}, {
  desc: "Append a new child to the document.",
  action: async ({ browser }) => ContentTask.spawn(browser, {}, () => {
    const doc = content.document;
    const button = doc.createElement("button");
    button.textContent = "Press Me!";
    doc.body.appendChild(button);
  }),
  expected: {
    sidebar: {
      childCount: 1
    }
  }
}];

/**
 * Test that checks the Accessibility panel sidebar.
 */
addA11yPanelTestsTask(tests, TEST_URI, "Test Accessibility panel sidebar.");
