// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import {
  isPaletteTargetAvailable,
  resolvePaletteTarget,
} from "../utils/targetContext.ts";

function createTargetWindow(): {
  targetWindow: Window;
  targetTab: XULElement;
  targetBrowser: {
    contentPrincipal: nsIPrincipal;
    browsingContext: { originAttributes: Record<string, unknown> };
  };
} {
  const targetTab = { isConnected: true } as XULElement;
  const targetBrowser = {
    contentPrincipal: Services.scriptSecurityManager.createNullPrincipal({
      privateBrowsingId: 1,
      userContextId: 2,
    }),
    browsingContext: {
      originAttributes: { privateBrowsingId: 1, userContextId: 2 },
    },
  };
  const gBrowser = {
    selectedTab: targetTab,
    getBrowserForTab(tab: XULElement) {
      return tab === targetTab ? targetBrowser : null;
    },
  };
  const targetWindow = {
    closed: false,
    gBrowser,
  } as unknown as Window;
  return { targetWindow, targetTab, targetBrowser };
}

const rawTests: TestCase[] = [
  {
    name: "resolvePaletteTarget uses only the supplied window",
    fn() {
      const { targetWindow, targetTab, targetBrowser } = createTargetWindow();
      const target = resolvePaletteTarget(targetWindow);

      assert(target !== null, "target should resolve");
      assertEquals(
        target.tab,
        targetTab,
        "selected tab should come from target window",
      );
      assertEquals(
        target.browser,
        targetBrowser,
        "browser should come from target window",
      );
      assertEquals(
        target.originAttributes.privateBrowsingId,
        1,
        "browser privateBrowsingId should be captured",
      );
    },
  },
  {
    name: "isPaletteTargetAvailable rejects a replaced browser",
    fn() {
      const { targetWindow } = createTargetWindow();
      const target = resolvePaletteTarget(targetWindow);
      assert(target !== null, "target should resolve");
      assert(
        isPaletteTargetAvailable(target),
        "unchanged target should remain available",
      );

      const gBrowser = (targetWindow as unknown as { gBrowser: GBrowser })
        .gBrowser;
      gBrowser.getBrowserForTab = () => ({
        contentPrincipal: target.principal,
        loadURI() {},
      });
      assert(
        !isPaletteTargetAvailable(target),
        "replaced browser should fail closed",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("targetContext.test.ts", rawTests);
}
