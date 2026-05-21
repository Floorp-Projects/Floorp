// SPDX-License-Identifier: MPL-2.0

/**
 * Module Loader - Data-Oriented Programming Style
 *
 * Main entry point for the module loader system.
 * Provides the public API and re-exports for external consumers.
 */

import { initI18NForBrowserChrome } from "#i18n/config-browser-chrome.ts";
import { MODULES_KEYS } from "./data/mod.ts";
import {
  setPrefFeatures,
  getEnabledFeatures,
  loadEnabledModules,
  initializeModules,
} from "./io/mod.ts";

console.log("[noraneko] Initializing scripts...");

// ============================================================================
// Public API
// ============================================================================

export default async function initScripts(): Promise<void> {
  console.log("[noraneko] initScripts called");
  try {
    Services.prefs.setStringPref("nora.loader.initialized", String(Date.now()));
  } catch (e) {
    console.error("[noraneko] Failed to set loader initialized marker:", e);
  }

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

  const enabledFeatures = getEnabledFeatures();
  const modules = await loadEnabledModules(enabledFeatures);
  await initializeModules(modules);
}

// ============================================================================
// Re-exports — Public API
// ============================================================================

// Types
export type {
  LoaderModule,
  ModulesRegistry,
  ModulesKeys,
} from "./types/mod.ts";

// Data
export { MODULES, MODULES_KEYS } from "./data/mod.ts";

// Hooks (public surface — the `_`-prefixed names are kept in index.ts for compat)
export { onModuleLoaded } from "./io/mod.ts";
