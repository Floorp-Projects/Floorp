// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  hasAnyWindowWithHiddenTabToPreserve,
  hasHiddenTabToPreserve,
  shouldCloseWindowForLastTabReplacement,
} from "../utils/workspace-last-tab-policy.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

type FakeTab = {
  hidden: boolean;
  hasAttribute(name: string): boolean;
};

function makeTab(hidden: boolean): FakeTab {
  return {
    hidden,
    hasAttribute: (name) => name === "hidden" && hidden,
  };
}

function testNativeLastTabCloseIsAllowedWithoutHiddenTabs(): void {
  assertEquals(
    hasHiddenTabToPreserve([makeTab(false)]),
    false,
    "a normal one-tab window does not need Workspace close handling",
  );
  assertEquals(
    hasHiddenTabToPreserve([makeTab(false), makeTab(false)]),
    false,
    "additional visible tabs already keep Firefox out of its last-tab path",
  );
}

function testHiddenTabRequiresFloorpHandling(): void {
  const closingTab = makeTab(false);
  assertEquals(
    hasHiddenTabToPreserve(
      [closingTab, makeTab(false), makeTab(true)],
      closingTab,
    ),
    true,
    "an additional hidden tab must keep native last-tab window close disabled",
  );
  const closingHiddenTab = makeTab(true);
  assertEquals(
    hasHiddenTabToPreserve([closingHiddenTab], closingHiddenTab),
    false,
    "the tab being closed does not need to be preserved",
  );
}

function testGlobalPolicyIncludesEveryBrowserWindow(): void {
  const visibleWindow = [makeTab(false), makeTab(false)];
  const hiddenWindow = [makeTab(false), makeTab(true)];

  assertEquals(
    hasAnyWindowWithHiddenTabToPreserve([visibleWindow, visibleWindow]),
    false,
    "native close is allowed when no window has hidden tabs",
  );
  assertEquals(
    hasAnyWindowWithHiddenTabToPreserve([visibleWindow, hiddenWindow]),
    true,
    "a hidden tab in another window disables native close globally",
  );
  assertEquals(
    hasAnyWindowWithHiddenTabToPreserve([hiddenWindow, hiddenWindow]),
    true,
    "native close remains disabled when multiple windows need preservation",
  );
  assertEquals(
    hasAnyWindowWithHiddenTabToPreserve([visibleWindow]),
    false,
    "native close is restored after the hidden-tab window goes away",
  );
}

function testCrossWindowReplacementRespectsExitOnLastTabPolicy(): void {
  const closingTab = makeTab(false);
  const replacementTab = makeTab(false);
  const currentWindow = [closingTab, replacementTab];
  const otherWindow = [makeTab(false), makeTab(true)];

  assertEquals(
    hasAnyWindowWithHiddenTabToPreserve(
      [currentWindow, otherWindow],
      closingTab,
    ),
    true,
    "another window keeps the profile-wide last-tab preference disabled",
  );
  assertEquals(
    shouldCloseWindowForLastTabReplacement(
      currentWindow,
      closingTab,
      false,
    ),
    false,
    "the replacement keeps this window open when Workspace exit is disabled",
  );
  assertEquals(
    shouldCloseWindowForLastTabReplacement(
      currentWindow,
      closingTab,
      true,
    ),
    true,
    "the replacement can use native window close when Workspace exit is enabled",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "native last-tab close is allowed without hidden tabs",
      fn: testNativeLastTabCloseIsAllowedWithoutHiddenTabs,
    },
    {
      name: "hidden tab requires Floorp handling",
      fn: testHiddenTabRequiresFloorpHandling,
    },
    {
      name: "global policy includes every browser window",
      fn: testGlobalPolicyIncludesEveryBrowserWindow,
    },
    {
      name: "cross-window replacement respects exit-on-last-tab policy",
      fn: testCrossWindowReplacementRespectsExitOnLastTabPolicy,
    },
  ];

  await runTests("workspaceLastTabPolicy.test.ts", tests);
}
