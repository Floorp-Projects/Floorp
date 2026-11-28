/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Manifest } from "#features-chrome/common/pwa/type.ts";
import { isPwaUrlEligible } from "#libs/pwa/urlPolicy.ts";
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

/**
 * Blocks top-level plaintext-HTTP navigation to non-loopback hosts inside PWA
 * windows. A chrome-side progress listener cannot cancel remote loads, so
 * enforcement uses a content policy whose shouldLoad/shouldProcess run in the
 * parent process. shouldLoad covers initial navigations and shouldProcess
 * covers redirect targets, which Gecko does not re-check with shouldLoad.
 *
 * PWA windows are identified structurally: createWindowArgs stamps `ssbid` on
 * the window's document element before any navigation starts, so enforcement
 * has no registration race and covers replaced or newly opened tabs too.
 */
let pwaNavigationPolicyRegistered = false;

const pwaWindows = new WeakSet<object>();

function isPwaWindowLoad(loadInfo: nsILoadInfo): boolean {
  try {
    // Track the chrome window rather than browsing contexts: discarding and
    // reinserting a tab replaces its browsing context, but the window object
    // is stable for the window's lifetime. Web panels cannot match because
    // their browsers live in the main browser window, which is never added.
    // deno-lint-ignore no-explicit-any
    const embedder = loadInfo.browsingContext?.embedderElement as any;
    const ownerGlobal = embedder?.ownerGlobal;
    return !!ownerGlobal && pwaWindows.has(ownerGlobal);
  } catch {
    return false;
  }
}

function checkPwaNavigationPolicy(
  contentLocation: nsIURI,
  loadInfo: nsILoadInfo,
): number {
  try {
    if (
      loadInfo.externalContentPolicyType !== Ci.nsIContentPolicy.TYPE_DOCUMENT
    ) {
      return Ci.nsIContentPolicy.ACCEPT as number;
    }
    if (!isPwaWindowLoad(loadInfo)) {
      return Ci.nsIContentPolicy.ACCEPT as number;
    }
    if (
      contentLocation?.scheme === "http" &&
      contentLocation.host !== "localhost" &&
      contentLocation.host !== "127.0.0.1"
    ) {
      console.warn(
        "[SsbRunnerUtils] Blocked non-loopback HTTP navigation in PWA window:",
        contentLocation.spec,
      );
      return Ci.nsIContentPolicy.REJECT_REQUEST as number;
    }
  } catch (error) {
    console.error(
      "[SsbRunnerUtils] PWA navigation policy check failed open:",
      error,
    );
  }
  return Ci.nsIContentPolicy.ACCEPT as number;
}

function registerPwaNavigationPolicy(): void {
  if (pwaNavigationPolicyRegistered) {
    return;
  }
  pwaNavigationPolicyRegistered = true;
  const policy = {
    classDescription: "Floorp PWA navigation policy",
    classID: Components.ID("{c8f47c3a-8b1d-4f41-9e1a-3f5a7b2c9d11}"),
    QueryInterface: ChromeUtils.generateQI(["nsIContentPolicy"]),
    shouldLoad(contentLocation: nsIURI, loadInfo: nsILoadInfo) {
      return checkPwaNavigationPolicy(contentLocation, loadInfo);
    },
    shouldProcess(contentLocation: nsIURI, loadInfo: nsILoadInfo) {
      return checkPwaNavigationPolicy(contentLocation, loadInfo);
    },
  };
  // Components.manager has QueryInterface at runtime that the TypeScript
  // definitions do not reflect.
  const componentManager = Components.manager as unknown as {
    QueryInterface?: (iid: unknown) => nsIComponentRegistrar;
  };
  const registrar = componentManager.QueryInterface?.(
    Ci.nsIComponentRegistrar,
  );
  if (!registrar) {
    console.error(
      "[SsbRunnerUtils] Cannot register PWA navigation policy: no registrar",
    );
    return;
  }
  registrar.registerFactory(
    policy.classID,
    policy.classDescription,
    "@floorp/pwa-navigation-policy;1",
    {
      // This runtime's nsIFactory.createInstance receives only the IID.
      createInstance: (iid: nsIID) =>
        // deno-lint-ignore no-explicit-any
        policy.QueryInterface(iid as any) as nsQIResult<nsIID>,
      // deno-lint-ignore no-explicit-any
    } as any,
  );
  Services.catMan.addCategoryEntry(
    "content-policy",
    "floorp-pwa-navigation-policy",
    "@floorp/pwa-navigation-policy;1",
    false,
    true,
  );

  // Content policies are not consulted again when a channel is redirected,
  // so redirect enforcement needs the channel event sink: it is invoked for
  // every redirect and can veto the target channel.
  const redirectSink = {
    classDescription: "Floorp PWA redirect policy",
    classID: Components.ID("{d91a5e2b-4c7f-4a83-b6d2-8e0f1a3c5b77}"),
    QueryInterface: ChromeUtils.generateQI(["nsIChannelEventSink"]),
    asyncOnChannelRedirect(
      _oldChannel: nsIChannel,
      newChannel: nsIChannel,
      _flags: number,
      callback: nsIAsyncVerifyRedirectCallback,
    ) {
      let verdict: number = Cr.NS_OK;
      try {
        const uri = newChannel.URI;
        const loadInfo = newChannel.loadInfo;
        if (
          loadInfo.externalContentPolicyType ===
            Ci.nsIContentPolicy.TYPE_DOCUMENT &&
          uri?.scheme === "http" &&
          uri.host !== "localhost" &&
          uri.host !== "127.0.0.1" &&
          isPwaWindowLoad(loadInfo)
        ) {
          console.warn(
            "[SsbRunnerUtils] Blocked redirect to non-loopback HTTP in PWA window:",
            uri.spec,
          );
          verdict = Cr.NS_ERROR_ABORT;
        }
      } catch (error) {
        console.error(
          "[SsbRunnerUtils] PWA redirect policy check failed open:",
          error,
        );
      }
      callback.onRedirectVerifyCallback(verdict);
    },
  };
  registrar.registerFactory(
    redirectSink.classID,
    redirectSink.classDescription,
    "@floorp/pwa-redirect-policy;1",
    {
      createInstance: (iid: nsIID) =>
        // deno-lint-ignore no-explicit-any
        redirectSink.QueryInterface(iid as any) as nsQIResult<nsIID>,
      // deno-lint-ignore no-explicit-any
    } as any,
  );
  Services.catMan.addCategoryEntry(
    "net-channel-event-sinks",
    "floorp-pwa-redirect-policy",
    "@floorp/pwa-redirect-policy;1",
    false,
    true,
  );
}

export class SsbRunnerUtils {
  static async openSsbWindow(ssb: Manifest, initialLaunch: boolean = false) {
    if (!isPwaUrlEligible(ssb.start_url)) {
      // Installed apps predate the install-time policy, so the shared launch
      // boundary must refuse non-loopback plain-HTTP apps as well.
      throw new Error(
        `[SsbRunnerUtils] Refusing to launch ineligible PWA URL: ${ssb.start_url}`,
      );
    }
    // Register before the window opens: enforcement must be in place before
    // the first navigation, which may itself redirect.
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

      // The window opened on about:blank, so no navigation has happened yet:
      // registering enforcement now still precedes the first real navigation.
      registerPwaNavigationPolicy();
      pwaWindows.add(win as object);

      // Enforcement now covers every browsing context this window ever
      // creates, so the real start URL can load — but only for a fresh
      // window. NativeAppRuntime.open can instead return and focus an
      // already-open app window, which must keep its current page.
      try {
        // deno-lint-ignore no-explicit-any
        const browser = (win as any).gBrowser?.selectedBrowser;
        const currentSpec = browser?.currentURI?.spec as string | undefined;
        if (!currentSpec || currentSpec === "about:blank") {
          const startUri = Services.io.newURI(ssb.start_url);
          browser?.loadURI(startUri, {
            triggeringPrincipal: Services.scriptSecurityManager
              .getSystemPrincipal(),
          });
        }
      } catch (error) {
        console.error(
          "[SsbRunnerUtils] Failed to load PWA start URL:",
          error,
        );
      }

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

    if (AppConstants.platform === "linux") {
      const { LinuxSupport } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/supports/Linux.sys.mjs",
      );
      const linuxSupport = new LinuxSupport();
      try {
        // Also repairs launchers installed before the launcher carried the
        // per-PWA app identity, so the taskbar entry is independent.
        await linuxSupport.repair(ssb);
      } catch (error) {
        console.error(
          "[SsbRunnerUtils] Failed to repair Linux launcher:",
          error,
        );
      }
      // The launcher repair above is a file-level fix and runs regardless of
      // the rollout; only the native window integration is experimental.
      if (!TaskbarExperiment.isEnabledForCurrentPlatform()) {
        console.debug(
          "[SsbRunnerUtils] PWA taskbar integration disabled by A/B test, skipping native integration",
        );
        return;
      }
      await linuxSupport.applyOSIntegration(ssb, win);
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
  }

  private static createWindowArgs(ssb: Manifest) {
    // Create extraOptions to set document.documentElement attributes
    const extraOptions = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag2,
    );
    extraOptions.setPropertyAsAString("ssbid", ssb.id);
    if (AppConstants.platform === "linux") {
      const { LinuxSupport } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/supports/Linux.sys.mjs",
      );
      // --name only affects process startup. A window opened in an existing
      // process still needs its own generated launcher identity. Set Gecko's
      // window attribute during creation; leave native taskbar rollout gated.
      extraOptions.setPropertyAsAString(
        "taskbartabclass",
        LinuxSupport.getWindowClass(ssb),
      );
    }
    // Set taskbartab attribute to leverage SessionStore's existing handling
    // This excludes PWA windows from session restore and treats them like TaskBarTabs
    extraOptions.setPropertyAsAString("taskbartab", ssb.id);

    if (ssb.userContextId && ssb.userContextId > 0) {
      extraOptions.setPropertyAsAString(
        "userContextId",
        String(ssb.userContextId),
      );
    }

    // Open on about:blank: navigation enforcement must observe the window's
    // browsing context before the first real navigation, so the actual start
    // URL is loaded explicitly after registration in openSsbWindow.
    const url = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString,
    );
    url.data = "about:blank";

    // Build args array
    const args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    args.appendElement(url);
    args.appendElement(extraOptions);

    if (
      AppConstants.platform === "linux" &&
      Number.isSafeInteger(ssb.userContextId) &&
      (ssb.userContextId ?? 0) > 0
    ) {
      const { PwaContainerExperiment } = ChromeUtils.importESModule(
        "resource://noraneko/modules/pwa/PwaContainerExperiment.sys.mjs",
      );
      const { ContextualIdentityService } = ChromeUtils.importESModule(
        "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
      );
      // Share the exact experiment gate: the control variant, an explicit
      // user disable, or a participation opt-out must all keep containerized
      // startup off. The gate reads persisted state, so it is correct even
      // before Experiments.init() completes during command-line startup.
      const enrolled = PwaContainerExperiment.isEnabled();
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
      if (
        AppConstants.platform === "linux" &&
        cmdLine.state === Ci.nsICommandLine.STATE_INITIAL_LAUNCH
      ) {
        const { LinuxSupport } = ChromeUtils.importESModule(
          "resource://noraneko/modules/pwa/supports/Linux.sys.mjs",
        );
        LinuxSupport.registerBrowserWindowIdentity();
      }
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
