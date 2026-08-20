import { createBirpc } from "birpc";
import type {
  NRSettingsParentFunctions,
  PrefGetParams,
  PrefSetParams,
} from "../common/defines.ts";

export class NRSettingsChild extends JSWindowActorChild {
  private static readonly MAX_INSTALL_ATTEMPTS = 200;
  private static readonly INSTALL_RETRY_DELAY_MS = 50;

  rpc: ReturnType<typeof createBirpc> | null = null;
  constructor() {
    super();
  }

  private installPageApi(): boolean {
    const document = this.document;
    const window = this.contentWindow;
    if (
      !document ||
      !window ||
      !(
        document.location.port === "5183" ||
        document.location.port === "5186" ||
        document.location.port === "5187" ||
        document.location.port === "5188" ||
        document.location.href.startsWith("chrome://noraneko-settings/")
      )
    ) {
      return false;
    }

    const page = window as unknown as Record<string, unknown>;
    if (typeof page.NRSPing !== "function") {
      Cu.exportFunction(this.NRSPing.bind(this), window, {
        defineAs: "NRSPing",
      });
    }
    if (typeof page.NRSettingsSend !== "function") {
      Cu.exportFunction(this.NRSettingsSend.bind(this), window, {
        defineAs: "NRSettingsSend",
      });
    }
    if (typeof page.NRSettingsRegisterReceiveCallback !== "function") {
      Cu.exportFunction(
        this.NRSettingsRegisterReceiveCallback.bind(this),
        window,
        {
          defineAs: "NRSettingsRegisterReceiveCallback",
        },
      );
    }
    return true;
  }

  private retryInstallPageApi(attempt = 0): void {
    if (
      this.installPageApi() ||
      attempt >= NRSettingsChild.MAX_INSTALL_ATTEMPTS
    ) {
      return;
    }
    this.contentWindow?.setTimeout(
      () => this.retryInstallPageApi(attempt + 1),
      NRSettingsChild.INSTALL_RETRY_DELAY_MS,
    );
  }

  actorCreated() {
    console.debug("NRSettingsChild created!");
    this.retryInstallPageApi();
  }
  NRSPing() {
    return true;
  }

  sendToPage: ((data: string) => void) | null = null;

  NRSettingsSend(data: string) {
    if (this.sendToPage) {
      this.sendToPage(data);
    }
  }

  NRSettingsRegisterReceiveCallback(callback: (data: string) => void) {
    this.rpc = createBirpc<
      Record<PropertyKey, never>,
      NRSettingsParentFunctions
    >(
      {
        getBoolPref: (prefName: string): Promise<boolean | null> => {
          return this.NRSPrefGet({ prefName, prefType: "boolean" });
        },
        getIntPref: (prefName: string): Promise<number | null> => {
          return this.NRSPrefGet({ prefName, prefType: "number" });
        },
        getStringPref: (prefName: string): Promise<string | null> => {
          return this.NRSPrefGet({ prefName, prefType: "string" });
        },
        setBoolPref: (prefName: string, prefValue: boolean): Promise<void> => {
          return this.NRSPrefSet({ prefName, prefValue, prefType: "boolean" });
        },
        setIntPref: (prefName: string, prefValue: number): Promise<void> => {
          return this.NRSPrefSet({ prefName, prefValue, prefType: "number" });
        },
        setStringPref: (prefName: string, prefValue: string): Promise<void> => {
          return this.NRSPrefSet({ prefName, prefValue, prefType: "string" });
        },
      },
      {
        post: (data) => callback(data),
        on: (callback) => {
          this.sendToPage = callback;
        },
        // these are required when using WebSocket
        serialize: (v) => JSON.stringify(v),
        deserialize: (v) => JSON.parse(v),
      },
    );
  }

  async NRSPrefGet(params: {
    prefName: string;
    prefType: "boolean";
  }): Promise<boolean | null>;
  async NRSPrefGet(params: {
    prefName: string;
    prefType: "number";
  }): Promise<number | null>;
  async NRSPrefGet(params: {
    prefName: string;
    prefType: "string";
  }): Promise<string | null>;
  async NRSPrefGet(
    params: PrefGetParams,
  ): Promise<boolean | number | string | null> {
    try {
      let funcName;
      switch (params.prefType) {
        case "boolean":
          funcName = "getBoolPref";
          break;
        case "number":
          funcName = "getIntPref";
          break;
        case "string":
          funcName = "getStringPref";
          break;
        default:
          throw new Error("Invalid pref type");
      }
      return await this.sendQuery(funcName, {
        name: params.prefName,
      });
    } catch (error) {
      console.error("Error in NRSPrefGet:", error);
      return null;
    }
  }

  async NRSPrefSet(params: PrefSetParams) {
    try {
      let funcName;
      switch (params.prefType) {
        case "boolean":
          funcName = "setBoolPref";
          break;
        case "number":
          funcName = "setIntPref";
          break;
        case "string":
          funcName = "setStringPref";
          break;
        default:
          throw new Error("Invalid pref type");
      }
      return await this.sendQuery(funcName, {
        name: params.prefName,
        prefValue: params.prefValue,
      });
    } catch (error) {
      console.error("Error in NRSPrefSet:", error);
      return null;
    }
  }
  handleEvent(_event: Event): void {
    // actorCreated can run before the final document URL is available. Retry
    // after the document insertion/DOMContentLoaded events so HTTP-loaded
    // settings pages always receive their RPC bridge.
    this.retryInstallPageApi();
  }
}
