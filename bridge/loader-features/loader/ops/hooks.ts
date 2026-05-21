// SPDX-License-Identifier: MPL-2.0

/**
 * Hooks Operations
 *
 * Pure function for creating a deferred promise tuple.
 * No side effects.
 */

import type { DeferredTuple } from "../types/mod.ts";

/**
 * Create a deferred promise tuple [promise, resolve, reject].
 * Pure function — creates a new triple each call.
 */
export const createDeferred = (): DeferredTuple => {
  let rs: (() => void) | null = null;
  let rj: ((reason: unknown) => void) | null = null;
  const p = new Promise<void>((resolve, reject) => {
    rs = resolve;
    rj = reject;
  });
  return [p, rs!, rj!];
};
