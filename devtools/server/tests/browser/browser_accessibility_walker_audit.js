/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checks for the AccessibleWalkerActor audit.
add_task(async function() {
  const {target, accessibility} =
    await initAccessibilityFrontForUrl(MAIN_DOMAIN + "doc_accessibility_audit.html");

  const accessibles = [{
    name: "",
    role: "document",
    childCount: 2,
    checks: {
      "CONTRAST": null,
    },
  }, {
    name: "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do " +
          "eiusmod tempor incididunt ut labore et dolore magna aliqua.",
    role: "heading",
    childCount: 1,
    checks: {
      "CONTRAST": null,
    },
  }, {
    name: "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do " +
           "eiusmod tempor incididunt ut labore et dolore magna aliqua.",
    role: "text leaf",
    childCount: 0,
    checks: {
      "CONTRAST": {
        "value": 21,
        "color": [0, 0, 0, 1],
        "backgroundColor": [255, 255, 255, 1],
        "isLargeText": true,
      },
    },
  }, {
    name: "",
    role: "paragraph",
    childCount: 1,
    checks: {
      "CONTRAST": null,
    },
  }, {
    name: "Accessible Paragraph",
    role: "text leaf",
    childCount: 0,
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
