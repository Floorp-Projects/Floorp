// SPDX-License-Identifier: MPL-2.0


const ovrride_modules = import.meta.glob("./modules/*/index.ts");
const modules = {
  override: {} as Record<string, () => Promise<unknown>>,
};

Object.entries(ovrride_modules).map((v) => {
  modules.override[v[0].replace("./modules/", "").replace("/index.ts", "")] =
    v[1] as () => Promise<unknown>;
});

export class Overrides {
  private static instance: Overrides;
  public static getInstance() {
    if (!Overrides.instance) {
      Overrides.instance = new Overrides();
    }
    return Overrides.instance;
  }

  constructor() {
    this.loadOverrides();
  }

  get loadedModules() {
    return JSON.parse(
      Services.prefs.getStringPref("noraneko.features.enabled", "{common: []}"),
    ).common as string[];
  }

  private loadOverrides() {
    for (const modulePath of this.loadedModules) {
      const moduleName = modulePath.replace("./", "").replace("/index.ts", "");
      if (modules.override[moduleName]) {
        (async () => {
          (
            (await modules.override[moduleName]()) as {
              overrides?: (typeof Function)[];
            }
          ).overrides?.forEach((override) => override());
        })();
      }
    }
  }
}
