/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checks for the AccessibleActor

add_task(async function() {
  const {target, walker, accessibility} =
    await initAccessibilityFrontForUrl(MAIN_DOMAIN + "doc_accessibility.html");
  const modifiers = Services.appinfo.OS === "Darwin" ? "\u2303\u2325" : "Alt+Shift+";

  const a11yWalker = await accessibility.getWalker();
  await accessibility.enable();
  const buttonNode = await walker.querySelector(walker.rootNode, "#button");
  const accessibleFront = await a11yWalker.getAccessibleFor(buttonNode);

  checkA11yFront(accessibleFront, {
    name: "Accessible Button",
    role: "pushbutton",
    childCount: 1,
  });

  await accessibleFront.hydrate();

  checkA11yFront(accessibleFront, {
    name: "Accessible Button",
    role: "pushbutton",
    value: "",
    description: "Accessibility Test",
    keyboardShortcut: modifiers + "b",
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
  const relations = await labelAccessibleFront.getRelations();
  is(relations.length, 2, "Accessible front has a correct number of relations");
  is(relations[0].type, "label for", "Label has a label for relation");
  is(relations[0].targets.length, 1, "Label is a label for one target");
  is(relations[0].targets[0], controlAccessibleFront,
     "Label is a label for control accessible front");
  is(relations[1].type, "containing document",
     "Label has a containing document relation");
  is(relations[1].targets.length, 1, "Label is contained by just one document");
  is(relations[1].targets[0], docAccessibleFront,
     "Label's containing document is a root document");

  info("Snapshot");
  const snapshot = await controlAccessibleFront.snapshot();
  Assert.deepEqual(snapshot, {
    "name": "Label",
    "role": "entry",
    "actions": [ "Activate" ],
    "value": "",
    "nodeCssSelector": "#control",
    "nodeType": 1,
    "description": "",
    "keyboardShortcut": "",
    "childCount": 0,
    "indexInParent": 1,
    "states": [
      "focusable",
      "autocompletion",
      "editable",
      "opaque",
      "single line",
      "enabled",
      "sensitive",
    ],
    "children": [],
    "attributes": {
      "margin-left": "0px",
      "text-align": "start",
      "text-indent": "0px",
      "id": "control",
      "tag": "input",
      "margin-top": "0px",
      "margin-bottom": "0px",
      "margin-right": "0px",
      "display": "inline",
      "explicit-name": "true",
    }});

  await accessibility.disable();
  await waitForA11yShutdown();
  await target.destroy();
  gBrowser.removeCurrentTab();
});
