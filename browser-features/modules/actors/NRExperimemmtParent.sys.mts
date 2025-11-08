export class NRExperimemmtParent extends JSWindowActorParent {
  constructor() {
    super();
  }

  async receiveMessage(message: {
    name: string;
    data?: unknown;
  }): Promise<unknown> {
    const { Experiments } = ChromeUtils.importESModule(
      "resource://noraneko/modules/experiments/Experiments.sys.mjs",
    );
    const data = message.data as Record<string, unknown> | undefined;

    switch (message.name) {
      case "getActiveExperiments": {
        return Experiments.getActiveExperiments();
      }
      case "disableExperiment": {
        const experimentId =
          data && typeof data.experimentId === "string"
            ? data.experimentId
            : null;
        if (!experimentId) {
          return { success: false, error: "Invalid experimentId" };
        }
        return Experiments.disableExperiment(experimentId);
      }
      case "enableExperiment": {
        const experimentId =
          data && typeof data.experimentId === "string"
            ? data.experimentId
            : null;
        if (!experimentId) {
          return { success: false, error: "Invalid experimentId" };
        }
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
      case "reinitializeExperiments": {
        try {
          await Experiments.init();
          return { success: true };
        } catch (error) {
          return { success: false, error: String(error) };
        }
      }
    }

    return undefined;
  }
}


