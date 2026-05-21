// SPDX-License-Identifier: MPL-2.0

/**
 * Lifecycle IO
 *
 * Side-effectful operations for module initialization.
 * Orchestrates initBeforeSessionStoreInit → SessionStore wait → init/default.
 */

import type { LoaderModule } from "../types/mod.ts";
import { registerModuleLoadState, rejectOtherLoadStates } from "./hooks.ts";

export const initializeModules = async (
  modules: LoaderModule[],
): Promise<void> => {
  for (const module of modules) {
    try {
      await module?.initBeforeSessionStoreInit?.();
    } catch (e) {
      console.error(
        `[noraneko] Failed to initBeforeSessionStoreInit module ${module.name}:`,
        e,
      );
    }
  }

  // @ts-expect-error SessionStore type not defined
  await SessionStore.promiseInitialized;

  for (const module of modules) {
    try {
      if (module?.init) {
        await module.init();
      }
      if (module?.default) {
        // module.default is expected to be a synchronous constructor.
        // Async initialization should use the init() lifecycle hook above.
        new module.default();
      }
      registerModuleLoadState(module.name, true);
    } catch (e) {
      console.error(`[noraneko] Failed to init module ${module.name}:`, e);
      registerModuleLoadState(module.name, false);
    }
  }

  registerModuleLoadState("__init_all__", true);
  await rejectOtherLoadStates();
};
