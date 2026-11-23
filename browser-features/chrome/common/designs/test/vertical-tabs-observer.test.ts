// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";
import { config, setConfig } from "../configs.ts";
import type { TabbarStyle } from "../utils/tabbar-style-sync.ts";
import { addVerticalTabsObserver } from "../utils/vertical-tabs-observer.ts";

const PREF = "sidebar.verticalTabs";

function withVerticalTabsPref<T>(value: boolean, fn: () => T): T {
  const hadUserValue = Services.prefs.prefHasUserValue(PREF);
  const previous = hadUserValue
    ? Services.prefs.getBoolPref(PREF)
    : undefined;
  const previousConfig = config();
  Services.prefs.setBoolPref(PREF, value);
  try {
    return fn();
  } finally {
    // Restore the pref first: the production observer in configs.ts reacts to
    // this pref and rewrites the design config, so restoring the config has to
    // be the last step.
    if (hadUserValue && previous !== undefined) {
      Services.prefs.setBoolPref(PREF, previous);
    } else {
      Services.prefs.clearUserPref(PREF);
    }
    setConfig(previousConfig);
  }
}

type ObserverState = {
  applied: TabbarStyle[];
  getStyle: () => TabbarStyle;
  remove: () => void;
};

function createObserver(initialStyle: TabbarStyle): ObserverState {
  let style = initialStyle;
  const applied: TabbarStyle[] = [];
  const remove = addVerticalTabsObserver(
    () => style,
    (next) => {
      style = next;
      applied.push(next);
    },
  );
  return {
    applied,
    getStyle: () => style,
    remove,
  };
}

function testTurningVerticalOnAppliesVertical(): void {
  withVerticalTabsPref(false, () => {
    const state = createObserver("horizontal");
    try {
      Services.prefs.setBoolPref(PREF, true);
      assertEquals(state.applied.length, 1, "applyStyle should run once");
      assertEquals(state.applied[0], "vertical", "style becomes vertical");
    } finally {
      state.remove();
    }
  });
}

function testTurningVerticalOffAppliesHorizontal(): void {
  withVerticalTabsPref(false, () => {
    const state = createObserver("horizontal");
    try {
      Services.prefs.setBoolPref(PREF, true);
      Services.prefs.setBoolPref(PREF, false);
      assertEquals(
        state.applied.length,
        2,
        "both transitions should apply",
      );
      assertEquals(
        state.applied[1],
        "horizontal",
        "vertical is demoted to horizontal",
      );
    } finally {
      state.remove();
    }
  });
}

function testTurningVerticalOffKeepsMultirow(): void {
  withVerticalTabsPref(true, () => {
    const state = createObserver("multirow");
    try {
      Services.prefs.setBoolPref(PREF, false);
      assertEquals(
        state.applied.length,
        0,
        "multirow must not be rewritten",
      );
      assertEquals(state.getStyle(), "multirow", "style remains multirow");
    } finally {
      state.remove();
    }
  });
}

function testMatchingStyleIsNoOp(): void {
  withVerticalTabsPref(false, () => {
    const state = createObserver("vertical");
    try {
      Services.prefs.setBoolPref(PREF, true);
      assertEquals(
        state.applied.length,
        0,
        "matching style should not be applied again",
      );
    } finally {
      state.remove();
    }
  });
}

function testCleanupRemovesObserver(): void {
  withVerticalTabsPref(false, () => {
    const state = createObserver("horizontal");
    state.remove();
    Services.prefs.setBoolPref(PREF, true);
    assertEquals(
      state.applied.length,
      0,
      "observer must not run after cleanup",
    );
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("vertical-tabs-observer.test.ts", [
    {
      name: "turning native vertical tabs on applies vertical",
      fn: testTurningVerticalOnAppliesVertical,
    },
    {
      name: "turning native vertical tabs off applies horizontal",
      fn: testTurningVerticalOffAppliesHorizontal,
    },
    {
      name: "turning native vertical tabs off keeps multirow",
      fn: testTurningVerticalOffKeepsMultirow,
    },
    {
      name: "matching native style is a no-op",
      fn: testMatchingStyleIsNoOp,
    },
    {
      name: "cleanup removes the pref observer",
      fn: testCleanupRemovesObserver,
    },
  ]);
}
