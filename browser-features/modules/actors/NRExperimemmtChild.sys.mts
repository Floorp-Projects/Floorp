import { createBirpc } from "birpc";
import type { NRExperimemmtParentFunctions } from "../common/defines.ts";
import { HUB_DEV_PAGES } from "#libs/shared/dev-ports/defines.ts";
import { launchDevPorts } from "../common/dev-port-offset.ts";

/** Launch-local dev port resolver (FDH Gate 2 task 2-B): resolves the
 * table names to the ports this launch actually uses (offset-aware). */
const launchDevPort = launchDevPorts();

export class NRExperimemmtChild extends JSWindowActorChild {
  rpc: ReturnType<typeof createBirpc> | null = null;

  constructor() {
    super();
  }

  actorCreated() {
    console.debug("NRExperimemmtChild created!");
    const window = this.contentWindow;
    if (
      launchDevPort.isTrustedPort(window?.location.port, HUB_DEV_PAGES) ||
      window?.location.href.startsWith("chrome://") ||
      window?.location.href.startsWith("about:")
    ) {
      console.debug(
        `NRExperimemmtChild ${
          launchDevPort.portString("settings")
        } ! or Chrome Page!`,
      );
      Cu.exportFunction(this.NRExperimemmtPing.bind(this), window, {
        defineAs: "NRExperimemmtPing",
      });
      Cu.exportFunction(this.NRExperimemmtSend.bind(this), window, {
        defineAs: "NRExperimemmtSend",
      });
      Cu.exportFunction(
        this.NRExperimemmtRegisterReceiveCallback.bind(this),
        window,
        {
          defineAs: "NRExperimemmtRegisterReceiveCallback",
        },
      );
    }
  }

  NRExperimemmtPing() {
    return true;
  }

  sendToPage: ((data: string) => void) | null = null;

  NRExperimemmtSend(data: string) {
    if (this.sendToPage) {
      this.sendToPage(data);
    }
  }

  NRExperimemmtRegisterReceiveCallback(callback: (data: string) => void) {
    this.rpc = createBirpc<
      Record<PropertyKey, never>,
      NRExperimemmtParentFunctions
    >(
      {
        getActiveExperiments: () => {
          return this.sendQuery("getActiveExperiments", {});
        },
        getAllExperiments: () => {
          return this.sendQuery("getAllExperiments", {});
        },
        disableExperiment: (experimentId: string) => {
          return this.sendQuery("disableExperiment", { experimentId });
        },
        enableExperiment: (experimentId: string) => {
          return this.sendQuery("enableExperiment", { experimentId });
        },
        forceEnrollExperiment: (experimentId: string) => {
          return this.sendQuery("forceEnrollExperiment", { experimentId });
        },
        removeForceEnrollment: (experimentId: string) => {
          return this.sendQuery("removeForceEnrollment", { experimentId });
        },
        clearExperimentCache: () => {
          return this.sendQuery("clearExperimentCache", {});
        },
        reinitializeExperiments: () => {
          return this.sendQuery("reinitializeExperiments", {});
        },
      },
      {
        post: (data) => callback(data),
        on: (postback) => {
          this.sendToPage = postback;
        },
        serialize: (value) => JSON.stringify(value),
        deserialize: (value) => JSON.parse(value),
      },
    );
  }
  handleEvent(_event: Event): void {
    // No-op
  }
}
