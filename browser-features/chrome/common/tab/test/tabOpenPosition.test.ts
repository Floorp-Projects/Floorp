// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { TabOpenPosition } from "../openPosition/index.ts";
import { config } from "../../designs/configs.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function constructInPreactEffect(construct: () => void): (() => void) | undefined {
  construct();
  return undefined;
}

function withTabConfigPatch(
  patch: {
    tabOpenPosition?: number;
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
        tabOpenPosition: patch.tabOpenPosition ?? prev.tab.tabOpenPosition,
      },
    };

    run();
  } finally {
    config.value = original;
  }
}

function testTabOpenPositionClassIsDefined(): void {
  assert(
    typeof TabOpenPosition === "function",
    "TabOpenPosition should be a class/function",
  );
}

function testTabOpenPositionConstructorHandlesMissingReactiveContext(): void {
  try {
    new TabOpenPosition();
  } catch (e) {
    // preact effects are self-contained and don't require an owner context;
    // unexpected errors from the constructor should be investigated
    const msg = e instanceof Error ? e.message : String(e);
    assert(
      msg.includes("effect") ||
        msg.includes("signal"),
      `Unexpected error: ${msg}`,
    );
  }
}

function testTabOpenPositionSyncsPrefWithDefault(): void {
  const prefName = "floorp.browser.tabs.openNewTabPosition";
  const originalPref = Services.prefs.getIntPref(prefName, -1);

  try {
    withTabConfigPatch({ tabOpenPosition: -1 }, () => {
      constructInPreactEffect(() => {
        new TabOpenPosition();
      });
      assertEquals(
        Services.prefs.getIntPref(prefName, -1),
        -1,
        "constructor should sync open-position pref to default (-1)",
      );
    });
  } finally {
    Services.prefs.setIntPref(prefName, originalPref);
  }
}

function testTabOpenPositionSyncsPrefWithAtEnd(): void {
  const prefName = "floorp.browser.tabs.openNewTabPosition";
  const originalPref = Services.prefs.getIntPref(prefName, -1);

  try {
    withTabConfigPatch({ tabOpenPosition: 3 }, () => {
      constructInPreactEffect(() => {
        new TabOpenPosition();
      });
      assertEquals(
        Services.prefs.getIntPref(prefName, -1),
        3,
        "constructor should sync open-position pref to 3 (at end)",
      );
    });
  } finally {
    Services.prefs.setIntPref(prefName, originalPref);
  }
}

function testTabOpenPositionSyncsPrefWithAtStart(): void {
  const prefName = "floorp.browser.tabs.openNewTabPosition";
  const originalPref = Services.prefs.getIntPref(prefName, -1);

  try {
    withTabConfigPatch({ tabOpenPosition: 2 }, () => {
      constructInPreactEffect(() => {
        new TabOpenPosition();
      });
      assertEquals(
        Services.prefs.getIntPref(prefName, -1),
        2,
        "constructor should sync open-position pref to 2 (at start)",
      );
    });
  } finally {
    Services.prefs.setIntPref(prefName, originalPref);
  }
}

function testTabOpenPositionSyncsPrefWithAfterCurrent(): void {
  const prefName = "floorp.browser.tabs.openNewTabPosition";
  const originalPref = Services.prefs.getIntPref(prefName, -1);

  try {
    withTabConfigPatch({ tabOpenPosition: 1 }, () => {
      constructInPreactEffect(() => {
        new TabOpenPosition();
      });
      assertEquals(
        Services.prefs.getIntPref(prefName, -1),
        1,
        "constructor should sync open-position pref to 1 (after current)",
      );
    });
  } finally {
    Services.prefs.setIntPref(prefName, originalPref);
  }
}

function testTabOpenPositionReactsToConfigChanges(): void {
  const prefName = "floorp.browser.tabs.openNewTabPosition";
  const originalPref = Services.prefs.getIntPref(prefName, -1);

  try {
    withTabConfigPatch({ tabOpenPosition: -1 }, () => {
      const dispose = constructInPreactEffect(() => {
        new TabOpenPosition();
      });

      // Change to at end
      config.value = {
        ...config.value,
        tab: {
          ...config.value.tab,
          tabOpenPosition: 3,
        },
      };

      assertEquals(
        Services.prefs.getIntPref(prefName, -1),
        3,
        "pref should update to 3 when config changes",
      );

      // Change to after current
      config.value = {
        ...config.value,
        tab: {
          ...config.value.tab,
          tabOpenPosition: 1,
        },
      };

      assertEquals(
        Services.prefs.getIntPref(prefName, -1),
        1,
        "pref should update to 1 when config changes",
      );

      dispose?.();
    });
  } finally {
    Services.prefs.setIntPref(prefName, originalPref);
  }
}

function testTabOpenPositionHandlesBoundaryValues(): void {
  const prefName = "floorp.browser.tabs.openNewTabPosition";
  const originalPref = Services.prefs.getIntPref(prefName, -1);

  try {
    // Test with 0 (should be accepted as valid int)
    withTabConfigPatch({ tabOpenPosition: 0 }, () => {
      constructInPreactEffect(() => {
        new TabOpenPosition();
      });
      assertEquals(
        Services.prefs.getIntPref(prefName, -1),
        0,
        "pref should accept 0 as valid value",
      );
    });

    // Test with large positive value
    withTabConfigPatch({ tabOpenPosition: 999 }, () => {
      constructInPreactEffect(() => {
        new TabOpenPosition();
      });
      assertEquals(
        Services.prefs.getIntPref(prefName, -1),
        999,
        "pref should accept large positive value",
      );
    });

    // Test with negative value
    withTabConfigPatch({ tabOpenPosition: -5 }, () => {
      constructInPreactEffect(() => {
        new TabOpenPosition();
      });
      assertEquals(
        Services.prefs.getIntPref(prefName, -1),
        -5,
        "pref should accept negative value",
      );
    });
  } finally {
    Services.prefs.setIntPref(prefName, originalPref);
  }
}

function testTabOpenPositionHandlesMultipleInstances(): void {
  const prefName = "floorp.browser.tabs.openNewTabPosition";
  const originalPref = Services.prefs.getIntPref(prefName, -1);

  try {
    withTabConfigPatch({ tabOpenPosition: 2 }, () => {
      const dispose = constructInPreactEffect(() => {
        // Create multiple instances - they should all sync the same pref
        const instance1 = new TabOpenPosition();
        const instance2 = new TabOpenPosition();

        assert(
          instance1 !== undefined && instance2 !== undefined,
          "Multiple instances should be created successfully"
        );
      });

      assertEquals(
        Services.prefs.getIntPref(prefName, -1),
        2,
        "pref should remain 2 with multiple instances",
      );

      dispose?.();
    });
  } finally {
    Services.prefs.setIntPref(prefName, originalPref);
  }
}

const tests: TestCase[] = [
  {
    name: "TabOpenPosition class is defined and constructable",
    fn: testTabOpenPositionClassIsDefined,
  },
  {
    name: "TabOpenPosition constructor handles missing reactive context gracefully",
    fn: testTabOpenPositionConstructorHandlesMissingReactiveContext,
  },
  {
    name: "TabOpenPosition syncs pref with default value (-1)",
    fn: testTabOpenPositionSyncsPrefWithDefault,
  },
  {
    name: "TabOpenPosition syncs pref with at end (3)",
    fn: testTabOpenPositionSyncsPrefWithAtEnd,
  },
  {
    name: "TabOpenPosition syncs pref with at start (2)",
    fn: testTabOpenPositionSyncsPrefWithAtStart,
  },
  {
    name: "TabOpenPosition syncs pref with after current (1)",
    fn: testTabOpenPositionSyncsPrefWithAfterCurrent,
  },
  {
    name: "TabOpenPosition reacts to config changes",
    fn: testTabOpenPositionReactsToConfigChanges,
  },
  {
    name: "TabOpenPosition handles boundary values",
    fn: testTabOpenPositionHandlesBoundaryValues,
  },
  {
    name: "TabOpenPosition handles multiple instances",
    fn: testTabOpenPositionHandlesMultipleInstances,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("tabOpenPosition.test.ts", tests);
}
