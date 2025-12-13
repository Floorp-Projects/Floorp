/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "#features-chrome/common/pwa/type.ts";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

const { BrowserWindowTracker } = ChromeUtils.importESModule(
  "resource:///modules/BrowserWindowTracker.sys.mjs",
);
const { TaskbarExperiment } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs",
);

export const PWA_WINDOW_NAME = "FloorpPWAWindow";

/**
 * Get window features based on PWA display mode
 */
function getWindowFeatures(displayMode?: string): string {
  switch (displayMode) {
    case "fullscreen":
      return "chrome,fullscreen=yes,resizable=yes";
    case "standalone":
      return "chrome,titlebar=no,toolbar=no,location=no,resizable=yes,minimizable";
    case "minimal-ui":
      return "chrome,titlebar,close,toolbar,location,personalbar=no,menubar=no,resizable,minimizable";
    case "browser":
      return "chrome,all";
    default:
      // Default to standalone for PWAs
      return "chrome,titlebar=no,toolbar=no,location=no,resizable=yes,minimizable,centerscreen";
  }
}

type TQueryInterface = <T extends nsIID>(aIID: T) => nsQIResult<T>;

export class SsbRunnerUtils {
  static async openSsbWindow(ssb: Manifest, initialLaunch: boolean = false) {
    // Use LastWindowClosingSurvivalArea to avoid showing empty window
    if (initialLaunch) {
      Services.startup.enterLastWindowClosingSurvivalArea();
    }

    try {
      const args = this.createWindowArgs(ssb);
      const windowFeatures = getWindowFeatures(ssb.display);

      const win = await BrowserWindowTracker.promiseOpenWindow({
        args,
        features: windowFeatures,
        all: false,
      });

      // Set displayMode on all tabs
      const displayMode = ssb.display || "standalone";
      (win as any).gBrowser?.tabs.forEach((tab: any) => {
        const browser = (win as any).gBrowser.getBrowserForTab(tab);
        if (browser?.browsingContext) {
          browser.browsingContext.displayMode = displayMode;
        }
      });

      (win as any).focus();
      return win;
    } finally {
      if (initialLaunch) {
        Services.startup.exitLastWindowClosingSurvivalArea();
      }
    }
  }

  static async applyOSIntegration(ssb: Manifest, win: Window) {
    // Check A/B test before applying taskbar integration
    if (!TaskbarExperiment.isEnabledForCurrentPlatform()) {
      console.debug(
        "[SsbRunnerUtils] PWA taskbar integration disabled by A/B test, skipping OS integration",
      );
      return;
    }

    if (AppConstants.platform === "win") {
      const { WindowsSupport } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/supports/Windows.sys.mjs",
      );
      const windowsSupport = new WindowsSupport();
      await windowsSupport.applyOSIntegration(ssb, win);
      return;
    }

    if (AppConstants.platform === "linux") {
      const { LinuxSupport } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/supports/Linux.sys.mjs",
      );
      const linuxSupport = new LinuxSupport();
      await linuxSupport.applyOSIntegration(ssb, win);
      return;
    }
  }

  private static createWindowArgs(ssb: Manifest) {
    // Create extraOptions to set document.documentElement attributes
    const extraOptions = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag2,
    );
    extraOptions.setPropertyAsAString("ssbid", ssb.id);
    // Set taskbartab attribute to leverage SessionStore's existing handling
    // This excludes PWA windows from session restore and treats them like TaskBarTabs
    extraOptions.setPropertyAsAString("taskbartab", ssb.id);

    // Create URL argument
    const url = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString,
    );
    url.data = ssb.start_url;

    // Build args array
    const args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    args.appendElement(url);
    args.appendElement(extraOptions);

    return args;
  }

  static async getSsbById(id: string): Promise<Manifest | null> {
    const { DataStoreProvider } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/DataStore.sys.mjs",
    );
    const dataManager = DataStoreProvider.getDataManager();
    const ssbData = await dataManager.getCurrentSsbData();

    const match = Object.values(ssbData).find(
      (value) => (value as Manifest).id === id,
    );

    return (match as Manifest) ?? null;
  }
}

async function startSSBFromCmdLine(id: string, initialLaunch: boolean) {
  // Loading the SSB is async. Until that completes and launches we will
  // be without an open window and the platform will not continue startup
  // in that case. Flag that a window is coming.
  Services.startup.enterLastWindowClosingSurvivalArea();

  // Whatever happens we must exitLastWindowClosingSurvivalArea when done.
  try {
    const ssb = await SsbRunnerUtils.getSsbById(id);
    if (!ssb) {
      console.warn(`[SSBCommandLineHandler] Unknown SSB id: ${id}`);
      return;
    }

    const win = await SsbRunnerUtils.openSsbWindow(ssb, initialLaunch);
    await SsbRunnerUtils.applyOSIntegration(ssb, win);
  } finally {
    Services.startup.exitLastWindowClosingSurvivalArea();
  }
}

export class SSBCommandLineHandler {
  QueryInterface = ChromeUtils.generateQI([
    "nsICommandLineHandler",
  ]) as TQueryInterface;

  private isInitialized = false;

  handle(cmdLine: nsICommandLine) {
    const id = cmdLine.handleFlagWithParam("start-ssb", false);
    if (id) {
      // If there is no Floorp browser window open, do not launch the PWA now.
      // Instead, persist the requested SSB id to a preference so the regular
      // Floorp startup can handle launching it later. This avoids requiring
      // the user to click the taskbar shortcut twice.
      const hasBrowserWindow =
        !!Services.wm.getMostRecentWindow("navigator:browser");

      if (!hasBrowserWindow) {
        try {
          Services.prefs.setCharPref("floorp.ssb.startup.id", id);
        } catch (e) {
          // If pref set fails for some reason, log and continue without
          // preventing default startup so the application still launches.
          console.error("Failed to set floorp.ssb.startup.id", e);
        }

        // Let normal startup continue (do not preventDefault). The PWA will
        // not be started now.
        return;
      }

      // If a browser window already exists, start the SSB immediately.
      startSSBFromCmdLine(id, !this.isInitialized);
      this.isInitialized = true;
      cmdLine.preventDefault = true;
    }
  }

  get helpInfo() {
    return "  --start-ssb <id>  Start the SSB with the given id.\n";
  }
}
