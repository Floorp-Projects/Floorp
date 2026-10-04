// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createRoot } from "@nora/preact-xul/lifetime";
import { KeyboardShortcutService } from "../service.ts";
import {
  isEnabled,
  KEYBOARD_SHORTCUT_ENABLED_PREF,
  setEnabled,
} from "../config.ts";
import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";

function trackedWindow() {
  const target = new EventTarget();
  const listeners = new Map<string, Set<EventListenerOrEventListenerObject>>();
  const win = {
    closed: false,
    addEventListener(
      type: string,
      listener: EventListenerOrEventListenerObject,
      options?: boolean | AddEventListenerOptions,
    ) {
      const set = listeners.get(type) ?? new Set();
      set.add(listener);
      listeners.set(type, set);
      target.addEventListener(type, listener, options);
    },
    removeEventListener(
      type: string,
      listener: EventListenerOrEventListenerObject,
      options?: boolean | EventListenerOptions,
    ) {
      listeners.get(type)?.delete(listener);
      target.removeEventListener(type, listener, options);
    },
  } as unknown as Window & { __keyboardShortcutControllerAttached?: boolean };
  return { win, listeners, target };
}

export async function runAllTests(): Promise<void> {
  await runTests("keyboardShortcutServiceLifecycle.test.ts", [{
    name: "owner disposal removes controllers markers and unload listeners",
    fn() {
      const wasEnabled = isEnabled();
      const hadUserValue = Services.prefs.prefHasUserValue(
        KEYBOARD_SHORTCUT_ENABLED_PREF,
      );
      const { win, listeners } = trackedWindow();
      let dispose = () => {};
      try {
        setEnabled(true);
        const service = createRoot((cleanup) => {
          dispose = cleanup;
          return new KeyboardShortcutService(null, win);
        });
        service.attachToWindow(win);
        for (const type of ["keydown", "keyup", "blur"]) {
          assertEquals(listeners.get(type)?.size, 1, `${type} attached once`);
        }
        assertEquals(win.__keyboardShortcutControllerAttached, true, "marker");

        // A config/enable change also replaces controllers before HMR.
        service.setEnabled(false);
        for (const type of ["keydown", "keyup", "blur"]) {
          assertEquals(
            listeners.get(type)?.size,
            0,
            `${type} removed on disable`,
          );
        }
        service.setEnabled(true);
        service.attachToWindow(win);
        dispose();
        for (const type of ["keydown", "keyup", "blur", "unload"]) {
          assertEquals(
            listeners.get(type)?.size,
            0,
            `${type} removed on disposal`,
          );
        }
        assertEquals(
          win.__keyboardShortcutControllerAttached,
          undefined,
          "marker removed",
        );

        service.attachToWindow(win);
        service.setEnabled(false);
        assertEquals(
          isEnabled(),
          true,
          "stale service cannot mutate shared state",
        );
        assertEquals(listeners.get("keydown")?.size, 0, "no late reattachment");
      } finally {
        dispose();
        setEnabled(wasEnabled);
        if (!hadUserValue) {
          Services.prefs.clearUserPref(KEYBOARD_SHORTCUT_ENABLED_PREF);
        }
      }
    },
  }, {
    name: "each service retains only its owning window across enable cycles",
    fn() {
      const wasEnabled = isEnabled();
      const hadUserValue = Services.prefs.prefHasUserValue(
        KEYBOARD_SHORTCUT_ENABLED_PREF,
      );
      const first = trackedWindow();
      const second = trackedWindow();
      let disposeFirst = () => {};
      let disposeSecond = () => {};
      try {
        setEnabled(true);
        const firstService = createRoot((dispose) => {
          disposeFirst = dispose;
          return new KeyboardShortcutService(null, first.win);
        });
        firstService.attachToWindow(second.win);
        assertEquals(
          second.listeners.get("keydown")?.size ?? 0,
          0,
          "foreign window is not reserved",
        );
        const secondService = createRoot((dispose) => {
          disposeSecond = dispose;
          return new KeyboardShortcutService(null, second.win);
        });
        for (let cycle = 0; cycle < 2; cycle++) {
          setEnabled(false);
          setEnabled(true);
          assertEquals(
            first.listeners.get("keydown")?.size,
            1,
            "first window retains one controller",
          );
          assertEquals(
            second.listeners.get("keydown")?.size,
            1,
            "second window retains one controller",
          );
        }
        first.target.dispatchEvent(new Event("unload"));
        assertEquals(
          first.listeners.get("keydown")?.size,
          0,
          "unload disposes the first service",
        );
        assertEquals(
          first.listeners.get("unload")?.size,
          0,
          "unload ownership listener removed",
        );
        setEnabled(false);
        setEnabled(true);
        assertEquals(
          first.listeners.get("keydown")?.size,
          0,
          "unloaded service cannot reattach",
        );
        assertEquals(
          second.listeners.get("keydown")?.size,
          1,
          "peer service stays active",
        );
        secondService.attachToWindow(first.win);
        assertEquals(
          first.listeners.get("keydown")?.size,
          0,
          "peer never takes ownership of the first window",
        );
      } finally {
        disposeSecond();
        disposeFirst();
        setEnabled(wasEnabled);
        if (!hadUserValue) {
          Services.prefs.clearUserPref(KEYBOARD_SHORTCUT_ENABLED_PREF);
        }
      }
    },
  }]);
}
