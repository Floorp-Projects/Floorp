// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createRoot } from "@nora/preact-xul/lifetime";

import { TabDoubleClickClose } from "../doubleClickClose/index.ts";
import { TabOpenPosition } from "../openPosition/index.ts";
import { TabScroll } from "../scroll/index.ts";
import { config } from "../../designs/configs.ts";
import {
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

const ownedTestRoots: Array<() => void> = [];

function constructInPreactRoot(construct: () => void): () => void {
  return createRoot((dispose) => {
    ownedTestRoots.push(dispose);
    construct();
    return dispose;
  });
}

function withTabConfigPatch(
  patch: {
    tabDoubleClickToClose?: boolean;
    tabOpenPosition?: number;
    tabScrollEnabled?: boolean;
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
        tabDoubleClickToClose: patch.tabDoubleClickToClose ??
          prev.tab.tabDoubleClickToClose,
        tabOpenPosition: patch.tabOpenPosition ?? prev.tab.tabOpenPosition,
        tabScroll: {
          ...prev.tab.tabScroll,
          enabled: patch.tabScrollEnabled ?? prev.tab.tabScroll.enabled,
        },
      },
    };

    run();
  } finally {
    for (const dispose of ownedTestRoots.splice(0)) dispose();
    config.value = original;
  }
}

function testTabDoubleClickCloseSyncsPrefWhenConstructed(): void {
  const prefName = "browser.tabs.closeTabByDblclick";
  const originalPref = Services.prefs.getBoolPref(prefName, false);

  try {
    withTabConfigPatch({ tabDoubleClickToClose: true }, () => {
      constructInPreactRoot(() => {
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
      constructInPreactRoot(() => {
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
      constructInPreactRoot(() => {
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
