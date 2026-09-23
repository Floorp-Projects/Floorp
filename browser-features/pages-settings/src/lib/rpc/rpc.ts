import type { NRSettingsParentFunctions } from "../../../../modules/common/defines.ts";
import { createBirpc } from "birpc";
import { usesSettingsActor } from "../../../../../libs/ui/settings-rpc-origin.ts";
import type { AppLifecycleSettings } from "#libs/pwa/appLifecycleTypes.ts";

// deno-lint-ignore no-explicit-any
declare const Services: any;
declare const ChromeUtils: {
  importESModule(uri: string): {
    AppLifecycle: { getSettings(): AppLifecycleSettings };
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

// about:hub is privileged even when its scripts are served by Vite.
const isLocalhost5183 = usesSettingsActor(globalThis.location.href, "5183");

const directServicesFunctions: NRSettingsParentFunctions = {
  getWebAppLifecycleSettings: () => {
    const { AppLifecycle } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/AppLifecycle.sys.mjs",
    );
    return Promise.resolve(AppLifecycle.getSettings());
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
};

export const rpc = isLocalhost5183
  ? createBirpc<NRSettingsParentFunctions, Record<string, never>>(
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
