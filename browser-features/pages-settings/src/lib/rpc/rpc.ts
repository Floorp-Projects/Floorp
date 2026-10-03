import type {
  NRContextMenuSettingsFunctions,
  NRSettingsAtomicPreferenceFunctions,
  NRSettingsParentFunctions,
  PrefCompareAndSetResult,
  PrefReadResult,
} from "../../../../modules/common/defines.ts";
import type { ContextMenuCatalogSnapshot } from "#features-chrome/common/context-menu/types.ts";
import { createBirpc } from "birpc";
import { usesSettingsActor } from "../../../../../libs/ui/settings-rpc-origin.ts";
import type { AppLifecycleSettings } from "#libs/pwa/appLifecycleTypes.ts";

type SettingsPageParentFunctions =
  & NRSettingsParentFunctions
  & NRSettingsAtomicPreferenceFunctions
  & NRContextMenuSettingsFunctions;

interface ContextMenuCatalogServiceModule {
  ContextMenuCatalogService: {
    getSnapshot(): ContextMenuCatalogSnapshot;
    getRevision(): number;
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

declare const Services: { prefs: DirectPreferenceService };
declare const ChromeUtils: {
  importESModule(moduleUri: string): unknown;
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

// about:hub is privileged even when its scripts are served by Vite.
const isLocalhost5183 = usesSettingsActor(globalThis.location.href, "5183");

export function createSettingsBridgeTransport(
  resolveBridge: () => Promise<Window> = waitForSettingsBridge,
): {
  post(data: string): Promise<void>;
  on(callback: (data: string) => void): Promise<void>;
} {
  let receiveCallback: ((data: string) => void) | null = null;
  let receiverReady: Promise<Window> | null = null;

  const ensureReceiver = (): Promise<Window> => {
    if (receiverReady) return receiverReady;
    if (!receiveCallback) {
      return Promise.reject(
        new Error("NRSettings page RPC receiver was not initialized"),
      );
    }

    const attempt = resolveBridge().then((page) => {
      if (!receiveCallback) {
        throw new Error("NRSettings page RPC receiver was not initialized");
      }
      page.NRSettingsRegisterReceiveCallback(receiveCallback);
      return page;
    });
    receiverReady = attempt;
    void attempt.catch(() => {
      // birpc only invokes `on` once. Clear a failed attempt so a later `post`
      // can register the retained callback again instead of reusing a rejected
      // promise forever.
      if (receiverReady === attempt) receiverReady = null;
    });
    return attempt;
  };

  return {
    on: (callback) => {
      receiveCallback = callback;
      receiverReady = null;
      return ensureReceiver().then(() => undefined);
    },
    post: (data) => ensureReceiver().then((page) => page.NRSettingsSend(data)),
  };
}

const settingsBridgeTransport = createSettingsBridgeTransport();

function registerSettingsBridgeReceiver(
  callback: (data: string) => void,
): Promise<void> {
  return settingsBridgeTransport.on(callback);
}

function sendSettingsBridgeMessage(data: string): Promise<void> {
  return settingsBridgeTransport.post(data);
}

const directServicesFunctions: SettingsPageParentFunctions = {
  getWebAppLifecycleSettings: () => {
    const { NativeAppRuntime } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/NativeAppRuntime.sys.mjs",
    ) as { NativeAppRuntime: { isAvailable(): boolean } };
    const { AppLifecycle } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/AppLifecycle.sys.mjs",
    ) as { AppLifecycle: { getSettings(): AppLifecycleSettings } };
    // Register the native runtime before querying adapter support.
    NativeAppRuntime.isAvailable();
    return Promise.resolve(AppLifecycle.getSettings());
  },
  getContextMenuCatalog: () => {
    const { ContextMenuCatalogService } = ChromeUtils.importESModule(
      "resource://noraneko/modules/context-menu/ContextMenuCatalogService.sys.mjs",
    ) as ContextMenuCatalogServiceModule;
    return Promise.resolve(ContextMenuCatalogService.getSnapshot());
  },
  getContextMenuCatalogRevision: () => {
    const { ContextMenuCatalogService } = ChromeUtils.importESModule(
      "resource://noraneko/modules/context-menu/ContextMenuCatalogService.sys.mjs",
    ) as ContextMenuCatalogServiceModule;
    return Promise.resolve(ContextMenuCatalogService.getRevision());
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
};

export const rpc = isLocalhost5183
  ? createBirpc<SettingsPageParentFunctions, Record<string, never>>(
    {},
    {
      // birpc waits for the value returned by `on` before sending its first
      // request. Return the bridge promises so an early catalog request cannot
      // race the actor callback installation and be silently dropped.
      post: sendSettingsBridgeMessage,
      on: registerSettingsBridgeReceiver,
      serialize: (v) => JSON.stringify(v),
      deserialize: (v) => JSON.parse(v),
    },
  )
  : directServicesFunctions;
