// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createRoot } from "solid-js";
import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";
import {
  getConfig,
  isEnabled,
  MOUSE_GESTURE_CONFIG_PREF,
  MOUSE_GESTURE_ENABLED_PREF,
  setConfig,
  setEnabled,
} from "../config.ts";
import { MouseGestureService } from "../service.ts";
import type { MouseGestureWindow } from "../types.ts";

function createWindow(closed = false) {
  const target = new EventTarget();
  const listeners = new Map<
    string,
    Set<EventListenerOrEventListenerObject>
  >();
  const add = target.addEventListener.bind(target);
  const remove = target.removeEventListener.bind(target);
  target.addEventListener = (type, listener, options) => {
    if (listener) {
      const current = listeners.get(type) ?? new Set();
      current.add(listener);
      listeners.set(type, current);
    }
    add(type, listener, options);
  };
  target.removeEventListener = (type, listener, options) => {
    if (listener) listeners.get(type)?.delete(listener);
    remove(type, listener, options);
  };
  return {
    win: Object.assign(target, {
      closed,
      setTimeout: window.setTimeout.bind(window),
      clearTimeout: window.clearTimeout.bind(window),
    }) as unknown as MouseGestureWindow,
    listenerCount: (type: string) => listeners.get(type)?.size ?? 0,
  };
}

function createService(win: MouseGestureWindow) {
  return createRoot((dispose) => ({
    service: new MouseGestureService(win),
    dispose,
  }));
}

function testLocalWindowOwnership(): void {
  const parent = createWindow();
  const child = createWindow();
  const parentRoot = createService(parent.win);
  const childRoot = createService(child.win);
  try {
    parentRoot.service.attachToWindow(parent.win);
    childRoot.service.attachToWindow(child.win);
    child.win.dispatchEvent(new Event("unload"));
    parentRoot.service.attachToWindow(child.win);
    assertEquals(
      child.listenerCount("mousedown"),
      0,
      "a surviving service must not install listeners in another window",
    );
    assertEquals(
      parent.listenerCount("mousedown"),
      1,
      "parent still owns input",
    );
    assertEquals(
      child.listenerCount("unload"),
      0,
      "child cleanup removes unload",
    );
  } finally {
    childRoot.dispose();
    parentRoot.dispose();
  }
}

function testConfigAndRootCleanup(): void {
  const target = createWindow();
  const root = createService(target.win);
  try {
    root.service.attachToWindow(target.win);
    for (let index = 0; index < 3; index++) {
      setConfig({ ...getConfig(), sensitivity: 40 + index });
      setEnabled(false);
      assertEquals(
        target.listenerCount("mousedown"),
        0,
        "disabled has no input",
      );
      setEnabled(true);
      assertEquals(
        target.listenerCount("mousedown"),
        1,
        "one active controller",
      );
      assertEquals(
        target.listenerCount("unload"),
        1,
        "no old unload listeners",
      );
    }
    root.dispose();
    assertEquals(
      target.listenerCount("mousedown"),
      0,
      "HMR removes input listeners",
    );
    assertEquals(
      target.listenerCount("unload"),
      0,
      "HMR removes unload listener",
    );
    assertEquals(
      target.win.__mouseGestureControllerAttached,
      undefined,
      "HMR clears marker",
    );
    setEnabled(false);
    setEnabled(true);
    root.service.attachToWindow(target.win);
    assertEquals(
      target.listenerCount("mousedown"),
      0,
      "disposed service stays inactive",
    );
  } finally {
    root.dispose();
  }
}

function testLateInitializationAndUnload(): void {
  for (const initiallyClosed of [false, true]) {
    const target = createWindow(initiallyClosed);
    const root = createService(target.win);
    try {
      if (!initiallyClosed) target.win.dispatchEvent(new Event("unload"));
      setEnabled(false);
      setEnabled(true);
      root.service.attachToWindow(target.win);
      assertEquals(
        target.listenerCount("mousedown"),
        0,
        "closed service cannot attach",
      );
      assertEquals(
        target.listenerCount("unload"),
        0,
        "closed service has no unload listener",
      );
      assertEquals(
        target.win.__mouseGestureControllerAttached,
        undefined,
        "closed service has no marker",
      );
    } finally {
      root.dispose();
    }
  }
}

export async function runAllTests(): Promise<void> {
  const previousEnabled = isEnabled();
  const previousConfig = getConfig();
  const previousEnabledPref =
    Services.prefs.prefHasUserValue(MOUSE_GESTURE_ENABLED_PREF)
      ? Services.prefs.getBoolPref(MOUSE_GESTURE_ENABLED_PREF)
      : null;
  const previousConfigPref =
    Services.prefs.prefHasUserValue(MOUSE_GESTURE_CONFIG_PREF)
      ? Services.prefs.getStringPref(MOUSE_GESTURE_CONFIG_PREF)
      : null;
  const contextMenuPref = "ui.context_menus.after_mouseup";
  const previousContextMenuPref =
    Services.prefs.prefHasUserValue(contextMenuPref)
      ? Services.prefs.getBoolPref(contextMenuPref)
      : null;
  try {
    setEnabled(true);
    await runTests("mouseGestureService.test.ts", [
      {
        name: "services only own their originating window",
        fn: testLocalWindowOwnership,
      },
      {
        name: "config refresh and HMR dispose release controller resources",
        fn: testConfigAndRootCleanup,
      },
      {
        name: "unloaded and late-created services stay inactive",
        fn: testLateInitializationAndUnload,
      },
    ]);
  } finally {
    setConfig(previousConfig);
    setEnabled(previousEnabled);
    if (previousConfigPref === null) {
      Services.prefs.clearUserPref(MOUSE_GESTURE_CONFIG_PREF);
    } else {
      Services.prefs.setStringPref(
        MOUSE_GESTURE_CONFIG_PREF,
        previousConfigPref,
      );
    }
    if (previousEnabledPref === null) {
      Services.prefs.clearUserPref(MOUSE_GESTURE_ENABLED_PREF);
    } else {
      Services.prefs.setBoolPref(
        MOUSE_GESTURE_ENABLED_PREF,
        previousEnabledPref,
      );
    }
    if (previousContextMenuPref === null) {
      Services.prefs.clearUserPref(contextMenuPref);
    } else {
      Services.prefs.setBoolPref(contextMenuPref, previousContextMenuPref);
    }
  }
}
