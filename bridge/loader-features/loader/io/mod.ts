// SPDX-License-Identifier: MPL-2.0

/**
 * IO Module
 *
 * Re-exports all side-effectful operations.
 */

export {
  onModuleLoaded,
  registerModuleLoadState,
  rejectOtherLoadStates,
} from "./hooks.ts";

export {
  setPrefFeatures,
  getEnabledFeatures,
  loadEnabledModules,
} from "./loading.ts";

export { initializeModules } from "./lifecycle.ts";
