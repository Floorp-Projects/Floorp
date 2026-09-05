import type {
  ActiveTabInfo,
  NRClipsParentFunctions,
  NRSettingsParentFunctions,
} from "../../../../modules/common/defines.ts";
import { createBirpc } from "birpc";

// The chrome globals. Only reachable when this page runs as a chrome:// page;
// in dev the actors stand in for them, so nothing here is touched.
// deno-lint-ignore no-explicit-any
declare const Services: any;
// deno-lint-ignore no-explicit-any
declare const Cc: any;
// deno-lint-ignore no-explicit-any
declare const Ci: any;

/** Just enough of nsIFile for what a clip does with a path. */
export interface ClipFile {
  readonly path: string;
  exists(): boolean;
  reveal(): void;
  launch(): void;
}

declare global {
  interface Window {
    NRSettingsSend: (data: string) => void;
    NRSettingsRegisterReceiveCallback: (
      callback: (data: string) => void,
    ) => void;
    NRClipsSend: (data: string) => void;
    NRClipsRegisterReceiveCallback: (
      callback: (data: string) => void,
    ) => void;
  }
}

/**
 * In dev the page is served from the Vite dev server and has no chrome
 * privileges, so everything goes through the NRSettings and NRClips actors.
 * In production it is a chrome:// page and can touch Services directly.
 */
const isDevServer = import.meta.url?.includes("localhost:5189");

/** The browser window this panel lives in — only reachable from a chrome page. */
function browserWindow(): {
  gBrowser?: {
    selectedTab?: { label?: string };
    selectedBrowser?: { currentURI?: { spec?: string } };
  };
  openWebLinkIn?: (url: string, where: string) => void;
} | null {
  try {
    return Services.wm.getMostRecentWindow("navigator:browser") ?? null;
  } catch {
    return null;
  }
}

/** A local file, reached from a path a clip is holding. Chrome page only. */
export function localFile(filePath: string): ClipFile | null {
  try {
    const file = Cc["@mozilla.org/file/local;1"].createInstance(
      Ci.nsIFile,
    ) as ClipFile & { initWithPath(path: string): void };
    file.initWithPath(filePath);
    return file;
  } catch {
    return null;
  }
}

// ──────────────────────────────────────────────────────────
// Preferences
// ──────────────────────────────────────────────────────────

const directPrefFunctions: NRSettingsParentFunctions = {
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

export const rpc = isDevServer
  ? createBirpc<NRSettingsParentFunctions, Record<string, never>>(
    {},
    {
      post: (data) => (globalThis as unknown as Window).NRSettingsSend(data),
      on: (callback) => {
        (globalThis as unknown as Window).NRSettingsRegisterReceiveCallback(
          callback,
        );
      },
      serialize: (v) => JSON.stringify(v),
      deserialize: (v) => JSON.parse(v),
    },
  )
  : directPrefFunctions;

// ──────────────────────────────────────────────────────────
// Clips-only things (the tab, the clipboard, local files)
// ──────────────────────────────────────────────────────────

const directClipsFunctions: NRClipsParentFunctions = {
  getActiveTabInfo: () => {
    const gBrowser = browserWindow()?.gBrowser;
    const url = gBrowser?.selectedBrowser?.currentURI?.spec;
    if (!url) return Promise.resolve(null);
    return Promise.resolve({ title: gBrowser?.selectedTab?.label ?? "", url });
  },
  openLinkInTab: (url) => {
    browserWindow()?.openWebLinkIn?.(url, "tab");
    return Promise.resolve();
  },
  readClipboardText: () => {
    try {
      const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
        Ci.nsITransferable,
      );
      // A null load context means "not tied to a browsing context", which is
      // what reading the global clipboard from here is.
      trans.init(null);
      trans.addDataFlavor("text/plain");
      Services.clipboard.getData(
        trans,
        Ci.nsIClipboard.kGlobalClipboard,
        undefined,
      );
      const result: {
        value?: { QueryInterface: (iid: unknown) => { data?: string } };
      } = {};
      trans.getTransferData("text/plain", result);
      // The text/plain flavor comes back as an nsISupportsString, but the JS
      // wrapper only shows `.data` after QueryInterface — without it the read
      // is silently undefined, and clipboard-history mode never adds.
      const text = result.value?.QueryInterface(Ci.nsISupportsString).data;
      return Promise.resolve(
        typeof text === "string" && text.length > 0 ? text : null,
      );
    } catch {
      // An empty clipboard, or one holding something that is not text.
      return Promise.resolve(null);
    }
  },
  fileExists: (path) => {
    try {
      return Promise.resolve(localFile(path)?.exists() ?? false);
    } catch {
      return Promise.resolve(false);
    }
  },
  revealFile: (path) => {
    try {
      const file = localFile(path);
      // No file is not a success. Saying it was leaves the panel reporting
      // that it revealed something that was never there.
      if (!file) return Promise.resolve(false);
      file.reveal();
      return Promise.resolve(true);
    } catch {
      return Promise.resolve(false);
    }
  },
  launchFile: (path) => {
    try {
      const file = localFile(path);
      // No file is not a success. Saying it was leaves the panel reporting
      // that it launched something that was never there.
      if (!file) return Promise.resolve(false);
      file.launch();
      return Promise.resolve(true);
    } catch {
      return Promise.resolve(false);
    }
  },
  getSessionStartTime: () => {
    try {
      const info = Services.startup.getStartupInfo();
      const started = info.process ?? info.main;
      return Promise.resolve(started ? started.getTime() : 0);
    } catch {
      return Promise.resolve(0);
    }
  },
};

export const clips: NRClipsParentFunctions = isDevServer
  ? createBirpc<NRClipsParentFunctions, Record<string, never>>(
    {},
    {
      post: (data) => (globalThis as unknown as Window).NRClipsSend(data),
      on: (callback) => {
        (globalThis as unknown as Window).NRClipsRegisterReceiveCallback(
          callback,
        );
      },
      serialize: (v) => JSON.stringify(v),
      deserialize: (v) => JSON.parse(v),
    },
  )
  : directClipsFunctions;

/** The URL and title of the active tab, for the suggestion button. */
export async function getActiveTabInfo(): Promise<ActiveTabInfo | null> {
  try {
    return await clips.getActiveTabInfo();
  } catch (e) {
    console.error("[Floorp Clips] Failed to read the active tab:", e);
    return null;
  }
}

/**
 * Open a clipped URL in a normal tab, never inside the panel.
 *
 * `openWebLinkIn` only accepts web schemes, so a clip holding something like
 * `javascript:` or `chrome:` cannot get itself opened this way.
 */
export async function openLinkInTab(url: string): Promise<void> {
  try {
    await clips.openLinkInTab(url);
  } catch (e) {
    console.error("[Floorp Clips] Failed to open the link:", e);
  }
}

// ──────────────────────────────────────────────────────────
// Watching a preference change
// ──────────────────────────────────────────────────────────

export type PrefChangeCallback = (prefName: string) => void;

/**
 * Watch a string preference. Returns the function that stops watching.
 *
 * In production this is a real pref observer. The dev bridge cannot carry
 * observers, so there it polls — which is also all this needs, since a change
 * arriving from another device is never urgent.
 */
export function addPrefObserver(
  prefName: string,
  callback: PrefChangeCallback,
): () => void {
  if (isDevServer) {
    let last: string | null = null;
    void rpc.getStringPref(prefName).then((v) => (last = v)).catch(() => {});
    const timer = setInterval(async () => {
      try {
        const current = await rpc.getStringPref(prefName);
        if (current !== last) {
          last = current;
          callback(prefName);
        }
      } catch {
        // The pref may not exist yet.
      }
    }, 2000);
    return () => clearInterval(timer);
  }

  const observer = {
    observe(_subject: unknown, topic: string, data: string) {
      if (topic === "nsPref:changed" && data === prefName) callback(prefName);
    },
  };
  Services.prefs.addObserver(prefName, observer);
  return () => Services.prefs.removeObserver(prefName, observer);
}

/** Watch several preferences with one callback. */
export function addPrefObservers(
  prefNames: string[],
  callback: PrefChangeCallback,
): () => void {
  const stops = prefNames.map((name) => addPrefObserver(name, callback));
  return () => stops.forEach((stop) => stop());
}
