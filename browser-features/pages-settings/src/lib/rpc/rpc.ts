import type {
  NRContextMenuSettingsFunctions,
  NRSettingsAtomicPreferenceFunctions,
  NRSettingsParentFunctions,
  PrefCompareAndSetResult,
  PrefReadResult,
} from "../../../../modules/common/defines.ts";
import type { ContextMenuCatalogSnapshot } from "#features-chrome/common/context-menu/types.ts";
import { createBirpc } from "birpc";

type SettingsPageParentFunctions =
  & NRSettingsParentFunctions
  & NRSettingsAtomicPreferenceFunctions
  & NRContextMenuSettingsFunctions;

interface LegacySettingsDirectFunctions {
  selectFolder(): Promise<null>;
  getRandomImageFromFolder(path: string): Promise<null>;
  sendToNRPanelSidebarChild(
    method: string,
    ...args: unknown[]
  ): Promise<unknown>;
}

type SettingsPageDirectFunctions =
  & SettingsPageParentFunctions
  & LegacySettingsDirectFunctions;

interface ContextMenuCatalogServiceModule {
  ContextMenuCatalogService: {
    getSnapshot(): ContextMenuCatalogSnapshot;
  };
}

interface DirectPreferenceService {
  readonly PREF_INVALID: number;
  readonly PREF_BOOL: number;
  readonly PREF_INT: number;
  readonly PREF_STRING: number;
  getPrefType(prefName: string): number;
  getBoolPref(prefName: string): boolean;
  getIntPref(prefName: string): number;
  getStringPref(prefName: string): string;
  setBoolPref(prefName: string, value: boolean): void;
  setIntPref(prefName: string, value: number): void;
  setStringPref(prefName: string, value: string): void;
}

function compareAndSetDirectPreference<T extends boolean | string>(
  prefName: string,
  expectedValue: T | null,
  prefValue: T,
  prefType: number,
  read: () => T,
  write: (value: T) => void,
): Promise<PrefCompareAndSetResult<T>> {
  const currentType = Services.prefs.getPrefType(prefName);
  if (
    currentType !== Services.prefs.PREF_INVALID && currentType !== prefType
  ) {
    return Promise.resolve({
      updated: false,
      currentValue: null,
      typeMismatch: true,
    });
  }
  const currentValue = currentType === prefType ? read() : null;
  if (currentValue !== expectedValue) {
    return Promise.resolve({ updated: false, currentValue });
  }
  write(prefValue);
  return Promise.resolve({ updated: true, currentValue: prefValue });
}

function readDirectPreference<T extends boolean | string>(
  prefName: string,
  prefType: number,
  read: () => T,
): Promise<PrefReadResult<T>> {
  const currentType = Services.prefs.getPrefType(prefName);
  return Promise.resolve({
    value: currentType === prefType ? read() : null,
    typeMismatch: currentType !== Services.prefs.PREF_INVALID &&
      currentType !== prefType,
  });
}

interface DirectActor {
  [method: string]: (...args: unknown[]) => unknown;
}

declare const Services: { prefs: DirectPreferenceService };
declare const ChromeUtils: {
  importESModule(moduleUri: string): unknown;
};
declare const Cu: {
  getGlobalForObject(value: unknown): {
    browsingContext: {
      currentWindowGlobal: {
        getActor(name: string): DirectActor;
      };
    };
  };
};
declare global {
  interface Window {
    NRSettingsSend: (data: string) => void;
    NRSettingsRegisterReceiveCallback: (
      callback: (data: string) => void,
    ) => void;
  }
}

const SETTINGS_BRIDGE_TIMEOUT_MS = 15_000;
const SETTINGS_BRIDGE_POLL_INTERVAL_MS = 25;

function waitForSettingsBridge(): Promise<Window> {
  const startedAt = Date.now();
  return new Promise((resolve, reject) => {
    const poll = () => {
      const page = globalThis as unknown as Window;
      if (
        typeof page.NRSettingsSend === "function" &&
        typeof page.NRSettingsRegisterReceiveCallback === "function"
      ) {
        resolve(page);
        return;
      }
      if (Date.now() - startedAt >= SETTINGS_BRIDGE_TIMEOUT_MS) {
        reject(new Error("NRSettings page RPC bridge did not initialize"));
        return;
      }
      globalThis.setTimeout(poll, SETTINGS_BRIDGE_POLL_INTERVAL_MS);
    };
    poll();
  });
}

const isLocalhost5183 = /(?:localhost|127\.0\.0\.1):5183/.test(
  import.meta.url ?? "",
);

const directServicesFunctions: SettingsPageDirectFunctions = {
  getContextMenuCatalog: () => {
    const { ContextMenuCatalogService } = ChromeUtils.importESModule(
      "resource://noraneko/modules/context-menu/ContextMenuCatalogService.sys.mjs",
    ) as ContextMenuCatalogServiceModule;
    return Promise.resolve(ContextMenuCatalogService.getSnapshot());
  },
  getBoolPref: (prefName) => {
    if (Services.prefs.getPrefType(prefName) !== Services.prefs.PREF_BOOL) {
      return Promise.resolve(null);
    }
    return Promise.resolve(Services.prefs.getBoolPref(prefName));
  },
  getIntPref: (prefName) => {
    if (Services.prefs.getPrefType(prefName) !== Services.prefs.PREF_INT) {
      return Promise.resolve(null);
    }
    return Promise.resolve(Services.prefs.getIntPref(prefName));
  },
  getStringPref: (prefName) => {
    if (Services.prefs.getPrefType(prefName) !== Services.prefs.PREF_STRING) {
      return Promise.resolve(null);
    }
    return Promise.resolve(Services.prefs.getStringPref(prefName));
  },
  getBoolPrefState: (prefName) =>
    readDirectPreference(
      prefName,
      Services.prefs.PREF_BOOL,
      () => Services.prefs.getBoolPref(prefName),
    ),
  getStringPrefState: (prefName) =>
    readDirectPreference(
      prefName,
      Services.prefs.PREF_STRING,
      () => Services.prefs.getStringPref(prefName),
    ),
  setBoolPref: (prefName, value) => {
    Services.prefs.setBoolPref(prefName, value);
    return Promise.resolve();
  },
  setIntPref: (prefName, value) => {
    Services.prefs.setIntPref(prefName, value);
    return Promise.resolve();
  },
  setStringPref: (prefName, value) => {
    Services.prefs.setStringPref(prefName, value);
    return Promise.resolve();
  },
  compareAndSetBoolPref: (prefName, expectedValue, prefValue) =>
    compareAndSetDirectPreference(
      prefName,
      expectedValue,
      prefValue,
      Services.prefs.PREF_BOOL,
      () => Services.prefs.getBoolPref(prefName),
      (value) => Services.prefs.setBoolPref(prefName, value),
    ),
  compareAndSetStringPref: (prefName, expectedValue, prefValue) =>
    compareAndSetDirectPreference(
      prefName,
      expectedValue,
      prefValue,
      Services.prefs.PREF_STRING,
      () => Services.prefs.getStringPref(prefName),
      (value) => Services.prefs.setStringPref(prefName, value),
    ),
  // フォルダ選択関連のメソッド
  selectFolder: () => {
    return Promise.resolve(null);
  },
  getRandomImageFromFolder: (_path) => {
    return Promise.resolve(null);
  },
  // Actor通信用メソッド
  sendToNRPanelSidebarChild: async (method, ...args) => {
    try {
      // NRPanelSidebarParentアクターを取得
      const windowGlobal = Cu.getGlobalForObject(Services);
      const actor = windowGlobal.browsingContext.currentWindowGlobal.getActor(
        "NRPanelSidebar",
      );

      // メソッドを実行
      return await actor[method](...args);
    } catch (error) {
      console.error(`Error calling NRPanelSidebarChild.${method}:`, error);
      throw error;
    }
  },
};

export const rpc = isLocalhost5183
  ? createBirpc<SettingsPageParentFunctions, Record<string, never>>(
    {},
    {
      post: (data) => {
        void waitForSettingsBridge()
          .then((page) => page.NRSettingsSend(data))
          .catch((error) => console.error(error));
      },
      on: (callback) => {
        void waitForSettingsBridge()
          .then((page) => page.NRSettingsRegisterReceiveCallback(callback))
          .catch((error) => console.error(error));
      },
      serialize: (v) => JSON.stringify(v),
      deserialize: (v) => JSON.parse(v),
    },
  )
  : directServicesFunctions;
