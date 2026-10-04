// Open a URL in a new tab from the browser window. A plain <a target="_blank">
// click inside a system-principal about page (about:hub) opens about:blank, so
// the page routes external links here instead. See Floorp issue #2787.
function openExternalLinkInBrowser(
  actor: NRSettingsParent,
  url: string,
): boolean {
  try {
    const uri = Services.io.newURI(url);
    if (!uri.schemeIs("http") && !uri.schemeIs("https")) {
      return false;
    }
    const browser = actor.browsingContext?.top?.embedderElement;
    const win = browser?.ownerGlobal as
      | (Window & {
        openTrustedLinkIn?: (
          url: string,
          where: string,
          options: {
            triggeringPrincipal: unknown;
            relatedToCurrent: boolean;
            allowInheritPrincipal: boolean;
          },
        ) => void;
      })
      | undefined;
    if (!win || typeof win.openTrustedLinkIn !== "function") {
      return false;
    }
    win.openTrustedLinkIn(uri.spec, "tab", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      relatedToCurrent: true,
      // Never let the new content tab inherit the system principal.
      allowInheritPrincipal: false,
    });
    return true;
  } catch (error) {
    console.error("[noraneko] openExternalLinkInBrowser failed", error);
    return false;
  }
}

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
      case "openExternalLink": {
        const url = data && typeof data.url === "string" ? data.url : null;
        const context = this.browsingContext;
        const manager = this.manager;
        const uri = manager?.documentURI;
        const isSettingsPage = !!uri && (
          (uri.schemeIs("chrome") && uri.host === "noraneko-settings") ||
          uri.spec.split(/[?#]/)[0] === "about:hub" ||
          (uri.schemeIs("http") &&
            (uri.host === "localhost" || uri.host === "127.0.0.1") &&
            [5183, 5186, 5187, 5188].includes(uri.port))
        );
        if (
          url && isSettingsPage && context && !context.parent &&
          context.currentWindowGlobal === manager
        ) {
          openExternalLinkInBrowser(this, url);
        }
        break;
      }
      case "getWebAppLifecycleSettings": {
        const { NativeAppRuntime } = ChromeUtils.importESModule(
          "resource://noraneko/modules/pwa/NativeAppRuntime.sys.mjs",
        );
        const { AppLifecycle } = ChromeUtils.importESModule(
          "resource://noraneko/modules/pwa/AppLifecycle.sys.mjs",
        );
        NativeAppRuntime.isAvailable();
        return AppLifecycle.getSettings();
      }
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
