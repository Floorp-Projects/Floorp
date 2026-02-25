import { createBirpc } from "birpc";
import type {
  NRSettingsParentFunctions,
  PrefGetParams,
  PrefSetParams,
} from "../common/defines.ts";

export class NRSettingsChild extends JSWindowActorChild {
  rpc: ReturnType<typeof createBirpc> | null = null;
  constructor() {
    super();
  }
  actorCreated() {
    console.debug("NRSettingsChild created!");
    const window = this.contentWindow;
    if (
      window?.location.port === "5183" ||
      window?.location.port === "5186" ||
      window?.location.port === "5187" ||
      window?.location.port === "5188" ||
      window?.location.port === "5190" ||
      window?.location.port === "5199" ||
      window?.location.href.startsWith("chrome://") ||
      window?.location.href.startsWith("about:")
    ) {
      console.debug("NRSettingsChild 5183 ! or Chrome Page!");
      Cu.exportFunction(this.NRSPing.bind(this), window, {
        defineAs: "NRSPing",
      });
      Cu.exportFunction(this.NRSettingsSend.bind(this), window, {
        defineAs: "NRSettingsSend",
      });
      Cu.exportFunction(
        this.NRSettingsRegisterReceiveCallback.bind(this),
        window,
        {
          defineAs: "NRSettingsRegisterReceiveCallback",
        },
      );
    }
  }
  NRSPing() {
    return true;
  }

  sendToPage: ((data: string) => void) | null = null;

  private formatRPCError(error: unknown): string {
    if (error instanceof Error && error.message) {
      return error.message;
    }
    if (typeof error === "string" && error.trim() !== "") {
      return error;
    }
    try {
      const json = JSON.stringify(error);
      if (json && json !== "{}" && json !== "[]") {
        return json;
      }
    } catch {
      // ignore
    }
    return "Unknown parent actor error";
  }

  private async sendQueryWithFallback<T extends object>(
    name: string,
    data: Record<string, unknown>,
    fallback: T,
  ): Promise<T & { error?: string }> {
    try {
      return (await this.sendQuery(name, data)) as T & { error?: string };
    } catch (error) {
      return {
        ...fallback,
        error: this.formatRPCError(error),
      };
    }
  }

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
        // Browser operations
        listTabs: async () => {
          return await this.sendQuery("listTabs", {});
        },
        createTab: async (url: string) => {
          return await this.sendQuery("createTab", { url });
        },
        navigateTab: async (tabId: string, url: string) => {
          return await this.sendQuery("navigateTab", { tabId, url });
        },
        closeTab: async (tabId: string) => {
          return await this.sendQuery("closeTab", { tabId });
        },
        searchWeb: async (query: string) => {
          return await this.sendQuery("searchWeb", { query });
        },
        getBrowserHistory: async (limit?: number) => {
          return await this.sendQuery("getBrowserHistory", { limit });
        },
        // Scraper operations - Content reading
        createScraperInstance: async () => {
          return await this.sendQueryWithFallback(
            "createScraperInstance",
            {},
            { success: false },
          );
        },
        destroyScraperInstance: async (id: string) => {
          return await this.sendQueryWithFallback(
            "destroyScraperInstance",
            { id },
            { success: false },
          );
        },
        scraperNavigate: async (id: string, url: string) => {
          return await this.sendQueryWithFallback(
            "scraperNavigate",
            { id, url },
            { success: false },
          );
        },
        scraperGetHtml: async (id: string) => {
          return await this.sendQueryWithFallback("scraperGetHtml", { id }, {
            html: "",
          });
        },
        scraperGetText: async (id: string) => {
          return await this.sendQueryWithFallback("scraperGetText", { id }, {
            text: "",
          });
        },
        scraperGetElementText: async (id: string, selector: string) => {
          return await this.sendQueryWithFallback(
            "scraperGetElementText",
            {
              id,
              selector,
            },
            { text: "" },
          );
        },
        scraperGetElements: async (id: string, selector: string) => {
          return await this.sendQueryWithFallback(
            "scraperGetElements",
            { id, selector },
            { elements: [] },
          );
        },
        scraperGetElementAttribute: async (
          id: string,
          selector: string,
          attribute: string,
        ) => {
          return await this.sendQueryWithFallback(
            "scraperGetElementAttribute",
            {
              id,
              selector,
              attribute,
            },
            { value: "" },
          );
        },
        // Scraper operations - Actions
        scraperClick: async (id: string, selector: string) => {
          return await this.sendQueryWithFallback(
            "scraperClick",
            { id, selector },
            { success: false },
          );
        },
        scraperFillForm: async (
          id: string,
          fields: Array<{ selector: string; value: string }>,
        ) => {
          return await this.sendQueryWithFallback(
            "scraperFillForm",
            { id, fields },
            { success: false },
          );
        },
        scraperClearInput: async (id: string, selector: string) => {
          return await this.sendQueryWithFallback(
            "scraperClearInput",
            { id, selector },
            { success: false },
          );
        },
        scraperSubmit: async (id: string, selector: string) => {
          return await this.sendQueryWithFallback(
            "scraperSubmit",
            { id, selector },
            { success: false },
          );
        },
        scraperWaitForElement: async (
          id: string,
          selector: string,
          timeout?: number,
        ) => {
          return await this.sendQueryWithFallback(
            "scraperWaitForElement",
            {
              id,
              selector,
              timeout,
            },
            { success: false },
          );
        },
        scraperExecuteScript: async (id: string, script: string) => {
          return await this.sendQueryWithFallback(
            "scraperExecuteScript",
            { id, script },
            { value: "" },
          );
        },
        // Scraper operations - Screenshot
        scraperTakeScreenshot: async (id: string, selector?: string) => {
          return await this.sendQueryWithFallback(
            "scraperTakeScreenshot",
            {
              id,
              selector,
            },
            { data: "" },
          );
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
    // No-op
  }
}
