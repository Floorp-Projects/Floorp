// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createEffect, createRoot } from "solid-js";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  COMMAND_PALETTE_ENABLED_PREF,
  COMMAND_PALETTE_SELECTABLE_COMMANDS_PREF,
  isEnabled,
  setEnabled,
} from "../config.ts";
import { CommandPaletteController } from "../controller.ts";
import { mountCommandPalette } from "../mount.ts";
import {
  COMMAND_PALETTE_TOGGLE_EVENT,
  CommandPaletteService,
  commandPaletteService,
} from "../service.ts";
import type { CommandPaletteWindow } from "../types.ts";
import { gestureActions } from "../../mouse-gesture/utils/gestures.ts";

// A Window-typed receiver keeps browser timer IDs numeric even when a host
// type-check also includes Node's global setTimeout overloads.
const timerWindow: Window = window;

function createWindow(closed = false) {
  const target = new EventTarget();
  const listeners = new Map<string, Set<EventListenerOrEventListenerObject>>();
  const timers = new Set<number>();
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
      setTimeout(handler: TimerHandler, timeout?: number): number {
        const timer = timerWindow.setTimeout(() => {
          timers.delete(timer);
          if (typeof handler === "function") handler();
        }, timeout);
        timers.add(timer);
        return timer;
      },
      clearTimeout(timer?: number): void {
        if (timer !== undefined) timers.delete(timer);
        timerWindow.clearTimeout(timer);
      },
    }) as unknown as CommandPaletteWindow,
    listenerCount: (type: string) => listeners.get(type)?.size ?? 0,
    timerCount: () => timers.size,
  };
}

function createService(win: CommandPaletteWindow) {
  return createRoot((dispose) => ({
    service: new CommandPaletteService(win),
    dispose,
  }));
}

function testWindowOwnership(): void {
  const parent = createWindow();
  const child = createWindow();
  const parentRoot = createService(parent.win);
  const childRoot = createService(child.win);
  try {
    assert(
      parentRoot.service.getController(parent.win),
      "parent owns a controller",
    );
    assert(
      childRoot.service.getController(child.win),
      "child owns a controller",
    );
    assertEquals(
      parentRoot.service.getController(child.win),
      undefined,
      "a service cannot return another realm's controller",
    );
    child.win.dispatchEvent(new Event("unload"));
    parentRoot.service.attachToWindow(child.win);
    assertEquals(
      child.listenerCount("keydown"),
      0,
      "closed child has no input",
    );
    assertEquals(
      child.listenerCount("unload"),
      0,
      "child releases unload listener",
    );
    assertEquals(
      child.listenerCount(COMMAND_PALETTE_TOGGLE_EVENT),
      0,
      "child releases toggle listener",
    );
    assertEquals(parent.listenerCount("keydown"), 1, "parent remains active");
  } finally {
    childRoot.dispose();
    parentRoot.dispose();
  }
}

function testToggleActionTargetsAnotherWindow(): void {
  const parent = createWindow();
  const child = createWindow();
  const parentRoot = createService(parent.win);
  const parentAction = gestureActions.getAction(
    "floorp-toggle-command-palette",
  );
  const childRoot = createService(child.win);
  try {
    assert(parentAction, "the originating realm registers the shortcut action");
    for (let index = 0; index < 3; index++) {
      const parentController = parentRoot.service.getController(parent.win);
      const childController = childRoot.service.getController(child.win);
      assert(
        parentController && childController,
        "each window has a controller",
      );
      // Keep the originating action even after another service registers one.
      parentAction(child.win);
      assertEquals(
        childController.state.isVisible(),
        true,
        "target window toggles",
      );
      assertEquals(
        parentController.state.isVisible(),
        false,
        "origin window stays hidden",
      );

      setEnabled(false);
      parentAction(child.win);
      assertEquals(
        childController.state.isVisible(),
        false,
        "OFF cannot reopen the old controller",
      );
      assertEquals(
        childRoot.service.getController(child.win),
        undefined,
        "OFF has no target controller",
      );
      setEnabled(true);
      assertEquals(
        child.listenerCount(COMMAND_PALETTE_TOGGLE_EVENT),
        1,
        "OFF/ON retains one local toggle listener",
      );
    }

    child.win.dispatchEvent(new Event("unload"));
    parentAction(child.win);
    assertEquals(
      childRoot.service.getController(child.win),
      undefined,
      "closed target stays inactive",
    );
    assertEquals(
      child.listenerCount(COMMAND_PALETTE_TOGGLE_EVENT),
      0,
      "close releases routing listener",
    );
    parentAction(parent.win);
    assertEquals(
      parentRoot.service.getController(parent.win)?.state.isVisible(),
      true,
      "the same action can still toggle a surviving window",
    );
  } finally {
    childRoot.dispose();
    parentRoot.dispose();
  }
}

function testPreferenceAndRootCleanup(): void {
  const target = createWindow();
  const root = createService(target.win);
  let renderedController: CommandPaletteController | undefined;
  // The UI consumes this accessor reactively; it must see replacement state.
  const stopRendering = createRoot((dispose) => {
    createEffect(() => {
      renderedController = root.service.getController(target.win);
    });
    return dispose;
  });
  try {
    for (let index = 0; index < 3; index++) {
      const previous = root.service.getController(target.win);
      assert(previous, "enabled palette has a controller");
      // Exercise the settings page's native preference path, not private fields.
      Services.prefs.setBoolPref(COMMAND_PALETTE_ENABLED_PREF, false);
      assertEquals(
        target.listenerCount("keydown"),
        0,
        "OFF removes input listener",
      );
      assertEquals(
        renderedController,
        undefined,
        "OFF detaches the rendered state",
      );
      assertEquals(
        target.win.__commandPaletteControllerAttached,
        undefined,
        "OFF clears the diagnostic marker",
      );
      Services.prefs.setBoolPref(COMMAND_PALETTE_ENABLED_PREF, true);
      const current = root.service.getController(target.win);
      assert(current && current !== previous, "ON creates a fresh controller");
      assertEquals(
        renderedController,
        current,
        "UI follows the replacement controller",
      );
      root.service.attachToWindow(target.win);
      assertEquals(
        target.listenerCount("keydown"),
        1,
        "ON has one input listener",
      );
      assertEquals(
        target.listenerCount("unload"),
        1,
        "toggles retain one service owner",
      );
      // F2 and mouse gestures dispatch this same action. It must use the
      // replacement controller rather than a captured, destroyed instance.
      current.state.setIsVisible(true);
      gestureActions.getAction("floorp-toggle-command-palette")?.(target.win);
      assertEquals(
        current.state.isVisible(),
        false,
        "the registered toggle action reaches the replacement controller",
      );
    }
    root.dispose();
    assertEquals(
      target.listenerCount("keydown"),
      0,
      "HMR releases input listener",
    );
    assertEquals(
      target.listenerCount("unload"),
      0,
      "HMR releases unload listener",
    );
    assertEquals(
      target.listenerCount(COMMAND_PALETTE_TOGGLE_EVENT),
      0,
      "HMR releases toggle listener",
    );
    assertEquals(
      renderedController,
      undefined,
      "HMR releases the rendered controller",
    );
    Services.prefs.setBoolPref(COMMAND_PALETTE_ENABLED_PREF, false);
    Services.prefs.setBoolPref(COMMAND_PALETTE_ENABLED_PREF, true);
    root.service.attachToWindow(target.win);
    assertEquals(
      target.listenerCount("keydown"),
      0,
      "disposed service cannot reactivate",
    );

    const replacement = createService(target.win);
    try {
      assert(
        replacement.service.getController(target.win),
        "HMR replacement can attach",
      );
      assertEquals(
        target.listenerCount("unload"),
        1,
        "replacement owns one unload listener",
      );
    } finally {
      replacement.dispose();
    }
  } finally {
    stopRendering();
    root.dispose();
  }
}

function testPaletteMountPreservesNativeChrome(): void {
  const mainWindow = document.getElementById("main-window");
  const nativeHead = document.head;
  const nativeBody = document.body;
  const tabbrowser = document.getElementById("tabbrowser-tabbox");
  assert(
    mainWindow && nativeHead && nativeBody && tabbrowser,
    "native browser chrome is present",
  );
  const previousEnabled = isEnabled();
  setEnabled(true);

  let paletteMount: HTMLElement | undefined;
  const dispose = createRoot((cleanup) => {
    // Exercise the exact production mounting boundary in the real browser DOM.
    paletteMount = mountCommandPalette(mainWindow);
    return cleanup;
  });
  const assertNativeChrome = () => {
    assertEquals(document.head, nativeHead, "the native head is unchanged");
    assertEquals(document.body, nativeBody, "the native body is unchanged");
    assertEquals(
      document.getElementById("tabbrowser-tabbox"),
      tabbrowser,
      "the native tabbrowser is unchanged",
    );
    assert(
      nativeHead.isConnected && nativeBody.isConnected &&
        tabbrowser.isConnected,
      "native browser elements remain connected",
    );
    assert(
      paletteMount?.isConnected,
      "the palette keeps its own mounting boundary",
    );
  };
  try {
    for (let cycle = 0; cycle < 3; cycle++) {
      const controller = commandPaletteService.getController(window);
      assert(controller, "the real window owns its enabled palette controller");
      controller.state.setIsVisible(true);
      assert(
        document.getElementById("command-palette-overlay")?.isConnected,
        "the real palette UI renders before disabling",
      );
      Services.prefs.setBoolPref(COMMAND_PALETTE_ENABLED_PREF, false);
      assertNativeChrome();
      Services.prefs.setBoolPref(COMMAND_PALETTE_ENABLED_PREF, true);
      assertNativeChrome();
      assert(
        commandPaletteService.getController(window) !== controller,
        "the real UI receives a replacement controller",
      );
    }
  } finally {
    dispose();
    setEnabled(previousEnabled);
  }
  assertEquals(
    paletteMount?.isConnected,
    false,
    "owner cleanup removes only the palette mounting boundary",
  );
  assert(
    nativeHead.isConnected && nativeBody.isConnected && tabbrowser.isConnected,
    "palette cleanup preserves native browser chrome",
  );
}

function testUnloadAndLateInitialization(): void {
  for (const initiallyClosed of [false, true]) {
    const target = createWindow(initiallyClosed);
    const root = createService(target.win);
    try {
      if (!initiallyClosed) target.win.dispatchEvent(new Event("unload"));
      setEnabled(false);
      setEnabled(true);
      root.service.attachToWindow(target.win);
      assertEquals(
        target.listenerCount("keydown"),
        0,
        "unloaded service stays inactive",
      );
      assertEquals(
        target.listenerCount("unload"),
        0,
        "unloaded service has no listener",
      );
      assertEquals(
        root.service.getController(target.win),
        undefined,
        "no closed controller",
      );
      assertEquals(
        target.win.__commandPaletteControllerAttached,
        undefined,
        "no stale marker",
      );
    } finally {
      root.dispose();
    }
  }
}

async function testControllerCancelsPendingWork(): Promise<void> {
  const target = createWindow();
  const controller = new CommandPaletteController(target.win);
  try {
    controller.state.setIsVisible(true);
    controller.destroy();
    assertEquals(
      target.listenerCount("keydown"),
      0,
      "destroy removes keyboard capture",
    );
    assertEquals(
      target.timerCount(),
      0,
      "destroy does not leave a closing animation timer",
    );
    assertEquals(
      controller.state.isVisible(),
      false,
      "destroy hides the palette",
    );
    assertEquals(
      controller.state.isAnimatingOut(),
      false,
      "destroy finishes animation state",
    );
  } finally {
    controller.destroy();
  }

  const hiddenController = new CommandPaletteController(target.win);
  try {
    // Hidden controllers can still have a pending debounced input callback.
    hiddenController.updateSearch("@b lifecycle-cleanup");
    hiddenController.destroy();
    await new Promise<void>((resolve) => timerWindow.setTimeout(resolve, 60));
    assertEquals(
      hiddenController.state.highlightQuery(),
      "",
      "destroy cancels pending search even when already hidden",
    );
  } finally {
    hiddenController.destroy();
  }
}

export async function runAllTests(): Promise<void> {
  const previousEnabled = isEnabled();
  const enabledPref =
    Services.prefs.prefHasUserValue(COMMAND_PALETTE_ENABLED_PREF)
      ? Services.prefs.getBoolPref(COMMAND_PALETTE_ENABLED_PREF)
      : null;
  const selectablePref =
    Services.prefs.prefHasUserValue(COMMAND_PALETTE_SELECTABLE_COMMANDS_PREF)
      ? Services.prefs.getStringPref(COMMAND_PALETTE_SELECTABLE_COMMANDS_PREF)
      : null;
  const actionName = "floorp-toggle-command-palette";
  const previousAction = gestureActions.getAction(actionName);
  try {
    setEnabled(true);
    await runTests("commandPaletteService.test.ts", [
      {
        name: "services only own their originating browser window",
        fn: testWindowOwnership,
      },
      {
        name: "an action from another realm toggles only its target window",
        fn: testToggleActionTargetsAnotherWindow,
      },
      {
        name: "production palette OFF/ON preserves native browser chrome",
        fn: testPaletteMountPreservesNativeChrome,
      },
      {
        name: "OFF/ON and HMR replace UI state without retaining listeners",
        fn: testPreferenceAndRootCleanup,
      },
      {
        name: "unload and delayed initialization cannot revive controllers",
        fn: testUnloadAndLateInitialization,
      },
      {
        name: "controller teardown cancels animation and hidden search work",
        fn: testControllerCancelsPendingWork,
      },
    ]);
  } finally {
    if (previousAction) {
      gestureActions.registerAction({ name: actionName, fn: previousAction });
    }
    setEnabled(previousEnabled);
    if (enabledPref === null) {
      Services.prefs.clearUserPref(COMMAND_PALETTE_ENABLED_PREF);
    } else {Services.prefs.setBoolPref(
        COMMAND_PALETTE_ENABLED_PREF,
        enabledPref,
      );}
    if (selectablePref === null) {
      Services.prefs.clearUserPref(COMMAND_PALETTE_SELECTABLE_COMMANDS_PREF);
    } else {Services.prefs.setStringPref(
        COMMAND_PALETTE_SELECTABLE_COMMANDS_PREF,
        selectablePref,
      );}
  }
}
