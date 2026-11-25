// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import { config, setConfig } from "#features-chrome/common/designs/configs.ts";
import type { TabbarStyle } from "#features-chrome/common/designs/utils/tabbar-style-sync.ts";
import {
  getDesignSettings,
  saveDesignSettings,
} from "../../../src/app/design/dataManager.ts";

const NATIVE_PREF = "sidebar.verticalTabs";
const SPLIT_PREF = "floorp.splitView.dragToSplitCreate.enabled";

// The colocated browser runner uses rpc.ts's real direct Services route.
// Persisting the second style models another window's edit, not a GUI race.
async function withDesignState(fn: () => Promise<void>): Promise<void> {
  const previousConfig = config();
  const nativeHadUserValue = Services.prefs.prefHasUserValue(NATIVE_PREF);
  const nativeValue = Services.prefs.getBoolPref(NATIVE_PREF, false);
  const splitHadUserValue = Services.prefs.prefHasUserValue(SPLIT_PREF);
  const splitValue = Services.prefs.getBoolPref(SPLIT_PREF, false);
  try {
    await fn();
  } finally {
    // Restore native first because its production observer rewrites config.
    if (nativeHadUserValue) {
      Services.prefs.setBoolPref(NATIVE_PREF, nativeValue);
    } else Services.prefs.clearUserPref(NATIVE_PREF);
    if (splitHadUserValue) Services.prefs.setBoolPref(SPLIT_PREF, splitValue);
    else Services.prefs.clearUserPref(SPLIT_PREF);
    setConfig(previousConfig);
  }
}

function storeStyle(style: TabbarStyle): void {
  setConfig((previous) => ({
    ...previous,
    tabbar: { ...previous.tabbar, tabbarStyle: style },
  }));
}

async function testUnrelatedSaveKeepsLatestStyle(
  stale: "horizontal" | "multirow",
  latest: "horizontal" | "multirow",
): Promise<void> {
  await withDesignState(async () => {
    Services.prefs.setBoolPref(NATIVE_PREF, false);
    storeStyle(stale);
    const form = await getDesignSettings();
    assert(form !== null, "design settings are available");
    assertEquals(form.style, stale, "first window reads its initial style");

    storeStyle(latest);
    assertEquals(config().tabbar.tabbarStyle, latest, "second writer persists");
    form.faviconColor = !form.faviconColor;
    await saveDesignSettings(form, { hasTabStyleChanged: false });

    const saved = await getDesignSettings();
    assert(saved !== null, "saved settings are available");
    assertEquals(saved.style, latest, "unrelated save preserves latest style");
    assertEquals(
      config().tabbar.tabbarStyle,
      latest,
      "stored style is preserved",
    );
    assertEquals(
      saved.faviconColor,
      form.faviconColor,
      "unrelated edit is saved",
    );
    assertEquals(
      Services.prefs.getBoolPref(NATIVE_PREF),
      false,
      "native stays off",
    );
  });
}

async function testExplicitStyleEditOverridesLatest(): Promise<void> {
  await withDesignState(async () => {
    Services.prefs.setBoolPref(NATIVE_PREF, false);
    storeStyle("horizontal");
    const form = await getDesignSettings();
    assert(form !== null, "design settings are available");
    storeStyle("multirow");
    form.style = "vertical";
    await saveDesignSettings(form, { hasTabStyleChanged: true });
    assertEquals(config().tabbar.tabbarStyle, "vertical", "explicit edit wins");
    assertEquals(
      Services.prefs.getBoolPref(NATIVE_PREF),
      true,
      "native follows",
    );

    form.style = "multirow";
    await saveDesignSettings(form, { hasTabStyleChanged: true });
    assertEquals(
      config().tabbar.tabbarStyle,
      "multirow",
      "multirow is retained",
    );
    assertEquals(
      Services.prefs.getBoolPref(NATIVE_PREF),
      false,
      "multirow disables native vertical",
    );
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("verticalTabsSync.test.ts", [
    {
      name: "unrelated stale horizontal form preserves latest multirow",
      fn: () => testUnrelatedSaveKeepsLatestStyle("horizontal", "multirow"),
    },
    {
      name: "unrelated stale multirow form preserves latest horizontal",
      fn: () => testUnrelatedSaveKeepsLatestStyle("multirow", "horizontal"),
    },
    {
      name:
        "explicit style edits still override latest stored style and native pref",
      fn: testExplicitStyleEditOverridesLatest,
    },
  ]);
}
