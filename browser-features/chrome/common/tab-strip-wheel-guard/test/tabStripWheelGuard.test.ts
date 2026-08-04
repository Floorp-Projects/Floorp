// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import indexSource from "../index.ts?raw";
import overridePrefs from "../../../../../static/gecko/pref/override.ini?raw";
import { classifyWheelEvent, emptyWheelGuardState } from "../classifier.ts";
import { installWheelGuard } from "../index.ts";
import {
  type InstalledWheelGuard,
  type WheelGuardEnvironment,
  type WheelGuardGlobalObject,
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
  const lowReversal = classifyWheelEvent(
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
  assertEquals(lowReversal.decision, "dropped-reversal", "low reversal should drop");
  assertEquals(lowReversal.releaseThreshold, 11.5, "threshold should be peak * 1.05 + 1");

  const threshold = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: -11.5,
      deltaY: 0,
      timestamp: 20,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    first.state,
  );
  assertEquals(threshold.outcome, "pass", "threshold magnitude should release");

  const afterGap = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: -1,
      deltaY: 0,
      timestamp: WHEEL_GUARD_STREAM_GAP_MS + 1,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    first.state,
  );
  assertEquals(afterGap.outcome, "pass", "reversal should pass after stream reset");
}

function testRtlInvertsOnlyVerticalMovement(): void {
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
  const ltrVertical = classifyWheelEvent(
    {
      mode: 2,
      deltaMode: 0,
      deltaX: 0,
      deltaY: 12,
      timestamp: 10,
      overflowing: true,
      verticalTabStrip: false,
      rtl: false,
    },
    first.state,
  );
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
  assertEquals(ltrVertical.outcome, "pass", "LTR vertical maps forward");
  assertEquals(rtlVertical.decision, "dropped-reversal", "RTL vertical maps backward");
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

function testStaticBoundariesAndSingleShippedDefault(): void {
  assert(
    !indexSource.includes("addObserver"),
    "wheel guard must not observe pref changes",
  );
  assert(
    !indexSource.includes("ensureElementIsVisible"),
    "wheel guard must not wrap native visibility behavior",
  );
  assert(!indexSource.includes("recenter"), "reserved recenter must stay unimplemented");
  const matches = overridePrefs.match(/^floorp\.tabstrip\.wheelguard\s*=.*$/gm) ?? [];
  assertEquals(matches.length, 1, "wheel guard must have one shipped default");
  assertEquals(
    matches[0].trim(),
    "floorp.tabstrip.wheelguard = 0",
    "wheel guard shipped default must be disabled",
  );
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
  { name: "static boundaries and default are unique", fn: testStaticBoundariesAndSingleShippedDefault },
];

export async function runAllTests(): Promise<void> {
  await runTests("tabStripWheelGuard.test.ts", tests);
}
