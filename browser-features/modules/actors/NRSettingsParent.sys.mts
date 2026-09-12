//TODO: make reject when the name is invalid
import type { ContextMenuCatalogSnapshot } from "#features-chrome/common/context-menu/types.ts";

interface ContextMenuCatalogServiceModule {
  ContextMenuCatalogService: {
    getSnapshot(): ContextMenuCatalogSnapshot;
    getRevision(): number;
  };
}

const { ContextMenuCatalogService } = ChromeUtils.importESModule(
  "resource://noraneko/modules/context-menu/ContextMenuCatalogService.sys.mjs",
) as ContextMenuCatalogServiceModule;

export class NRSettingsParent extends JSWindowActorParent {
  constructor() {
    super();
  }
  // deno-lint-ignore require-await
  async receiveMessage(
    message: { name: string; data?: unknown },
  ): Promise<unknown> {
    const data = message.data as Record<string, unknown> | undefined;
    switch (message.name) {
      case "getContextMenuCatalog":
        return ContextMenuCatalogService.getSnapshot();
      case "getContextMenuCatalogRevision":
        return ContextMenuCatalogService.getRevision();
      case "getBoolPref": {
        const name = data && typeof data.name === "string" ? data.name : null;
        if (!name) return null;
        if (Services.prefs.getPrefType(name) != Services.prefs.PREF_BOOL) {
          return null;
        }
        return Services.prefs.getBoolPref(name);
      }
      case "getIntPref": {
        const name = data && typeof data.name === "string" ? data.name : null;
        if (!name) return null;
        if (Services.prefs.getPrefType(name) != Services.prefs.PREF_INT) {
          return null;
        }
        return Services.prefs.getIntPref(name);
      }
      case "getStringPref": {
        const name = data && typeof data.name === "string" ? data.name : null;
        if (!name) return null;
        if (Services.prefs.getPrefType(name) != Services.prefs.PREF_STRING) {
          return null;
        }
        return Services.prefs.getStringPref(name);
      }
      case "getBoolPrefState": {
        const name = data && typeof data.name === "string" ? data.name : null;
        if (!name) throw new TypeError("Invalid boolean preference read");
        const currentType = Services.prefs.getPrefType(name);
        return {
          value: currentType === Services.prefs.PREF_BOOL
            ? Services.prefs.getBoolPref(name)
            : null,
          typeMismatch: currentType !== Services.prefs.PREF_INVALID &&
            currentType !== Services.prefs.PREF_BOOL,
        };
      }
      case "getStringPrefState": {
        const name = data && typeof data.name === "string" ? data.name : null;
        if (!name) throw new TypeError("Invalid string preference read");
        const currentType = Services.prefs.getPrefType(name);
        return {
          value: currentType === Services.prefs.PREF_STRING
            ? Services.prefs.getStringPref(name)
            : null,
          typeMismatch: currentType !== Services.prefs.PREF_INVALID &&
            currentType !== Services.prefs.PREF_STRING,
        };
      }
      case "setBoolPref": {
        {
          const name = data && typeof data.name === "string" ? data.name : null;
          const val = data && typeof data.prefValue === "boolean"
            ? data.prefValue
            : null;
          if (!name || val === null) return null;
          Services.prefs.setBoolPref(name, val);
        }
        break;
      }
      case "setIntPref": {
        {
          const name = data && typeof data.name === "string" ? data.name : null;
          const val = data && typeof data.prefValue === "number"
            ? data.prefValue
            : null;
          if (!name || val === null) return null;
          Services.prefs.setIntPref(name, val);
        }
        break;
      }
      case "setStringPref": {
        {
          const name = data && typeof data.name === "string" ? data.name : null;
          const val = data && typeof data.prefValue === "string"
            ? data.prefValue
            : null;
          if (!name || val === null) return null;
          Services.prefs.setStringPref(name, val);
        }
        break;
      }
      case "compareAndSetBoolPref": {
        const name = data && typeof data.name === "string" ? data.name : null;
        const expectedValue = data?.expectedValue;
        const prefValue = data?.prefValue;
        if (
          !name ||
          (expectedValue !== null && typeof expectedValue !== "boolean") ||
          typeof prefValue !== "boolean"
        ) {
          throw new TypeError("Invalid boolean preference comparison");
        }
        const currentType = Services.prefs.getPrefType(name);
        if (
          currentType !== Services.prefs.PREF_INVALID &&
          currentType !== Services.prefs.PREF_BOOL
        ) {
          return { updated: false, currentValue: null, typeMismatch: true };
        }
        const currentValue = currentType === Services.prefs.PREF_BOOL
          ? Services.prefs.getBoolPref(name)
          : null;
        if (currentValue !== expectedValue) {
          return { updated: false, currentValue };
        }
        Services.prefs.setBoolPref(name, prefValue);
        return { updated: true, currentValue: prefValue };
      }
      case "compareAndSetStringPref": {
        const name = data && typeof data.name === "string" ? data.name : null;
        const expectedValue = data?.expectedValue;
        const prefValue = data?.prefValue;
        if (
          !name ||
          (expectedValue !== null && typeof expectedValue !== "string") ||
          typeof prefValue !== "string"
        ) {
          throw new TypeError("Invalid string preference comparison");
        }
        const currentType = Services.prefs.getPrefType(name);
        if (
          currentType !== Services.prefs.PREF_INVALID &&
          currentType !== Services.prefs.PREF_STRING
        ) {
          return { updated: false, currentValue: null, typeMismatch: true };
        }
        const currentValue = currentType === Services.prefs.PREF_STRING
          ? Services.prefs.getStringPref(name)
          : null;
        if (currentValue !== expectedValue) {
          return { updated: false, currentValue };
        }
        Services.prefs.setStringPref(name, prefValue);
        return { updated: true, currentValue: prefValue };
      }
    }
  }
}
