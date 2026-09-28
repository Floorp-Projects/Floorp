// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  type AppMutationScope,
  MacNativeAppRuntime,
  NativeAppRuntime,
} from "../NativeAppRuntime.sys.mts";
import { MacAppShimInstaller } from "../MacAppShimInstaller.sys.mts";
import { AppRegistry } from "../AppRegistry.sys.mts";
import { getMacAppBundle, MacOSSupport } from "../supports/MacOS.sys.mts";
import type { Manifest } from "../type.ts";
import type { NativeAppService } from "#libs/pwa/nativeAppRuntimeTypes.ts";
import { buildSsbKey } from "#libs/pwa/ssbKeyUtils.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../../chrome/test/utils/test_harness.ts";

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const EVENT_TOPIC = "floorp-web-app-shim-event";

function within<T>(promise: Promise<T>, milliseconds: number, label: string) {
  return new Promise<T>((resolve, reject) => {
    const timer = setTimeout(
      () => reject(new Error(`${label} timed out`)),
      milliseconds,
    );
    promise.then(
      (value) => {
        clearTimeout(timer);
        resolve(value);
      },
      (error) => {
        clearTimeout(timer);
        reject(error);
      },
    );
  });
}

async function nativeLaunchRegression(
  scenario:
    | "prepare-cancel"
    | "prepare-uninstall"
    | "pre-enrollment-uninstall"
    | "discovery-reopen"
    | "commit-close"
    | "commit-veto"
    | "initial-document-unload",
): Promise<void> {
  const uninstallScenario = scenario === "prepare-uninstall" ||
    scenario === "pre-enrollment-uninstall" || scenario === "discovery-reopen";
  const beforeEnrollment = scenario === "pre-enrollment-uninstall" ||
    scenario === "discovery-reopen";
  const pauseBeforeLaunch = scenario === "prepare-cancel" || uninstallScenario;
  const contract = "@floorp.org/mac-web-app-service;1";
  if (Services.appinfo.OS !== "Darwin" || !(contract in Cc)) {
    console.info(
      `[NativeAppRuntime.test] SKIP ${scenario}: native Runtime unavailable`,
    );
    return;
  }
  const service = Cc[contract].getService(
    (Ci as unknown as Record<string, nsIID>).nsIMacWebAppService,
  ) as unknown as NativeAppService;
  const capabilities = JSON.parse(service.capabilitiesJSON) as Record<
    string,
    unknown
  >;
  if (
    service.protocolVersion !== 1 ||
    [
      "authenticatedTransport",
      "sharedBrowserProfile",
      "nativeWindowOwnership",
      "transactionalInstall",
    ].some((name) => capabilities[name] !== true)
  ) {
    console.info(
      `[NativeAppRuntime.test] SKIP ${scenario}: required native capabilities unavailable`,
    );
    return;
  }

  const pref = "floorp.browser.nativeApp.appShim.enabled";
  const hadValue = Services.prefs.prefHasUserValue(pref);
  const oldValue = Services.prefs.getBoolPref(pref, false);
  const root = PathUtils.join(
    PathUtils.tempDir,
    `floorp-shim-cancellation-${crypto.randomUUID()}`,
  );
  const options = {
    profileDir: PathUtils.profileDir,
    applicationsDir: PathUtils.join(root, "Applications"),
    executable: Services.dirsvc.get("XREExeF", Ci.nsIFile).path,
  };
  const app: Manifest = {
    id: crypto.randomUUID(),
    name: "Native Cancellation Test",
    start_url: "https://example.invalid/",
    icon: "",
    userContextId: 0,
  };
  const bundle = await getMacAppBundle(app, options);
  const store = {
    getCurrentSsbData: () =>
      Promise.resolve({ [buildSsbKey(app.start_url, 0)]: app }),
    saveSsbData: () => Promise.resolve(),
    removeSsbData: () => Promise.resolve(),
  };
  const runtime = new MacNativeAppRuntime({
    profileDirectory: options.profileDir,
    applicationsDirectory: options.applicationsDir,
    browserExecutable: options.executable,
    shimExecutable: PathUtils.join(
      Services.dirsvc.get("GreBinD", Ci.nsIFile).path,
      "floorp-app-shim",
    ),
  });
  const prepared = Promise.withResolvers<void>();
  const release = Promise.withResolvers<void>();
  const preparingAgain = Promise.withResolvers<void>();
  const committed = Promise.withResolvers<void>();
  const mutating = Promise.withResolvers<void>();
  const discovering = Promise.withResolvers<void>();
  const releaseDiscovery = Promise.withResolvers<void>();
  const originalPrepare = MacAppShimInstaller.prototype.prepare;
  const originalCommit = MacAppShimInstaller.prototype.commit;
  const originalRollback = MacAppShimInstaller.prototype.rollback;
  const originalMutation = NativeAppRuntime.withMutation;
  const originalDiscovery = MacOSSupport.prototype.findNativeAppBundle;
  const operations: Promise<unknown>[] = [];
  const windows: Window[] = [];
  let attempts = 0;
  let discoveryPaused = false;
  let connected = 0;
  let presented = 0;
  let peerRunning = false;
  let rollbackCalls = 0;
  const documentUnloads: {
    closed: boolean;
    documentURI: string | undefined;
  }[] = [];
  let cleanupPage = () => {};
  let cleanupPrompt = () => {};
  const stopped = new Set<() => void>();
  const observer: nsIObserver = {
    observe(_subject, _topic, data) {
      const event = JSON.parse(data!) as {
        appId: string;
        type: string | number;
        payload?: { pid?: number };
      };
      if (event.appId !== app.id) return;
      if (event.type === "connected") {
        connected++;
        peerRunning = true;
      }
      if (event.type === 111) presented++;
      if (event.type === "disconnected") {
        peerRunning = false;
        for (const resolve of stopped) resolve();
        stopped.clear();
      }
    },
  };
  let observing = false;
  let initialized = false;
  try {
    Services.prefs.setBoolPref(pref, true);
    await new MacOSSupport(options).install(app, store);
    assert(runtime.isAvailable(), "real native Runtime is available");
    initialized = true;
    Services.obs.addObserver(observer, EVENT_TOPIC);
    observing = true;
    MacAppShimInstaller.prototype.prepare = async function (manifest) {
      if (manifest.id !== app.id || !pauseBeforeLaunch) {
        return originalPrepare.call(this, manifest);
      }
      attempts++;
      if (attempts !== 1) {
        preparingAgain.resolve();
        return originalPrepare.call(this, manifest);
      }
      if (beforeEnrollment) {
        prepared.resolve();
        await release.promise;
        return originalPrepare.call(this, manifest);
      }
      const candidate = await originalPrepare.call(this, manifest);
      assert(candidate?.transactionId, "prepare created a real signed bundle");
      prepared.resolve();
      await release.promise;
      return candidate;
    };
    MacAppShimInstaller.prototype.commit = async function (appId) {
      await originalCommit.call(this, appId);
      if (appId === app.id && !pauseBeforeLaunch) {
        committed.resolve();
        await release.promise;
      }
    };
    MacAppShimInstaller.prototype.rollback = function (appId, transactionId) {
      if (appId === app.id) rollbackCalls++;
      return originalRollback.call(this, appId, transactionId);
    };
    if (uninstallScenario) {
      // Maintenance targets the production singleton. Route only this fixture's
      // UUID to the real runtime instance configured with its disposable paths.
      // Both instances still use the unchanged authenticated native service.
      NativeAppRuntime.withMutation = function <T>(
        appId: string,
        operation: (scope: AppMutationScope) => Promise<T>,
      ): Promise<T | null> {
        if (appId === app.id) {
          mutating.resolve();
          return runtime.withMutation(appId, operation);
        }
        return originalMutation.bind(NativeAppRuntime)(appId, operation);
      };
    }
    if (scenario === "discovery-reopen") {
      MacOSSupport.prototype.findNativeAppBundle = async function (manifest) {
        const result = await originalDiscovery.call(this, manifest);
        if (manifest.id === app.id && !discoveryPaused) {
          discoveryPaused = true;
          discovering.resolve();
          await releaseDiscovery.promise;
        }
        return result;
      };
    }
    const { BrowserWindowTracker } = ChromeUtils.importESModule(
      "resource:///modules/BrowserWindowTracker.sys.mjs",
    );
    const createWindow = () => {
      const args = Cc["@mozilla.org/supports-string;1"].createInstance(
        Ci.nsISupportsString,
      );
      args.data = "about:blank";
      const window = BrowserWindowTracker.openWindow({
        args,
        features: "width=640,height=480",
        remote: scenario !== "commit-veto",
        fission: scenario !== "commit-veto",
      });
      if (scenario === "initial-document-unload") {
        window.addEventListener("unload", (event: Event) => {
          documentUnloads.push({
            closed: window.closed,
            documentURI: (event.target as Document | null)?.documentURI,
          });
        }, { once: true });
      }
      windows.push(window);
      return window;
    };
    const first = runtime.open(app, createWindow).then(
      (window) => ({ window, error: null }),
      (error: unknown) => ({ window: null, error }),
    );
    operations.push(first);
    await within(
      Promise.race([
        pauseBeforeLaunch ? prepared.promise : committed.promise,
        first.then((result) => {
          throw result.error ??
            new Error("Launch ended before the fixture pause");
        }),
      ]),
      pauseBeforeLaunch ? 30000 : 45000,
      "real installation fixture pause",
    );
    if (uninstallScenario) {
      if (beforeEnrollment) {
        assert(
          !(await AppRegistry.getForProfile().read())?.apps.some((entry) =>
            entry.installId === app.id
          ),
          "pending launch has not enrolled in native metadata",
        );
      }
      const removal = new MacOSSupport(options).uninstall(app, store);
      operations.push(removal);
      await within(mutating.promise, 5000, "queued native uninstall handoff");
      release.resolve();
      const result = await within(
        first,
        15000,
        "uninstall cancels preparation",
      );
      assert(
        result.error instanceof Error &&
          result.error.name === "AppLaunchCancelled",
        "uninstall stops the pending launch before native registration",
      );
      if (scenario === "discovery-reopen") {
        await within(discovering.promise, 5000, "mutation-held discovery");
        const reopen = await runtime.open(app, createWindow).then(
          () => null,
          (error: unknown) => error,
        );
        assert(
          reopen instanceof Error &&
            reopen.message === "Web App is being updated",
          "reopen cannot pass the mutation guard while native classification waits",
        );
        releaseDiscovery.resolve();
      }
      await within(
        removal,
        15000,
        "uninstall after cancelled prepare rollback",
      );
      assert(
        !await IOUtils.exists(bundle.path),
        "owned legacy bundle is removed",
      );
      assertEquals(connected, 0, "cancelled uninstall does not launch a peer");
      assertEquals(windows.length, 0, "cancelled uninstall opens no window");
      assert(
        !(await AppRegistry.getForProfile().read())?.apps.some((entry) =>
          entry.installId === app.id
        ),
        "completed uninstall removes the restored legacy registration",
      );
      return;
    }
    if (!pauseBeforeLaunch) {
      const window = windows[0];
      assert(
        window && !window.closed,
        "commit has a live native browser window",
      );
      assertEquals(connected, 1, "commit follows authenticated launch");
      assert(presented > 0, "commit follows actual frame presentation");
      const installation = (await AppRegistry.getForProfile().read())!.apps
        .find(
          (entry) => entry.installId === app.id,
        )!;
      const receipt = installation.installedShim;
      assert(receipt, "the gated commit performed a real durable installation");
      const fingerprint = service.fingerprintBundle(bundle.path);
      if (scenario === "commit-close") {
        const exit = new Promise<void>((resolve) => stopped.add(resolve));
        window.close();
        assert(
          window.closed,
          "close reaches the browser during pending commit",
        );
        release.resolve();
        const result = await within(first, 15000, "closed launch cancellation");
        assert(
          result.error instanceof Error &&
            result.error.name === "AppLaunchCancelled",
          "closing during commit rejects instead of returning a closed window",
        );
        await within(exit, 10000, "committed app process exit");
        assert(!peerRunning, "closed app leaves no running Shim");
      } else if (scenario === "initial-document-unload") {
        assert(
          documentUnloads.some((event) => !event.closed),
          "browser startup unloads a document while its outer window remains open",
        );
        assert(
          !window.closed && peerRunning && runtime.ownsWindow(window),
          "document replacement preserves the live native owner",
        );
        release.resolve();
        const result = await within(
          first,
          15000,
          "initial document replacement",
        );
        assertEquals(
          result.error,
          null,
          "document unload does not cancel launch",
        );
        assertEquals(
          result.window,
          window,
          "launch keeps the same outer window",
        );
      } else {
        const browser = (window as unknown as {
          gBrowser: {
            selectedBrowser: {
              isRemoteBrowser: boolean;
              contentWindow: Window;
            };
          };
        }).gBrowser.selectedBrowser;
        assert(
          !browser.isRemoteBrowser,
          "this prompt fixture uses supported in-process tabs",
        );
        const content = browser.contentWindow;
        const input = content.document.createElement("input");
        input.value = "preserve unsaved work";
        content.document.body.append(input);
        let beforeUnload = 0;
        let prompts = 0;
        const protect = (event: Event) => {
          beforeUnload++;
          event.preventDefault();
          (event as BeforeUnloadEvent).returnValue = "Unsaved work";
        };
        content.addEventListener("beforeunload", protect);
        cleanupPage = () =>
          content.removeEventListener("beforeunload", protect);
        const userInput =
          (content as unknown as { windowUtils: nsIDOMWindowUtils })
            .windowUtils.setHandlingUserInput(true);
        userInput.destruct();
        const promptObserver: nsIObserver = {
          observe(subject) {
            const prompt = subject as unknown as Window & {
              Dialog: { ui: { button1: HTMLElement } };
            };
            if (prompt.opener !== window && prompt.opener !== content) return;
            prompts++;
            prompt.Dialog.ui.button1.click();
          },
        };
        Services.obs.addObserver(promptObserver, "common-dialog-loaded");
        cleanupPrompt = () => {
          Services.obs.removeObserver(promptObserver, "common-dialog-loaded");
          cleanupPrompt = () => {};
        };
        assertEquals(
          await within(
            runtime.stopForMutation(app.id),
            10000,
            "beforeunload veto",
          ),
          false,
          "pending launch mutation respects the live page's veto",
        );
        assert(
          beforeUnload > 0 && prompts > 0,
          "a real beforeunload prompt was cancelled",
        );
        assert(
          !window.closed && peerRunning,
          "veto preserves browser and native owner",
        );
        assertEquals(
          input.value,
          "preserve unsaved work",
          "veto preserves page state",
        );
        cleanupPage();
        cleanupPrompt();
        release.resolve();
        const result = await within(first, 15000, "vetoed launch completion");
        assertEquals(
          result.error,
          null,
          "veto does not cancel the pending launch",
        );
        assertEquals(
          result.window,
          window,
          "launch completes with the same page",
        );
      }
      assertEquals(
        rollbackCalls,
        0,
        "successful commit is never rolled back by cancellation",
      );
      assertEquals(
        service.fingerprintBundle(bundle.path),
        fingerprint,
        "committed bundle remains intact",
      );
      assertEquals(
        (await AppRegistry.getForProfile().read())!.apps.find((entry) =>
          entry.installId === app.id
        )
          ?.installedShim?.transactionId,
        receipt!.transactionId,
        "committed installation receipt is preserved",
      );
      return;
    }
    assertEquals(
      connected,
      0,
      "paused preparation has not authenticated a peer",
    );
    assertEquals(
      windows.length,
      0,
      "paused preparation has not opened a window",
    );

    const stop = runtime.stopForMutation(app.id);
    operations.push(stop.catch(() => {}));
    // stopForMutation marks the launch closing synchronously; this request
    // deliberately avoids withMutation so it exercises the closing-session wait.
    const reopened = runtime.open(app, createWindow);
    operations.push(reopened.catch(() => {}));
    release.resolve();

    const cancelled = await within(first, 15000, "cancelled launch rollback");
    assert(
      cancelled.error instanceof Error &&
        cancelled.error.name === "AppLaunchCancelled",
      "the first launch rejects cancellation instead of returning legacy fallback",
    );
    assertEquals(
      await within(stop, 5000, "cancel stop"),
      true,
      "stop resolves",
    );
    await within(
      preparingAgain.promise,
      5000,
      "reopen preparation without a stale disconnected wait",
    );
    const window = await within(reopened, 45000, "authenticated reopen frame");
    assert(window && !window.closed, "reopen returns a live native window");
    assertEquals(
      windows.length,
      1,
      "cancelled launch never creates a fallback",
    );
    assertEquals(connected, 1, "only the reopened launch authenticates");
    assert(
      presented > 0,
      "real authenticated Shim acknowledges a rendered frame",
    );
    assertEquals(
      service.getAppIdForWindow(window!),
      app.id,
      "native app owns window",
    );
    assertEquals(
      (await AppRegistry.getForProfile().read())?.apps.find((entry) =>
        entry.installId === app.id
      )?.integration,
      "app-shim",
      "reopened launch commits the native transaction",
    );
  } finally {
    cleanupPage();
    cleanupPrompt();
    MacAppShimInstaller.prototype.prepare = originalPrepare;
    MacAppShimInstaller.prototype.commit = originalCommit;
    MacAppShimInstaller.prototype.rollback = originalRollback;
    NativeAppRuntime.withMutation = originalMutation;
    MacOSSupport.prototype.findNativeAppBundle = originalDiscovery;
    release.resolve();
    releaseDiscovery.resolve();
    try {
      await within(Promise.allSettled(operations), 65000, "launch cleanup");
      if (initialized) {
        assert(
          await within(runtime.stopForMutation(app.id), 35000, "fixture stop"),
          "fixture window permits closing",
        );
      }
      for (const window of windows) {
        if (!window.closed) window.close();
      }
      if (peerRunning) {
        const exit = new Promise<void>((resolve) => stopped.add(resolve));
        service.cancelLaunch(app.id);
        await within(exit, 30000, "fixture process exit");
      }
      await IOUtils.remove(root, { recursive: true, ignoreAbsent: true });
      await AppRegistry.getForProfile().forgetApp(app.id, bundle.path);
      const digest = new Uint8Array(
        await crypto.subtle.digest(
          "SHA-256",
          new TextEncoder().encode(app.id).buffer as ArrayBuffer,
        ),
      );
      const token = Array.from(
        digest,
        (byte) => byte.toString(16).padStart(2, "0"),
      ).join("");
      const journal = PathUtils.join(
        PathUtils.profileDir,
        "ssb",
        "app-shim-transactions",
        `${token}.json`,
      );
      await IOUtils.remove(journal, { ignoreAbsent: true });
      await IOUtils.remove(`${journal}.tmp`, { ignoreAbsent: true });
    } finally {
      stopped.clear();
      if (observing) Services.obs.removeObserver(observer, EVENT_TOPIC);
      try {
        if (initialized) runtime.dispose();
      } finally {
        if (hadValue) Services.prefs.setBoolPref(pref, oldValue);
        else Services.prefs.clearUserPref(pref);
      }
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("NativeAppRuntime.test.mts", [
    {
      name:
        "mutation scopes reject forgery, another app and use after completion",
      async fn() {
        const runtime = new MacNativeAppRuntime();
        const appId = crypto.randomUUID();
        const rejected = (scope: AppMutationScope, id = appId) => {
          try {
            runtime.assertMutationScope(id, scope);
            return false;
          } catch {
            return true;
          }
        };
        assert(rejected({ appId }), "matching fields cannot forge a scope");
        let expired: AppMutationScope | null = null;
        await runtime.withMutation(appId, (scope) => {
          runtime.assertMutationScope(appId, scope);
          assert(
            rejected(scope, crypto.randomUUID()),
            "scope belongs to one app",
          );
          expired = scope;
          return Promise.resolve();
        });
        assert(
          expired && rejected(expired),
          "completed scope cannot be reused",
        );
        runtime.dispose();
      },
    },
    {
      name: "uninstall cancels a launch before native registry enrollment",
      fn: () => nativeLaunchRegression("pre-enrollment-uninstall"),
    },
    {
      name: "reopen remains blocked during uninstall bundle discovery",
      fn: () => nativeLaunchRegression("discovery-reopen"),
    },
    {
      name:
        "uninstall during native preparation waits for rollback without deadlock",
      fn: () => nativeLaunchRegression("prepare-uninstall"),
    },
    {
      name: "initial chrome document unload preserves the native window launch",
      fn: () => nativeLaunchRegression("initial-document-unload"),
    },
    {
      name:
        "cancelled preparation rejects and immediate reopen authenticates without a stale wait",
      fn: () => nativeLaunchRegression("prepare-cancel"),
    },
    {
      name:
        "closing during commit stops the native process and preserves the installed receipt",
      fn: () => nativeLaunchRegression("commit-close"),
    },
    {
      name: "mutation during pending commit honours a real beforeunload veto",
      fn: () => nativeLaunchRegression("commit-veto"),
    },
  ]);
}
