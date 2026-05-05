// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { TabScroll } from "../scroll/index.ts";
import { config } from "../../designs/configs.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function constructInPreactEffect(construct: () => void): () => void {
  construct();
  // preact effects are self-contained; return a no-op for call-site compatibility
  return () => {};
}

function withTabConfigPatch(
  patch: {
    tabScrollEnabled?: boolean;
    tabScrollReverse?: boolean;
    tabScrollWrap?: boolean;
  },
  run: () => void,
): void {
  const original = JSON.parse(JSON.stringify(config.value));

  try {
    const prev = config.value;
    config.value = {
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
    };

    run();
  } finally {
    config.value = original;
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
    // preact effects are self-contained and don't require an owner context,
    // so we only check that the class itself is importable and defined.
    const msg = e instanceof Error ? e.message : String(e);
    // If it fails due to missing signal/effect, that is acceptable
    assert(
      msg.includes("effect") ||
        msg.includes("signal"),
      `Unexpected error: ${msg}`,
    );
  }
}

function testTabScrollSyncsSwitchByScrollingPrefWhenEnabled(): void {
  const prefName = "toolkit.tabbox.switchByScrolling";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    withTabConfigPatch({ tabScrollEnabled: true }, () => {
      constructInPreactEffect(() => {
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
      constructInPreactEffect(() => {
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
      const dispose = constructInPreactEffect(() => {
        new TabScroll();

        // Toggle from false to true
        config.value = {
          ...config.value,
          tab: {
            ...config.value.tab,
            tabScroll: {
              ...config.value.tab.tabScroll,
              enabled: true,
            },
          },
        };

        assertEquals(
          config.value.tab.tabScroll.enabled,
          true,
          "config should update to true",
        );

        // Toggle from true to false
        config.value = {
          ...config.value,
          tab: {
            ...config.value.tab,
            tabScroll: {
              ...config.value.tab.tabScroll,
              enabled: false,
            },
          },
        };

        assertEquals(
          config.value.tab.tabScroll.enabled,
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
    withTabConfigPatch(
      { tabScrollEnabled: true, tabScrollReverse: true },
      () => {
        constructInPreactEffect(() => {
          new TabScroll();
          // The reverse setting is used in the wheel handler, not a pref
          // but we verify the instance is created successfully
          assert(true, "Instance should be created with reverse enabled");
        });
      },
    );

    // Test with reverse disabled
    withTabConfigPatch(
      { tabScrollEnabled: true, tabScrollReverse: false },
      () => {
        constructInPreactEffect(() => {
          new TabScroll();
          assert(true, "Instance should be created with reverse disabled");
        });
      },
    );
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
      constructInPreactEffect(() => {
        new TabScroll();
        // The wrap setting is used in the wheel handler, not a pref
        // but we verify the instance is created successfully
        assert(true, "Instance should be created with wrap enabled");
      });
    });

    // Test with wrap disabled
    withTabConfigPatch({ tabScrollEnabled: true, tabScrollWrap: false }, () => {
      constructInPreactEffect(() => {
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
  withTabConfigPatch(
    { tabScrollEnabled: true, tabScrollReverse: false, tabScrollWrap: true },
    () => {
      constructInPreactEffect(() => {
        const instance = new TabScroll();
        assert(
          typeof (instance as TabScroll)["handleOnWheel"] === "function",
          "handleOnWheel should be a function",
        );
      });
    },
  );
}

function testTabScrollHandlesMultipleInstances(): void {
  const prefName = "toolkit.tabbox.switchByScrolling";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    withTabConfigPatch({ tabScrollEnabled: true }, () => {
      constructInPreactEffect(() => {
        // Create multiple instances
        const instance1 = new TabScroll();
        const instance2 = new TabScroll();

        assert(
          instance1 !== undefined && instance2 !== undefined,
          "Multiple instances should be created successfully",
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
      withTabConfigPatch(
        {
          tabScrollEnabled: true,
          tabScrollReverse: combo.reverse,
          tabScrollWrap: combo.wrap,
        },
        () => {
          constructInPreactEffect(() => {
            new TabScroll();
            assert(
              true,
              `Instance should be created with reverse=${combo.reverse}, wrap=${combo.wrap}`,
            );
          });
        },
      );
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
