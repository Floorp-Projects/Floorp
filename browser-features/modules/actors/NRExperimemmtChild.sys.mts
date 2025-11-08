import { createBirpc } from "birpc";
import type { NRExperimemmtParentFunctions } from "../common/defines.ts";

export class NRExperimemmtChild extends JSWindowActorChild {
  rpc: ReturnType<typeof createBirpc> | null = null;

  constructor() {
    super();
  }

  actorCreated() {
    console.debug("NRExperimemmtChild created!");
    const window = this.contentWindow;
    if (
      window?.location.port === "5183" ||
      window?.location.port === "5186" ||
      window?.location.port === "5187" ||
      window?.location.port === "5188" ||
      window?.location.href.startsWith("chrome://") ||
      window?.location.href.startsWith("about:")
    ) {
      console.debug("NRExperimemmtChild 5183 ! or Chrome Page!");
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
        disableExperiment: (experimentId: string) => {
          return this.sendQuery("disableExperiment", { experimentId });
        },
        enableExperiment: (experimentId: string) => {
          return this.sendQuery("enableExperiment", { experimentId });
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
}


