/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "#features-chrome/common/pwa/type.ts";
import { NativeAppRuntime } from "./NativeAppRuntime.sys.mts";

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
 * Get window features based on PWA display mode.
 * Note: toolbar/titlebar visibility is handled by CSS in pwa-window.tsx
 * based on the showToolbar config setting.
 */
function getWindowFeatures(displayMode?: string): string {
  // All display modes now include toolbar for consistent PWA window behavior
  switch (displayMode) {
    case "fullscreen":
    case "standalone":
    case "minimal-ui":
    case "browser":
    default:
      return "chrome,titlebar,close,toolbar,location,personalbar=no,menubar=no,resizable,minimizable,centerscreen";
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

      const options = {
        args,
        features: windowFeatures,
        all: false,
      };
      const win = await NativeAppRuntime.open(ssb, () =>
        BrowserWindowTracker.openWindow(options)) ??
        await BrowserWindowTracker.promiseOpenWindow(options);

      // Set displayMode on all tabs
      const displayMode = ssb.display || "standalone";
      // deno-lint-ignore no-explicit-any
      win.gBrowser?.tabs.forEach((tab: any) => {
        const browser = win.gBrowser.getBrowserForTab(tab);
        if (browser?.browsingContext) {
          browser.browsingContext.displayMode = displayMode;
        }
      });

      win.focus();
      return win;
    } finally {
      if (initialLaunch) {
        Services.startup.exitLastWindowClosingSurvivalArea();
      }
    }
  }

  static async applyOSIntegration(ssb: Manifest, win: Window) {
    if (AppConstants.platform === "macosx") {
      if (NativeAppRuntime.ownsWindow(win)) return;
      const { MacOSSupport } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/supports/MacOS.sys.mjs",
      );
      try {
        // Also repairs apps installed before macOS launcher support existed.
        const { DataStoreProvider } = ChromeUtils.importESModule(
          "resource://noraneko/modules/pwa/DataStore.sys.mjs",
        );
        await new MacOSSupport().repair(
          ssb,
          DataStoreProvider.getDataManager(),
        );
      } catch (error) {
        console.error(
          "[SsbRunnerUtils] Failed to create macOS app launcher:",
          error,
        );
      }
      return;
    }

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

    if (ssb.userContextId && ssb.userContextId > 0) {
      extraOptions.setPropertyAsAString(
        "userContextId",
        String(ssb.userContextId),
      );
    }

    // Create URL argument
    const url = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString,
    );
    url.data = ssb.start_url;

    // Build args array
    const args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    args.appendElement(url);
    args.appendElement(extraOptions);

    if (
      AppConstants.platform === "linux" &&
      Number.isSafeInteger(ssb.userContextId) &&
      (ssb.userContextId ?? 0) > 0
    ) {
      const { Experiments } = ChromeUtils.importESModule(
        "resource://noraneko/modules/experiments/Experiments.sys.mjs",
      );
      const { ContextualIdentityService } = ChromeUtils.importESModule(
        "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
      );
      // Command-line startup can run before Experiments.init(). Use the same
      // enrollment gate as PwaContainerExperiment, with the persisted assignment
      // only while the in-memory experiment manifest is unavailable.
      const experiment = "pwa_container_support";
      const enrolled = Experiments.getVariant(experiment) !== null ||
        (!Experiments.manifestAvailable &&
          Experiments.getCachedEnrollment(experiment).variantId !== null);
      const exists = ContextualIdentityService.getPublicIdentities().some(
        (identity: { userContextId: number }) =>
          identity.userContextId === ssb.userContextId,
      );
      if (enrolled && exists) {
        // Match Gecko's TaskbarTabsWindowManager argument contract. The bag
        // drives the PWA UI, but argument 5 selects the initial browser's
        // container before any navigation; migrating the tab later is too late.
        const context = Cc["@mozilla.org/supports-PRUint32;1"].createInstance(
          Ci.nsISupportsPRUint32,
        );
        context.data = ssb.userContextId!;
        for (
          const value of [
            null,
            null,
            undefined,
            context,
            null,
            null,
            Services.scriptSecurityManager.getSystemPrincipal(),
          ]
        ) {
          // nsIMutableArray accepts null slots; its generated type omits them.
          args.appendElement(value as nsISupports);
        }
      }
    }

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
      // Prevent default browser window from opening - PWA should launch standalone
      cmdLine.preventDefault = true;

      // Start SSB directly (works even without existing browser window)
      // The startSSBFromCmdLine function handles LastWindowClosingSurvivalArea
      void startSSBFromCmdLine(id, !this.isInitialized).catch((error) => {
        console.error("[SSBCommandLineHandler] Failed to start SSB:", error);
      });
      this.isInitialized = true;
    }
  }

  get helpInfo() {
    return "  --start-ssb <id>  Start the SSB with the given id.\n";
  }
}
