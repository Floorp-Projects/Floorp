// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { MouseGestureController } from "../controller.ts";
import {
  defaultConfig,
  getConfig,
  isEnabled,
  MOUSE_GESTURE_CONFIG_PREF,
  MOUSE_GESTURE_ENABLED_PREF,
  type MouseGestureConfig,
  setConfig,
  setEnabled,
} from "../config.ts";
import { type GestureActionFn, gestureActions } from "../utils/gestures.ts";

const PREVIOUS_TAB_ACTION = "gecko-show-previous-tab";
const NEXT_TAB_ACTION = "gecko-show-next-tab";
const ROCKER_RIGHT_LEFT_ACTION = "gecko-back";
const DRAWN_RIGHT_ACTION = "gecko-forward";
const TRACKED_ACTIONS = [
  PREVIOUS_TAB_ACTION,
  NEXT_TAB_ACTION,
  ROCKER_RIGHT_LEFT_ACTION,
  DRAWN_RIGHT_ACTION,
] as const;

type TrackedAction = (typeof TRACKED_ACTIONS)[number];
type ActionCounts = Record<TrackedAction, number>;

interface FakeWindowHarness {
  win: Window;
  pendingTimerCount(): number;
  runAllTimers(): void;
}

interface TestConfigOptions {
  enabled?: boolean;
  wheelGesturesEnabled?: boolean;
  preventionTimeout?: number;
  actions?: MouseGestureConfig["actions"];
}

function attachTimerShim<T extends EventTarget>(
  target: T,
): FakeWindowHarness {
  const timers = new Map<number, () => void>();
  let nextTimerId = 1;
  const fakeWindow = target as T & {
    setTimeout(callback: () => void, delay?: number): number;
    clearTimeout(timerId?: number): void;
  };

  fakeWindow.setTimeout = (callback: () => void, _delay?: number): number => {
    const timerId = nextTimerId++;
    timers.set(timerId, callback);
    return timerId;
  };
  fakeWindow.clearTimeout = (timerId?: number): void => {
    if (timerId !== undefined) {
      timers.delete(timerId);
    }
  };

  return {
    win: fakeWindow as unknown as Window,
    pendingTimerCount: () => timers.size,
    runAllTimers: () => {
      const pendingTimers = [...timers.entries()];
      timers.clear();
      for (const [, callback] of pendingTimers) {
        callback();
      }
    },
  };
}

function createFakeWindow(): FakeWindowHarness {
  return attachTimerShim(new EventTarget());
}

// A bare EventTarget has no ancestor/descendant relationship, so it cannot
// exercise capture-vs-bubble ordering. This harness uses a real (detached)
// DOM parent/child pair instead, so a "content" node can stopPropagation()
// its own mousedown the way a page's own JS might, letting tests verify the
// window-level listener still runs because it's registered on capture.
function createFakeWindowWithContent(): FakeWindowHarness & {
  contentEl: HTMLElement;
} {
  const container = document.createElement("div");
  const contentEl = document.createElement("div");
  container.appendChild(contentEl);
  return { ...attachTimerShim(container), contentEl };
}

function dispatchMouseFrom(
  target: EventTarget,
  type: "mousedown" | "mouseup" | "mousemove" | "contextmenu",
  button: number,
  clientX = 0,
  clientY = 0,
): MouseEvent {
  // Firefox's chrome test context treats these pointer-derived messages as
  // trusted and asserts if they are created with MouseEvent (Bug 1675848).
  const event = new PointerEvent(type, {
    button,
    clientX,
    clientY,
    bubbles: true,
    cancelable: true,
  });
  target.dispatchEvent(event);
  return event;
}

function dispatchMouse(
  win: Window,
  type: "mousedown" | "mouseup" | "mousemove" | "contextmenu",
  button: number,
  clientX = 0,
  clientY = 0,
): MouseEvent {
  return dispatchMouseFrom(win, type, button, clientX, clientY);
}

// Mirrors buildLineTrail in mouseGestureRecognizer.test.ts: enough steps and
// distance per step to clear both the 1.5px move-noise filter and the
// default 5px activation distance.
function dispatchDrag(
  win: Window,
  endX: number,
  endY: number,
  steps = 16,
): void {
  for (let i = 1; i <= steps; i++) {
    const t = i / steps;
    dispatchMouse(win, "mousemove", 0, endX * t, endY * t);
  }
}

function dispatchWheel(win: Window, deltaY: number): WheelEvent {
  const event = new WheelEvent("wheel", {
    deltaY,
    bubbles: true,
    cancelable: true,
  });
  win.dispatchEvent(event);
  return event;
}

function createTestConfig(options: TestConfigOptions): MouseGestureConfig {
  return {
    ...defaultConfig,
    enabled: options.enabled ?? true,
    wheelGesturesEnabled: options.wheelGesturesEnabled ?? true,
    contextMenu: {
      ...defaultConfig.contextMenu,
      preventionTimeout: options.preventionTimeout ?? 200,
    },
    actions: (options.actions ?? defaultConfig.actions).map((action) => ({
      pattern: [...action.pattern],
      action: action.action,
    })),
    rockerActions: { ...defaultConfig.rockerActions },
  };
}

async function withTestConfig(
  options: TestConfigOptions,
  fn: () => void | Promise<void>,
): Promise<void> {
  const previousEnabled = isEnabled();
  const previousConfig = getConfig();
  const hadEnabledPref = Services.prefs.prefHasUserValue(
    MOUSE_GESTURE_ENABLED_PREF,
  );
  const hadConfigPref = Services.prefs.prefHasUserValue(
    MOUSE_GESTURE_CONFIG_PREF,
  );
  const previousEnabledPref = hadEnabledPref
    ? Services.prefs.getBoolPref(MOUSE_GESTURE_ENABLED_PREF)
    : null;
  const previousConfigPref = hadConfigPref
    ? Services.prefs.getStringPref(MOUSE_GESTURE_CONFIG_PREF)
    : null;

  try {
    const config = createTestConfig(options);
    setConfig(config);
    setEnabled(options.enabled ?? true);
    await fn();
  } finally {
    setConfig(previousConfig);
    setEnabled(previousEnabled);
    if (previousConfigPref !== null) {
      Services.prefs.setStringPref(
        MOUSE_GESTURE_CONFIG_PREF,
        previousConfigPref,
      );
    } else {
      Services.prefs.clearUserPref(MOUSE_GESTURE_CONFIG_PREF);
    }
    if (previousEnabledPref !== null) {
      Services.prefs.setBoolPref(
        MOUSE_GESTURE_ENABLED_PREF,
        previousEnabledPref,
      );
    } else {
      Services.prefs.clearUserPref(MOUSE_GESTURE_ENABLED_PREF);
    }
  }
}

async function withTrackedActions(
  fn: (counts: ActionCounts) => void | Promise<void>,
): Promise<void> {
  const originals = new Map<TrackedAction, GestureActionFn>();
  const counts: ActionCounts = {
    [PREVIOUS_TAB_ACTION]: 0,
    [NEXT_TAB_ACTION]: 0,
    [ROCKER_RIGHT_LEFT_ACTION]: 0,
    [DRAWN_RIGHT_ACTION]: 0,
  };

  for (const actionName of TRACKED_ACTIONS) {
    const original = gestureActions.getAction(actionName);
    assert(original, `expected built-in action ${actionName} to exist`);
    originals.set(actionName, original);
    gestureActions.registerAction({
      name: actionName,
      fn: () => {
        counts[actionName] += 1;
      },
    });
  }

  try {
    await fn(counts);
  } finally {
    for (const actionName of TRACKED_ACTIONS) {
      const original = originals.get(actionName);
      if (original) {
        gestureActions.registerAction({ name: actionName, fn: original });
      }
    }
  }
}

async function withControllerAndContent(
  options: TestConfigOptions,
  fn: (
    harness: FakeWindowHarness & { contentEl: HTMLElement },
    controller: MouseGestureController,
  ) => void | Promise<void>,
): Promise<void> {
  await withTestConfig(options, async () => {
    const harness = createFakeWindowWithContent();
    const controller = new MouseGestureController(harness.win);
    try {
      await fn(harness, controller);
    } finally {
      controller.destroy();
    }
  });
}

async function withController(
  options: TestConfigOptions,
  fn: (
    harness: FakeWindowHarness,
    controller: MouseGestureController,
  ) => void | Promise<void>,
): Promise<void> {
  await withTestConfig(options, async () => {
    const harness = createFakeWindow();
    const controller = new MouseGestureController(harness.win);
    try {
      await fn(harness, controller);
    } finally {
      controller.destroy();
    }
  });
}

async function testWheelGestureSuppressesPostMouseUpContextMenu(): Promise<
  void
> {
  await withTrackedActions(async (counts) => {
    await withController({}, ({ win }) => {
      dispatchMouse(win, "mousedown", 2);
      const wheel = dispatchWheel(win, 120);
      const mouseUp = dispatchMouse(win, "mouseup", 2);
      const contextMenu = dispatchMouse(win, "contextmenu", 2);

      assertEquals(
        wheel.defaultPrevented,
        true,
        "wheel gesture should be consumed",
      );
      assertEquals(
        mouseUp.defaultPrevented,
        true,
        "right mouseup should be consumed",
      );
      assertEquals(
        contextMenu.defaultPrevented,
        true,
        "post-mouseup contextmenu should remain suppressed",
      );
      assertEquals(
        counts[NEXT_TAB_ACTION],
        1,
        "wheel gesture should execute its action once",
      );
    });
  });
}

async function testWheelGestureChainsWhileHeldAndConsumesResidualWheel(): Promise<
  void
> {
  await withTrackedActions(async (counts) => {
    await withController({}, (harness) => {
      const { win } = harness;
      dispatchMouse(win, "mousedown", 2);
      const firstWheel = dispatchWheel(win, 120);
      dispatchMouse(win, "mousemove", 0, 100, 0);
      // While the right button is held, each wheel notch switches tabs
      // (Floorp issue #2586 regression from the #2559 exact-once latch).
      const heldResidualWheel = dispatchWheel(win, -120);
      dispatchMouse(win, "mouseup", 2);
      const postMouseUpResidualWheel = dispatchWheel(win, 120);
      harness.runAllTimers();

      assertEquals(
        firstWheel.defaultPrevented,
        true,
        "first wheel should be consumed",
      );
      assertEquals(
        heldResidualWheel.defaultPrevented,
        true,
        "wheel events while the right button remains held should be consumed",
      );
      assertEquals(
        postMouseUpResidualWheel.defaultPrevented,
        true,
        "residual wheel after mouseup should be consumed",
      );
      assertEquals(
        counts[NEXT_TAB_ACTION],
        1,
        "first wheel should execute next-tab once",
      );
      assertEquals(
        counts[PREVIOUS_TAB_ACTION],
        1,
        "a subsequent wheel while held must execute its own action (multi-tab switching)",
      );
      assertEquals(
        counts[DRAWN_RIGHT_ACTION],
        0,
        "pointer movement after a wheel action must not execute a drawn gesture",
      );
    });
  });
}

async function testNormalRightClickRemainsAllowed(): Promise<void> {
  await withController({}, ({ win }) => {
    dispatchMouse(win, "mousedown", 2);
    const mouseUp = dispatchMouse(win, "mouseup", 2);
    const contextMenu = dispatchMouse(win, "contextmenu", 2);

    assertEquals(
      mouseUp.defaultPrevented,
      false,
      "normal mouseup should be allowed",
    );
    assertEquals(
      contextMenu.defaultPrevented,
      false,
      "normal right click should still open the context menu",
    );
  });
}

async function testDisabledWheelGesturesRemainPassive(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({ wheelGesturesEnabled: false }, ({ win }) => {
      dispatchMouse(win, "mousedown", 2);
      const wheel = dispatchWheel(win, 120);
      dispatchMouse(win, "mouseup", 2);
      const contextMenu = dispatchMouse(win, "contextmenu", 2);

      assertEquals(
        wheel.defaultPrevented,
        false,
        "disabled wheel gesture should pass through",
      );
      assertEquals(
        contextMenu.defaultPrevented,
        false,
        "disabled wheel gesture should not suppress a normal right click",
      );
      assertEquals(
        counts[NEXT_TAB_ACTION],
        0,
        "disabled wheel action must not run",
      );
    });
  });
}

async function testDisabledFeatureRemainsPassive(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({ enabled: false }, ({ win }) => {
      dispatchMouse(win, "mousedown", 2);
      const wheel = dispatchWheel(win, 120);
      const mouseUp = dispatchMouse(win, "mouseup", 2);
      const contextMenu = dispatchMouse(win, "contextmenu", 2);

      assertEquals(
        wheel.defaultPrevented,
        false,
        "disabled feature should pass wheel",
      );
      assertEquals(
        mouseUp.defaultPrevented,
        false,
        "disabled feature should pass mouseup",
      );
      assertEquals(
        contextMenu.defaultPrevented,
        false,
        "disabled feature should allow contextmenu",
      );
      assertEquals(counts[NEXT_TAB_ACTION], 0, "disabled feature must not act");
    });
  });
}

async function testZeroDeltaWheelRemainsPassive(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({}, ({ win }) => {
      dispatchMouse(win, "mousedown", 2);
      const wheel = dispatchWheel(win, 0);
      dispatchMouse(win, "mouseup", 2);
      const contextMenu = dispatchMouse(win, "contextmenu", 2);

      assertEquals(
        wheel.defaultPrevented,
        false,
        "zero-delta wheel should pass through",
      );
      assertEquals(
        contextMenu.defaultPrevented,
        false,
        "zero-delta wheel should not turn a right click into a gesture",
      );
      assertEquals(
        counts[NEXT_TAB_ACTION],
        0,
        "zero-delta wheel must not run next-tab",
      );
      assertEquals(
        counts[PREVIOUS_TAB_ACTION],
        0,
        "zero-delta wheel must not run previous-tab",
      );
    });
  });
}

async function testWheelSuppressionExpiresWithoutExtending(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({ preventionTimeout: 25 }, (harness) => {
      const { win } = harness;
      dispatchMouse(win, "mousedown", 2);
      dispatchWheel(win, 120);
      assertEquals(
        harness.pendingTimerCount(),
        0,
        "suppression timeout should not start while the right button is held",
      );
      assertEquals(
        dispatchMouse(win, "contextmenu", 2).defaultPrevented,
        true,
        "contextmenu should be suppressed while the wheel gesture is active",
      );
      dispatchMouse(win, "mouseup", 2);

      assertEquals(
        harness.pendingTimerCount(),
        1,
        "mouseup should arm one bounded timer",
      );
      assertEquals(
        dispatchMouse(win, "contextmenu", 2).defaultPrevented,
        true,
        "contextmenu should be suppressed before timeout",
      );
      assertEquals(
        dispatchWheel(win, -120).defaultPrevented,
        true,
        "residual wheel should be suppressed before timeout",
      );
      assertEquals(
        harness.pendingTimerCount(),
        1,
        "residual events must not extend the suppression timeout",
      );

      harness.runAllTimers();

      assertEquals(
        harness.pendingTimerCount(),
        0,
        "suppression timer should complete",
      );
      assertEquals(
        dispatchMouse(win, "contextmenu", 2).defaultPrevented,
        false,
        "contextmenu should be allowed after timeout",
      );
      assertEquals(
        dispatchWheel(win, -120).defaultPrevented,
        false,
        "wheel should be allowed after timeout",
      );
      assertEquals(
        counts[NEXT_TAB_ACTION],
        1,
        "timeout must not repeat the action",
      );
      assertEquals(
        counts[PREVIOUS_TAB_ACTION],
        0,
        "residual wheel must remain action-free",
      );
    });
  });
}

async function testNewRightClickResetsWheelSuppression(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({}, (harness) => {
      const { win } = harness;
      dispatchMouse(win, "mousedown", 2);
      dispatchWheel(win, 120);
      dispatchMouse(win, "mouseup", 2);
      assertEquals(
        harness.pendingTimerCount(),
        1,
        "wheel suppression should be armed",
      );

      dispatchMouse(win, "mousedown", 2);
      assertEquals(
        harness.pendingTimerCount(),
        0,
        "a new right-button cycle should clear the old suppression timer",
      );
      dispatchMouse(win, "mouseup", 2);
      assertEquals(
        dispatchMouse(win, "contextmenu", 2).defaultPrevented,
        false,
        "the new normal right click should be allowed",
      );
      assertEquals(counts[NEXT_TAB_ACTION], 1, "wheel action should fire once");
    });
  });
}

async function testNewRightClickRecoversFromLostMouseUp(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({}, ({ win }) => {
      dispatchMouse(win, "mousedown", 2);
      dispatchWheel(win, 120);

      // Simulate a mouseup lost outside the chrome window. A later physical
      // right-button press establishes a new cycle and must not stay latched.
      const nextMouseDown = dispatchMouse(win, "mousedown", 2);
      const nextMouseUp = dispatchMouse(win, "mouseup", 2);
      const nextContextMenu = dispatchMouse(win, "contextmenu", 2);

      assertEquals(
        nextMouseDown.defaultPrevented,
        false,
        "a fresh right-button cycle should clear a stale wheel latch",
      );
      assertEquals(
        nextMouseUp.defaultPrevented,
        false,
        "the recovered normal mouseup should remain passive",
      );
      assertEquals(
        nextContextMenu.defaultPrevented,
        false,
        "the recovered normal right click should open its context menu",
      );
      assertEquals(counts[NEXT_TAB_ACTION], 1, "wheel action should fire once");
    });
  });
}

async function testBlurClearsWheelGestureWithLostMouseUp(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({}, ({ win }) => {
      dispatchMouse(win, "mousedown", 2);
      dispatchWheel(win, 120);
      win.dispatchEvent(new Event("blur"));

      assertEquals(
        dispatchWheel(win, -120).defaultPrevented,
        false,
        "wheel should become passive after the interrupted cycle is cleared",
      );
      assertEquals(
        dispatchMouse(win, "contextmenu", 2).defaultPrevented,
        false,
        "contextmenu should be allowed after focus interruption",
      );
      assertEquals(counts[NEXT_TAB_ACTION], 1, "wheel action should fire once");
      assertEquals(
        counts[PREVIOUS_TAB_ACTION],
        0,
        "focus cleanup must not execute another wheel action",
      );
    });
  });
}

async function testDisableTransitionClearsWheelSuppression(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({}, (harness) => {
      const { win } = harness;
      dispatchMouse(win, "mousedown", 2);
      dispatchWheel(win, 120);
      dispatchMouse(win, "mouseup", 2);
      assertEquals(
        harness.pendingTimerCount(),
        1,
        "wheel suppression should be armed",
      );

      setEnabled(false);
      const residualWheel = dispatchWheel(win, -120);
      const contextMenu = dispatchMouse(win, "contextmenu", 2);

      assertEquals(
        harness.pendingTimerCount(),
        0,
        "disabled transition should clear the suppression timer",
      );
      assertEquals(
        residualWheel.defaultPrevented,
        false,
        "disabled controller should pass wheel",
      );
      assertEquals(
        contextMenu.defaultPrevented,
        false,
        "disabled controller should allow contextmenu",
      );
      assertEquals(
        counts[NEXT_TAB_ACTION],
        1,
        "disable transition must not repeat action",
      );
      assertEquals(
        counts[PREVIOUS_TAB_ACTION],
        0,
        "disabled residual wheel must not act",
      );
    });
  });
}

async function testDestroyClearsWheelSuppressionTimer(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({}, (harness, controller) => {
      const { win } = harness;
      dispatchMouse(win, "mousedown", 2);
      dispatchWheel(win, 120);
      dispatchMouse(win, "mouseup", 2);
      assertEquals(
        harness.pendingTimerCount(),
        1,
        "wheel suppression should be armed",
      );

      controller.destroy();

      assertEquals(
        harness.pendingTimerCount(),
        0,
        "destroy should clear wheel timer",
      );
      assertEquals(
        dispatchMouse(win, "contextmenu", 2).defaultPrevented,
        false,
        "destroyed controller should no longer intercept contextmenu",
      );
      assertEquals(
        dispatchWheel(win, 120).defaultPrevented,
        false,
        "destroyed controller should no longer intercept wheel",
      );
      assertEquals(counts[NEXT_TAB_ACTION], 1, "wheel action should fire once");
    });
  });
}

async function testWheelGestureCannotBecomeRockerGesture(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({}, ({ win }) => {
      dispatchMouse(win, "mousedown", 2);
      dispatchWheel(win, 120);
      const leftMouseDown = dispatchMouse(win, "mousedown", 0);
      dispatchMouse(win, "mouseup", 2);

      assertEquals(
        leftMouseDown.defaultPrevented,
        true,
        "button presses after a wheel action should be consumed for this cycle",
      );
      assertEquals(counts[NEXT_TAB_ACTION], 1, "wheel action should fire once");
      assertEquals(
        counts[ROCKER_RIGHT_LEFT_ACTION],
        0,
        "wheel-first cycle must not also execute a rocker action",
      );
    });
  });
}

async function testRockerGestureCannotBecomeWheelGesture(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({}, ({ win }) => {
      dispatchMouse(win, "mousedown", 2);
      dispatchMouse(win, "mousedown", 0);
      const wheel = dispatchWheel(win, 120);
      dispatchMouse(win, "mouseup", 2);
      dispatchMouse(win, "mouseup", 0);

      assertEquals(
        wheel.defaultPrevented,
        true,
        "wheel after rocker should be consumed",
      );
      assertEquals(
        counts[ROCKER_RIGHT_LEFT_ACTION],
        1,
        "rocker action should fire once",
      );
      assertEquals(
        counts[NEXT_TAB_ACTION],
        0,
        "rocker-first cycle must not also execute a wheel action",
      );
    });
  });
}

async function testRecognizedGestureExecutesSynchronously(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({}, ({ win }) => {
      dispatchMouse(win, "mousedown", 2);
      dispatchDrag(win, 200, 0);
      const mouseUp = dispatchMouse(win, "mouseup", 2);

      // No harness.runAllTimers() call: the action must already have run.
      // The old code deferred execution behind a 100ms setTimeout, so this
      // assertion would fail against that version.
      assertEquals(
        counts[DRAWN_RIGHT_ACTION],
        1,
        "recognized gesture should execute its action synchronously on mouseup",
      );
      assertEquals(
        mouseUp.defaultPrevented,
        true,
        "recognized gesture's mouseup should be consumed",
      );
    });
  });
}

async function testUnrecognizedMovedGestureConsumesMouseUp(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController(
      { actions: [{ pattern: ["right"], action: DRAWN_RIGHT_ACTION }] },
      ({ win }) => {
        dispatchMouse(win, "mousedown", 2);
        // Moves past the activation distance but does not match the only
        // configured pattern ("right"), so no gesture is recognized.
        dispatchDrag(win, 0, 200);
        const mouseUp = dispatchMouse(win, "mouseup", 2);
        const contextMenu = dispatchMouse(win, "contextmenu", 2);

        assertEquals(
          mouseUp.defaultPrevented,
          true,
          "an unrecognized but moved gesture should still consume its mouseup",
        );
        assertEquals(
          contextMenu.defaultPrevented,
          true,
          "context menu should stay suppressed after an unrecognized gesture",
        );
        assertEquals(
          counts[DRAWN_RIGHT_ACTION],
          0,
          "no action should run for an unrecognized gesture",
        );
      },
    );
  });
}

async function testGenuineInterruptionResetsActiveDrawnGesture(): Promise<
  void
> {
  await withTrackedActions(async (counts) => {
    await withController({}, ({ win }) => {
      dispatchMouse(win, "mousedown", 2);
      dispatchDrag(win, 100, 0);

      // The fake window can never equal Services.focus.activeWindow, so this
      // exercises the "genuine interruption" branch (as opposed to the
      // same-window internal-focus-churn case the fix now ignores).
      win.dispatchEvent(new Event("blur"));

      const mouseUp = dispatchMouse(win, "mouseup", 2);
      const contextMenu = dispatchMouse(win, "contextmenu", 2);

      assertEquals(
        mouseUp.defaultPrevented,
        false,
        "mouseup after a genuine interruption should be passive",
      );
      assertEquals(
        contextMenu.defaultPrevented,
        false,
        "context menu should be allowed after a genuine interruption",
      );
      assertEquals(
        counts[DRAWN_RIGHT_ACTION],
        0,
        "an interrupted drawn gesture must not execute",
      );
    });
  });
}

async function testNewRightMouseDownRecoversFromStaleDrawnGesture(): Promise<
  void
> {
  await withTrackedActions(async (counts) => {
    await withController({}, ({ win }) => {
      // Start a drawn gesture and lose its mouseup entirely (never
      // dispatched) - simulates the mouseup being dropped mid-drag, e.g. by
      // the same tab-close/focus churn that motivated ignoring same-window
      // blur. Without the fix, isGestureActive stays stuck true forever and
      // every future right-button mousedown is silently swallowed.
      dispatchMouse(win, "mousedown", 2);
      dispatchDrag(win, 100, 0);

      const nextMouseDown = dispatchMouse(win, "mousedown", 2);
      dispatchDrag(win, 200, 0);
      const nextMouseUp = dispatchMouse(win, "mouseup", 2);

      assertEquals(
        nextMouseDown.defaultPrevented,
        false,
        "a fresh right-button mousedown is not itself prevented",
      );
      assertEquals(
        counts[DRAWN_RIGHT_ACTION],
        1,
        "the recovered cycle should still recognize and execute its gesture",
      );
      assertEquals(
        nextMouseUp.defaultPrevented,
        true,
        "the recovered cycle's recognized mouseup should be consumed",
      );
    });
  });
}

async function testRockerGestureConsumesMouseMoveWhileHeld(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({}, ({ win }) => {
      // Left-then-right ("leftRight") rocker: the left mousedown anchors a
      // native text-selection drag before the combo can be detected (a lone
      // left click must still behave normally). Once the right mousedown
      // completes the combo, every mousemove while both buttons stay down
      // must be consumed - left unhandled, it would let that selection keep
      // extending as the rocker's action (e.g. scrolling) moves content
      // under the still-held cursor.
      dispatchMouse(win, "mousedown", 0);
      const rightMouseDown = dispatchMouse(win, "mousedown", 2);
      const moveWhileHeld = dispatchMouse(win, "mousemove", 0, 50, 0);
      // Releasing only the right button doesn't end the cycle - the left
      // button is still physically held, so movement must stay consumed.
      dispatchMouse(win, "mouseup", 2);
      const moveAfterRightRelease = dispatchMouse(win, "mousemove", 0, 55, 0);
      dispatchMouse(win, "mouseup", 0);
      const moveAfterRelease = dispatchMouse(win, "mousemove", 0, 60, 0);

      assertEquals(
        rightMouseDown.defaultPrevented,
        true,
        "the rocker-completing mousedown should be consumed",
      );
      assertEquals(
        moveWhileHeld.defaultPrevented,
        true,
        "mousemove while a rocker action is active must not leak through " +
          "to the page (it would extend a native selection drag)",
      );
      assertEquals(
        moveAfterRightRelease.defaultPrevented,
        true,
        "mousemove must stay consumed after only the right button releases " +
          "- the left button is still held, so the cycle isn't over yet",
      );
      assertEquals(
        moveAfterRelease.defaultPrevented,
        false,
        "mousemove after the rocker cycle ends should be passive again",
      );
      assertEquals(
        counts[DRAWN_RIGHT_ACTION],
        1,
        "leftRight rocker action should fire once",
      );
    });
  });
}

async function testMouseDownCaptureSurvivesContentStopPropagation(): Promise<
  void
> {
  await withTrackedActions(async (counts) => {
    await withControllerAndContent({}, ({ contentEl }) => {
      // Simulate a page (e.g. an image gallery, map, or editor) that stops
      // propagation on its own mousedown. Before mousedown was capture-phase,
      // this would prevent the gesture controller - registered on the window
      // ancestor - from ever seeing the event, silently breaking rocker
      // detection on that page.
      contentEl.addEventListener("mousedown", (event) => {
        event.stopPropagation();
      });
      contentEl.addEventListener("mousemove", (event) => {
        event.stopPropagation();
      });

      dispatchMouseFrom(contentEl, "mousedown", 2);
      dispatchMouseFrom(contentEl, "mousedown", 0);
      // Content also stops propagation on its own mousemove. A regression
      // that moved the mousemove listener back to bubble phase would still
      // pass every other assertion here, since rocker detection itself only
      // depends on mousedown - this is what actually exercises capture-phase
      // mousemove suppression.
      const moveWhileHeld = dispatchMouseFrom(contentEl, "mousemove", 0, 5, 0);
      dispatchMouseFrom(contentEl, "mouseup", 2);
      dispatchMouseFrom(contentEl, "mouseup", 0);

      assertEquals(
        counts[ROCKER_RIGHT_LEFT_ACTION],
        1,
        "rocker gesture should still be detected even when content stops " +
          "propagation on its own mousedown",
      );
      assertEquals(
        moveWhileHeld.defaultPrevented,
        true,
        "mousemove while a rocker action is active should stay consumed " +
          "even when content stops propagation on its own mousemove",
      );
    });
  });
}

async function testLeftRightRockerLetsLeftMouseUpThrough(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({}, ({ win }) => {
      // The left button's own mousedown is never prevented (a lone left
      // click must still behave normally), so the browser already started
      // real native selection-drag tracking for it once pressed. That
      // tracking only ends once the page actually receives the matching
      // left mouseup. If the rocker cleanup swallowed every button's
      // mouseup indiscriminately, the page would never find out the left
      // button was released, leaving native selection mode stuck "on" -
      // even with zero mouse movement during the whole sequence.
      const leftMouseDown = dispatchMouse(win, "mousedown", 0);
      const rightMouseDown = dispatchMouse(win, "mousedown", 2);
      const rightMouseUp = dispatchMouse(win, "mouseup", 2);
      const leftMouseUp = dispatchMouse(win, "mouseup", 0);

      assertEquals(
        leftMouseDown.clickEventPrevented(),
        false,
        "the ordinary left mousedown must not suppress its click before a " +
          "rocker is confirmed",
      );

      assertEquals(
        rightMouseDown.defaultPrevented,
        true,
        "the rocker-completing mousedown should be consumed",
      );
      assertEquals(
        rightMouseUp.defaultPrevented,
        true,
        "the right button's mouseup should stay consumed (suppresses its " +
          "context menu)",
      );
      assertEquals(
        rightMouseDown.clickEventPrevented(),
        true,
        "the rocker-completing mousedown should suppress its auxclick",
      );
      assertEquals(
        rightMouseUp.clickEventPrevented(),
        true,
        "the rocker-owned right mouseup should suppress its auxclick",
      );
      assertEquals(
        leftMouseUp.defaultPrevented,
        false,
        "the left button's own mouseup must reach the page so native " +
          "selection-drag tracking can terminate",
      );
      assertEquals(
        leftMouseUp.clickEventPrevented(),
        true,
        "the forwarded left mouseup must not synthesize a page click",
      );
      assertEquals(
        counts[DRAWN_RIGHT_ACTION],
        1,
        "leftRight rocker action should fire once",
      );

      const nextLeftMouseDown = dispatchMouse(win, "mousedown", 0);
      const nextLeftMouseUp = dispatchMouse(win, "mouseup", 0);
      const nextRightMouseDown = dispatchMouse(win, "mousedown", 2);
      const nextRightMouseUp = dispatchMouse(win, "mouseup", 2);
      assertEquals(
        nextLeftMouseDown.clickEventPrevented(),
        false,
        "click suppression must not leak into the next left mousedown",
      );
      assertEquals(
        nextLeftMouseUp.clickEventPrevented(),
        false,
        "click suppression must not leak into the next left mouseup",
      );
      assertEquals(
        nextRightMouseDown.clickEventPrevented(),
        false,
        "click suppression must not leak into the next right mousedown",
      );
      assertEquals(
        nextRightMouseUp.clickEventPrevented(),
        false,
        "click suppression must not leak into the next right mouseup",
      );
    });
  });
}

async function testRightLeftRockerSuppressesLeftMouseUp(): Promise<void> {
  await withTrackedActions(async (counts) => {
    await withController({}, ({ win }) => {
      // Mirror of the leftRight case above, but for the opposite order:
      // right pressed first starts the normal drawn-gesture path (also
      // unprevented, but right-click doesn't anchor a native selection
      // drag), and the left mousedown that completes the combo *is*
      // prevented. Unlike leftRight, there's no unblocked native default
      // here for the left mouseup to terminate, so it should stay
      // suppressed like every other button in this cleanup path - letting
      // it through would leave the page with an unmatched mouseup that
      // never had a corresponding unprevented mousedown.
      const rightMouseDown = dispatchMouse(win, "mousedown", 2);
      const leftMouseDown = dispatchMouse(win, "mousedown", 0);
      const rightMouseUp = dispatchMouse(win, "mouseup", 2);
      const leftMouseUp = dispatchMouse(win, "mouseup", 0);

      assertEquals(
        rightMouseDown.clickEventPrevented(),
        false,
        "the ordinary right mousedown must not suppress its click before a " +
          "rocker is confirmed",
      );

      assertEquals(
        leftMouseDown.defaultPrevented,
        true,
        "the rocker-completing mousedown should be consumed",
      );
      assertEquals(
        leftMouseDown.clickEventPrevented(),
        true,
        "the rocker-completing left mousedown should suppress its click",
      );
      assertEquals(
        rightMouseUp.defaultPrevented,
        true,
        "the right button's mouseup should stay consumed",
      );
      assertEquals(
        rightMouseUp.clickEventPrevented(),
        true,
        "the rocker-owned right mouseup should suppress its auxclick",
      );
      assertEquals(
        leftMouseUp.defaultPrevented,
        true,
        "the left button's mouseup should stay consumed too - its own " +
          "mousedown was already prevented, so there's no native default " +
          "left to terminate",
      );
      assertEquals(
        leftMouseUp.clickEventPrevented(),
        true,
        "the rocker-owned left mouseup should suppress its click",
      );
      assertEquals(
        counts[ROCKER_RIGHT_LEFT_ACTION],
        1,
        "rightLeft rocker action should fire once",
      );
    });
  });
}

const tests: TestCase[] = [
  {
    name: "wheel gesture suppresses post-mouseup contextmenu",
    fn: testWheelGestureSuppressesPostMouseUpContextMenu,
  },
  {
    name: "wheel gesture chains while held and consumes residual wheel",
    fn: testWheelGestureChainsWhileHeldAndConsumesResidualWheel,
  },
  {
    name: "normal right click remains allowed",
    fn: testNormalRightClickRemainsAllowed,
  },
  {
    name: "disabled wheel gestures remain passive",
    fn: testDisabledWheelGesturesRemainPassive,
  },
  {
    name: "disabled feature remains passive",
    fn: testDisabledFeatureRemainsPassive,
  },
  {
    name: "zero-delta wheel remains passive",
    fn: testZeroDeltaWheelRemainsPassive,
  },
  {
    name: "wheel suppression expires without extending",
    fn: testWheelSuppressionExpiresWithoutExtending,
  },
  {
    name: "new right click resets wheel suppression",
    fn: testNewRightClickResetsWheelSuppression,
  },
  {
    name: "new right click recovers from a lost mouseup",
    fn: testNewRightClickRecoversFromLostMouseUp,
  },
  {
    name: "blur clears a wheel gesture whose mouseup was lost",
    fn: testBlurClearsWheelGestureWithLostMouseUp,
  },
  {
    name: "disable transition clears wheel suppression",
    fn: testDisableTransitionClearsWheelSuppression,
  },
  {
    name: "destroy clears wheel suppression timer",
    fn: testDestroyClearsWheelSuppressionTimer,
  },
  {
    name: "wheel gesture cannot become rocker gesture",
    fn: testWheelGestureCannotBecomeRockerGesture,
  },
  {
    name: "rocker gesture cannot become wheel gesture",
    fn: testRockerGestureCannotBecomeWheelGesture,
  },
  {
    name: "recognized gesture executes synchronously on mouseup",
    fn: testRecognizedGestureExecutesSynchronously,
  },
  {
    name: "unrecognized but moved gesture consumes its mouseup",
    fn: testUnrecognizedMovedGestureConsumesMouseUp,
  },
  {
    name: "a genuine interruption still resets an active drawn gesture",
    fn: testGenuineInterruptionResetsActiveDrawnGesture,
  },
  {
    name: "a new right mousedown recovers from a stale drawn gesture",
    fn: testNewRightMouseDownRecoversFromStaleDrawnGesture,
  },
  {
    name: "rocker gesture consumes mousemove while held",
    fn: testRockerGestureConsumesMouseMoveWhileHeld,
  },
  {
    name: "mousedown capture survives content stopPropagation",
    fn: testMouseDownCaptureSurvivesContentStopPropagation,
  },
  {
    name: "leftRight rocker lets the left mouseup through",
    fn: testLeftRightRockerLetsLeftMouseUpThrough,
  },
  {
    name: "rightLeft rocker suppresses the left mouseup",
    fn: testRightLeftRockerSuppressesLeftMouseUp,
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("mouseGestureController.test.ts", tests);
}
