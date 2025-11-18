/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "#features-chrome/common/pwa/type.ts";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

const { SessionStore } = ChromeUtils.importESModule(
  "resource:///modules/sessionstore/SessionStore.sys.mjs",
);
const { TaskbarExperiment } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs",
);

export const PWA_WINDOW_NAME = "FloorpPWAWindow";
const SSB_WINDOW_FEATURES =
  "chrome,location=yes,centerscreen,dialog=no,resizable=yes,scrollbars=yes";

type TQueryInterface = <T extends nsIID>(aIID: T) => nsQIResult<T>;

export class SsbRunnerUtils {
    static async openSsbWindow(ssb: Manifest, initialLaunch: boolean = false) {
      let initialLaunchWin: nsIDOMWindow | null = null;
      if (initialLaunch) {
        initialLaunchWin = Services.ww.openWindow(
          null as unknown as mozIDOMWindowProxy,
          AppConstants.BROWSER_CHROME_URL,
          "_blank",
          "",
          {},
        ) as nsIDOMWindow;
      }

      const args = this.createWindowArgs(ssb.start_url);
      const uniqueWindowName = this.generateWindowName(ssb.id);

      const win = Services.ww.openWindow(
        null as unknown as mozIDOMWindowProxy,
        AppConstants.BROWSER_CHROME_URL,
        uniqueWindowName,
        SSB_WINDOW_FEATURES,
        args,
      ) as nsIDOMWindow;

      win.focus();
      SessionStore.promiseAllWindowsRestored.then(() => {
        initialLaunchWin?.close();
      });

      await this.waitForWindowLoaded(win);
      return win;
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
      }
    }

  private static createWindowArgs(startUrl: string) {
    const args = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString,
    );
    args.data = startUrl;
    return args;
  }

  private static generateWindowName(id: string) {
    return `${PWA_WINDOW_NAME}_${id}_${Date.now()}`;
  }

  private static async waitForWindowLoaded(win: Window) {
    if (win.document.readyState === "complete") {
      return;
    }

    await new Promise<void>((resolve) => {
      win.addEventListener("load", () => resolve(), { once: true });
    });
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
  if (initialLaunch) {
    return;
  }

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
