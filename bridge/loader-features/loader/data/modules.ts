// SPDX-License-Identifier: MPL-2.0

/**
 * Module Data
 *
 * Module registry — populated at module initialization time from feature entries.
 */

import { getFeaturesCommonEntries } from "#features-chrome/common/mod.ts";
import { getFeaturesStaticEntries } from "#features-chrome/static/mod.ts";
import type { ModulesRegistry, ModulesKeys } from "../types/mod.ts";

export const MODULES: ModulesRegistry = {
  common: {} as Record<string, () => Promise<unknown>>,
  static: {} as Record<string, () => Promise<unknown>>,
};

{
  const MODULES_COMMON = getFeaturesCommonEntries();
  Object.entries(MODULES_COMMON).forEach((v) => {
    MODULES.common[v[0]] = v[1] as () => Promise<unknown>;
  });
}

{
  MODULES.static = getFeaturesStaticEntries();
}

export const MODULES_KEYS: ModulesKeys = {
  common: Object.keys(MODULES.common),
  static: Object.keys(MODULES.static),
};
