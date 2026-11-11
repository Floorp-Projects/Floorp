// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  shouldBlockWorkspaceOpenLink,
  workspaceDefaultIsPending,
} from "../open-link-user-context.ts";
import {
  assertEquals as assertValue,
  runTests,
} from "../../../../../test/utils/test_harness.ts";

function assertEquals(actual: unknown, expected: unknown): void {
  assertValue(
    actual,
    expected,
    "container operation must preserve the expected state",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("pendingWorkspaceContainer.test.ts", [{
    name:
      "early startup waits for workspace service while private and explicit opens remain available",
    fn: () => {
      const noService = workspaceDefaultIsPending(true, false, undefined);
      assertEquals(noService, true);
      assertEquals(workspaceDefaultIsPending(true, true, undefined), false);
      assertEquals(workspaceDefaultIsPending(false, false, undefined), false);
      assertEquals(workspaceDefaultIsPending(true, false, true), false);
      assertEquals(
        shouldBlockWorkspaceOpenLink(undefined, "tab", noService),
        true,
      );
      assertEquals(
        shouldBlockWorkspaceOpenLink({ userContextId: 0 }, "tab", noService),
        false,
      );
    },
  }, {
    name: "unresolved defaults block only implicit new tabs",
    fn: () => {
      assertEquals(shouldBlockWorkspaceOpenLink(undefined, "tab", true), true);
      assertEquals(shouldBlockWorkspaceOpenLink({}, "tabshifted", true), true);
      assertEquals(
        shouldBlockWorkspaceOpenLink(undefined, "current", true),
        false,
      );
      assertEquals(
        shouldBlockWorkspaceOpenLink({ userContextId: 0 }, "tab", true),
        false,
      );
      assertEquals(
        shouldBlockWorkspaceOpenLink({ userContextId: 7 }, "tab", true),
        false,
      );
      assertEquals(
        shouldBlockWorkspaceOpenLink(
          { targetBrowser: { userContextId: 0 } },
          "tab",
          true,
        ),
        false,
      );
      assertEquals(
        shouldBlockWorkspaceOpenLink(undefined, "tab", false),
        false,
      );
    },
  }]);
}
