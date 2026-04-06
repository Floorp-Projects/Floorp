// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  getOldInterfaceConfig,
  getOldTabbarStyleConfig,
  getOldTabbarPositionConfig,
} from "../utils/old-config-migrator.ts";
import {
  assertEquals,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Helpers — set/clear prefs safely
// ---------------------------------------------------------------------------

function withIntPref(prefName: string, value: number, fn: () => void): void {
  Services.prefs.setIntPref(prefName, value);
  try {
    fn();
  } finally {
    Services.prefs.clearUserPref(prefName);
  }
}

function withIntPrefs(prefs: [string, number][], fn: () => void): void {
  for (const [name, value] of prefs) {
    Services.prefs.setIntPref(name, value);
  }
  try {
    fn();
  } finally {
    for (const [name] of prefs) {
      Services.prefs.clearUserPref(name);
    }
  }
}

// ---------------------------------------------------------------------------
// Tests — getOldInterfaceConfig
// ---------------------------------------------------------------------------

function testInterfaceDefaultLepton(): void {
  withIntPref("floorp.browser.user.interface", 0, () => {
    assertEquals(
      getOldInterfaceConfig(),
      "lepton",
      "default (0) should return lepton",
    );
  });
}

function testInterfaceCase3DefaultLepton(): void {
  withIntPrefs(
    [
      ["floorp.browser.user.interface", 3],
      ["floorp.lepton.interface", 0],
    ],
    () => {
      assertEquals(
        getOldInterfaceConfig(),
        "lepton",
        "interface=3, lepton=0 should return lepton",
      );
    },
  );
}

function testInterfaceCase3Photon(): void {
  withIntPrefs(
    [
      ["floorp.browser.user.interface", 3],
      ["floorp.lepton.interface", 1],
    ],
    () => {
      assertEquals(
        getOldInterfaceConfig(),
        "photon",
        "interface=3, lepton=1 should return photon",
      );
    },
  );
}

function testInterfaceCase3Protonfix(): void {
  withIntPrefs(
    [
      ["floorp.browser.user.interface", 3],
      ["floorp.lepton.interface", 3],
    ],
    () => {
      assertEquals(
        getOldInterfaceConfig(),
        "protonfix",
        "interface=3, lepton=3 should return protonfix",
      );
    },
  );
}

function testInterfaceCase8Fluerial(): void {
  withIntPref("floorp.browser.user.interface", 8, () => {
    assertEquals(
      getOldInterfaceConfig(),
      "fluerial",
      "interface=8 should return fluerial",
    );
  });
}

// ---------------------------------------------------------------------------
// Tests — getOldTabbarStyleConfig
// ---------------------------------------------------------------------------

function testTabbarStyleHorizontal(): void {
  withIntPref("floorp.tabbar.style", 0, () => {
    assertEquals(
      getOldTabbarStyleConfig(),
      "horizontal",
      "style=0 should return horizontal",
    );
  });
}

function testTabbarStyleMultirow(): void {
  withIntPref("floorp.tabbar.style", 1, () => {
    assertEquals(
      getOldTabbarStyleConfig(),
      "multirow",
      "style=1 should return multirow",
    );
  });
}

function testTabbarStyleVertical(): void {
  withIntPref("floorp.tabbar.style", 2, () => {
    assertEquals(
      getOldTabbarStyleConfig(),
      "vertical",
      "style=2 should return vertical",
    );
  });
}

// ---------------------------------------------------------------------------
// Tests — getOldTabbarPositionConfig
// ---------------------------------------------------------------------------

function testTabbarPositionDefault(): void {
  withIntPref("floorp.browser.tabbar.settings", 0, () => {
    assertEquals(
      getOldTabbarPositionConfig(),
      "default",
      "settings=0 should return default",
    );
  });
}

function testTabbarPositionHide(): void {
  withIntPref("floorp.browser.tabbar.settings", 1, () => {
    assertEquals(
      getOldTabbarPositionConfig(),
      "hide-horizontal-tabbar",
      "settings=1 should return hide-horizontal-tabbar",
    );
  });
}

function testTabbarPositionOptimise(): void {
  withIntPref("floorp.browser.tabbar.settings", 2, () => {
    assertEquals(
      getOldTabbarPositionConfig(),
      "optimise-to-vertical-tabbar",
      "settings=2 should return optimise-to-vertical-tabbar",
    );
  });
}

function testTabbarPositionBottomNav(): void {
  withIntPref("floorp.browser.tabbar.settings", 3, () => {
    assertEquals(
      getOldTabbarPositionConfig(),
      "bottom-of-navigation-toolbar",
      "settings=3 should return bottom-of-navigation-toolbar",
    );
  });
}

function testTabbarPositionBottomWindow(): void {
  withIntPref("floorp.browser.tabbar.settings", 4, () => {
    assertEquals(
      getOldTabbarPositionConfig(),
      "bottom-of-window",
      "settings=4 should return bottom-of-window",
    );
  });
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "interface default → lepton", fn: testInterfaceDefaultLepton },
    {
      name: "interface 3+0 → lepton",
      fn: testInterfaceCase3DefaultLepton,
    },
    { name: "interface 3+1 → photon", fn: testInterfaceCase3Photon },
    { name: "interface 3+3 → protonfix", fn: testInterfaceCase3Protonfix },
    { name: "interface 8 → fluerial", fn: testInterfaceCase8Fluerial },
    { name: "tabbar style 0 → horizontal", fn: testTabbarStyleHorizontal },
    { name: "tabbar style 1 → multirow", fn: testTabbarStyleMultirow },
    { name: "tabbar style 2 → vertical", fn: testTabbarStyleVertical },
    { name: "tabbar position 0 → default", fn: testTabbarPositionDefault },
    { name: "tabbar position 1 → hide", fn: testTabbarPositionHide },
    { name: "tabbar position 2 → optimise", fn: testTabbarPositionOptimise },
    {
      name: "tabbar position 3 → bottom-nav",
      fn: testTabbarPositionBottomNav,
    },
    {
      name: "tabbar position 4 → bottom-window",
      fn: testTabbarPositionBottomWindow,
    },
  ];

  const failures: string[] = [];

  for (const test of tests) {
    try {
      test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }

  if (failures.length > 0) {
    throw new Error(
      `oldConfigMigrator.test.ts failures: ${failures.join(" | ")}`,
    );
  }
}
