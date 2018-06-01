/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checks for the AccessibleActor

add_task(async function() {
  const {client, walker, accessibility} =
    await initAccessibilityFrontForUrl(MAIN_DOMAIN + "doc_accessibility.html");

  const a11yWalker = await accessibility.getWalker();
  await accessibility.enable();
  const buttonNode = await walker.querySelector(walker.rootNode, "#button");
  const accessibleFront = await a11yWalker.getAccessibleFor(buttonNode);

  checkA11yFront(accessibleFront, {
    name: "Accessible Button",
    role: "pushbutton",
    value: "",
    description: "Accessibility Test",
    help: "",
    keyboardShortcut: "",
    childCount: 1,
    domNodeType: 1,
    indexInParent: 1,
    states: ["focusable", "selectable text", "opaque", "enabled", "sensitive"],
    actions: [ "Press" ],
    attributes: {
      "margin-top": "0px",
      display: "inline-block",
      "text-align": "center",
      "text-indent": "0px",
      "margin-left": "0px",
      tag: "button",
      "margin-right": "0px",
      id: "button",
      "margin-bottom": "0px"
    }
  });

  info("Children");
  const children = await accessibleFront.children();
  is(children.length, 1, "Accessible Front has correct number of children");
  checkA11yFront(children[0], {
    name: "Accessible Button",
    role: "text leaf"
  });

  await accessibility.disable();
  await waitForA11yShutdown();
  await client.close();
  gBrowser.removeCurrentTab();
});
