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
}

function createFakeWindow(): FakeWindowHarness {
  const eventTarget = new EventTarget();
  const timers = new Map<number, () => void>();
  let nextTimerId = 1;
  const fakeWindow = eventTarget as EventTarget & {
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

function dispatchMouse(
  win: Window,
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
  win.dispatchEvent(event);
  return event;
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
    actions: defaultConfig.actions.map((action) => ({
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

async function testWheelGestureIsExactOnceAndConsumesResidualWheel(): Promise<
  void
> {
  await withTrackedActions(async (counts) => {
    await withController({}, (harness) => {
      const { win } = harness;
      dispatchMouse(win, "mousedown", 2);
      const firstWheel = dispatchWheel(win, 120);
      dispatchMouse(win, "mousemove", 0, 100, 0);
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
        "next-tab action should fire once",
      );
      assertEquals(
        counts[PREVIOUS_TAB_ACTION],
        0,
        "opposite residual wheel must not execute another action",
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

const tests: TestCase[] = [
  {
    name: "wheel gesture suppresses post-mouseup contextmenu",
    fn: testWheelGestureSuppressesPostMouseUpContextMenu,
  },
  {
    name: "wheel gesture is exact-once and consumes residual wheel",
    fn: testWheelGestureIsExactOnceAndConsumesResidualWheel,
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
];

export async function runAllTests(): Promise<void> {
  await runTests("mouseGestureController.test.ts", tests);
}
