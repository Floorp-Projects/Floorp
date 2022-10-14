/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { require } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/Loader.sys.mjs"
);

const TEST_DATA = [
  {
    description: "Test for no order in preference",
    preferenceOrder: [],
    currentTabsOrder: ["T1", "T2", "T3", "T4", "T5"],
    dragTarget: "T1",
    expectedOrder: ["T1", "T2", "T3", "T4", "T5"],
  },
  {
    description: "Test for drag a tab to left with hidden tab",
    preferenceOrder: ["T1", "T2", "T3", "E1", "T4", "T5"],
    currentTabsOrder: ["T1", "T2", "T4", "T3", "T5"],
    dragTarget: "T4",
    expectedOrder: ["T1", "T2", "T4", "T3", "E1", "T5"],
  },
  {
    description: "Test for drag a tab to right with hidden tab",
    preferenceOrder: ["T1", "T2", "T3", "E1", "T4", "T5"],
    currentTabsOrder: ["T1", "T3", "T4", "T2", "T5"],
    dragTarget: "T2",
    expectedOrder: ["T1", "T3", "E1", "T4", "T2", "T5"],
  },
  {
    description:
      "Test for drag a tab to left end in case hidden tab was left end",
    preferenceOrder: ["E1", "T1", "T2", "T3", "T4", "T5"],
    currentTabsOrder: ["T4", "T1", "T2", "T3", "T5"],
    dragTarget: "T4",
    expectedOrder: ["E1", "T4", "T1", "T2", "T3", "T5"],
  },
  {
    description:
      "Test for drag a tab to right end in case hidden tab was right end",
    preferenceOrder: ["T1", "T2", "T3", "T4", "T5", "E1"],
    currentTabsOrder: ["T2", "T3", "T4", "T5", "T1"],
    dragTarget: "T1",
    expectedOrder: ["T2", "T3", "T4", "T5", "E1", "T1"],
  },
  {
    description: "Test for multiple hidden tabs",
    preferenceOrder: ["T1", "T2", "E1", "E2", "E3", "E4"],
    currentTabsOrder: ["T2", "T1"],
    dragTarget: "T1",
    expectedOrder: ["T2", "E1", "E2", "E3", "E4", "T1"],
  },
];

function run_test() {
  const {
    toAbsoluteOrder,
  } = require("resource://devtools/client/framework/toolbox-tabs-order-manager.js");

  for (const {
    description,
    preferenceOrder,
    currentTabsOrder,
    dragTarget,
    expectedOrder,
  } of TEST_DATA) {
    info(description);
    const resultOrder = toAbsoluteOrder(
      preferenceOrder,
      currentTabsOrder,
      dragTarget
    );
    equal(
      resultOrder.join(","),
      expectedOrder.join(","),
      "Result should be correct"
    );
  }
}
