/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  ChromeExtensionUpdater,
  ChromeExtensionUpdaterClass,
} from "./ChromeExtensionUpdater.sys.mts";
import {
  notifyChromeUpdateStatus,
  ChromeExtensionUpdateRegistry,
  ChromeExtensionUpdateRegistryClass,
  type ChromeUpdateEligibility,
  type UpdateRegistryAddon,
} from "./ChromeExtensionUpdateRegistry.sys.mts";

export interface ChromeAutomaticUpdateStatus {
  addonId: string;
  eligibility: ChromeUpdateEligibility["eligibility"];
  enabled: boolean;
  policyAllowed: boolean;
  state:
    | "idle"
    | "checking"
    | "up-to-date"
    | "available"
    | "waiting-for-window"
    | "installing"
    | "installed"
    | "postponed"
    | "error"
    | "migration-required";
  lastCheck?: number;
  nextCheck?: number;
  availableVersion?: string;
  error?: string;
}

interface SchedulerManager {
  getAddonByID(id: string): Promise<UpdateRegistryAddon | null>;
  shouldAutoUpdate(addon: UpdateRegistryAddon): boolean;
  addAddonListener(listener: object): void;
  removeAddonListener(listener: object): void;
}

interface SchedulerOptions {
  registry?: ChromeExtensionUpdateRegistryClass;
  updater?: ChromeExtensionUpdaterClass;
  manager?: SchedulerManager;
  browser?: () => Element | null;
  now?: () => number;
  intervalMs?: number;
  retryMs?: number;
  startupDelayMs?: number;
  timerIntervalMs?: number;
}

const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
const DAY = 24 * 60 * 60 * 1000;

function nativeManager(): SchedulerManager {
  return (
    ChromeUtils.importESModule(
      "resource://gre/modules/AddonManager.sys.mjs",
    ) as unknown as {
      AddonManager: SchedulerManager;
    }
  ).AddonManager;
}

function currentBrowser(): Element | null {
  const win = Services.wm.getMostRecentWindow(
    "navigator:browser",
  ) as unknown as {
    closed: boolean;
    gBrowser?: { selectedBrowser: Element };
  } | null;
  const browser = !win?.closed && win?.gBrowser?.selectedBrowser;
  return browser && browser.isConnected ? browser : null;
}

export class ChromeExtensionUpdateSchedulerClass {
  private readonly registry: ChromeExtensionUpdateRegistryClass;
  private readonly updater: ChromeExtensionUpdaterClass;
  private readonly manager: SchedulerManager;
  private readonly getBrowser: () => Element | null;
  private readonly now: () => number;
  private readonly interval: number;
  private readonly retry: number;
  private readonly startupDelay: number;
  private readonly timerInterval: number;
  private readonly states = new Map<
    string,
    Partial<ChromeAutomaticUpdateStatus>
  >();
  private timer?: nsITimer;
  private running?: Promise<void>;
  private started = false;

  constructor(options: SchedulerOptions = {}) {
    this.registry = options.registry ?? ChromeExtensionUpdateRegistry;
    this.updater = options.updater ?? ChromeExtensionUpdater;
    this.manager = options.manager ?? nativeManager();
    this.getBrowser = options.browser ?? currentBrowser;
    this.now = options.now ?? Date.now;
    this.interval = options.intervalMs ?? DAY;
    this.retry = options.retryMs ?? 15 * 60 * 1000;
    this.startupDelay = options.startupDelayMs ?? 60 * 1000;
    this.timerInterval = options.timerIntervalMs ?? 60 * 60 * 1000;
  }

  private publish(
    addonId: string,
    value: Partial<ChromeAutomaticUpdateStatus>,
  ): void {
    this.states.set(addonId, value);
    notifyChromeUpdateStatus(addonId);
  }

  async getStatus(addonId: string): Promise<ChromeAutomaticUpdateStatus> {
    const eligibility = await this.registry.getEligibility(addonId);
    const state = this.states.get(addonId);
    const addon = await this.manager.getAddonByID(addonId);
    const policyAllowed =
      !!addon &&
      Services.prefs.getBoolPref("extensions.update.enabled", true) &&
      this.manager.shouldAutoUpdate(addon);
    const currentState =
      state?.state === "postponed" && !eligibility.pendingVersion
        ? eligibility.registration?.version === state.availableVersion
          ? "installed"
          : "idle"
        : state?.state;
    return {
      addonId,
      eligibility: eligibility.eligibility,
      enabled: eligibility.enabled,
      policyAllowed,
      state: eligibility.pendingVersion
        ? "postponed"
        : eligibility.eligibility === "verified"
          ? (currentState ?? "idle")
          : "migration-required",
      ...(eligibility.registration?.lastCheck !== undefined
        ? { lastCheck: eligibility.registration.lastCheck }
        : {}),
      ...(eligibility.registration?.nextCheck !== undefined
        ? { nextCheck: eligibility.registration.nextCheck }
        : {}),
      ...(state?.availableVersion
        ? { availableVersion: state.availableVersion }
        : {}),
      ...(state?.error ? { error: state.error } : {}),
    };
  }

  async setAutomaticUpdates(
    addonId: string,
    enabled: boolean,
  ): Promise<ChromeAutomaticUpdateStatus> {
    await this.registry.setEnabled(addonId, enabled);
    if (enabled && this.started) {
      if (this.timer) clearTimeout(this.timer);
      this.arm(0);
    }
    return await this.getStatus(addonId);
  }

  private readonly addonListener = {
    onUninstalled: (addon: { id: string }) => {
      this.states.delete(addon.id);
      void this.registry
        .remove(addon.id)
        .catch((error) =>
          console.error("[ChromeExtensionUpdateScheduler]", error),
        );
    },
    onInstalled: (addon: { id: string }) => {
      void this.registry
        .getEligibility(addon.id)
        .then(() => {
          notifyChromeUpdateStatus(addon.id);
        })
        .catch((error) =>
          console.error("[ChromeExtensionUpdateScheduler]", error),
        );
    },
  };

  observe(_subject: nsISupports | null, topic: string): void {
    if (topic === "browser-delayed-startup-finished") {
      void this.runDueUpdates().catch((error) =>
        console.error("[ChromeExtensionUpdateScheduler]", error),
      );
    } else if (topic === "quit-application-granted") {
      void this.stop();
    }
  }

  start(): void {
    if (this.started) return;
    this.started = true;
    this.manager.addAddonListener(this.addonListener);
    Services.obs.addObserver(this, "browser-delayed-startup-finished");
    Services.obs.addObserver(this, "quit-application-granted");
    this.arm(this.startupDelay);
  }

  async stop(): Promise<void> {
    if (!this.started) return;
    this.started = false;
    if (this.timer) clearTimeout(this.timer);
    this.timer = undefined;
    this.manager.removeAddonListener(this.addonListener);
    Services.obs.removeObserver(this, "browser-delayed-startup-finished");
    Services.obs.removeObserver(this, "quit-application-granted");
    await this.running;
  }

  private arm(delay: number): void {
    this.timer = setTimeout(() => {
      this.timer = undefined;
      void this.runDueUpdates()
        .catch((error) =>
          console.error("[ChromeExtensionUpdateScheduler]", error),
        )
        .finally(() => {
          if (this.started) this.arm(this.timerInterval);
        });
    }, delay);
  }

  runDueUpdates(options: { force?: boolean } = {}): Promise<void> {
    return (this.running ??= this.run(options.force ?? false).finally(() => {
      this.running = undefined;
    }));
  }

  private async run(force: boolean): Promise<void> {
    if (!Services.prefs.getBoolPref("extensions.update.enabled", true)) return;
    for (const addonId of await this.registry.listIds()) {
      const eligibility = await this.registry.getEligibility(addonId);
      const addon = await this.manager.getAddonByID(addonId);
      if (
        eligibility.eligibility !== "verified" ||
        !eligibility.enabled ||
        eligibility.pendingVersion ||
        !addon ||
        !this.manager.shouldAutoUpdate(addon)
      )
        continue;
      const now = this.now();
      if (!force && (eligibility.registration?.nextCheck ?? 0) > now) continue;
      this.publish(addonId, { state: "checking" });
      try {
        const check = await this.updater.checkForUpdate(addonId);
        if (check.status === "up-to-date") {
          await this.registry.setSchedule(addonId, now, now + this.interval);
          this.publish(addonId, { state: "up-to-date" });
          continue;
        }
        this.publish(addonId, {
          state: "available",
          availableVersion: check.availableVersion,
        });
        const browser = this.getBrowser();
        if (!browser) {
          await this.registry.setSchedule(addonId, now, now);
          this.publish(addonId, {
            state: "waiting-for-window",
            availableVersion: check.availableVersion,
          });
          continue;
        }
        this.publish(addonId, {
          state: "installing",
          availableVersion: check.availableVersion,
        });
        const result = await this.updater.installUpdate(addonId, browser);
        await this.registry.setSchedule(addonId, now, now + this.interval);
        this.publish(addonId, {
          state: result.status,
          availableVersion: result.version,
        });
      } catch (error) {
        await this.registry.setSchedule(addonId, now, now + this.retry);
        this.publish(addonId, {
          state: "error",
          error: error instanceof Error ? error.message : String(error),
        });
      }
    }
  }
}

export const ChromeExtensionUpdateScheduler =
  new ChromeExtensionUpdateSchedulerClass();
