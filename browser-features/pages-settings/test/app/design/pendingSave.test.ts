// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";
import { rpc } from "../../../src/lib/rpc/rpc.ts";
import {
  getDesignSettings,
  saveDesignSettings,
} from "../../../src/app/design/dataManager.ts";
import type { TabbarStyle } from "#features-chrome/common/designs/utils/tabbar-style-sync.ts";

const CONFIG = "floorp.design.configs";
const NATIVE = "sidebar.verticalTabs";
const SPLIT = "floorp.splitView.dragToSplitCreate.enabled";

async function withState(fn: () => Promise<void>): Promise<void> {
  const config = Services.prefs.getStringPref(CONFIG);
  const bools = [NATIVE, SPLIT].map((name) => ({
    name,
    user: Services.prefs.prefHasUserValue(name),
    value: Services.prefs.getBoolPref(name, false),
  }));
  const getBool = rpc.getBoolPref;
  const compare = rpc.compareAndSetStringPref;
  try {
    await fn();
  } finally {
    rpc.getBoolPref = getBool;
    rpc.compareAndSetStringPref = compare;
    for (const pref of bools) {
      if (pref.user) Services.prefs.setBoolPref(pref.name, pref.value);
      else Services.prefs.clearUserPref(pref.name);
    }
    Services.prefs.setStringPref(CONFIG, config);
  }
}

function writeStyle(style: TabbarStyle): void {
  Services.prefs.setBoolPref(NATIVE, style === "vertical");
  const data = JSON.parse(Services.prefs.getStringPref(CONFIG));
  data.tabbar.tabbarStyle = style;
  Services.prefs.setStringPref(CONFIG, JSON.stringify(data));
}

// Inject a competing real preference write after the pending save has read
// both config and native state. The actual RPC write/CAS remains unmodified.
// This controls scheduling at the API boundary, not physical two-window input.
async function pendingSave(
  initial: TabbarStyle,
  competing: TabbarStyle,
  explicit = false,
): Promise<void> {
  await withState(async () => {
    writeStyle(initial);
    const form = await getDesignSettings();
    assert(form !== null, "design form exists");
    form.faviconColor = !form.faviconColor;
    const original = rpc.getBoolPref;
    let interrupted = false;
    rpc.getBoolPref = async (name) => {
      const value = await original(name);
      if (name === NATIVE && !interrupted) {
        interrupted = true;
        writeStyle(competing);
      }
      return value;
    };
    await saveDesignSettings(form, { hasTabStyleChanged: explicit });
    rpc.getBoolPref = original;
    assert(interrupted, "competing writer ran during the pending save");
    const expected = explicit ? initial : competing;
    const saved = JSON.parse(Services.prefs.getStringPref(CONFIG));
    assertEquals(
      saved.tabbar.tabbarStyle,
      expected,
      "pending save preserves the correct style",
    );
    assertEquals(
      Services.prefs.getBoolPref(NATIVE),
      expected === "vertical",
      "native agrees with stored style",
    );
    assertEquals(
      saved.globalConfigs.faviconColor,
      form.faviconColor,
      "unrelated edit persists",
    );
  });
}

async function contentionFailure(): Promise<void> {
  await withState(async () => {
    writeStyle("horizontal");
    const form = await getDesignSettings();
    assert(form !== null, "design form exists");
    const before = Services.prefs.getStringPref(CONFIG);
    const split = Services.prefs.getBoolPref(SPLIT, false);
    const compare = rpc.compareAndSetStringPref;
    let attempts = 0;
    rpc.compareAndSetStringPref = () => {
      attempts++;
      assert(attempts <= 20, "contention must not retry forever");
      return Promise.resolve({ updated: false, currentValue: before });
    };
    let error: unknown;
    try {
      await saveDesignSettings({
        ...form,
        style: "vertical",
        tabDragToSplitCreate: !split,
      }, { hasTabStyleChanged: true });
    } catch (caught) {
      error = caught;
    }
    assert(
      error instanceof Error && error.message.includes("changed repeatedly"),
      "contention is reported",
    );
    assert(attempts > 1 && attempts <= 20, "retry count is bounded");
    assertEquals(
      Services.prefs.getStringPref(CONFIG),
      before,
      "failed save does not replace config",
    );
    assertEquals(
      Services.prefs.getBoolPref(NATIVE),
      false,
      "failed save does not change native style",
    );
    assertEquals(
      Services.prefs.getBoolPref(SPLIT, false),
      split,
      "failed save does not change secondary pref",
    );
    rpc.compareAndSetStringPref = compare;
    await saveDesignSettings({ ...form, faviconColor: !form.faviconColor });
    assertEquals(
      JSON.parse(Services.prefs.getStringPref(CONFIG)).globalConfigs
        .faviconColor,
      !form.faviconColor,
      "queue recovers after contention",
    );
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("pendingSave.test.ts", [
    {
      name: "pending horizontal save preserves competing vertical",
      fn: () => pendingSave("horizontal", "vertical"),
    },
    {
      name: "pending vertical save preserves competing horizontal",
      fn: () => pendingSave("vertical", "horizontal"),
    },
    {
      name: "pending horizontal save preserves competing multirow",
      fn: () => pendingSave("horizontal", "multirow"),
    },
    {
      name: "explicit pending vertical edit still wins",
      fn: () => pendingSave("vertical", "multirow", true),
    },
    {
      name: "persistent contention rejects and queue recovers",
      fn: contentionFailure,
    },
  ]);
}
