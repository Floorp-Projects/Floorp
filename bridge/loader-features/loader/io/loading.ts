// SPDX-License-Identifier: MPL-2.0

/**
 * Loading IO
 *
 * Side-effectful operations for preference management and dynamic module loading.
 */

import { MODULES } from "../data/mod.ts";
import type { LoaderModule, ModulesKeys } from "../types/mod.ts";

// ============================================================================
// Preferences
// ============================================================================

export const setPrefFeatures = (allFeaturesKeys: ModulesKeys): void => {
  const prefs = Services.prefs.getDefaultBranch(null as unknown as string);
  prefs.setStringPref(
    "noraneko.features.all",
    JSON.stringify(allFeaturesKeys),
  );
  Services.prefs.lockPref("noraneko.features.all");
  // Only write the enabled-features default when the user has not set a custom
  // value. This allows individual modules to be opt-out via user preferences.
  if (!Services.prefs.prefHasUserValue("noraneko.features.enabled")) {
    prefs.setStringPref(
      "noraneko.features.enabled",
      JSON.stringify(allFeaturesKeys),
    );
  }
};

const EMPTY_MODULES_KEYS: ModulesKeys = { common: [], static: [] };

export const getEnabledFeatures = (): ModulesKeys =>
  JSON.parse(
    Services.prefs.getStringPref(
      "noraneko.features.enabled",
      JSON.stringify(EMPTY_MODULES_KEYS),
    ),
  ) as ModulesKeys;

// ============================================================================
// Module Loading
// ============================================================================

export const loadEnabledModules = async (
  enabledFeatures: ModulesKeys,
): Promise<LoaderModule[]> => {
  const promises = Object.entries(MODULES).flatMap(
    ([categoryKey, categoryValue]) =>
      Object.keys(categoryValue)
        .filter(
          (moduleName) =>
            categoryKey in enabledFeatures &&
            enabledFeatures[
              categoryKey as keyof typeof enabledFeatures
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
};
