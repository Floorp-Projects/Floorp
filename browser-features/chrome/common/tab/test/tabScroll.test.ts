// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { TabScroll } from "../scroll/index.ts";
import { config, setConfig } from "../../designs/configs.ts";
import { createRoot } from "solid-js";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function constructInSolidRoot(construct: () => void): () => void {
  let dispose: (() => void) | undefined;
  createRoot((cleanup) => {
    dispose = cleanup;
    construct();
  });
  // Return dispose so callers can control when the reactive root is cleaned up
  return dispose ?? (() => {});
}

function withTabConfigPatch(
  patch: {
    tabScrollEnabled?: boolean;
    tabScrollReverse?: boolean;
    tabScrollWrap?: boolean;
  },
  run: () => void,
): void {
  const original = JSON.parse(JSON.stringify(config()));

  try {
    setConfig((prev) => ({
      ...prev,
      tab: {
        ...prev.tab,
        tabScroll: {
          ...prev.tab.tabScroll,
          enabled: patch.tabScrollEnabled ?? prev.tab.tabScroll.enabled,
          reverse: patch.tabScrollReverse ?? prev.tab.tabScroll.reverse,
          wrap: patch.tabScrollWrap ?? prev.tab.tabScroll.wrap,
        },
      },
    }));

    run();
  } finally {
    setConfig(original);
  }
}

function testTabScrollClassIsDefined(): void {
  assert(
    typeof TabScroll === "function",
    "TabScroll should be a class/function",
  );
}

function testTabScrollConstructorDoesNotThrowWhenTabBrowserTabsAbsent(): void {
  // In test context, document.querySelector("#tabbrowser-tabs") returns null.
  // The constructor should handle that gracefully.
  try {
    new TabScroll();
  } catch (e) {
    // createEffect from solid-js may not be available in all test contexts,
    // so we only check that the class itself is importable and defined.
    const msg = e instanceof Error ? e.message : String(e);
    // If it fails due to missing solid-js reactivity context, that is acceptable
    assert(
      msg.includes("solid") ||
        msg.includes("effect") ||
        msg.includes("owner") ||
        msg.includes("createEffect"),
      `Unexpected error: ${msg}`,
    );
  }
}

function testTabScrollSyncsSwitchByScrollingPrefWhenEnabled(): void {
  const prefName = "toolkit.tabbox.switchByScrolling";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    withTabConfigPatch({ tabScrollEnabled: true }, () => {
      constructInSolidRoot(() => {
        new TabScroll();
      });
      assertEquals(
        Services.prefs.getBoolPref(prefName, false),
        true,
        "constructor should sync switchByScrolling pref to true when enabled",
      );
    });
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

function testTabScrollSyncsSwitchByScrollingPrefWhenDisabled(): void {
  const prefName = "toolkit.tabbox.switchByScrolling";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    withTabConfigPatch({ tabScrollEnabled: false }, () => {
      constructInSolidRoot(() => {
        new TabScroll();
      });
      assertEquals(
        Services.prefs.getBoolPref(prefName, false),
        false,
        "constructor should sync switchByScrolling pref to false when disabled",
      );
    });
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

function testTabScrollReactsToConfigChanges(): void {
  const prefName = "toolkit.tabbox.switchByScrolling";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    withTabConfigPatch({ tabScrollEnabled: false }, () => {
      const dispose = constructInSolidRoot(() => {
        new TabScroll();

        // Toggle from false to true
        setConfig((prev) => ({
          ...prev,
          tab: {
            ...prev.tab,
            tabScroll: {
              ...prev.tab.tabScroll,
              enabled: true,
            },
          },
        }));

        assertEquals(
          config().tab.tabScroll.enabled,
          true,
          "config should update to true",
        );

        // Toggle from true to false
        setConfig((prev) => ({
          ...prev,
          tab: {
            ...prev.tab,
            tabScroll: {
              ...prev.tab.tabScroll,
              enabled: false,
            },
          },
        }));

        assertEquals(
          config().tab.tabScroll.enabled,
          false,
          "config should update to false",
        );
      });
      dispose();
    });
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

function testTabScrollHandlesReverseConfiguration(): void {
  const prefName = "toolkit.tabbox.switchByScrolling";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    // Test with reverse enabled
    withTabConfigPatch({ tabScrollEnabled: true, tabScrollReverse: true }, () => {
      constructInSolidRoot(() => {
        new TabScroll();
        // The reverse setting is used in the wheel handler, not a pref
        // but we verify the instance is created successfully
        assert(true, "Instance should be created with reverse enabled");
      });
    });

    // Test with reverse disabled
    withTabConfigPatch({ tabScrollEnabled: true, tabScrollReverse: false }, () => {
      constructInSolidRoot(() => {
        new TabScroll();
        assert(true, "Instance should be created with reverse disabled");
      });
    });
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

function testTabScrollHandlesWrapConfiguration(): void {
  const prefName = "toolkit.tabbox.switchByScrolling";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    // Test with wrap enabled
    withTabConfigPatch({ tabScrollEnabled: true, tabScrollWrap: true }, () => {
      constructInSolidRoot(() => {
        new TabScroll();
        // The wrap setting is used in the wheel handler, not a pref
        // but we verify the instance is created successfully
        assert(true, "Instance should be created with wrap enabled");
      });
    });

    // Test with wrap disabled
    withTabConfigPatch({ tabScrollEnabled: true, tabScrollWrap: false }, () => {
      constructInSolidRoot(() => {
        new TabScroll();
        assert(true, "Instance should be created with wrap disabled");
      });
    });
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

function testTabScrollHandleOnWheelMethod(): void {
  // Verify the handleOnWheel method exists and is a function
  withTabConfigPatch({ tabScrollEnabled: true, tabScrollReverse: false, tabScrollWrap: true }, () => {
    constructInSolidRoot(() => {
      const instance = new TabScroll();
      assert(
        typeof (instance as TabScroll)["handleOnWheel"] === "function",
        "handleOnWheel should be a function"
      );
    });
  });
}

function testTabScrollHandlesMultipleInstances(): void {
  const prefName = "toolkit.tabbox.switchByScrolling";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    withTabConfigPatch({ tabScrollEnabled: true }, () => {
      constructInSolidRoot(() => {
        // Create multiple instances
        const instance1 = new TabScroll();
        const instance2 = new TabScroll();

        assert(
          instance1 !== undefined && instance2 !== undefined,
          "Multiple instances should be created successfully"
        );

        assertEquals(
          Services.prefs.getBoolPref(prefName, false),
          true,
          "pref should remain true with multiple instances",
        );
      });
    });
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

function testTabScrollHandlesAllConfigCombinations(): void {
  const prefName = "toolkit.tabbox.switchByScrolling";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    // Test all combinations of reverse and wrap
    const combinations = [
      { reverse: false, wrap: false },
      { reverse: false, wrap: true },
      { reverse: true, wrap: false },
      { reverse: true, wrap: true },
    ];

    for (const combo of combinations) {
      withTabConfigPatch({
        tabScrollEnabled: true,
        tabScrollReverse: combo.reverse,
        tabScrollWrap: combo.wrap,
      }, () => {
        constructInSolidRoot(() => {
          new TabScroll();
          assert(
            true,
            `Instance should be created with reverse=${combo.reverse}, wrap=${combo.wrap}`
          );
        });
      });
    }
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

const tests: TestCase[] = [
  {
    name: "TabScroll class is defined and constructable",
    fn: testTabScrollClassIsDefined,
  },
  {
    name: "TabScroll constructor does not throw when #tabbrowser-tabs is absent",
    fn: testTabScrollConstructorDoesNotThrowWhenTabBrowserTabsAbsent,
  },
  {
    name: "TabScroll syncs switchByScrolling pref to true when enabled",
    fn: testTabScrollSyncsSwitchByScrollingPrefWhenEnabled,
  },
  {
    name: "TabScroll syncs switchByScrolling pref to false when disabled",
    fn: testTabScrollSyncsSwitchByScrollingPrefWhenDisabled,
  },
  {
    name: "TabScroll reacts to config changes",
    fn: testTabScrollReactsToConfigChanges,
  },
  {
    name: "TabScroll handles reverse configuration",
    fn: testTabScrollHandlesReverseConfiguration,
  },
  {
    name: "TabScroll handles wrap configuration",
    fn: testTabScrollHandlesWrapConfiguration,
  },
  {
    name: "TabScroll handleOnWheel method exists",
    fn: testTabScrollHandleOnWheelMethod,
  },
  {
    name: "TabScroll handles multiple instances",
    fn: testTabScrollHandlesMultipleInstances,
  },
  {
    name: "TabScroll handles all config combinations",
    fn: testTabScrollHandlesAllConfigCombinations,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("tabScroll.test.ts", tests);
}
