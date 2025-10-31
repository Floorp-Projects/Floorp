//TODO: make reject when the name is invalid
export class NRSettingsParent extends JSWindowActorParent {
  constructor() {
    super();
  }
  receiveMessage(message) {
    const { Experiments } = ChromeUtils.importESModule(
      "resource://noraneko/modules/experiments/Experiments.sys.mjs",
    );
    switch (message.name) {
      case "getBoolPref": {
        if (
          Services.prefs.getPrefType(message.data.name) !=
            Services.prefs.PREF_BOOL
        ) {
          return null;
        }
        return Services.prefs.getBoolPref(message.data.name);
      }
      case "getIntPref": {
        if (
          Services.prefs.getPrefType(message.data.name) !=
            Services.prefs.PREF_INT
        ) {
          return null;
        }
        return Services.prefs.getIntPref(message.data.name);
      }
      case "getStringPref": {
        if (
          Services.prefs.getPrefType(message.data.name) !=
            Services.prefs.PREF_STRING
        ) {
          return null;
        }
        return Services.prefs.getStringPref(message.data.name);
      }
      case "setBoolPref": {
        Services.prefs.setBoolPref(
          message.data.name,
          message.data.prefValue,
        );
        break;
      }
      case "setIntPref": {
        Services.prefs.setIntPref(
          message.data.name,
          message.data.prefValue,
        );
        break;
      }
      case "setStringPref": {
        Services.prefs.setStringPref(
          message.data.name,
          message.data.prefValue,
        );
        break;
      }
      case "getActiveExperiments": {
        return Experiments.getActiveExperiments();
      }
      case "disableExperiment": {
        const { experimentId } = message.data;
        return Experiments.disableExperiment(experimentId);
      }
      case "enableExperiment": {
        const { experimentId } = message.data;
        return Experiments.enableExperiment(experimentId);
      }
      case "clearExperimentCache": {
        try {
          Experiments.clearCache();
          return { success: true };
        } catch (error) {
          return { success: false, error: String(error) };
        }
      }
    }
  }
}
