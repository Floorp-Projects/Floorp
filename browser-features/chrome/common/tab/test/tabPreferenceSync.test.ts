// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { TabDoubleClickClose } from "../doubleClickClose/index.ts";
import { TabOpenPosition } from "../openPosition/index.ts";
import { TabScroll } from "../scroll/index.ts";
import { config, setConfig } from "../../designs/configs.ts";
import { createRoot } from "solid-js";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

function constructInSolidRoot(construct: () => void): void {
  let dispose: (() => void) | undefined;
  createRoot((cleanup) => {
    dispose = cleanup;
    construct();
  });
  dispose?.();
}

function withTabConfigPatch(
  patch: {
    tabDubleClickToClose?: boolean;
    tabOpenPosition?: number;
    tabScrollEnabled?: boolean;
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
        tabOpenPosition: patch.tabOpenPosition ?? prev.tab.tabOpenPosition,
        tabScroll: {
          ...prev.tab.tabScroll,
          enabled: patch.tabScrollEnabled ?? prev.tab.tabScroll.enabled,
        },
      },
    }));

    run();
  } finally {
    setConfig(original);
  }
}

function testTabDoubleClickCloseSyncsPrefWhenConstructed(): void {
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
        "constructor should sync double-click close pref from config",
      );
    });
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

function testTabOpenPositionSyncsPrefWhenConstructed(): void {
  const prefName = "floorp.browser.tabs.openNewTabPosition";
  const originalPref = Services.prefs.getIntPref(prefName, -1);

  try {
    withTabConfigPatch({ tabOpenPosition: 2 }, () => {
      constructInSolidRoot(() => {
        new TabOpenPosition();
      });
      assertEquals(
        Services.prefs.getIntPref(prefName, -1),
        2,
        "constructor should sync open-position pref from config",
      );
    });
  } finally {
    Services.prefs.setIntPref(prefName, originalPref);
  }
}

function testTabScrollSyncsSwitchByScrollingPrefWhenConstructed(): void {
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
        "constructor should sync switchByScrolling pref from config",
      );
    });
  } finally {
    Services.prefs.setBoolPref(prefName, originalPref);
  }
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "TabDoubleClickClose syncs pref when constructed",
      fn: testTabDoubleClickCloseSyncsPrefWhenConstructed,
    },
    {
      name: "TabOpenPosition syncs pref when constructed",
      fn: testTabOpenPositionSyncsPrefWhenConstructed,
    },
    {
      name: "TabScroll syncs switchByScrolling pref when constructed",
      fn: testTabScrollSyncsSwitchByScrollingPrefWhenConstructed,
    },
  ];

  await runTests("tabPreferenceSync.test.ts", tests);
}
