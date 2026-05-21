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
  prefs.setStringPref(
    "noraneko.features.enabled",
    JSON.stringify(allFeaturesKeys),
  );
};

export const getEnabledFeatures = (): ModulesKeys =>
  JSON.parse(
    Services.prefs.getStringPref("noraneko.features.enabled", "{}"),
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
