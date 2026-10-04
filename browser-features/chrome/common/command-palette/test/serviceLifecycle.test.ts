// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createRoot, rootEffect } from "@nora/preact-xul/lifetime";
import { CommandPaletteService } from "../service.ts";
import {
  COMMAND_PALETTE_ENABLED_PREF,
  isEnabled,
  setEnabled,
} from "../config.ts";
import { gestureActions } from "../../mouse-gesture/utils/gestures.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const ACTION = "floorp-toggle-command-palette";

function testWindow(): Window {
  return Object.assign(new EventTarget(), {
    closed: false,
  }) as unknown as Window;
}

function withRestore(run: () => void): void {
  const enabled = isEnabled();
  const hadUserValue = Services.prefs.prefHasUserValue(
    COMMAND_PALETTE_ENABLED_PREF,
  );
  const previousAction = gestureActions.getAllActions().get(ACTION);
  let dispose = () => {};
  try {
    setEnabled(true);
    createRoot((cleanup) => {
      dispose = cleanup;
      run();
    });
  } finally {
    dispose();
    setEnabled(enabled);
    if (!hadUserValue) {
      Services.prefs.clearUserPref(COMMAND_PALETTE_ENABLED_PREF);
    }
    const actions = gestureActions.getAllActions();
    if (previousAction) actions.set(ACTION, previousAction);
    else actions.delete(ACTION);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("serviceLifecycle.test.ts", [{
    name: "window-local UI controller ownership survives disable and enable",
    fn() {
      withRestore(() => {
        const firstWindow = testWindow();
        const secondWindow = testWindow();
        const first = new CommandPaletteService(firstWindow);
        first.attachToWindow(secondWindow);
        assertEquals(
          first.getController(secondWindow),
          undefined,
          "first service must not take the second window",
        );
        const firstAction = gestureActions.getAction(ACTION);
        const second = new CommandPaletteService(secondWindow);
        const secondAction = gestureActions.getAction(ACTION);
        const seen: boolean[] = [];
        rootEffect(() => {
          seen.push(second.getController(secondWindow) !== undefined);
        });
        const initial = second.getController(secondWindow);
        assert(initial, "second window initially has a UI controller");
        setEnabled(false);
        assertEquals(
          first.getController(firstWindow),
          undefined,
          "first disabled",
        );
        assertEquals(
          second.getController(secondWindow),
          undefined,
          "second disabled",
        );
        setEnabled(true);
        const firstController = first.getController(firstWindow);
        const secondController = second.getController(secondWindow);
        assert(
          firstController && secondController,
          "each UI receives its own replacement controller",
        );
        assert(secondController !== initial, "old controller is replaced");
        assertEquals(
          seen.join(","),
          "true,false,true",
          "UI subscribers see removal and replacement",
        );
        let firstToggles = 0;
        let secondToggles = 0;
        firstController.togglePalette = () => {
          firstToggles++;
        };
        secondController.togglePalette = () => {
          secondToggles++;
        };
        firstAction?.(firstWindow);
        firstAction?.(secondWindow);
        secondAction?.(secondWindow);
        assertEquals(
          firstToggles,
          1,
          "first realm action routes only to its window",
        );
        assertEquals(
          secondToggles,
          1,
          "second realm action routes to its own UI",
        );
        secondWindow.dispatchEvent(new Event("unload"));
        second.attachToWindow(secondWindow);
        secondAction?.(secondWindow);
        assertEquals(
          second.getController(secondWindow),
          undefined,
          "unloaded service stays disposed",
        );
        assertEquals(
          secondToggles,
          1,
          "retained action cannot toggle a disposed controller",
        );
        assertEquals(
          first.getController(firstWindow),
          firstController,
          "peer unload preserves first controller",
        );
      });
    },
  }, {
    name:
      "action disposal restores its predecessor without replacing a newer owner",
    fn() {
      withRestore(() => {
        const before = gestureActions.getAllActions().get(ACTION);
        const service = new CommandPaletteService(testWindow());
        assert(
          gestureActions.getAllActions().get(ACTION) !== before,
          "service owns its action",
        );
        service.destroy();
        assertEquals(
          gestureActions.getAllActions().get(ACTION),
          before,
          "previous action restored",
        );

        const obsolete = new CommandPaletteService(testWindow());
        const newer = { name: ACTION, fn: (_win: Window) => {} };
        gestureActions.registerAction(newer);
        obsolete.destroy();
        assertEquals(
          gestureActions.getAllActions().get(ACTION),
          newer,
          "new registration survives old owner disposal",
        );
      });
    },
  }]);
}
