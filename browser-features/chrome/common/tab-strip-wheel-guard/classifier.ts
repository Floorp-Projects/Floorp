// SPDX-License-Identifier: MPL-2.0

import {
  type WheelGuardAxis,
  type WheelGuardClassification,
  type WheelGuardDirection,
  type WheelGuardInput,
  type WheelGuardState,
  WHEEL_DELTA_PIXEL,
  WHEEL_GUARD_AXIS_QUARANTINE,
  WHEEL_GUARD_REVERSAL_HOLD,
  WHEEL_GUARD_STREAM_GAP_MS,
} from "./types.ts";

export function emptyWheelGuardState(): WheelGuardState {
  return {
    lastTimestamp: null,
    lastAxis: null,
    direction: null,
    runPeak: 0,
  };
}

function copyState(state: WheelGuardState): WheelGuardState {
  return { ...state };
}

function currentStream(
  state: WheelGuardState,
  timestamp: number,
): WheelGuardState {
  if (
    state.lastTimestamp === null ||
    timestamp < state.lastTimestamp ||
    timestamp - state.lastTimestamp > WHEEL_GUARD_STREAM_GAP_MS
  ) {
    return emptyWheelGuardState();
  }
  return copyState(state);
}

function movement(input: WheelGuardInput): {
  axis: WheelGuardAxis;
  direction: WheelGuardDirection;
  magnitude: number;
} {
  const axis = Math.abs(input.deltaY) > Math.abs(input.deltaX)
    ? "vertical"
    : "horizontal";
  const delta = axis === "horizontal"
    ? input.deltaX
    : input.rtl
    ? -input.deltaY
    : input.deltaY;
  return {
    axis,
    direction: delta < 0 ? -1 : 1,
    magnitude: Math.abs(delta),
  };
}

export function classifyWheelEvent(
  input: WheelGuardInput,
  previousState: WheelGuardState,
): WheelGuardClassification {
  if (input.deltaMode !== WHEEL_DELTA_PIXEL) {
    return {
      outcome: "ignore",
      decision: "ignored-non-pixel",
      state: copyState(previousState),
    };
  }

  if (input.deltaX === 0 && input.deltaY === 0) {
    return {
      outcome: "pass",
      decision: "passed-zero",
      state: copyState(previousState),
    };
  }

  if (!input.overflowing || input.verticalTabStrip) {
    return {
      outcome: "pass",
      decision: "passed-inactive",
      state: emptyWheelGuardState(),
    };
  }

  const state = currentStream(previousState, input.timestamp);
  const { axis, direction, magnitude } = movement(input);

  if (
    (input.mode & WHEEL_GUARD_AXIS_QUARANTINE) !== 0 &&
    axis === "vertical" &&
    state.lastAxis === "horizontal"
  ) {
    return {
      outcome: "drop",
      decision: "dropped-axis",
      state,
      axis,
      direction,
      magnitude,
    };
  }

  if (
    (input.mode & WHEEL_GUARD_REVERSAL_HOLD) !== 0 &&
    state.direction !== null &&
    state.direction !== direction
  ) {
    const releaseThreshold = state.runPeak * 1.05 + 1;
    if (magnitude < releaseThreshold) {
      return {
        outcome: "drop",
        decision: "dropped-reversal",
        state,
        axis,
        direction,
        magnitude,
        releaseThreshold,
      };
    }
  }

  return {
    outcome: "pass",
    decision: "passed",
    state: {
      lastTimestamp: input.timestamp,
      lastAxis: axis,
      direction,
      runPeak: state.direction === direction
        ? Math.max(state.runPeak, magnitude)
        : magnitude,
    },
    axis,
    direction,
    magnitude,
  };
}
