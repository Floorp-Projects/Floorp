import { createBirpc } from "birpc";
import type {
  NRExperimemmtParentFunctions,
} from "../../../../modules/common/defines.ts";

// deno-lint-ignore no-explicit-any
declare const ChromeUtils: any;

declare global {
  interface Window {
    NRExperimemmtSend: (data: string) => void;
    NRExperimemmtRegisterReceiveCallback: (
      callback: (data: string) => void,
    ) => void;
  }
}

const hasBridge =
  typeof window !== "undefined" &&
  typeof window.NRExperimemmtSend === "function" &&
  typeof window.NRExperimemmtRegisterReceiveCallback === "function";

const getExperimentsModule = async () => {
  // We lazily import to avoid unnecessary module loading when the actor bridge is available.
  return ChromeUtils.importESModule(
    "resource://noraneko/modules/experiments/Experiments.sys.mjs",
  );
};

const directExperimentsFunctions: NRExperimemmtParentFunctions = {
  async getActiveExperiments() {
    const { Experiments } = await getExperimentsModule();
    return Experiments.getActiveExperiments();
  },
  async disableExperiment(experimentId) {
    const { Experiments } = await getExperimentsModule();
    if (typeof experimentId !== "string" || !experimentId) {
      return { success: false, error: "Invalid experimentId" };
    }
    return Experiments.disableExperiment(experimentId);
  },
  async enableExperiment(experimentId) {
    const { Experiments } = await getExperimentsModule();
    if (typeof experimentId !== "string" || !experimentId) {
      return { success: false, error: "Invalid experimentId" };
    }
    return Experiments.enableExperiment(experimentId);
  },
  async clearExperimentCache() {
    const { Experiments } = await getExperimentsModule();
    try {
      Experiments.clearCache();
      return { success: true };
    } catch (error) {
      return { success: false, error: String(error) };
    }
  },
  async reinitializeExperiments() {
    const { Experiments } = await getExperimentsModule();
    try {
      await Experiments.init();
      return { success: true };
    } catch (error) {
      return { success: false, error: String(error) };
    }
  },
};

export const experimentsRpc = hasBridge
  ? createBirpc<NRExperimemmtParentFunctions, Record<string, never>>(
    {},
    {
      post: (data) => window.NRExperimemmtSend(data),
      on: (callback) => {
        window.NRExperimemmtRegisterReceiveCallback(callback);
      },
      serialize: (value) => JSON.stringify(value),
      deserialize: (value) => JSON.parse(value),
    },
  )
  : directExperimentsFunctions;


