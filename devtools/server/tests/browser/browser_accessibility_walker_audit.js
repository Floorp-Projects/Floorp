/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checks for the AccessibleWalkerActor audit.
add_task(async function() {
  const {target, accessibility} =
    await initAccessibilityFrontForUrl(MAIN_DOMAIN + "doc_accessibility_infobar.html");

  const accessibles = [{
    name: "",
    role: "document",
    value: "",
    description: "",
    keyboardShortcut: "",
    childCount: 2,
    domNodeType: 9,
    indexInParent: 0,
    states: [
      "focused", "readonly", "focusable", "active", "opaque", "enabled", "sensitive",
    ],
    actions: [],
    attributes: {
      display: "block",
      "explicit-name": "true",
      "margin-bottom": "8px",
      "margin-left": "8px",
      "margin-right": "8px",
      "margin-top": "8px",
      tag: "body",
      "text-align": "start",
      "text-indent": "0px",
    },
    checks: {
      "CONTRAST": null,
    },
  }, {
    name: "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do " +
          "eiusmod tempor incididunt ut labore et dolore magna aliqua.",
    role: "heading",
    value: "",
    description: "",
    keyboardShortcut: "",
    childCount: 1,
    domNodeType: 1,
    indexInParent: 0,
    states: [ "selectable text", "opaque", "enabled", "sensitive" ],
    actions: [],
    attributes: {
      display: "block",
      formatting: "block",
      id: "h1",
      level: "1",
      "margin-bottom": "21.4333px",
      "margin-left": "0px",
      "margin-right": "0px",
      "margin-top": "21.4333px",
      tag: "h1",
      "text-align": "start",
      "text-indent": "0px",
    },
    checks: {
      "CONTRAST": null,
    },
  }, {
    name: "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do " +
           "eiusmod tempor incididunt ut labore et dolore magna aliqua.",
    role: "text leaf",
    value: "",
    description: "",
    keyboardShortcut: "",
    childCount: 0,
    domNodeType: 3,
    indexInParent: 0,
    states: [ "opaque", "enabled", "sensitive" ],
    actions: [],
    attributes: { "explicit-name": "true" },
    checks: {
      "CONTRAST": {
        "value": 21,
        "color": [0, 0, 0, 1],
        "backgroundColor": [255, 255, 255, 1],
        "isLargeText": true,
      },
    },
  }, {
    name: "Accessible Button",
    role: "pushbutton",
    value: "",
    description: "",
    keyboardShortcut: "",
    childCount: 1,
    domNodeType: 1,
    indexInParent: 1,
    states: [ "focusable", "selectable text", "opaque", "enabled", "sensitive" ],
    actions: [ "Press" ],
    attributes: {
      display: "inline-block",
      id: "button",
      "margin-bottom": "0px",
      "margin-left": "0px",
      "margin-right": "0px",
      "margin-top": "0px",
      tag: "button",
      "text-align": "center",
      "text-indent": "0px",
    },
    checks: {
      "CONTRAST": null,
    },
  }, {
    name: "Accessible Button",
    role: "text leaf",
    value: "",
    description: "",
    keyboardShortcut: "",
    childCount: 0,
    domNodeType: 3,
    indexInParent: 0,
    states: [ "opaque", "enabled", "sensitive" ],
    actions: [],
    attributes: { "explicit-name": "true" },
    checks: {
      "CONTRAST": {
        "value": 21,
        "color": [0, 0, 0, 1],
        "backgroundColor": [255, 255, 255, 1],
        "isLargeText": false,
      },
    },
  }];

  function findAccessible(name, role) {
    return accessibles.find(accessible =>
      accessible.name === name && accessible.role === role);
  }

  const a11yWalker = await accessibility.getWalker();
  ok(a11yWalker, "The AccessibleWalkerFront was returned");
  await accessibility.enable();

  info("Checking AccessibleWalker audit functionality");
  const ancestries = await a11yWalker.audit();

  for (const ancestry of ancestries) {
    for (const { accessible, children } of ancestry) {
      checkA11yFront(accessible,
                     findAccessible(accessibles.name, accessibles.role));
      for (const child of children) {
        checkA11yFront(child,
                       findAccessible(child.name, child.role));
      }
    }
  }

  await accessibility.disable();
  await waitForA11yShutdown();
  await target.destroy();
  gBrowser.removeCurrentTab();
});
