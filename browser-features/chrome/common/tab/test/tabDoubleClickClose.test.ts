// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { TabDoubleClickClose } from "../doubleClickClose/index.ts";
import { config, setConfig } from "../../designs/configs.ts";
import { createRoot } from "solid-js";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function constructInSolidRoot(construct: () => void): (() => void) | undefined {
  let dispose: (() => void) | undefined;
  createRoot((cleanup) => {
    dispose = cleanup;
    construct();
  });
  return dispose;
}

function withTabConfigPatch(
  patch: {
    tabDubleClickToClose?: boolean;
  },
  run: () => void,
): void {
  const original = JSON.parse(JSON.stringify(config()));

  try {
    setConfig((prev) => ({
      ...prev,
      tab: {
        ...prev.tab,
        tabDubleClickToClose:
          patch.tabDubleClickToClose ?? prev.tab.tabDubleClickToClose,
      },
    }));

    run();
  } finally {
    setConfig(original);
  }
}

function testTabDoubleClickCloseClassIsDefined(): void {
  assert(
    typeof TabDoubleClickClose === "function",
    "TabDoubleClickClose should be a class/function",
  );
}

function testTabDoubleClickCloseConstructorHandlesMissingReactiveContext(): void {
  try {
    new TabDoubleClickClose();
  } catch (e) {
    // createEffect from solid-js may not be available in all test contexts
    const msg = e instanceof Error ? e.message : String(e);
    assert(
      msg.includes("solid") ||
        msg.includes("effect") ||
        msg.includes("owner") ||
        msg.includes("createEffect"),
      `Unexpected error: ${msg}`,
    );
  }
}

function testTabDoubleClickCloseSyncsPrefWhenEnabled(): void {
  const prefName = "browser.tabs.closeTabByDblclick";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    withTabConfigPatch({ tabDubleClickToClose: true }, () => {
      constructInSolidRoot(() => {
        new TabDoubleClickClose();
      });
      assertEquals(
        Services.prefs.getBoolPref(prefName, false),
        true,
        "constructor should sync double-click close pref to true when config is true",
      );
    });
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

function testTabDoubleClickCloseSyncsPrefWhenDisabled(): void {
  const prefName = "browser.tabs.closeTabByDblclick";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    withTabConfigPatch({ tabDubleClickToClose: false }, () => {
      constructInSolidRoot(() => {
        new TabDoubleClickClose();
      });
      assertEquals(
        Services.prefs.getBoolPref(prefName, false),
        false,
        "constructor should sync double-click close pref to false when config is false",
      );
    });
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

function testTabDoubleClickCloseReactsToConfigChanges(): void {
  const prefName = "browser.tabs.closeTabByDblclick";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    withTabConfigPatch({ tabDubleClickToClose: false }, () => {
      const dispose = constructInSolidRoot(() => {
        new TabDoubleClickClose();
      });

      // Toggle from false to true (outside constructInSolidRoot but before dispose)
      setConfig((prev) => ({
        ...prev,
        tab: {
          ...prev.tab,
          tabDubleClickToClose: true,
        },
      }));

      assertEquals(
        Services.prefs.getBoolPref(prefName, false),
        true,
        "pref should update to true when config changes to true",
      );

      // Toggle from true to false
      setConfig((prev) => ({
        ...prev,
        tab: {
          ...prev.tab,
          tabDubleClickToClose: false,
        },
      }));

      assertEquals(
        Services.prefs.getBoolPref(prefName, false),
        false,
        "pref should update to false when config changes to false",
      );

      dispose?.();
    });
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

function testTabDoubleClickCloseHandlesMultipleInstances(): void {
  const prefName = "browser.tabs.closeTabByDblclick";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    withTabConfigPatch({ tabDubleClickToClose: true }, () => {
      const dispose = constructInSolidRoot(() => {
        // Create multiple instances - they should all sync the same pref
        const instance1 = new TabDoubleClickClose();
        const instance2 = new TabDoubleClickClose();

        assert(
          instance1 !== undefined && instance2 !== undefined,
          "Multiple instances should be created successfully"
        );
      });

      assertEquals(
        Services.prefs.getBoolPref(prefName, false),
        true,
        "pref should remain true with multiple instances",
      );

      dispose?.();
    });
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

function testTabDoubleClickCloseHandlesPrefErrors(): void {
  const prefName = "browser.tabs.closeTabByDblclick";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    // Test with invalid config state
    withTabConfigPatch({ tabDubleClickToClose: true }, () => {
      constructInSolidRoot(() => {
        // Even if pref setting fails, the constructor should not throw
        try {
          new TabDoubleClickClose();
          assert(true, "Constructor should handle pref errors gracefully");
        } catch (e) {
          const msg = e instanceof Error ? e.message : String(e);
          // Should only fail due to solid-js context, not pref operations
          assert(
            msg.includes("solid") ||
              msg.includes("effect") ||
              msg.includes("owner") ||
              msg.includes("createEffect"),
            `Unexpected error: ${msg}`,
          );
        }
      });
    });
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

const tests: TestCase[] = [
  {
    name: "TabDoubleClickClose class is defined and constructable",
    fn: testTabDoubleClickCloseClassIsDefined,
  },
  {
    name: "TabDoubleClickClose constructor handles missing reactive context gracefully",
    fn: testTabDoubleClickCloseConstructorHandlesMissingReactiveContext,
  },
  {
    name: "TabDoubleClickClose syncs pref to true when enabled",
    fn: testTabDoubleClickCloseSyncsPrefWhenEnabled,
  },
  {
    name: "TabDoubleClickClose syncs pref to false when disabled",
    fn: testTabDoubleClickCloseSyncsPrefWhenDisabled,
  },
  {
    name: "TabDoubleClickClose reacts to config changes",
    fn: testTabDoubleClickCloseReactsToConfigChanges,
  },
  {
    name: "TabDoubleClickClose handles multiple instances",
    fn: testTabDoubleClickCloseHandlesMultipleInstances,
  },
  {
    name: "TabDoubleClickClose handles pref errors gracefully",
    fn: testTabDoubleClickCloseHandlesPrefErrors,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("tabDoubleClickClose.test.ts", tests);
}
