/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checks for the AccessibleActor

add_task(async function () {
  const { target, walker, a11yWalker, parentAccessibility } =
    await initAccessibilityFrontsForUrl(MAIN_DOMAIN + "doc_accessibility.html");
  const modifiers =
    Services.appinfo.OS === "Darwin" ? "\u2303\u2325" : "Alt+Shift+";

  const buttonNode = await walker.querySelector(walker.rootNode, "#button");
  const accessibleFront = await a11yWalker.getAccessibleFor(buttonNode);

  checkA11yFront(accessibleFront, {
    name: "Accessible Button",
    role: "button",
    childCount: 1,
  });

  await accessibleFront.hydrate();

  checkA11yFront(accessibleFront, {
    name: "Accessible Button",
    role: "button",
    value: "",
    description: "Accessibility Test",
    keyboardShortcut: modifiers + "b",
    childCount: 1,
    domNodeType: 1,
    indexInParent: 1,
    states: ["focusable", "opaque", "enabled", "sensitive"],
    actions: ["Press"],
    attributes: {
      "margin-top": "0px",
      display: "inline-block",
      "text-align": "center",
      "text-indent": "0px",
      "margin-left": "0px",
      tag: "button",
      "margin-right": "0px",
      id: "button",
      "margin-bottom": "0px",
    },
  });

  info("Children");
  const children = await accessibleFront.children();
  is(children.length, 1, "Accessible Front has correct number of children");
  checkA11yFront(children[0], {
    name: "Accessible Button",
    role: "text leaf",
  });

  info("Relations");
  const labelNode = await walker.querySelector(walker.rootNode, "#label");
  const controlNode = await walker.querySelector(walker.rootNode, "#control");
  const labelAccessibleFront = await a11yWalker.getAccessibleFor(labelNode);
  const controlAccessibleFront = await a11yWalker.getAccessibleFor(controlNode);
  const docAccessibleFront = await a11yWalker.getAccessibleFor(walker.rootNode);
  const labelRelations = await labelAccessibleFront.getRelations();
  is(labelRelations.length, 2, "Label has correct number of relations");
  is(labelRelations[0].type, "label for", "Label has a label for relation");
  is(labelRelations[0].targets.length, 1, "Label is a label for one target");
  is(
    labelRelations[0].targets[0],
    controlAccessibleFront,
    "Label is a label for control accessible front"
  );
  is(
    labelRelations[1].type,
    "containing document",
    "Label has a containing document relation"
  );
  is(
    labelRelations[1].targets.length,
    1,
    "Label is contained by just one document"
  );
  is(
    labelRelations[1].targets[0],
    docAccessibleFront,
    "Label's containing document is a root document"
  );

  const controlRelations = await controlAccessibleFront.getRelations();
  is(controlRelations.length, 3, "Control has correct number of relations");
  is(controlRelations[2].type, "details", "Control has a details relation");
  is(controlRelations[2].targets.length, 1, "Control has one details target");
  const detailsNode = await walker.querySelector(walker.rootNode, "#details");
  const detailsAccessibleFront = await a11yWalker.getAccessibleFor(detailsNode);
  is(
    controlRelations[2].targets[0],
    detailsAccessibleFront,
    "Control has correct details target"
  );

  info("Snapshot");
  const snapshot = await controlAccessibleFront.snapshot();
  Assert.deepEqual(snapshot, {
    name: "Label",
    role: "textbox",
    actions: ["Activate"],
    value: "",
    nodeCssSelector: "#control",
    nodeType: 1,
    description: "",
    keyboardShortcut: "",
    childCount: 0,
    indexInParent: 1,
    states: [
      "focusable",
      "autocompletion",
      "selectable text",
      "editable",
      "opaque",
      "single line",
      "enabled",
      "sensitive",
    ],
    children: [],
    attributes: {
      "margin-left": "0px",
      "text-align": "start",
      "text-indent": "0px",
      id: "control",
      tag: "input",
      "margin-top": "0px",
      "margin-bottom": "0px",
      "margin-right": "0px",
      display: "inline-block",
      "explicit-name": "true",
    },
  });

  // Check that we're using ARIA role tokens for landmarks implicit in native
  // markup.
  const headerNode = await walker.querySelector(walker.rootNode, "#header");
  const headerAccessibleFront = await a11yWalker.getAccessibleFor(headerNode);
  checkA11yFront(headerAccessibleFront, {
    name: null,
    role: "banner",
    childCount: 1,
  });
  const navNode = await walker.querySelector(walker.rootNode, "#nav");
  const navAccessibleFront = await a11yWalker.getAccessibleFor(navNode);
  checkA11yFront(navAccessibleFront, {
    name: null,
    role: "navigation",
    childCount: 1,
  });
  const footerNode = await walker.querySelector(walker.rootNode, "#footer");
  const footerAccessibleFront = await a11yWalker.getAccessibleFor(footerNode);
  checkA11yFront(footerAccessibleFront, {
    name: null,
    role: "contentinfo",
    childCount: 1,
  });

  await waitForA11yShutdown(parentAccessibility);
  await target.destroy();
  gBrowser.removeCurrentTab();
});
