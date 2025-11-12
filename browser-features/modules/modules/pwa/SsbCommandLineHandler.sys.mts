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

export const PWA_WINDOW_NAME = "FloorpPWAWindow";

type TQueryInterface = <T extends nsIID>(aIID: T) => nsQIResult<T>;

type MacSupportModule = typeof import("./supports/Mac.sys.mts");
type MacIntegrationSupport =
  import("./supports/Mac.sys.mts").MacIntegrationSupport;

let gMacIntegrationSupport: MacIntegrationSupport | null = null;

function getMacIntegrationSupport(): MacIntegrationSupport | null {
  if (AppConstants.platform !== "macosx") {
    return null;
  }
  if (!gMacIntegrationSupport) {
    const { MacIntegrationSupport } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/supports/Mac.sys.mjs",
    ) as MacSupportModule;
    gMacIntegrationSupport = new MacIntegrationSupport();
  }
  return gMacIntegrationSupport;
}

export class SsbRunnerUtils {
  static openSsbWindow(ssb: Manifest, initialLaunch: boolean = false) {
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

    const browserWindowFeatures =
      "chrome,location=yes,centerscreen,dialog=no,resizable=yes,scrollbars=yes";

    const args = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString,
    );

    args.data = ssb.start_url;

    const uniqueWindowName = `${PWA_WINDOW_NAME}_${ssb.id}_${Date.now()}`;

    const macSupport = getMacIntegrationSupport();
    macSupport?.notifyLaunchRequest(ssb, initialLaunch);

    const win = Services.ww.openWindow(
      null as unknown as mozIDOMWindowProxy,
      AppConstants.BROWSER_CHROME_URL,
      uniqueWindowName,
      browserWindowFeatures,
      args,
    ) as nsIDOMWindow;

    win.focus();
    macSupport?.attachWindowMetadata(win, ssb);
    SessionStore.promiseAllWindowsRestored.then(() => {
      initialLaunchWin?.close();
    });
    return win;
  }

  static async applyWindowsIntegration(ssb: Manifest, win: Window) {
    if (AppConstants.platform === "win") {
      const { WindowsSupport } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/supports/Windows.sys.mjs",
      );
      const windowsSupport = new WindowsSupport();
      await windowsSupport.applyOSIntegration(ssb, win);
    }
  }
}

async function startSSBFromCmdLine(
  id: string,
  initialLaunch: boolean,
): Promise<boolean> {
  let launched = false;
  // Loading the SSB is async. Until that completes and launches we will
  // be without an open window and the platform will not continue startup
  // in that case. Flag that a window is coming.
  Services.startup.enterLastWindowClosingSurvivalArea();

  // Whatever happens we must exitLastWindowClosingSurvivalArea when done.
  try {
    const { DataStoreProvider } = ChromeUtils.importESModule(
      "resource://noraneko/modules/pwa/DataStore.sys.mjs",
    );

    const dataManager = DataStoreProvider.getDataManager();
    const ssbData = await dataManager.getCurrentSsbData();

    for (const value of Object.values(ssbData)) {
      if ((value as Manifest).id === id) {
        const ssb = value as Manifest;
        const win = SsbRunnerUtils.openSsbWindow(ssb, initialLaunch);
        await SsbRunnerUtils.applyWindowsIntegration(ssb, win);
        launched = true;
        break;
      }
    }
  } catch (error) {
    console.error("Failed to launch SSB from command line", error);
    throw error;
  } finally {
    Services.startup.exitLastWindowClosingSurvivalArea();
  }

  if (!launched) {
    console.warn(
      `No SSB manifest matched the requested id "${id}". Falling back to normal startup.`,
    );
  }

  return launched;
}

export class SSBCommandLineHandler {
  QueryInterface = ChromeUtils.generateQI([
    "nsICommandLineHandler",
  ]) as TQueryInterface;

  private isInitialized = false;

  private handleLaunchFailure(
    reason: unknown,
    cmdLine: nsICommandLine,
    id: string,
  ) {
    if (reason) {
      console.error("SSB launch failed", reason);
    }

    console.warn(
      `SSB launch request for id "${id}" could not be fulfilled; continuing with normal startup.`,
    );

    try {
      if (Services.prefs.prefHasUserValue("floorp.ssb.startup.id")) {
        Services.prefs.clearUserPref("floorp.ssb.startup.id");
      }
    } catch (prefError) {
      console.error(
        "Unable to clear floorp.ssb.startup.id after failed SSB launch",
        prefError,
      );
    }

    // Ensure normal startup continues so the user still gets a browser window.
    cmdLine.preventDefault = false;
  }

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
      let completed = false;
      let launchSucceeded = false;
      let launchError: unknown = undefined;
      startSSBFromCmdLine(id, !this.isInitialized)
        .then(result => {
          launchSucceeded = result;
        })
        .catch(error => {
          launchError = error;
        })
        .finally(() => {
          completed = true;
        });

      Services.tm.spinEventLoopUntil(
        "floorp.ssb.commandLineHandler",
        () => completed,
      );

      this.isInitialized = true;

      if (launchSucceeded) {
        cmdLine.preventDefault = true;
      } else {
        this.handleLaunchFailure(launchError, cmdLine, id);
      }
    }
  }

  get helpInfo() {
    return "  --start-ssb <id>  Start the SSB with the given id.\n";
  }
}
