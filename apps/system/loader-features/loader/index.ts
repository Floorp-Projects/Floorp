import "vite/modulepreload-polyfill";

import { initI18N } from "#i18n/chrome-features/config.ts";

import { MODULES, MODULES_KEYS } from "./modules.ts";
import {
  _registerModuleLoadState,
  _rejectOtherLoadStates,
} from "./modules-hooks.ts";

console.log("[noraneko] Initializing scripts...");

export default async function initScripts() {
  // Import required modules and initialize i18n
  ChromeUtils.importESModule("resource://noraneko/modules/BrowserGlue.sys.mjs");
  initI18N();
  console.debug(
    `[noraneko-buildid2]\nuuid: ${import.meta.env.__BUILDID2__}\ndate: ${
      new Date(
        Number.parseInt(
          import.meta.env.__BUILDID2__.slice(0, 13).replace("-", ""),
          16,
        ),
      ).toISOString()
    }`,
  );

  setPrefFeatures(MODULES_KEYS);

  // Get enabled features from preferences
  const enabled_features = JSON.parse(
    Services.prefs.getStringPref("noraneko.features.enabled", "{}"),
  ) as typeof MODULES_KEYS;

  // Load enabled modules
  const modules = await loadEnabledModules(enabled_features);

  // Initialize modules after session is ready
  await initializeModules(modules);
}

async function setPrefFeatures(all_features_keys: typeof MODULES_KEYS) {
  // Set up preferences for features
  const prefs = Services.prefs.getDefaultBranch(null as unknown as string);
  prefs.setStringPref(
    "noraneko.features.all",
    JSON.stringify(all_features_keys),
  );
  Services.prefs.lockPref("noraneko.features.all");

  prefs.setStringPref(
    "noraneko.features.enabled",
    JSON.stringify(all_features_keys),
  );
}

async function loadEnabledModules(enabled_features: typeof MODULES_KEYS) {
  const modules: Array<{ init?: typeof Function; name: string }> = [];

  const loadModulePromises = Object.entries(MODULES).flatMap(
    ([categoryKey, categoryValue]) =>
      Object.keys(categoryValue).map(async (moduleName) => {
        if (
          categoryKey in enabled_features &&
          enabled_features[
            categoryKey as keyof typeof enabled_features
          ].includes(moduleName)
        ) {
          try {
            const module = await categoryValue[moduleName]();
            modules.push(
              Object.assign(
                { name: moduleName },
                module as {
                  init?: typeof Function;
                  initBeforeSessionStoreInit?: typeof Function;
                },
              ),
            );
          } catch (e) {
            console.error(`[noraneko] Failed to load module ${moduleName}:`, e);
          }
        }
      }),
  );

  await Promise.all(loadModulePromises);
  return modules;
}

async function initializeModules(
  modules: Array<
    {
      init?: typeof Function;
      initBeforeSessionStoreInit?: typeof Function;
      name: string;
      default?: typeof Function;
    }
  >,
) {
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
        new module.default();
      }
      _registerModuleLoadState(module.name, true);
    } catch (e) {
      console.error(`[noraneko] Failed to init module ${module.name}:`, e);
      _registerModuleLoadState(module.name, false);
    }
  }
  _registerModuleLoadState("__init_all__", true);
  await _rejectOtherLoadStates();
}
