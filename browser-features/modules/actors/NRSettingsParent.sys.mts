//TODO: make reject when the name is invalid
export class NRSettingsParent extends JSWindowActorParent {
  constructor() {
    super();
  }
  async receiveMessage(message: { name: string; data?: unknown }): Promise<unknown> {
    const data = message.data as Record<string, unknown> | undefined;
    switch (message.name) {
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
