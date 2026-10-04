import { createBirpc } from "birpc";
import type { NRExperimemmtParentFunctions } from "../../../../modules/common/defines.ts";
import type { ExperimentsModule } from "../experiments/types.ts";

declare const ChromeUtils: { importESModule(uri: string): unknown };

export const EXPERIMENTS_CHANGED_EVENT = "floorp-experiments-changed";
export const EXPERIMENTS_POLICY_PREF = "floorp.experiments.participationPolicy";

export function notifyExperimentsChanged(): void {
  globalThis.dispatchEvent(new Event(EXPERIMENTS_CHANGED_EVENT));
}

export function withExperimentChangeNotifications(
  api: NRExperimemmtParentFunctions,
  notify: () => void = notifyExperimentsChanged,
): NRExperimemmtParentFunctions {
  const afterMutation = async (
    result: ReturnType<NRExperimemmtParentFunctions["disableExperiment"]>,
  ) => {
    const response = await result;
    if (response.success === true) notify();
    return response;
  };
  return {
    getActiveExperiments: () => api.getActiveExperiments(),
    getAllExperiments: () => api.getAllExperiments(),
    disableExperiment: (id) => afterMutation(api.disableExperiment(id)),
    enableExperiment: (id) => afterMutation(api.enableExperiment(id)),
    forceEnrollExperiment: (id) => afterMutation(api.forceEnrollExperiment(id)),
    removeForceEnrollment: (id) => afterMutation(api.removeForceEnrollment(id)),
    clearExperimentCache: () => afterMutation(api.clearExperimentCache()),
    reinitializeExperiments: () => afterMutation(api.reinitializeExperiments()),
  };
}

declare global {
  interface Window {
    NRExperimemmtSend: (data: string) => void;
    NRExperimemmtRegisterReceiveCallback: (
      callback: (data: string) => void,
    ) => void;
  }
}

const page = globalThis as unknown as Window;
const hasBridge = typeof window !== "undefined" &&
  typeof page.NRExperimemmtSend === "function" &&
  typeof page.NRExperimemmtRegisterReceiveCallback === "function";

const getExperimentsModule = () => {
  // We lazily import to avoid unnecessary module loading when the actor bridge is available.
  return ChromeUtils.importESModule(
    "resource://noraneko/modules/experiments/Experiments.sys.mjs",
  ) as ExperimentsModule;
};

const directExperimentsFunctions: NRExperimemmtParentFunctions = {
  async getActiveExperiments() {
    const { Experiments } = await getExperimentsModule();
    return Experiments.getActiveExperiments();
  },
  async getAllExperiments() {
    const { Experiments } = await getExperimentsModule();
    return Experiments.getAllExperiments();
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
  async forceEnrollExperiment(experimentId) {
    const { Experiments } = await getExperimentsModule();
    if (typeof experimentId !== "string" || !experimentId) {
      return { success: false, error: "Invalid experimentId" };
    }
    return Experiments.forceEnrollExperiment(experimentId);
  },
  async removeForceEnrollment(experimentId) {
    const { Experiments } = await getExperimentsModule();
    if (typeof experimentId !== "string" || !experimentId) {
      return { success: false, error: "Invalid experimentId" };
    }
    return Experiments.removeForceEnrollment(experimentId);
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

const experimentsTransport = hasBridge
  ? createBirpc<NRExperimemmtParentFunctions, Record<string, never>>(
    {},
    {
      post: (data) => page.NRExperimemmtSend(data),
      on: (callback) => {
        page.NRExperimemmtRegisterReceiveCallback(callback);
      },
      serialize: (value) => JSON.stringify(value),
      deserialize: (value) => JSON.parse(value),
    },
  )
  : directExperimentsFunctions;

export const experimentsRpc = withExperimentChangeNotifications(
  experimentsTransport,
);
