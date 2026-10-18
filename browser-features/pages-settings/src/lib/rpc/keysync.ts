import type { NRKeySyncParentFunctions } from "../../../../modules/common/defines.ts";
import type {
  FirefoxKeyEntry,
  KeySyncChangeResult,
} from "../../../../modules/common/NRKeySyncTypes.ts";
import type { XulCombo } from "../../../../modules/common/NRKeySyncCombo.ts";
import { createBirpc } from "birpc";
import { usesSettingsActor } from "../../../../../libs/ui/settings-rpc-origin.ts";

/** Public client for the key sync feature (actor bridge or direct service). */
export interface KeySyncClient {
  getKeys(): Promise<FirefoxKeyEntry[]>;
  isWriteEnabled(): Promise<boolean>;
  changeKey(id: string, combo: XulCombo): Promise<KeySyncChangeResult>;
  resetKey(id: string): Promise<KeySyncChangeResult>;
  openAboutKeyboard(): Promise<void>;
  /** Registers a mirror-change listener; returns its unsubscribe function. */
  subscribe(listener: (keys: FirefoxKeyEntry[]) => void): () => void;
}

/** Structural type of the key sync module loaded via resource URI. */
interface KeySyncServiceModule {
  keySyncService: {
    getKeys(): FirefoxKeyEntry[];
    isWriteEnabled(): boolean;
    changeKey(id: string, combo: XulCombo): KeySyncChangeResult;
    resetKey(id: string): KeySyncChangeResult;
    openAboutKeyboard(win: Window): void;
    addListener(listener: (keys: FirefoxKeyEntry[]) => void): void;
    removeListener(listener: (keys: FirefoxKeyEntry[]) => void): void;
  };
}

// Privileged globals available on the packaged settings chrome route.
declare const ChromeUtils: {
  importESModule(moduleUri: string): unknown;
};
declare const Services: {
  wm: {
    getMostRecentBrowserWindow(): Window | null;
  };
};

const KEYSYNC_SERVICE_URI =
  "resource://noraneko/modules/NRKeySyncService.sys.mjs";

const KEYSYNC_BRIDGE_TIMEOUT_MS = 15_000;
const KEYSYNC_BRIDGE_POLL_INTERVAL_MS = 25;

/** Window surface exported by NRKeySyncChild. */
interface KeySyncPageWindow {
  NRKeySyncSend: (data: string) => void;
  NRKeySyncRegisterReceiveCallback: (callback: (data: string) => void) => void;
  NRKeySyncRegisterPushCallback: (callback: (data: string) => void) => void;
}

function waitForKeysyncBridge(): Promise<KeySyncPageWindow> {
  const startedAt = Date.now();
  return new Promise((resolve, reject) => {
    const poll = () => {
      const page = globalThis as unknown as Partial<KeySyncPageWindow>;
      if (
        typeof page.NRKeySyncSend === "function" &&
        typeof page.NRKeySyncRegisterReceiveCallback === "function" &&
        typeof page.NRKeySyncRegisterPushCallback === "function"
      ) {
        resolve(page as KeySyncPageWindow);
        return;
      }
      if (Date.now() - startedAt >= KEYSYNC_BRIDGE_TIMEOUT_MS) {
        reject(new Error("NRKeySync page RPC bridge did not initialize"));
        return;
      }
      globalThis.setTimeout(poll, KEYSYNC_BRIDGE_POLL_INTERVAL_MS);
    };
    poll();
  });
}

/**
 * JSON-string transport between the page birpc instance and the child actor,
 * mirroring rpc.ts's settings bridge transport: birpc invokes `on` once and
 * awaits it before the first request, so the bridge wait happens there.
 */
function createKeysyncBridgeTransport(): {
  post(data: string): Promise<void>;
  on(callback: (data: string) => void): Promise<void>;
} {
  let receiveCallback: ((data: string) => void) | null = null;
  let receiverReady: Promise<KeySyncPageWindow> | null = null;

  const ensureReceiver = (): Promise<KeySyncPageWindow> => {
    if (receiverReady) return receiverReady;
    if (!receiveCallback) {
      return Promise.reject(
        new Error("NRKeySync page RPC receiver was not initialized"),
      );
    }

    const attempt = waitForKeysyncBridge().then((page) => {
      if (!receiveCallback) {
        throw new Error("NRKeySync page RPC receiver was not initialized");
      }
      page.NRKeySyncRegisterReceiveCallback(receiveCallback);
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
    post: (data) => ensureReceiver().then((page) => page.NRKeySyncSend(data)),
  };
}

function createBridgeKeySyncClient(): KeySyncClient {
  const transport = createKeysyncBridgeTransport();

  const rpcClient = createBirpc<
    NRKeySyncParentFunctions,
    Record<string, never>
  >(
    {},
    {
      post: (data) => transport.post(data),
      on: (callback) => transport.on(callback),
      serialize: (v) => JSON.stringify(v),
      deserialize: (v) => JSON.parse(v),
    },
  );

  const pushListeners = new Set<(keys: FirefoxKeyEntry[]) => void>();
  let pushRegisterAttempt: Promise<void> | null = null;

  const ensurePushRegistration = (): void => {
    if (pushRegisterAttempt) return;
    const attempt = waitForKeysyncBridge()
      .then((page) => {
        page.NRKeySyncRegisterPushCallback((data: string) => {
          let keys: FirefoxKeyEntry[];
          try {
            keys = JSON.parse(data) as FirefoxKeyEntry[];
          } catch (error) {
            console.error("[NRKeySync] Failed to parse pushed keys", error);
            return;
          }
          for (const listener of Array.from(pushListeners)) {
            try {
              listener(keys);
            } catch (error) {
              console.error("[NRKeySync] key listener failed", error);
            }
          }
        });
      })
      .catch((error) => {
        // Allow a later subscribe() to retry the registration.
        if (pushRegisterAttempt === attempt) pushRegisterAttempt = null;
        console.error(
          "[NRKeySync] Failed to register the push callback",
          error,
        );
      });
    pushRegisterAttempt = attempt;
  };

  return {
    getKeys: () => rpcClient.getFirefoxKeys(),
    isWriteEnabled: () => rpcClient.isKeySyncWriteEnabled(),
    changeKey: (id, combo) => rpcClient.changeFirefoxKey(id, combo),
    resetKey: (id) => rpcClient.resetFirefoxKey(id),
    openAboutKeyboard: () => rpcClient.openAboutKeyboard(),
    subscribe(listener) {
      pushListeners.add(listener);
      ensurePushRegistration();
      return () => {
        pushListeners.delete(listener);
      };
    },
  };
}

function createUnavailableKeySyncClient(): KeySyncClient {
  return {
    getKeys: () => Promise.resolve([]),
    isWriteEnabled: () => Promise.resolve(false),
    changeKey: () =>
      Promise.resolve({
        ok: false,
        error: "KeySyncService is unavailable",
      }),
    resetKey: () =>
      Promise.resolve({
        ok: false,
        error: "KeySyncService is unavailable",
      }),
    openAboutKeyboard: () => Promise.resolve(),
    subscribe: () => () => undefined,
  };
}

function createDirectKeySyncClient(): KeySyncClient {
  let service: KeySyncServiceModule["keySyncService"] | null = null;
  try {
    const imported = ChromeUtils.importESModule(
      KEYSYNC_SERVICE_URI,
    ) as Partial<KeySyncServiceModule>;
    // Some embedded contexts expose an importESModule that resolves without
    // the noraneko module registered. Validate the shape instead of trusting
    // the import succeeding.
    if (
      imported &&
      typeof imported.keySyncService?.getKeys === "function" &&
      typeof imported.keySyncService.addListener === "function"
    ) {
      service = imported.keySyncService;
    }
  } catch (error) {
    console.error(
      "[NRKeySync] Failed to load NRKeySyncService; key settings are unavailable",
      error,
    );
  }
  if (!service) {
    return createUnavailableKeySyncClient();
  }
  return {
    getKeys: () => Promise.resolve(service.getKeys()),
    isWriteEnabled: () => Promise.resolve(service.isWriteEnabled()),
    changeKey: (id, combo) => Promise.resolve(service.changeKey(id, combo)),
    resetKey: (id) => Promise.resolve(service.resetKey(id)),
    openAboutKeyboard: () => {
      // The page window has no openTrustedLinkIn; target the browser window.
      const win = Services.wm.getMostRecentBrowserWindow();
      if (!win) {
        console.error(
          "[NRKeySync] No browser window available to open about:keyboard",
        );
        return Promise.resolve();
      }
      service.openAboutKeyboard(win);
      return Promise.resolve();
    },
    subscribe(listener) {
      service.addListener(listener);
      return () => {
        service.removeListener(listener);
      };
    },
  };
}

// about:hub is privileged even when its scripts are served by Vite.
const isDevBridge = usesSettingsActor(globalThis.location.href, "5183");

export const keySync: KeySyncClient = isDevBridge
  ? createBridgeKeySyncClient()
  : createDirectKeySyncClient();
