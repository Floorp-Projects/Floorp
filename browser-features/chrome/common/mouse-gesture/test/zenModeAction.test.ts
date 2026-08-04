// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { executeGestureAction } from "../utils/gestures.ts";
import {
  attachZenModeToWindow,
  destroyZenModeForWindow,
  ZEN_MODE_PREF,
} from "#features-chrome/common/zen-mode/zen-mode.tsx";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

function createTargetWindow(): Window {
  const doc = document.implementation.createHTMLDocument(
    "Zen gesture target",
  );
  const eventTarget = new EventTarget();
  const toolbox = doc.createElement("div");
  toolbox.id = "navigator-toolbox";
  doc.body!.appendChild(toolbox);

  return {
    document: doc,
    closed: false,
    innerWidth: 1000,
    innerHeight: 800,
    MutationObserver,
    ResizeObserver,
    requestAnimationFrame: globalThis.requestAnimationFrame.bind(globalThis),
    cancelAnimationFrame: globalThis.cancelAnimationFrame.bind(globalThis),
    setTimeout: globalThis.setTimeout.bind(globalThis),
    clearTimeout: globalThis.clearTimeout.bind(globalThis),
    addEventListener: eventTarget.addEventListener.bind(eventTarget),
    removeEventListener: eventTarget.removeEventListener.bind(eventTarget),
    gNavToolbox: toolbox,
  } as unknown as Window;
}

function testGestureChangesOnlySuppliedWindow(): void {
  const hadUserValue = Services.prefs.prefHasUserValue(ZEN_MODE_PREF);
  const originalSeed = Services.prefs.getBoolPref(ZEN_MODE_PREF, false);
  const firstWindow = createTargetWindow();
  const secondWindow = createTargetWindow();

  Services.prefs.setBoolPref(ZEN_MODE_PREF, false);
  const first = attachZenModeToWindow(firstWindow);
  const second = attachZenModeToWindow(secondWindow);

  try {
    assert(first !== null && second !== null, "both controllers should exist");
    const executed = executeGestureAction(
      "floorp-toggle-zen-mode",
      secondWindow,
    );
    assertEquals(executed, true, "the Zen gesture should execute");
    assertEquals(
      first.enabled(),
      false,
      "the non-target window must remain unchanged",
    );
    assertEquals(
      second.enabled(),
      true,
      "the supplied gesture window should toggle",
    );
    assertEquals(
      Services.prefs.getBoolPref(ZEN_MODE_PREF, false),
      true,
      "the explicit gesture should persist the seed for future windows",
    );
  } finally {
    destroyZenModeForWindow(firstWindow);
    destroyZenModeForWindow(secondWindow);
    if (hadUserValue) {
      Services.prefs.setBoolPref(ZEN_MODE_PREF, originalSeed);
    } else {
      Services.prefs.clearUserPref(ZEN_MODE_PREF);
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("zenModeAction.test.ts", [
    {
      name: "gesture changes only its supplied window controller",
      fn: testGestureChangesOnlySuppliedWindow,
    },
  ]);
}
