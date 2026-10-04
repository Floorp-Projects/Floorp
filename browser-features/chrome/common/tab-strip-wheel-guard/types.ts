// SPDX-License-Identifier: MPL-2.0

export const TAB_STRIP_WHEEL_GUARD_PREF = "floorp.tabstrip.wheelguard";
export const WHEEL_GUARD_AXIS_QUARANTINE = 1;
export const WHEEL_GUARD_REVERSAL_HOLD = 2;
export const WHEEL_GUARD_SUPPORTED_MASK = 3;
export const WHEEL_GUARD_RESERVED_RECENTER = 4;
export const WHEEL_GUARD_STREAM_GAP_MS = 400;
export const WHEEL_DELTA_PIXEL = 0;

export type WheelGuardAxis = "horizontal" | "vertical";
export type WheelGuardDirection = -1 | 1;
export type WheelGuardOutcome = "pass" | "drop" | "ignore";
export type WheelGuardDecision =
  | "ignored-non-pixel"
  | "passed-zero"
  | "passed-inactive"
  | "passed"
  | "dropped-axis"
  | "dropped-reversal";

export interface WheelGuardState {
  lastTimestamp: number | null;
  lastAxis: WheelGuardAxis | null;
  direction: WheelGuardDirection | null;
  runPeak: number;
}

export interface WheelGuardInput {
  mode: number;
  deltaMode: number;
  deltaX: number;
  deltaY: number;
  timestamp: number;
  overflowing: boolean;
  verticalTabStrip: boolean;
  rtl: boolean;
}

export interface WheelGuardClassification {
  outcome: WheelGuardOutcome;
  decision: WheelGuardDecision;
  state: WheelGuardState;
  axis?: WheelGuardAxis;
  direction?: WheelGuardDirection;
  magnitude?: number;
  releaseThreshold?: number;
}

export interface WheelGuardReadout {
  readonly mode: number;
  readonly unsupportedBits: number;
  axisDropped: number;
  reversalDropped: number;
  passed: number;
  ignored: number;
  lastDecision: WheelGuardDecision | null;
  reset(): void;
}

export interface WheelGuardGlobalObject {
  __floorpWheelGuard?: WheelGuardReadout;
}

export interface WheelGuardEnvironment {
  target: EventTarget;
  globalObject: WheelGuardGlobalObject;
  isOverflowing(): boolean;
  isVerticalTabStrip(): boolean;
  isRtl(): boolean;
  timestampFor(event: WheelEvent): number;
}

export interface InstalledWheelGuard {
  readout: WheelGuardReadout;
  destroy(): void;
}
