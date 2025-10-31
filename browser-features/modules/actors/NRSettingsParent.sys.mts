//TODO: make reject when the name is invalid
export class NRSettingsParent extends JSWindowActorParent {
  constructor() {
    super();
  }
  receiveMessage(message: { name: string; data?: unknown }): unknown {
    const { Experiments } = ChromeUtils.importESModule(
      "resource://noraneko/modules/experiments/Experiments.sys.mjs",
    );
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
      case "getActiveExperiments": {
        return Experiments.getActiveExperiments();
      }
      case "disableExperiment": {
        const experimentId =
          data && typeof data.experimentId === "string"
            ? data.experimentId
            : null;
        if (!experimentId)
          return { success: false, error: "Invalid experimentId" };
        return Experiments.disableExperiment(experimentId);
      }
      case "enableExperiment": {
        const experimentId =
          data && typeof data.experimentId === "string"
            ? data.experimentId
            : null;
        if (!experimentId)
          return { success: false, error: "Invalid experimentId" };
        return Experiments.enableExperiment(experimentId);
      }
      case "clearExperimentCache": {
        const res = Experiments.clearCache();
        if (res && res.success) {
          return { success: true };
        }
        const errorMsg =
          res && res.errors
            ? res.errors.join(", ")
            : "Failed to clear experiments cache";
        return { success: false, error: String(errorMsg) };
      }
    }
  }
}
