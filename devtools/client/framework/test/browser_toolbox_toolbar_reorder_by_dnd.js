/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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

const { Toolbox } = require("devtools/client/framework/toolbox");

const TEST_STARTING_ORDER = ["inspector", "webconsole", "jsdebugger", "styleeditor",
                             "performance", "memory", "netmonitor", "storage"];
const TEST_DATA = [
  {
    description: "DragAndDrop the target component to back",
    dragTarget: "toolbox-tab-webconsole",
    dropTarget: "toolbox-tab-jsdebugger",
    expectedOrder: ["inspector", "jsdebugger", "webconsole", "styleeditor",
                    "performance", "memory", "netmonitor", "storage"],
  },
  {
    description: "DragAndDrop the target component to front",
    dragTarget: "toolbox-tab-webconsole",
    dropTarget: "toolbox-tab-inspector",
    expectedOrder: ["webconsole", "inspector", "jsdebugger", "styleeditor",
                    "performance", "memory", "netmonitor", "storage"],
  },
  {
    description: "DragAndDrop the target component over the starting of the tab",
    dragTarget: "toolbox-tab-netmonitor",
    passedTargets: ["toolbox-tab-memory", "toolbox-tab-performance",
                    "toolbox-tab-styleeditor", "toolbox-tab-jsdebugger",
                    "toolbox-tab-webconsole", "toolbox-tab-inspector"],
    dropTarget: "toolbox-buttons-start",
    expectedOrder: ["netmonitor", "inspector", "webconsole", "jsdebugger",
                    "styleeditor", "performance", "memory", "storage"],
  },
  {
    description: "DragAndDrop the target component over the ending of the tab",
    dragTarget: "toolbox-tab-webconsole",
    passedTargets: ["toolbox-tab-jsdebugger", "toolbox-tab-styleeditor",
                    "toolbox-tab-performance", "toolbox-tab-memory",
                    "toolbox-tab-netmonitor", "toolbox-tab-storage"],
    dropTarget: "toolbox-buttons-end",
    expectedOrder: ["inspector", "jsdebugger", "styleeditor", "performance",
                    "memory", "netmonitor", "storage", "webconsole", ],
  },
  {
    description: "Mouse was out from the document while dragging",
    dragTarget: "toolbox-tab-webconsole",
    passedTargets: ["toolbox-tab-inspector"],
    dropTarget: null,
    expectedOrder: ["webconsole", "inspector", "jsdebugger", "styleeditor",
                    "performance", "memory", "netmonitor", "storage"],
  },
];

add_task(async function() {
  const originalPreference = Services.prefs.getCharPref("devtools.toolbox.tabsOrder");
  const tab = await addTab("about:blank");
  const toolbox = await openToolboxForTab(tab, "inspector", Toolbox.HostType.BOTTOM);

  for (const testData of TEST_DATA) {
    info(`Test for '${ testData.description }'`);
    prepareTest(toolbox);
    await dnd(toolbox, testData.dragTarget, testData.dropTarget, testData.passedTargets);
    assertTabsOrder(toolbox, testData.expectedOrder);
    assertSelectedTab(toolbox, testData.dragTarget);
    assertPreferenceOrder(testData.expectedOrder);
  }

  info("Test with overflowing tabs");
  prepareTest(toolbox);
  const { originalWidth, originalHeight } = await resizeWindow(toolbox, 800);
  await toolbox.selectTool("storage");
  const dragTarget = "toolbox-tab-storage";
  const dropTarget = "toolbox-tab-inspector";
  const expectedOrder = ["storage", "inspector", "webconsole", "jsdebugger",
                         "styleeditor", "performance", "memory", "netmonitor"];
  await dnd(toolbox, dragTarget, dropTarget);
  assertSelectedTab(toolbox, dragTarget);
  assertPreferenceOrder(expectedOrder);

  await resizeWindow(toolbox, originalWidth, originalHeight);
  Services.prefs.setCharPref("devtools.toolbox.tabsOrder", originalPreference);
});

function prepareTest(toolbox) {
  Services.prefs.setCharPref("devtools.toolbox.tabsOrder", TEST_STARTING_ORDER.join(","));
  ok(!toolbox.doc.getElementById("tools-chevron-menu-button"),
     "The size of the screen being too small");

  const ids = [...toolbox.doc.querySelectorAll(".devtools-tab")]
                .map(tabElement => tabElement.dataset.id);
  is(ids.join(","), TEST_STARTING_ORDER.join(","),
     "The order on the toolbar should be correct");
}

function assertTabsOrder(toolbox, expectedOrder) {
  info("Check the order of the tabs on the toolbar");
  const currentIds = [...toolbox.doc.querySelectorAll(".devtools-tab")]
                       .map(tabElement => tabElement.dataset.id);
  is(currentIds.join(","), expectedOrder.join(","),
     "The order on the toolbar should be correct");
}

function assertSelectedTab(toolbox, dragTarget) {
  info("Check whether the drag target was selected");
  const dragTargetEl = toolbox.doc.getElementById(dragTarget);
  ok(dragTargetEl.classList.contains("selected"), "The dragged tool should be selected");
}

function assertPreferenceOrder(expectedOrder) {
  info("Check the order in DevTools preference for tabs order");
  is(Services.prefs.getCharPref("devtools.toolbox.tabsOrder"), expectedOrder.join(","),
     "The preference should be correct");
}

async function dnd(toolbox, dragTarget, dropTarget, passedTargets = []) {
  info(`Drag ${ dragTarget } to ${ dropTarget }`);
  const dragTargetEl = toolbox.doc.getElementById(dragTarget);

  const onReady = dragTargetEl.classList.contains("selected")
                    ? Promise.resolve() : toolbox.once("select");
  EventUtils.synthesizeMouseAtCenter(dragTargetEl,
                                     { type: "mousedown" },
                                     dragTargetEl.ownerGlobal);
  await onReady;

  for (const passedTarget of passedTargets) {
    info(`Via ${ passedTarget }`);
    const passedTargetEl = toolbox.doc.getElementById(passedTarget);
    EventUtils.synthesizeMouseAtCenter(passedTargetEl,
                                       { type: "mousemove" },
                                       passedTargetEl.ownerGlobal);
  }

  if (dropTarget) {
    const dropTargetEl = toolbox.doc.getElementById(dropTarget);
    EventUtils.synthesizeMouseAtCenter(dropTargetEl,
                                       { type: "mousemove" },
                                       dropTargetEl.ownerGlobal);
    EventUtils.synthesizeMouseAtCenter(dropTargetEl,
                                       { type: "mouseup" },
                                       dropTargetEl.ownerGlobal);
  } else {
    const containerEl = toolbox.doc.getElementById("toolbox-container");
    EventUtils.synthesizeMouse(containerEl, 0, 0,
                               { type: "mouseout" }, containerEl.ownerGlobal);
  }
}

async function resizeWindow(toolbox, width, height) {
  const hostWindow = toolbox.win.parent;
  const originalWidth = hostWindow.outerWidth;
  const originalHeight = hostWindow.outerHeight;
  const toWidth = width || originalWidth;
  const toHeight = height || originalHeight;

  const onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(toWidth, toHeight);
  await onResize;

  return { originalWidth, originalHeight };
}
