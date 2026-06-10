// SPDX-License-Identifier: MPL-2.0

// Shared type for modules loaded by the loader.
// Used as the common contract between loadEnabledModules and initializeModules.
interface LoaderModule {
  name: string;
  init?: () => Promise<void>;
  initBeforeSessionStoreInit?: () => Promise<void>;
  default?: new () => unknown;
}

import { initI18NForBrowserChrome } from "#i18n/config-browser-chrome.ts";

import { MODULES, MODULES_KEYS } from "./modules.ts";
import {
  _registerModuleLoadState,
  _rejectOtherLoadStates,
} from "./modules-hooks.ts";

console.log("[noraneko] Initializing scripts...");

export default async function initScripts() {
  console.log("[noraneko] initScripts called");
  try {
    Services.prefs.setStringPref("nora.loader.initialized", String(Date.now()));
  } catch (e) {
    console.error("[noraneko] Failed to set loader initialized marker:", e);
  }

  // Import required modules and initialize i18n
  ChromeUtils.importESModule("resource://noraneko/modules/BrowserGlue.sys.mjs");
  const { NoranekoConstants } = ChromeUtils.importESModule(
    "resource://noraneko/modules/NoranekoConstants.sys.mjs",
  );
  initI18NForBrowserChrome();
  console.debug(
    `[noraneko-buildid2]\nuuid: ${NoranekoConstants.buildID2}\ndate: ${new Date(
      Number.parseInt(
        NoranekoConstants.buildID2.slice(0, 13).replace("-", ""),
        16,
      ),
    ).toISOString()}`,
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

function setPrefFeatures(all_features_keys: typeof MODULES_KEYS) {
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

async function loadEnabledModules(
  enabled_features: typeof MODULES_KEYS,
): Promise<LoaderModule[]> {
  const promises = Object.entries(MODULES).flatMap(
    ([categoryKey, categoryValue]) =>
      Object.keys(categoryValue)
        .filter(
          (moduleName) =>
            categoryKey in enabled_features &&
            enabled_features[
              categoryKey as keyof typeof enabled_features
            ].includes(moduleName),
        )
        .map(async (moduleName): Promise<LoaderModule | null> => {
          try {
            const module = await categoryValue[moduleName]();
            return {
              name: moduleName,
              ...(module as Omit<LoaderModule, "name">),
            };
          } catch (e) {
            console.error(`[noraneko] Failed to load module ${moduleName}:`, e);
            return null;
          }
        }),
  );

  const results = await Promise.all(promises);
  return results.filter((m): m is LoaderModule => m !== null);
}

async function initializeModules(modules: LoaderModule[]) {
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
