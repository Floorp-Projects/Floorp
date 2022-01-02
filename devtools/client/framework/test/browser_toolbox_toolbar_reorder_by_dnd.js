/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for following reordering operation:
// * DragAndDrop the target component to back
// * DragAndDrop the target component to front
// * DragAndDrop the target component over the starting of the tab
// * DragAndDrop the target component over the ending of the tab
// * Mouse was out from the document while dragging
// * Select overflowed item, then DnD that
//
// This test is on the assumption which default toolbar has following tools.
//   * inspector
//   * webconsole
//   * jsdebugger
//   * styleeditor
//   * performance
//   * memory
//   * netmonitor
//   * storage
//   * accessibility
//   * application

const { Toolbox } = require("devtools/client/framework/toolbox");

const TEST_STARTING_ORDER = [
  "inspector",
  "webconsole",
  "jsdebugger",
  "styleeditor",
  "performance",
  "memory",
  "netmonitor",
  "storage",
  "accessibility",
  "application",
];
const TEST_DATA = [
  {
    description: "DragAndDrop the target component to back",
    dragTarget: "webconsole",
    dropTarget: "jsdebugger",
    expectedOrder: [
      "inspector",
      "jsdebugger",
      "webconsole",
      "styleeditor",
      "performance",
      "memory",
      "netmonitor",
      "storage",
      "accessibility",
      "application",
    ],
  },
  {
    description: "DragAndDrop the target component to front",
    dragTarget: "webconsole",
    dropTarget: "inspector",
    expectedOrder: [
      "webconsole",
      "inspector",
      "jsdebugger",
      "styleeditor",
      "performance",
      "memory",
      "netmonitor",
      "storage",
      "accessibility",
      "application",
    ],
  },
  {
    description:
      "DragAndDrop the target component over the starting of the tab",
    dragTarget: "netmonitor",
    passedTargets: [
      "memory",
      "performance",
      "styleeditor",
      "jsdebugger",
      "webconsole",
      "inspector",
    ],
    dropTarget: "#toolbox-buttons-start",
    expectedOrder: [
      "netmonitor",
      "inspector",
      "webconsole",
      "jsdebugger",
      "styleeditor",
      "performance",
      "memory",
      "storage",
      "accessibility",
      "application",
    ],
  },
  {
    description: "DragAndDrop the target component over the ending of the tab",
    dragTarget: "webconsole",
    passedTargets: [
      "jsdebugger",
      "styleeditor",
      "performance",
      "memory",
      "netmonitor",
      "storage",
    ],
    dropTarget: "#toolbox-buttons-end",
    expectedOrder: [
      "inspector",
      "jsdebugger",
      "styleeditor",
      "performance",
      "memory",
      "netmonitor",
      "storage",
      "accessibility",
      "application",
      "webconsole",
    ],
  },
];

add_task(async function() {
  // Enable the Application panel (atm it's only available on Nightly)
  await pushPref("devtools.application.enabled", true);

  const tab = await addTab("about:blank");
  const toolbox = await openToolboxForTab(
    tab,
    "inspector",
    Toolbox.HostType.BOTTOM
  );

  const originalPreference = Services.prefs.getCharPref(
    "devtools.toolbox.tabsOrder"
  );
  const win = getWindow(toolbox);
  const {
    outerWidth: originalWindowWidth,
    outerHeight: originalWindowHeight,
  } = win;
  registerCleanupFunction(() => {
    Services.prefs.setCharPref(
      "devtools.toolbox.tabsOrder",
      originalPreference
    );
    win.resizeTo(originalWindowWidth, originalWindowHeight);
  });

  for (const testData of TEST_DATA) {
    info(`Test for '${testData.description}'`);
    prepareToolTabReorderTest(toolbox, TEST_STARTING_ORDER);
    await dndToolTab(
      toolbox,
      testData.dragTarget,
      testData.dropTarget,
      testData.passedTargets
    );
    assertToolTabOrder(toolbox, testData.expectedOrder);
    assertToolTabSelected(toolbox, testData.dragTarget);
    assertToolTabPreferenceOrder(testData.expectedOrder);
  }

  info("Test with overflowing tabs");
  prepareToolTabReorderTest(toolbox, TEST_STARTING_ORDER);
  await resizeWindow(toolbox, 800);
  await toolbox.selectTool("storage");
  const dragTarget = "storage";
  const dropTarget = "inspector";
  const expectedOrder = [
    "storage",
    "inspector",
    "webconsole",
    "jsdebugger",
    "styleeditor",
    "performance",
    "memory",
    "netmonitor",
    "accessibility",
    "application",
  ];
  await dndToolTab(toolbox, dragTarget, dropTarget);
  assertToolTabSelected(toolbox, dragTarget);
  assertToolTabPreferenceOrder(expectedOrder);
});
