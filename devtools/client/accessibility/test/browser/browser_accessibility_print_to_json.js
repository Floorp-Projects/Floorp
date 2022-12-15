/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "<h1>Top level header</h1>";

function getMenuItems(toolbox) {
  const menuDoc = toolbox.doc.defaultView.windowRoot.ownerGlobal.document;
  const menu = menuDoc.getElementById("accessibility-row-contextmenu");
  return {
    menu,
    items: [...menu.getElementsByTagName("menuitem")],
  };
}

async function newTabSelected(tab) {
  info("Waiting for the JSON viewer tab.");
  await BrowserTestUtils.waitForCondition(
    () => gBrowser.selectedTab !== tab,
    "Current tab updated."
  );
  return gBrowser.selectedTab;
}

function parseSnapshotFromTabURI(tab) {
  let snapshot = tab.label.split("data:application/json;charset=UTF-8,")[1];
  snapshot = decodeURIComponent(snapshot);
  return JSON.parse(snapshot);
}

async function checkJSONSnapshotForRow({ doc, tab, toolbox }, index, expected) {
  info(`Triggering context menu for row #${index}.`);
  EventUtils.synthesizeMouseAtCenter(
    doc.querySelectorAll(".treeRow")[index],
    { type: "contextmenu" },
    doc.defaultView
  );

  info(`Triggering "Print To JSON" menu item for row ${index}.`);
  const {
    menu,
    items: [printToJSON],
  } = getMenuItems(toolbox);

  await BrowserTestUtils.waitForPopupEvent(menu, "shown");

  menu.activateItem(printToJSON);

  const jsonViewTab = await newTabSelected(tab);
  Assert.deepEqual(
    parseSnapshotFromTabURI(jsonViewTab),
    expected,
    "JSON snapshot for the whole document is correct"
  );

  await removeTab(jsonViewTab);
}

const OOP_FRAME_DOCUMENT_SNAPSHOT = {
  childCount: 1,
  description: "",
  indexInParent: 0,
  keyboardShortcut: "",
  name: "Accessibility Panel Test (OOP)",
  nodeCssSelector: "",
  nodeType: 9,
  role: "document",
  value: "",
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
  states: ["readonly", "focusable", "opaque", "enabled", "sensitive"],
  children: [
    {
      childCount: 1,
      description: "",
      indexInParent: 0,
      keyboardShortcut: "",
      name: "Top level header",
      nodeCssSelector: "body > h1:nth-child(1)",
      nodeType: 1,
      role: "heading",
      value: "",
      actions: [],
      attributes: {
        display: "block",
        formatting: "block",
        level: "1",
        "margin-bottom": "21.44px",
        "margin-left": "0px",
        "margin-right": "0px",
        "margin-top": "0px",
        tag: "h1",
        "text-align": "start",
        "text-indent": "0px",
      },
      states: ["selectable text", "opaque", "enabled", "sensitive"],
      children: [
        {
          childCount: 0,
          description: "",
          indexInParent: 0,
          keyboardShortcut: "",
          name: "Top level header",
          nodeCssSelector: "body > h1:nth-child(1)#text",
          nodeType: 3,
          role: "text leaf",
          value: "",
          actions: [],
          attributes: {
            "explicit-name": "true",
          },
          states: ["opaque", "enabled", "sensitive"],
          children: [],
        },
      ],
    },
  ],
};

const OOP_FRAME_SNAPSHOT = {
  childCount: 1,
  description: "",
  indexInParent: 0,
  keyboardShortcut: "",
  name: "Accessibility Panel Test (OOP)",
  nodeCssSelector: "body > iframe:nth-child(1)",
  nodeType: 1,
  role: "internal frame",
  value: "",
  actions: [],
  attributes: {
    display: "inline",
    "explicit-name": "true",
    "margin-bottom": "0px",
    "margin-left": "0px",
    "margin-right": "0px",
    "margin-top": "0px",
    tag: "iframe",
    "text-align": "start",
    "text-indent": "0px",
  },
  states: ["focusable", "opaque", "enabled", "sensitive"],
  children: [OOP_FRAME_DOCUMENT_SNAPSHOT],
};

const EXPECTED_SNAPSHOT = {
  childCount: 1,
  description: "",
  indexInParent: 0,
  keyboardShortcut: "",
  name: "",
  nodeCssSelector: "",
  nodeType: 9,
  role: "document",
  value: "",
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
  states: ["readonly", "focusable", "opaque", "enabled", "sensitive"],
  children: [OOP_FRAME_SNAPSHOT],
};

addA11YPanelTask(
  "Test print to JSON functionality.",
  TEST_URI,
  async env => {
    const { doc } = env;
    await runA11yPanelTests(
      [
        {
          desc: "Test the initial accessibility tree.",
          expected: {
            tree: [
              {
                role: "document",
                name: `""text label`,
                badges: ["text label"],
              },
            ],
          },
        },
      ],
      env
    );

    await toggleRow(doc, 0);
    await toggleRow(doc, 1);

    await runA11yPanelTests(
      [
        {
          desc: "Test expanded accessibility tree.",
          expected: {
            tree: [
              {
                role: "document",
                name: `""text label`,
                badges: ["text label"],
              },
              {
                role: "internal frame",
                name: `"Accessibility Panel Test (OOP)"`,
              },
              {
                role: "document",
                name: `"Accessibility Panel Test (OOP)"`,
              },
            ],
          },
        },
      ],
      env
    );

    // Complete snapshot that includes OOP frame document (crossing process boundry).
    await checkJSONSnapshotForRow(env, 0, EXPECTED_SNAPSHOT);
    // Snapshot of an OOP frame (crossing process boundry).
    await checkJSONSnapshotForRow(env, 1, OOP_FRAME_SNAPSHOT);
    // Snapshot of an OOP frame document (not crossing process boundry).
    await checkJSONSnapshotForRow(env, 2, OOP_FRAME_DOCUMENT_SNAPSHOT);
  },
  {
    remoteIframe: true,
  }
);
