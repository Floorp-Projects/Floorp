// SPDX-License-Identifier: MPL-2.0

/**
 * Hooks IO
 *
 * Side-effectful operations for module load state tracking.
 * Wraps the state map with the sentinel-based deferred promise pattern
 * from the original modules-hooks.ts.
 */

import { createDeferred } from "../ops/mod.ts";
import { getPromiseStateMap, isRejected, setRejected } from "../state/mod.ts";

const getOrCreate = (module: string) => {
  const map = getPromiseStateMap();
  if (!map.has(module)) {
    map.set(module, createDeferred());
  }
  return map.get(module)!;
};

/**
 * Returns a promise that resolves when the named module has loaded,
 * or rejects if the module was not found after init completed.
 */
export const onModuleLoaded = (module: string): Promise<void> => {
  if (getPromiseStateMap().has(module)) {
    return getPromiseStateMap().get(module)![0];
  }
  if (isRejected()) {
    return Promise.reject(new Error("Module Not Found"));
  }
  return getOrCreate(module)[0];
};

/**
 * Resolve or reject the deferred for the given module.
 * Internal to the loader subsystem.
 */
export const registerModuleLoadState = (
  module: string,
  isLoaded: boolean,
): void => {
  const pms = getOrCreate(module);
  if (isLoaded) {
    pms[1]();
  } else {
    pms[2](new Error(`Failed to load module: ${module}`));
  }
};

/**
 * Reject all still-pending deferreds and mark init as complete.
 * Called once at the end of initializeModules.
 */
export const rejectOtherLoadStates = async (): Promise<void> => {
  for (const [, pms] of getPromiseStateMap()) {
    const sentinel = {};
    if (sentinel === (await Promise.race([pms[0], sentinel]))) {
      pms[2](new Error("Module Not Found"));
    }
  }
  setRejected(true);
};
