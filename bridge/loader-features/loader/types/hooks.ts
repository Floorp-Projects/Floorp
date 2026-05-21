// SPDX-License-Identifier: MPL-2.0

/**
 * Hooks Types
 *
 * Type definitions for module load hooks.
 */

/** Reject function type */
export type RejectFn = (reason: unknown) => void;

/** Tuple: [promise, resolve, reject] for a single deferred */
export type DeferredTuple = [Promise<void>, () => void, RejectFn];
