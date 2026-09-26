// Open a URL in a new tab from the browser window. A plain <a target="_blank">
// click inside a system-principal about page (about:hub) opens about:blank, so
// the page routes external links here instead. See Floorp issue #2787.
// deno-lint-ignore no-explicit-any
function openExternalLinkInBrowser(url: string): boolean {
  try {
    const win = Services.wm.getMostRecentWindow("navigator:browser") as any;
    if (!win || typeof win.openTrustedLinkIn !== "function") {
      return false;
    }
    win.openTrustedLinkIn(url, "tab", {
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
export class NRSettingsParent extends JSWindowActorParent {
  constructor() {
    super();
  }
  // deno-lint-ignore require-await
  async receiveMessage(message: { name: string; data?: unknown }): Promise<unknown> {
    const data = message.data as Record<string, unknown> | undefined;
    switch (message.name) {
      case "openExternalLink": {
        const url = data && typeof data.url === "string" ? data.url : null;
        if (url) {
          openExternalLinkInBrowser(url);
        }
        break;
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
      case "setBoolPref": {
        {
          const name = data && typeof data.name === "string" ? data.name : null;
          const val =
            data && typeof data.prefValue === "boolean" ? data.prefValue : null;
          if (!name || val === null) return null;
          Services.prefs.setBoolPref(name, val);
        }
        break;
      }
      case "setIntPref": {
        {
          const name = data && typeof data.name === "string" ? data.name : null;
          const val =
            data && typeof data.prefValue === "number" ? data.prefValue : null;
          if (!name || val === null) return null;
          Services.prefs.setIntPref(name, val);
        }
        break;
      }
      case "setStringPref": {
        {
          const name = data && typeof data.name === "string" ? data.name : null;
          const val =
            data && typeof data.prefValue === "string" ? data.prefValue : null;
          if (!name || val === null) return null;
          Services.prefs.setStringPref(name, val);
        }
        break;
      }
    }
  }
}
