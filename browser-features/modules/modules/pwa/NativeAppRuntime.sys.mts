// SPDX-License-Identifier: MPL-2.0

import type { Manifest } from "#features-chrome/common/pwa/type.ts";
import type {
  NativeAppService,
  NativeAppWindow,
} from "#libs/pwa/nativeAppRuntimeTypes.ts";
import type { AppLifecycleSnapshot } from "#libs/pwa/appLifecycleTypes.ts";
import { AppLifecycle } from "./AppLifecycle.sys.mts";
import { MacAppShimInstaller } from "./MacAppShimInstaller.sys.mts";
import type { MacAppShimInstallerPaths } from "./MacAppShimInstaller.sys.mts";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const CONTRACT = "@floorp.org/mac-web-app-service;1";
const ENABLED = "floorp.browser.nativeApp.appShim.enabled";
const EVENT_TOPIC = "floorp-web-app-shim-event";
const BROWSER_QUIT_TOPIC = "floorp-browser-quit-requested";
const PRESENTATION_FAILED_TOPIC = "floorp-web-app-presentation-failed";
const OBSERVER_TOPICS = [
  EVENT_TOPIC,
  BROWSER_QUIT_TOPIC,
  PRESENTATION_FAILED_TOPIC,
  "domwindowopened",
  "domwindowclosed",
  "quit-application-granted",
] as const;

/**
 * Gecko exposes the owning docShell on browser windows. `libs/` types the
 * window without it, because it is type-checked without the Firefox globals.
 */
type NativeAppWindowWithDocShell = NativeAppWindow & {
  readonly docShell: nsIDocShell;
};

interface Session {
  appId: string;
  windows: Map<string, NativeAppWindow>;
  launching: boolean;
  connected: boolean;
  closing: boolean;
  deferTermination?: boolean;
  terminateSent?: boolean;
  recovery?: Promise<void>;
  lastRecoveryAt?: number;
  waiters: Set<
    { type: string | number; resolve(): void; reject(error: Error): void }
  >;
}

function record(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

class AppLaunchCancelled extends Error {
  constructor() {
    super("Web App launch was cancelled");
    this.name = "AppLaunchCancelled";
  }
}

/** Valid only within the callback that received it from withMutation. */
export interface AppMutationScope {
  readonly appId: string;
}

export class MacNativeAppRuntime {
  private static mutationOwners = new WeakMap<
    AppMutationScope,
    MacNativeAppRuntime
  >();
  private service: NativeAppService | null = null;
  private installer: MacAppShimInstaller | null = null;
  private sessions = new Map<string, Session>();
  private launches = new Map<string, Promise<NativeAppWindow | null>>();
  private nextWindow = 0;
  private mutations = new Map<string, AppMutationScope>();
  private unloadedWindows = new WeakSet<Window>();
  private unregisterLifecycle: (() => void) | null = null;
  private shuttingDown = false;
  private browserClosedForBackground = false;

  constructor(private readonly installerPaths?: MacAppShimInstallerPaths) {}

  isAvailable(): boolean {
    return this.initialize();
  }

  dispose(): void {
    if (this.sessions.size || this.launches.size || this.mutations.size) {
      throw new Error("Close this runtime's Web Apps before disposing it");
    }
    if (!this.service) return;
    for (const topic of OBSERVER_TOPICS) {
      Services.obs.removeObserver(this, topic);
    }
    this.unregisterLifecycle?.();
    this.unregisterLifecycle = null;
    this.installer = null;
    this.service = null;
  }

  ownsWindow(window: Window): boolean {
    return !!this.appIdForWindow(window);
  }

  private appIdForWindow(window: Window): string {
    if (window.closed || this.unloadedWindows.has(window) || !this.service) {
      return "";
    }
    try {
      return this.service.getAppIdForWindow(window);
    } catch {
      return "";
    }
  }

  private trackWindow(window: NativeAppWindow): void {
    const session = this.sessions.get(this.appIdForWindow(window));
    if (!session || [...session.windows.values()].includes(window)) return;
    const id = String(++this.nextWindow);
    session.windows.set(id, window);
  }

  private forgetClosedWindow(window: NativeAppWindow): void {
    // A newly opened chrome window replaces its initial about:blank document.
    // Its unload event does not close the outer window. WindowWatcher reports
    // the actual outer-window removal, including closure after beforeunload.
    this.unloadedWindows.add(window);
    for (const session of this.sessions.values()) {
      for (const [id, tracked] of session.windows) {
        if (tracked !== window) continue;
        session.windows.delete(id);
        // Related windows may exist before their load event arrives.
        this.snapshot();
        if (session.launching && !session.windows.size) session.closing = true;
        if (
          !session.launching && !session.deferTermination &&
          !session.windows.size
        ) {
          this.terminate(session);
        }
        return;
      }
    }
  }

  private requestWindowClose(window: NativeAppWindow): void {
    if (window.closed) return;
    if (window.BrowserCommands) window.BrowserCommands.tryToCloseWindow();
    else window.close();
  }

  private initialize(): boolean {
    if (this.service) return true;
    if (
      AppConstants.platform !== "macosx" ||
      !Services.prefs.getBoolPref(ENABLED, false) || !(CONTRACT in Cc)
    ) return false;
    const interfaces = Ci as unknown as Record<string, nsIID>;
    const service = Cc[CONTRACT].getService(
      interfaces.nsIMacWebAppService,
    ) as unknown as NativeAppService;
    if (service.protocolVersion !== 1) return false;
    const capabilities: unknown = JSON.parse(service.capabilitiesJSON);
    if (
      !record(capabilities) || capabilities.nativeWindowOwnership !== true ||
      capabilities.authenticatedTransport !== true ||
      capabilities.sharedBrowserProfile !== true ||
      capabilities.transactionalInstall !== true
    ) return false;
    this.service = service;
    this.installer = new MacAppShimInstaller(service, this.installerPaths);
    for (const topic of OBSERVER_TOPICS) {
      Services.obs.addObserver(this, topic);
    }
    this.unregisterLifecycle = AppLifecycle.registerAdapter({
      getCapabilities: () =>
        this.service && !this.shuttingDown
          ? {
            protocolVersion: 1,
            sharedProfile: true,
            nativeWindowOwnership: true,
            independentAppQuit: true,
            backgroundSession: true,
          }
          : null,
    });
    return true;
  }

  open(
    ssb: Manifest,
    createWindow: () => Window,
  ): Promise<NativeAppWindow | null> {
    if (this.mutations.has(ssb.id)) {
      return Promise.reject(new Error("Web App is being updated"));
    }
    if (!this.initialize()) return Promise.resolve(null);
    const pending = this.launches.get(ssb.id);
    const existing = this.sessions.get(ssb.id);
    if (existing?.recovery && !existing.closing) {
      return existing.recovery.then(() => this.open(ssb, createWindow));
    }
    if (existing?.closing) {
      return (async () => {
        // A cancelled launch can still be restoring the previous bundle after
        // process exit. Do not race that transaction with a new registration.
        try {
          await pending;
        } catch { /* The previous request reports its error. */ }
        if (this.sessions.get(ssb.id) === existing) {
          await this.waitFor(existing, "disconnected", () => {});
        }
        return this.open(ssb, createWindow);
      })();
    }
    if (pending) return pending;
    const openWindow = existing?.windows.values().next().value;
    if (openWindow && !openWindow.closed) {
      openWindow.focus();
      return Promise.resolve(openWindow);
    }
    const launch = this.launch(ssb, createWindow).finally(() =>
      this.launches.delete(ssb.id)
    );
    this.launches.set(ssb.id, launch);
    return launch;
  }

  private async launch(
    ssb: Manifest,
    createWindow: () => Window,
  ): Promise<NativeAppWindow | null> {
    const service = this.service!;
    const installer = this.installer!;
    const session: Session = {
      appId: ssb.id,
      windows: new Map(),
      launching: true,
      connected: false,
      closing: false,
      waiters: new Set(),
    };
    this.sessions.set(ssb.id, session);
    let transactionId: string | undefined;
    let launchRequested = false;
    Services.startup.enterLastWindowClosingSurvivalArea();
    try {
      const prepared = await installer.prepare(ssb);
      if (!prepared) {
        this.sessions.delete(ssb.id);
        return null;
      }
      transactionId = prepared.transactionId;
      if (this.shuttingDown || session.closing) {
        throw new AppLaunchCancelled();
      }
      service.configure(prepared.profileId);
      service.registerApp(ssb.id, prepared.bundlePath);
      await this.waitFor(session, "connected", () => {
        service.launchApp(ssb.id);
        launchRequested = true;
      });
      let window: NativeAppWindow | null = null;
      // Both observers are registered before synchronous window creation.
      const startup = new Promise<void>((resolve, reject) => {
        const observer: nsIObserver = {
          observe(subject) {
            if (subject !== window) return;
            clearTimeout(timeout);
            Services.obs.removeObserver(
              observer,
              "browser-delayed-startup-finished",
            );
            resolve();
          },
        };
        const timeout = setTimeout(() => {
          Services.obs.removeObserver(
            observer,
            "browser-delayed-startup-finished",
          );
          reject(new Error("Web App browser startup timed out"));
        }, 30000);
        Services.obs.addObserver(observer, "browser-delayed-startup-finished");
      });
      const frame = this.waitFor(session, 111, () => {
        window = service.withWindowContext(ssb.id, {
          createWindow,
        }) as NativeAppWindow;
        this.trackWindow(window);
      });
      await Promise.all([startup, frame]);
      if (
        this.shuttingDown || session.closing || !window ||
        (window as NativeAppWindow).closed
      ) {
        throw new AppLaunchCancelled();
      }
      await installer.commit(ssb.id);
      transactionId = undefined;
      if (
        this.shuttingDown || session.closing ||
        (window as NativeAppWindow).closed
      ) {
        throw new AppLaunchCancelled();
      }
      session.launching = false;
      (window as NativeAppWindow).focus();
      return window;
    } catch (error) {
      const cancelled = error instanceof AppLaunchCancelled ||
        this.shuttingDown || session.closing || this.mutations.has(ssb.id);
      console.error("[NativeAppRuntime] App Shim launch failed:", error);
      session.launching = false;
      for (const window of session.windows.values()) {
        if (!window.closed) window.close();
      }
      try {
        if (launchRequested && this.sessions.get(ssb.id) === session) {
          await this.waitFor(
            session,
            "disconnected",
            () =>
              session.connected
                ? service.sendControl(ssb.id, 40, "{}")
                : service.cancelLaunch(ssb.id),
          );
        }
        this.sessions.delete(session.appId);
        if (transactionId) await installer.rollback(ssb.id, transactionId);
      } catch (rollbackError) {
        console.error("[NativeAppRuntime] Recovery required:", rollbackError);
      }
      if (cancelled) throw new AppLaunchCancelled();
      // Preserve the working legacy launcher when native initialization fails.
      return null;
    } finally {
      session.launching = false;
      Services.startup.exitLastWindowClosingSurvivalArea();
    }
  }

  private waitFor(
    session: Session,
    type: string | number,
    action: () => void,
  ): Promise<void> {
    return new Promise((resolve, reject) => {
      const finish = (error?: Error) => {
        clearTimeout(timeout);
        session.waiters.delete(waiter);
        if (error) reject(error);
        else resolve();
      };
      const waiter = {
        type,
        resolve: () => finish(),
        reject: (error: Error) => finish(error),
      };
      const timeout = setTimeout(
        () => finish(new Error(`App Shim event ${type} timed out`)),
        30000,
      );
      session.waiters.add(waiter);
      try {
        action();
      } catch (error) {
        finish(error instanceof Error ? error : new Error(String(error)));
      }
    });
  }

  async withMutation<T>(
    appId: string,
    operation: (scope: AppMutationScope) => Promise<T>,
  ): Promise<T | null> {
    if (this.mutations.has(appId)) {
      throw new Error("Web App mutation already in progress");
    }
    const scope = Object.freeze({ appId });
    this.mutations.set(appId, scope);
    MacNativeAppRuntime.mutationOwners.set(scope, this);
    try {
      if (!await this.stopForMutation(appId)) return null;
      return await operation(scope);
    } finally {
      MacNativeAppRuntime.mutationOwners.delete(scope);
      this.mutations.delete(appId);
    }
  }

  assertMutationScope(appId: string, scope: AppMutationScope): void {
    const owner = MacNativeAppRuntime.mutationOwners.get(scope);
    if (scope.appId !== appId || owner?.mutations.get(appId) !== scope) {
      throw new Error("Web App mutation scope is invalid or has expired");
    }
  }

  async stopForMutation(appId: string): Promise<boolean> {
    const session = this.sessions.get(appId);
    if (!session) return true;
    if (session.recovery) {
      await session.recovery;
      if (this.sessions.get(appId) !== session) return true;
    }
    if (session.launching) {
      session.deferTermination = true;
      try {
        this.snapshot();
        for (const window of session.windows.values()) {
          this.requestWindowClose(window);
        }
        if ([...session.windows.values()].some((window) => !window.closed)) {
          return false;
        }
        session.closing = true;
        try {
          this.service?.cancelLaunch(appId);
        } catch { /* Not launched yet. */ }
        try {
          await this.launches.get(appId);
        } catch (error) {
          if (!(error instanceof AppLaunchCancelled)) throw error;
        }
        return !this.sessions.has(appId);
      } finally {
        session.deferTermination = false;
      }
    }
    session.deferTermination = true;
    try {
      this.snapshot();
      for (const window of session.windows.values()) {
        this.requestWindowClose(window);
      }
      if ([...session.windows.values()].some((window) => !window.closed)) {
        return false;
      }
      await this.waitFor(
        session,
        "disconnected",
        () => this.terminate(session),
      );
      return true;
    } finally {
      session.deferTermination = false;
    }
  }

  private terminate(session: Session): void {
    if (this.sessions.get(session.appId) !== session || session.terminateSent) {
      return;
    }
    session.closing = true;
    session.terminateSent = true;
    for (const waiter of [...session.waiters]) {
      if (waiter.type !== "disconnected") {
        waiter.reject(new Error("App Shim is closing"));
      }
    }
    try {
      if (session.connected) this.service?.sendControl(session.appId, 40, "{}");
      else this.service?.cancelLaunch(session.appId);
    } catch (error) {
      session.terminateSent = false;
      console.error("[NativeAppRuntime] Shim termination failed:", error);
    }
  }

  private maybeQuitBackground(): void {
    Services.tm.dispatchToMainThread(() => {
      if (
        !this.browserClosedForBackground || this.shuttingDown ||
        this.sessions.size
      ) return;
      const snapshot = this.snapshot().policy;
      if (!snapshot.windows.length && !snapshot.pendingLaunches.length) {
        Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit!);
      }
    });
  }

  private forceCloseOrphan(window: NativeAppWindow): void {
    if (window.closed) return;
    // The native process is gone, so a beforeunload dialog has no owner to
    // display it. Only used when restoring the same live docshell has failed.
    const docShell = (window as NativeAppWindowWithDocShell).docShell;
    docShell.treeOwner.QueryInterface!(Ci.nsIBaseWindow).destroy();
  }

  private async recover(session: Session): Promise<void> {
    let requested = false;
    Services.startup.enterLastWindowClosingSurvivalArea();
    try {
      if (
        session.lastRecoveryAt && Date.now() - session.lastRecoveryAt < 60000
      ) {
        throw new Error(
          "App Shim exited repeatedly; automatic restart stopped",
        );
      }
      session.lastRecoveryAt = Date.now();
      // Existing widgets recreate their native windows on authentication and
      // republish their compositor surfaces. No page or profile is recreated.
      await Promise.all([
        this.waitFor(session, 111, () => {}),
        this.waitFor(
          session,
          "connected",
          () => {
            this.service!.launchApp(session.appId);
            requested = true;
          },
        ),
      ]);
      session.windows.values().next().value?.focus();
    } catch (error) {
      if (this.shuttingDown || session.closing) return;
      console.error("[NativeAppRuntime] App window recovery failed:", error);
      session.closing = true;
      for (const window of [...session.windows.values()]) {
        try {
          this.forceCloseOrphan(window);
        } catch (closeError) {
          console.error(
            "[NativeAppRuntime] Orphan window cleanup failed:",
            closeError,
          );
        }
      }
      if (requested && this.sessions.get(session.appId) === session) {
        try {
          await this.waitFor(
            session,
            "disconnected",
            () => this.terminate(session),
          );
        } catch (stopError) {
          console.error(
            "[NativeAppRuntime] Recovery process cleanup is pending:",
            stopError,
          );
        }
      } else {
        this.sessions.delete(session.appId);
        this.maybeQuitBackground();
      }
      Services.obs.notifyObservers(
        Services.appinfo,
        "floorp-web-app-shim-recovery-failed",
        session.appId,
      );
    } finally {
      Services.startup.exitLastWindowClosingSurvivalArea();
    }
  }

  private snapshot(): {
    policy: AppLifecycleSnapshot;
    windows: Map<string, NativeAppWindow>;
  } {
    const windows = new Map<string, NativeAppWindow>();
    const policyWindows: AppLifecycleSnapshot["windows"][number][] = [];
    for (const entry of Services.wm.getEnumerator("")) {
      const window = entry as unknown as NativeAppWindow;
      if (window.closed || this.unloadedWindows.has(window)) continue;
      const appId = this.appIdForWindow(window);
      if (appId) this.trackWindow(window);
      else if (
        window.document.documentElement?.getAttribute("windowtype") !==
          "navigator:browser"
      ) continue;
      const id = String(windows.size + 1);
      windows.set(id, window);
      policyWindows.push(
        appId ? { id, kind: "web-app", appId } : { id, kind: "browser" },
      );
    }
    return {
      windows,
      policy: {
        windows: policyWindows,
        // Once a live Gecko window exists, app quit must ask beforeunload
        // before terminating its native owner, including during recovery.
        pendingLaunches: [...this.sessions.values()].filter((s) =>
          s.launching && !s.windows.size
        )
          .map((s) => ({ id: s.appId, appId: s.appId })),
      },
    };
  }

  private closeWindows(appId?: string): boolean {
    const snapshot = this.snapshot();
    const plan = AppLifecycle.planQuit(
      appId ? { kind: "web-app", appId } : { kind: "browser" },
      snapshot.policy,
    );
    if (plan.kind === "noop") return true;
    if (plan.kind !== "close-windows") return false;
    if (!appId) this.browserClosedForBackground = true;
    Services.startup.enterLastWindowClosingSurvivalArea();
    try {
      for (const id of plan.cancelLaunchIds) {
        const session = this.sessions.get(id);
        if (session) {
          session.closing = true;
          this.terminate(session);
        }
      }
      for (const id of plan.windowIds) {
        const window = snapshot.windows.get(id);
        if (window) this.requestWindowClose(window);
      }
      if (appId) {
        const session = this.sessions.get(appId);
        if (session) {
          if ([...session.windows.values()].some((w) => !w.closed)) {
            this.service?.sendControl(appId, 41, "{}");
          } else this.terminate(session);
        }
      }
    } finally {
      Services.startup.exitLastWindowClosingSurvivalArea();
    }
    return true;
  }

  observe(
    subject: nsISupports | null,
    topic: string,
    data?: string | null,
  ): void {
    if (topic === PRESENTATION_FAILED_TOPIC && data) {
      const session = this.sessions.get(data);
      if (!session || session.closing || this.shuttingDown) return;
      if (session.launching || session.recovery) {
        for (const waiter of [...session.waiters]) {
          if (waiter.type === 111) {
            waiter.reject(new Error("App Shim presentation failed"));
          }
        }
      } else if (session.connected) {
        // Restart the native owner while preserving the existing Gecko page.
        this.service?.sendControl(data, 40, "{}");
      }
      return;
    }
    if (topic === "domwindowopened" && subject) {
      const window = subject as unknown as NativeAppWindow;
      this.trackWindow(window);
      window.addEventListener("load", () => this.trackWindow(window), {
        once: true,
      });
      return;
    }
    if (topic === "domwindowclosed" && subject) {
      this.forgetClosedWindow(subject as unknown as NativeAppWindow);
      return;
    }
    if (topic === "quit-application-granted") {
      this.shuttingDown = true;
      for (const session of [...this.sessions.values()]) {
        this.terminate(session);
      }
      this.unregisterLifecycle?.();
      this.unregisterLifecycle = null;
      this.service?.stop();
      return;
    }
    if (topic === BROWSER_QUIT_TOPIC) {
      if (subject && !this.shuttingDown && this.closeWindows()) {
        subject.QueryInterface!(Ci.nsISupportsPRBool).data = true;
      }
      return;
    }
    if (topic !== EVENT_TOPIC || !data) return;
    try {
      const event: unknown = JSON.parse(data);
      if (!record(event) || typeof event.appId !== "string") return;
      const session = this.sessions.get(event.appId);
      if (!session) return;
      if (event.type === "connected") session.connected = true;
      if (event.type === "disconnected") {
        session.connected = false;
        for (const waiter of [...session.waiters]) {
          if (waiter.type === "disconnected") waiter.resolve();
          else waiter.reject(new Error("App Shim connection closed"));
        }
        if (
          !session.launching && !session.closing && !this.shuttingDown &&
          session.windows.size
        ) {
          if (!session.recovery) {
            session.recovery = this.recover(session).finally(() => {
              session.recovery = undefined;
            });
          }
          return;
        }
        this.sessions.delete(session.appId);
        this.maybeQuitBackground();
        return;
      }
      for (const waiter of [...session.waiters]) {
        if (waiter.type === event.type) waiter.resolve();
      }
      if (event.type === 102) this.closeWindows(event.appId);
      if (event.type === 103) session.windows.values().next().value?.focus();
      if (
        event.type === 104 && record(event.payload) &&
        typeof event.payload.command === "string"
      ) {
        const commands: Record<string, string> = {
          copy: "cmd_copy",
          cut: "cmd_cut",
          paste: "cmd_paste",
          selectAll: "cmd_selectAll",
          undo: "cmd_undo",
          redo: "cmd_redo",
        };
        const command = commands[event.payload.command];
        const window = [...session.windows.values()].find((w) =>
          w.document.hasFocus()
        ) ?? session.windows.values().next().value;
        if (command && window) window.goDoCommand?.(command);
      }
    } catch (error) {
      console.error("[NativeAppRuntime] Shim event failed:", error);
    }
  }
}

export const NativeAppRuntime = new MacNativeAppRuntime();
