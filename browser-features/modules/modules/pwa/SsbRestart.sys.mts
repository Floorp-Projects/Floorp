// SPDX-License-Identifier: MPL-2.0

import type { Manifest } from "#features-chrome/common/pwa/type.ts";
import { SsbRunnerUtils } from "./SsbCommandLineHandler.sys.mts";

const { PrivateBrowsingUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
);

const RESTART_PREF = "floorp.ssb.restart.pending";

export interface RestartEntry {
  id: string;
  url: string;
}

/** Preserve a page only when it is an ordinary URL in the installed app's origin. */
export function restartUrl(startUrl: string, previousUrl: string): string {
  try {
    const start = new URL(startUrl);
    const previous = new URL(previousUrl);
    if (
      (start.protocol === "https:" || start.protocol === "http:") &&
      previous.origin === start.origin
    ) return previous.href;
  } catch {
    // Fall back to the registered start URL for invalid or external pages.
  }
  return startUrl;
}

export function parseRestartEntries(value: string): RestartEntry[] {
  try {
    const data: unknown = JSON.parse(value);
    if (!Array.isArray(data)) return [];
    return data.filter((entry): entry is RestartEntry =>
      typeof entry === "object" && entry !== null &&
      typeof entry.id === "string" && entry.id.length > 0 &&
      typeof entry.url === "string"
    );
  } catch {
    return [];
  }
}

type SsbWindow = Window & {
  arguments?: ArrayLike<unknown>;
  gBrowser?: { selectedBrowser?: { currentURI?: { spec?: string } } };
};

function ssbId(window: SsbWindow): string | null {
  try {
    const bag = window.arguments?.[1] as nsIPropertyBag2 | undefined;
    const id = bag?.getPropertyAsAString("ssbid");
    // Firefox taskbar tabs and other Floorp windows must not be relaunched.
    return id &&
        window.document.documentElement.getAttribute("taskbartab") === id
      ? id
      : null;
  } catch {
    return null;
  }
}

function currentWindows(): RestartEntry[] {
  const entries: RestartEntry[] = [];
  for (const value of Services.wm.getEnumerator("navigator:browser")) {
    const window = value as unknown as SsbWindow;
    if (window.closed || PrivateBrowsingUtils.isWindowPrivate(window)) continue;
    const id = ssbId(window);
    if (id) {
      entries.push({
        id,
        url: window.gBrowser?.selectedBrowser?.currentURI?.spec ?? "",
      });
    }
  }
  return entries;
}

export interface SsbRestartDependencies {
  restarting(): boolean;
  wasRestarted(): boolean;
  enabled(): boolean;
  windows(): RestartEntry[];
  read(): string;
  write(value: string): void;
  clear(): void;
  flush(): void;
  getApp(id: string): Promise<Manifest | null>;
  openApp(app: Manifest, url: string): Promise<void>;
}

const defaultDependencies: SsbRestartDependencies = {
  restarting: () => Services.startup.restarting,
  wasRestarted: () => Services.startup.wasRestarted,
  enabled: () => Services.prefs.getBoolPref("floorp.browser.ssb.enabled", true),
  windows: currentWindows,
  read: () => Services.prefs.getStringPref(RESTART_PREF, ""),
  write: (value) => Services.prefs.setStringPref(RESTART_PREF, value),
  clear: () => {
    if (Services.prefs.prefHasUserValue(RESTART_PREF)) {
      Services.prefs.clearUserPref(RESTART_PREF);
    }
  },
  // Gecko accepts null to flush the active profile; generated typings omit it.
  flush: () => Services.prefs.savePrefFile(null as unknown as nsIFile),
  getApp: (id) => SsbRunnerUtils.getSsbById(id),
  openApp: async (app, url) => {
    const win = await SsbRunnerUtils.openSsbWindow({ ...app, start_url: url });
    await SsbRunnerUtils.applyOSIntegration(app, win);
  },
};

export class SsbRestartManager {
  private initialized = false;
  private restoreStarted = false;

  constructor(
    private readonly deps: SsbRestartDependencies = defaultDependencies,
  ) {}

  init(): void {
    if (this.initialized) return;
    this.initialized = true;
    Services.obs.addObserver(this, "quit-application-granted");
    Services.obs.addObserver(this, "browser-delayed-startup-finished");
  }

  observe(_subject: nsISupports | null, topic: string): void {
    if (topic === "quit-application-granted") {
      this.saveForRestart();
    } else if (
      topic === "browser-delayed-startup-finished" && !this.restoreStarted
    ) {
      this.restoreStarted = true;
      Services.obs.removeObserver(this, "browser-delayed-startup-finished");
      void this.restoreAfterRestart().catch((error) => {
        console.error("[SsbRestart] Could not read restart state:", error);
      });
    }
  }

  saveForRestart(): void {
    try {
      if (this.deps.restarting() && this.deps.enabled()) {
        this.deps.write(JSON.stringify(this.deps.windows()));
      } else if (this.deps.read()) {
        this.deps.clear();
      } else return;
      // Shutdown will close the windows immediately after this notification.
      this.deps.flush();
    } catch (error) {
      console.error("[SsbRestart] Could not save restart state:", error);
    }
  }

  async restoreAfterRestart(): Promise<void> {
    const value = this.deps.read();
    if (value) {
      // Consume synchronously before opening any windows, including on failure,
      // so a subsequent normal launch cannot replay an old restart request.
      this.deps.clear();
      try {
        this.deps.flush();
      } catch (error) {
        console.error("[SsbRestart] Could not clear restart state:", error);
        return;
      }
    }
    if (!this.deps.wasRestarted() || !this.deps.enabled()) return;

    for (const entry of parseRestartEntries(value)) {
      // An already-open window may be an independent --start-ssb request, not
      // a restored instance. The consumed queue itself prevents replay.
      try {
        const ssb = await this.deps.getApp(entry.id);
        if (!ssb) continue; // The app may have been uninstalled during restart.
        await this.deps.openApp(ssb, restartUrl(ssb.start_url, entry.url));
      } catch (error) {
        console.error("[SsbRestart] Could not restore a PWA window:", error);
      }
    }
  }
}

export const SsbRestart = new SsbRestartManager();
