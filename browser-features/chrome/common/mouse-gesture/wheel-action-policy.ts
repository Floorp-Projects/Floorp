// SPDX-License-Identifier: MPL-2.0

// Wheel gestures may fire repeatedly while the secondary button is held.
// Keep this list deliberately narrow: every entry must be safe to repeat
// without destroying tabs, windows, user data, or the application session.
export const REPEAT_SAFE_WHEEL_ACTIONS = [
  "gecko-show-previous-tab",
  "gecko-show-next-tab",
  "gecko-scroll-line-up",
  "gecko-scroll-line-down",
  "gecko-scroll-up",
  "gecko-scroll-down",
  "gecko-scroll-left",
  "gecko-scroll-right",
  "gecko-scroll-to-top",
  "gecko-scroll-to-bottom",
  "gecko-zoom-in",
  "gecko-zoom-out",
  "gecko-reset-zoom",
  "gecko-workspace-next",
  "gecko-workspace-previous",
  "gecko-show-next-search-result",
  "gecko-show-previous-search-result",
] as const;

export type RepeatSafeWheelAction = (typeof REPEAT_SAFE_WHEEL_ACTIONS)[number];

export interface WheelActions {
  scrollUp: RepeatSafeWheelAction;
  scrollDown: RepeatSafeWheelAction;
}

export const DEFAULT_WHEEL_ACTIONS = {
  scrollUp: "gecko-show-previous-tab",
  scrollDown: "gecko-show-next-tab",
} as const satisfies WheelActions;

const repeatSafeWheelActionSet = new Set<string>(REPEAT_SAFE_WHEEL_ACTIONS);

export function isRepeatSafeWheelAction(
  action: unknown,
): action is RepeatSafeWheelAction {
  return typeof action === "string" && repeatSafeWheelActionSet.has(action);
}

export function normalizeWheelActions(value: unknown): WheelActions {
  const configured = value !== null && typeof value === "object"
    ? value as Record<string, unknown>
    : {};

  return {
    scrollUp: isRepeatSafeWheelAction(configured.scrollUp)
      ? configured.scrollUp
      : DEFAULT_WHEEL_ACTIONS.scrollUp,
    scrollDown: isRepeatSafeWheelAction(configured.scrollDown)
      ? configured.scrollDown
      : DEFAULT_WHEEL_ACTIONS.scrollDown,
  };
}
