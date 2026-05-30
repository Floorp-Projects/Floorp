// SPDX-License-Identifier: MPL-2.0

/**
 * Hooks State
 *
 * Module-level state for module load promises.
 * State is isolated here; mutated only through the accessor functions below.
 */

import type { DeferredTuple } from "../types/mod.ts";

/** Map of module name -> deferred tuple [promise, resolve, reject] */
const _mapPromiseModuleState: Map<string, DeferredTuple> = new Map();

/** Flag: init has completed; late callers for unknown modules get an instant reject */
let _rejected = false;

export const getPromiseStateMap = (): Map<string, DeferredTuple> =>
  _mapPromiseModuleState;

export const isRejected = (): boolean => _rejected;

export const setRejected = (value: boolean): void => {
  _rejected = value;
};
