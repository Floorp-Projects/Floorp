import { getFeaturesCommonEntries } from "#features-chrome/common/mod.ts";
import { getFeaturesStaticEntries } from "#features-chrome/static/mod.ts";

export const MODULES = {
  common: {} as Record<string, () => Promise<unknown>>,
  static: {} as Record<string, () => Promise<unknown>>,
};

{
  const MODULES_COMMON = getFeaturesCommonEntries();

  Object.entries(MODULES_COMMON).map((v) => {
    MODULES.common[v[0]] = v[1] as () => Promise<unknown>;
  });
}

{
  MODULES.static = getFeaturesStaticEntries();
}

export const MODULES_KEYS = {
  common: Object.keys(MODULES.common),
  static: Object.keys(MODULES.static),
};
