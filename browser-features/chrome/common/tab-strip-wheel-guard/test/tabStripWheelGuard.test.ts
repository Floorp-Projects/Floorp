// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { classifyWheelEvent, emptyWheelGuardState } from "../classifier.ts";
import { installWheelGuard } from "../index.ts";
import {
  type InstalledWheelGuard,
  type WheelGuardEnvironment,
  type WheelGuardGlobalObject,
  TAB_STRIP_WHEEL_GUARD_PREF,
  WHEEL_GUARD_STREAM_GAP_MS,
} from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

class RecordingTarget extends EventTarget {
  additions: Array<{
    type: string;
    callback: EventListenerOrEventListenerObject | null;
    options: boolean | AddEventListenerOptions | undefined;
  }> = [];
  removals: Array<{
    type: string;
    callback: EventListenerOrEventListenerObject | null;
    options: boolean | EventListenerOptions | undefined;
  }> = [];

  override addEventListener(
    type: string,
    callback: EventListenerOrEventListenerObject | null,
    options?: boolean | AddEventListenerOptions,
  ): void {
    this.additions.push({ type, callback, options });
    super.addEventListener(type, callback, options);
  }

  override removeEventListener(
    type: string,
    callback: EventListenerOrEventListenerObject | null,
    options?: boolean | EventListenerOptions,
  ): void {
    this.removals.push({ type, callback, options });
    super.removeEventListener(type, callback, options);
  }
}

interface GuardHarness {
  target: RecordingTarget;
  globalObject: WheelGuardGlobalObject;
  installed: InstalledWheelGuard | null;
  setTime(value: number): void;
  setOverflowing(value: boolean): void;
  setVertical(value: boolean): void;
  setRtl(value: boolean): void;
  wheel(options: {
    deltaX?: number;
    deltaY?: number;
    deltaMode?: number;
  }): WheelEvent;
}

function guardHarness(prefValue: number): GuardHarness {
  const target = new RecordingTarget();
  const globalObject: WheelGuardGlobalObject = {};
  let timestamp = 0;
  let overflowing = true;
  let vertical = false;
  let rtl = false;
  const environment: WheelGuardEnvironment = {
    target,
    globalObject,
    isOverflowing: () => overflowing,
    isVerticalTabStrip: () => vertical,
    isRtl: () => rtl,
    timestampFor: () => timestamp,
  };
  const installed = installWheelGuard(prefValue, environment);
  return {
    target,
    globalObject,
    installed,
    setTime: (value) => {
      timestamp = value;
    },
    setOverflowing: (value) => {
      overflowing = value;
    },
    setVertical: (value) => {
      vertical = value;
    },
    setRtl: (value) => {
      rtl = value;
    },
    wheel: ({ deltaX = 0, deltaY = 0, deltaMode = 0 }) => {
      const event = new WheelEvent("wheel", {
        deltaX,
        deltaY,
        deltaMode,
        bubbles: true,
        cancelable: true,
      });
      target.dispatchEvent(event);
      return event;
    },
  };
}

function testDisabledModeHasZeroLifecycle(): void {
  const harness = guardHarness(0);
  assertEquals(harness.installed, null, "mode 0 should not install an owner");
  assertEquals(harness.target.additions.length, 0, "mode 0 must add no listener");
  assertEquals(
    harness.globalObject.__floorpWheelGuard,
    undefined,
    "mode 0 must expose no readout",
  );
}

function testEnabledModesAndReservedBitReadout(): void {
  for (const prefValue of [1, 2, 3, 7]) {
    const harness = guardHarness(prefValue);
    assert(harness.installed, `mode ${prefValue} should install`);
    assertEquals(
      harness.installed.readout.mode,
      prefValue & 3,
      "readout should expose only active bits",
    );
    assertEquals(
      harness.installed.readout.unsupportedBits,
      prefValue & ~3,
      "readout should expose reserved bits without implementing them",
    );
    assertEquals(
      Object.keys(harness.installed.readout).toSorted().join(","),
      [
        "axisDropped",
        "ignored",
        "lastDecision",
        "mode",
        "passed",
        "reset",
        "reversalDropped",
        "unsupportedBits",
      ].toSorted().join(","),
      "readout must contain only the bounded diagnostic surface",
    );
    harness.installed.destroy();
  }
}

function testListenerUsesExactCaptureOwnerAndTeardown(): void {
  const harness = guardHarness(1);
  assert(harness.installed, "mode 1 should install");
  assertEquals(harness.target.additions.length, 1, "one listener should attach");
  const addition = harness.target.additions[0];
  assertEquals(addition.type, "wheel", "the sole listener should be wheel");
  assert(
    typeof addition.options === "object" && addition.options.capture === true,
    "the ancestor listener must use capture phase",
  );
  assert(
    typeof addition.options === "object" && addition.options.passive === false,
    "the dropping listener must be non-passive",
  );

  harness.installed.destroy();
  harness.installed.destroy();
  assertEquals(harness.target.removals.length, 1, "destroy should remove once");
  assertEquals(
    harness.target.removals[0].callback,
    addition.callback,
    "destroy should remove the identical callback",
  );
  assertEquals(
    harness.target.removals[0].options,
    addition.options,
    "destroy should use the identical options object",
  );
  assertEquals(
    harness.globalObject.__floorpWheelGuard,
    undefined,
    "destroy should remove the owned readout",
  );
}

function testNonPixelZeroAndInactiveEventsPass(): void {
  const harness = guardHarness(3);
  assert(harness.installed, "mode 3 should install");

  const line = harness.wheel({ deltaY: 1, deltaMode: 1 });
  const page = harness.wheel({ deltaY: 1, deltaMode: 2 });
  const zero = harness.wheel({});
  harness.setOverflowing(false);
  const underflow = harness.wheel({ deltaX: 10 });
  harness.setOverflowing(true);
  harness.setVertical(true);
  const vertical = harness.wheel({ deltaY: 10 });

  for (const event of [line, page, zero, underflow, vertical]) {
    assertEquals(event.defaultPrevented, false, "pass/ignore must stay passive");
  }
  assertEquals(harness.installed.readout.ignored, 2, "line/page should be ignored");
  assertEquals(harness.installed.readout.passed, 3, "zero/inactive should pass");
  harness.installed.destroy();
}

function testAxisQuarantineUsesStrictDominanceAndGap(): void {
  const state = emptyWheelGuardState();
  const tie = classifyWheelEvent(
    {
      mode: 1,
      deltaMode: 0,
      deltaX: 8,
      deltaY: 8,
      timestamp: 0,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    state,
  );
  assertEquals(tie.state.lastAxis, "horizontal", "axis ties are horizontal");

  const dropped = classifyWheelEvent(
    {
      mode: 1,
      deltaMode: 0,
      deltaX: 1,
      deltaY: 2,
      timestamp: WHEEL_GUARD_STREAM_GAP_MS,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    tie.state,
  );
  assertEquals(dropped.decision, "dropped-axis", "vertical momentum should drop");

  const released = classifyWheelEvent(
    {
      mode: 1,
      deltaMode: 0,
      deltaX: 1,
      deltaY: 2,
      timestamp: WHEEL_GUARD_STREAM_GAP_MS + 1,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    tie.state,
  );
  assertEquals(released.outcome, "pass", "stream should reset after 400 ms");
}

function testAxisDropPreventsNativeTargetHandling(): void {
  const harness = guardHarness(1);
  assert(harness.installed, "mode 1 should install");
  harness.wheel({ deltaX: 12 });
  harness.setTime(10);
  const dropped = harness.wheel({ deltaY: 3 });
  assertEquals(dropped.defaultPrevented, true, "axis quarantine should drop");
  assertEquals(harness.installed.readout.axisDropped, 1, "axis counter should increment");
  assertEquals(harness.installed.readout.reversalDropped, 0, "other counter should not increment");
  harness.installed.destroy();
}

function testReversalThresholdAndResetGap(): void {
  const first = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: 10,
      deltaY: 0,
      timestamp: 0,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    emptyWheelGuardState(),
  );
  assertEquals(first.outcome, "pass", "first event should pass");
  assertEquals(first.state.runPeak, -1, "first pass resets runPeak");

  // First reversal always drops because runPeak was -1 (runPeak >= 0 is false)
  const firstReversal = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: -11.49,
      deltaY: 0,
      timestamp: 20,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    first.state,
  );
  assertEquals(firstReversal.decision, "dropped-reversal", "first reversal should drop");
  assertEquals(firstReversal.state.runPeak, 11.49, "dropped reversal grows runPeak from magnitude");
  assertEquals(firstReversal.releaseThreshold, 13.0645, "threshold is reversal-run peak * 1.05 + 1");

  // Subsequent reversal below new threshold still drops and grows peak
  const secondReversal = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: -12,
      deltaY: 0,
      timestamp: 40,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    firstReversal.state,
  );
  assertEquals(secondReversal.decision, "dropped-reversal", "second reversal below threshold should drop");
  assertEquals(secondReversal.state.runPeak, 12, "runPeak grows from second reversal");
  assertEquals(secondReversal.releaseThreshold, 13.600000000000001, "threshold grows with reversal-run peak");

  // Reversal exceeding threshold releases
  const releasingReversal = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: -14,
      deltaY: 0,
      timestamp: 60,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    secondReversal.state,
  );
  assertEquals(releasingReversal.outcome, "pass", "reversal above threshold should release");
  assertEquals(releasingReversal.state.runPeak, -1, "release resets runPeak");

  // Same-direction pass (continuing the released reversal direction, -1)
  // resets runPeak.
  const sameDirection = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: -16,
      deltaY: 0,
      timestamp: 80,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    releasingReversal.state,
  );
  assertEquals(sameDirection.outcome, "pass", "same-direction pass should pass");
  assertEquals(sameDirection.state.runPeak, -1, "same-direction pass resets runPeak");

  // After gap, stream resets
  const afterGap = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: -1,
      deltaY: 0,
      timestamp: 500,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    sameDirection.state,
  );
  assertEquals(afterGap.outcome, "pass", "event should pass after stream reset");
  assertEquals(afterGap.state.runPeak, -1, "gap reset initializes runPeak to -1");
}

function testRtlInvertsOnlyVerticalMovement(): void {
  // Establish a vertical stream (LTR, forward direction)
  const first = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: 0,
      deltaY: 12,
      timestamp: 0,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    emptyWheelGuardState(),
  );
  // Same physical direction under RTL maps to negative deltaY → a reversal
  // of the vertical stream. First reversal always drops.
  const rtlVertical = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: 0,
      deltaY: 1,
      timestamp: 10,
      overflowing: true,
      verticalTabStrip: false,
      rtl: true,
    },
    first.state,
  );
  assertEquals(first.outcome, "pass", "first vertical should pass");
  assertEquals(
    rtlVertical.decision,
    "dropped-reversal",
    "RTL vertical maps backward and drops",
  );

  // Horizontal movement is NOT inverted by RTL: a horizontal stream maps
  // forward regardless of rtl.
  const horizontal = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: 10,
      deltaY: 0,
      timestamp: 20,
      overflowing: true,
      verticalTabStrip: false,
      rtl: true,
    },
    first.state,
  );
  assertEquals(horizontal.outcome, "pass", "RTL horizontal maps forward");
  assertEquals(horizontal.decision, "passed", "RTL horizontal should pass");
}

function testReadoutResetAndPostDestroyPassivity(): void {
  const harness = guardHarness(3);
  assert(harness.installed, "mode 3 should install");
  harness.wheel({ deltaX: 10 });
  harness.setTime(10);
  harness.wheel({ deltaY: 2 });
  assertEquals(harness.installed.readout.axisDropped, 1, "precondition counter");
  harness.installed.readout.reset();
  assertEquals(harness.installed.readout.axisDropped, 0, "reset clears axis count");
  assertEquals(harness.installed.readout.reversalDropped, 0, "reset clears reversal count");
  assertEquals(harness.installed.readout.passed, 0, "reset clears passed count");
  assertEquals(harness.installed.readout.ignored, 0, "reset clears ignored count");
  assertEquals(harness.installed.readout.lastDecision, null, "reset clears decision");

  harness.installed.destroy();
  const afterDestroy = harness.wheel({ deltaX: 10 });
  assertEquals(afterDestroy.defaultPrevented, false, "destroyed owner is passive");
}

function testShippedDefaultIsDisabled(): void {
  // The shipped default (override.ini) is 0 = disabled. The test browser
  // loads prefs from override.ini, so the runtime default must be 0.
  assertEquals(
    Services.prefs.getIntPref(TAB_STRIP_WHEEL_GUARD_PREF, 7),
    0,
    "wheel guard shipped default must be disabled",
  );
}

function testCrossAxisVerticalToHorizontalHandoffPasses(): void {
  // Vertical stream establishes axis
  const vertical = classifyWheelEvent(
    {
      mode: 3,
      deltaMode: 0,
      deltaX: 0,
      deltaY: 10,
      timestamp: 0,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    emptyWheelGuardState(),
  );
  assertEquals(vertical.outcome, "pass", "vertical event should pass");
  assertEquals(vertical.state.lastAxis, "vertical", "stream should be vertical");

  // Horizontal event after vertical stream: v→h handoff, should pass (not dropped-reversal)
  const horizontal = classifyWheelEvent(
    {
      mode: 3,
      deltaMode: 0,
      deltaX: 10,
      deltaY: 0,
      timestamp: 20,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    vertical.state,
  );
  assertEquals(horizontal.outcome, "pass", "horizontal after vertical should pass");
  assertEquals(horizontal.decision, "passed", "horizontal handoff should be passed, not dropped-reversal");
  assertEquals(horizontal.state.lastAxis, "horizontal", "stream should re-latch to horizontal");
}

function testTailEntryAboveForwardPeakStillDrops(): void {
  // Forward event establishes stream
  const forward = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: 10,
      deltaY: 0,
      timestamp: 0,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    emptyWheelGuardState(),
  );
  assertEquals(forward.outcome, "pass", "forward event should pass");

  // Reversal entering at 12: first reversal always drops (runPeak was -1 after forward pass)
  const tailEntry = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: -12,
      deltaY: 0,
      timestamp: 20,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    forward.state,
  );
  assertEquals(tailEntry.decision, "dropped-reversal", "tail entry above forward peak should still drop");
  assertEquals(tailEntry.state.runPeak, 12, "dropped tail establishes reversal-run peak");
  assertEquals(tailEntry.releaseThreshold, 13.600000000000001, "threshold grows from reversal-run peak");
}

function testObserveThroughUnderflowPreservesStream(): void {
  // Horizontal stream establishes state
  const first = classifyWheelEvent(
    {
      mode: 1,
      deltaMode: 0,
      deltaX: 10,
      deltaY: 0,
      timestamp: 0,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    emptyWheelGuardState(),
  );
  assertEquals(first.outcome, "pass", "first event should pass");
  assertEquals(first.state.lastAxis, "horizontal", "stream should be horizontal");

  // Underflow event passes but preserves stream state
  const underflow = classifyWheelEvent(
    {
      mode: 1,
      deltaMode: 0,
      deltaX: 5,
      deltaY: 0,
      timestamp: 10,
      overflowing: false,
      verticalTabStrip: false,
      rtl: false,
    },
    first.state,
  );
  assertEquals(underflow.outcome, "pass", "underflow event should pass");
  assertEquals(underflow.decision, "passed-inactive", "underflow should be passed-inactive");
  assertEquals(underflow.state.lastAxis, "horizontal", "underflow should preserve stream axis");

  // Next vertical-dominant event in horizontal stream should still drop (mode 1 axis quarantine)
  const verticalAfterUnderflow = classifyWheelEvent(
    {
      mode: 1,
      deltaMode: 0,
      deltaX: 1,
      deltaY: 2,
      timestamp: 20,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    underflow.state,
  );
  assertEquals(verticalAfterUnderflow.decision, "dropped-axis", "vertical after underflow should still drop");
}

const tests: TestCase[] = [
  { name: "mode 0 has zero lifecycle", fn: testDisabledModeHasZeroLifecycle },
  { name: "modes 1/2/3/7 expose bounded readout", fn: testEnabledModesAndReservedBitReadout },
  { name: "listener uses exact capture owner and teardown", fn: testListenerUsesExactCaptureOwnerAndTeardown },
  { name: "non-pixel, zero, underflow, and vertical events pass", fn: testNonPixelZeroAndInactiveEventsPass },
  { name: "axis quarantine uses strict dominance and reset gap", fn: testAxisQuarantineUsesStrictDominanceAndGap },
  { name: "axis drop prevents native target handling", fn: testAxisDropPreventsNativeTargetHandling },
  { name: "reversal uses threshold and reset gap", fn: testReversalThresholdAndResetGap },
  { name: "RTL inverts only vertical movement", fn: testRtlInvertsOnlyVerticalMovement },
  { name: "readout reset and teardown are complete", fn: testReadoutResetAndPostDestroyPassivity },
  { name: "shipped default is disabled", fn: testShippedDefaultIsDisabled },
  { name: "cross-axis v→h handoff in mode 3 passes", fn: testCrossAxisVerticalToHorizontalHandoffPasses },
  { name: "tail-entry above forward peak still drops", fn: testTailEntryAboveForwardPeakStillDrops },
  { name: "observe-through underflow preserves stream", fn: testObserveThroughUnderflowPreservesStream },
];

export async function runAllTests(): Promise<void> {
  await runTests("tabStripWheelGuard.test.ts", tests);
}
