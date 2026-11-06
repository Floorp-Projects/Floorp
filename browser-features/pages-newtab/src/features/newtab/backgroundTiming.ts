// SPDX-License-Identifier: MPL-2.0
export const MAX_BACKGROUND_TIMER_DELAY = 2_147_483_647;

/** Null disables updates; zero means due. Preserve fractional second settings. */
export function nextBackgroundDelay(
  intervalSeconds: number,
  elapsedMilliseconds: number,
): number | null {
  if (!Number.isFinite(intervalSeconds) || intervalSeconds <= 0) return null;
  return Math.min(
    MAX_BACKGROUND_TIMER_DELAY,
    Math.max(0, intervalSeconds * 1000 - elapsedMilliseconds),
  );
}
