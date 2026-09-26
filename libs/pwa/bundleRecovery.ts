// SPDX-License-Identifier: MPL-2.0

export type BundleRecoveryState =
  | "staged"
  | "ready-to-swap"
  | "swapped"
  | "committed"
  | "rolled-back"
  | "conflict";

/** Inspect disk identity, not the last journal write, after an interrupted swap. */
export function classifyBundleRecovery(
  live: string | null,
  stage: string | null,
  backup: string | null,
  previous: string,
  candidate: string,
): BundleRecoveryState {
  if (
    !/^[a-f0-9]{64}$/.test(previous) || !/^[a-f0-9]{64}$/.test(candidate) ||
    previous === candidate
  ) return "conflict";
  if (live === previous && stage === candidate && backup === null) {
    return "staged";
  }
  if (live === previous && stage === null && backup === candidate) {
    return "ready-to-swap";
  }
  if (live === candidate && stage === null && backup === previous) {
    return "swapped";
  }
  if (live === candidate && stage === null && backup === null) {
    return "committed";
  }
  if (live === previous && stage === null && backup === null) {
    return "rolled-back";
  }
  return "conflict";
}
