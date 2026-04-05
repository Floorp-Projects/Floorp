// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import UndoClosedTab from "../../common/undo-closed-tab/index.ts";

import { assert, runTests, type TestCase } from "../utils/test_harness.ts";

function testUndoClosedTabClassExists(): void {
  assert(
    typeof UndoClosedTab === "function",
    "UndoClosedTab should be a constructor function",
  );
}

function testUndoClosedTabHasInitMethod(): void {
  assert(
    typeof UndoClosedTab.prototype.init === "function",
    "UndoClosedTab.prototype.init should be a function",
  );
}

function testWindowUndoCloseTabDeclaration(): void {
  // The module declares window.undoCloseTab on the global Window interface.
  // Verify that the global undoCloseTab function or SessionWindowUI is available.
  assert(
    typeof SessionWindowUI !== "undefined",
    "SessionWindowUI should be available in the browser environment",
  );
  assert(
    typeof SessionWindowUI.undoCloseTab === "function",
    "SessionWindowUI.undoCloseTab should be a function",
  );
}

function testServicesWmGetMostRecentWindow(): void {
  // The module uses Services.wm.getMostRecentWindow to find the browser window
  const win = Services.wm.getMostRecentWindow("navigator:browser");
  assert(
    win !== null,
    "Services.wm.getMostRecentWindow should return a window",
  );
  assert(
    typeof (win as any).undoCloseTab === "function" ||
      typeof SessionWindowUI.undoCloseTab === "function",
    "undoCloseTab should be accessible either on window or SessionWindowUI",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("undoClosedTab.test.ts", [
    { name: "UndoClosedTab class exists", fn: testUndoClosedTabClassExists },
    {
      name: "UndoClosedTab has init method",
      fn: testUndoClosedTabHasInitMethod,
    },
    {
      name: "SessionWindowUI.undoCloseTab available",
      fn: testWindowUndoCloseTabDeclaration,
    },
    {
      name: "Services.wm.getMostRecentWindow works",
      fn: testServicesWmGetMostRecentWindow,
    },
  ]);
}
