/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { onCleanup } from "solid-js";
import {
  BYTES_PER_MB,
  DEFAULT_SETTINGS,
  DEFAULT_STATS,
  IDLE_MEMORY_RECLAIM_PREF,
  IDLE_MEMORY_RECLAIM_STATS_PREF,
  type IdleMemoryReclaimSettings,
  type IdleMemoryReclaimStats,
  MEMORY_REPORTER_MANAGER_CONTRACT_ID,
  MIN_IDLE_THRESHOLD_SEC,
  MIN_POLL_INTERVAL_SEC,
  MIN_RECLAIM_INTERVAL_SEC,
  USER_IDLE_SERVICE_CONTRACT_ID,
} from "./types.ts";

/**
 * Notification topic other components and tests can use to request a reclaim
 * explicitly, via `Services.obs.notifyObservers(null, RECLAIM_REQUEST_TOPIC)`.
 */
export const RECLAIM_REQUEST_TOPIC = "floorp-idle-memory-reclaim:run";

/** Memory figures the reclaim decision is made from. */
export interface MemorySnapshot {
  /**
   * Resident memory of the whole browser in bytes: the parent process plus
   * every content process.
   *
   * nsIMemoryReporterManager.residentUnique cannot be used here because it only
   * measures the parent. Tabs live in separate content processes, so a browser
   * holding 4GB across all of them still reads as a few hundred megabytes from
   * the parent alone, and the minResidentMB check would never pass.
   */
  residentBytes: number;
  /** Windows still referenced after being closed - a direct leak signal. */
  ghostWindows: number;
}

function readBoolean(value: unknown, fallback: boolean): boolean {
  return typeof value === "boolean" ? value : fallback;
}

function readNumber(value: unknown, fallback: number, min: number): number {
  if (typeof value !== "number" || !Number.isFinite(value)) {
    return fallback;
  }
  return Math.max(min, value);
}

/**
 * Normalizes a raw pref value into safe settings.
 *
 * Wrong types fall back to the defaults and small values are clamped to their
 * floors, so a hand-edited pref cannot drive the reclaim at an excessive rate.
 *
 * @param raw the pref value after JSON.parse
 * @returns the normalized settings
 */
export function sanitizeSettings(raw: unknown): IdleMemoryReclaimSettings {
  if (typeof raw !== "object" || raw === null) {
    return { ...DEFAULT_SETTINGS };
  }

  const source = raw as Record<string, unknown>;
  return {
    enabled: readBoolean(source.enabled, DEFAULT_SETTINGS.enabled),
    idleThresholdSec: readNumber(
      source.idleThresholdSec,
      DEFAULT_SETTINGS.idleThresholdSec,
      MIN_IDLE_THRESHOLD_SEC,
    ),
    pollIntervalSec: readNumber(
      source.pollIntervalSec,
      DEFAULT_SETTINGS.pollIntervalSec,
      MIN_POLL_INTERVAL_SEC,
    ),
    minIntervalSec: readNumber(
      source.minIntervalSec,
      DEFAULT_SETTINGS.minIntervalSec,
      MIN_RECLAIM_INTERVAL_SEC,
    ),
    minResidentMB: readNumber(
      source.minResidentMB,
      DEFAULT_SETTINGS.minResidentMB,
      0,
    ),
    reclaimOnGhostWindows: readBoolean(
      source.reclaimOnGhostWindows,
      DEFAULT_SETTINGS.reclaimOnGhostWindows,
    ),
  };
}

/**
 * Normalizes a raw pref value into safe stats.
 *
 * @param raw the pref value after JSON.parse
 * @returns the normalized stats
 */
export function sanitizeStats(raw: unknown): IdleMemoryReclaimStats {
  if (typeof raw !== "object" || raw === null) {
    return { ...DEFAULT_STATS };
  }

  const source = raw as Record<string, unknown>;
  const lastFreedBytes = source.lastFreedBytes;
  return {
    runCount: readNumber(source.runCount, DEFAULT_STATS.runCount, 0),
    lastRunAt: readNumber(source.lastRunAt, DEFAULT_STATS.lastRunAt, 0),
    // Memory can grow instead of shrink, so this one is not clamped.
    lastFreedBytes:
      typeof lastFreedBytes === "number" && Number.isFinite(lastFreedBytes)
        ? lastFreedBytes
        : DEFAULT_STATS.lastFreedBytes,
    totalFreedBytes: readNumber(
      source.totalFreedBytes,
      DEFAULT_STATS.totalFreedBytes,
      0,
    ),
  };
}

/**
 * Decides whether a reclaim should run right now.
 *
 * Pure and side-effect free, so it can be tested directly.
 *
 * @param settings normalized settings
 * @param snapshot current memory figures
 * @param stats stats from the last run, shared across windows
 * @param now current time in epoch milliseconds
 * @returns true when a reclaim should run
 */
export function shouldReclaim(
  settings: IdleMemoryReclaimSettings,
  snapshot: MemorySnapshot,
  stats: IdleMemoryReclaimStats,
  now: number,
): boolean {
  if (!settings.enabled) {
    return false;
  }

  // Throttle. A backwards system clock (elapsed < 0) must not count as "too
  // soon", or the feature would stall forever.
  const elapsed = now - stats.lastRunAt;
  if (elapsed >= 0 && elapsed < settings.minIntervalSec * 1000) {
    return false;
  }

  // A ghost window is by definition a window that failed to be released, so it
  // is worth reclaiming even below the resident threshold.
  if (settings.reclaimOnGhostWindows && snapshot.ghostWindows > 0) {
    return true;
  }

  return snapshot.residentBytes >= settings.minResidentMB * BYTES_PER_MB;
}

/**
 * Runs an explicit memory reclaim while the user is not interacting.
 *
 * Firefox schedules GC/CC lazily off an idle timer and jemalloc keeps freed
 * pages mapped, so resident memory stays high after long uptime. This performs
 * the same work as the "Minimize memory usage" button in about:memory
 * (nsIMemoryReporterManager.minimizeMemoryUsage) automatically, but only while
 * the user is away.
 */
@noraComponent(import.meta.hot)
export default class IdleMemoryReclaim extends NoraComponentBase {
  private settings: IdleMemoryReclaimSettings = { ...DEFAULT_SETTINGS };
  /** Seconds passed to addIdleObserver; removal needs the same value. */
  private registeredIdleSec: number | null = null;
  private idleObserver: nsIObserver | null = null;
  private prefObserver: nsIObserver | null = null;
  /** Timer that re-evaluates idle state periodically. */
  private pollTimerId: number | null = null;
  /** Guards against a reclaim starting while one is already running. */
  private reclaiming = false;

  init(): void {
    this.settings = this.loadSettings();
    this.setupPrefObserver();
    this.setupIdleObserver();
    this.setupPollTimer();
    onCleanup(() => this.teardown());
  }

  /** Loads settings from the pref, falling back to defaults on failure. */
  private loadSettings(): IdleMemoryReclaimSettings {
    try {
      const raw = Services.prefs.getStringPref(IDLE_MEMORY_RECLAIM_PREF, "");
      if (!raw) {
        return { ...DEFAULT_SETTINGS };
      }
      return sanitizeSettings(JSON.parse(raw));
    } catch (error) {
      console.error("[IdleMemoryReclaim] Failed to load settings:", error);
      return { ...DEFAULT_SETTINGS };
    }
  }

  /** Loads stats from the pref, falling back to zeroed stats on failure. */
  private loadStats(): IdleMemoryReclaimStats {
    try {
      const raw = Services.prefs.getStringPref(
        IDLE_MEMORY_RECLAIM_STATS_PREF,
        "",
      );
      if (!raw) {
        return { ...DEFAULT_STATS };
      }
      return sanitizeStats(JSON.parse(raw));
    } catch (error) {
      console.error("[IdleMemoryReclaim] Failed to load stats:", error);
      return { ...DEFAULT_STATS };
    }
  }

  /** Writes stats back to the pref, which every window shares. */
  private saveStats(stats: IdleMemoryReclaimStats): void {
    try {
      Services.prefs.setStringPref(
        IDLE_MEMORY_RECLAIM_STATS_PREF,
        JSON.stringify(stats),
      );
    } catch (error) {
      console.error("[IdleMemoryReclaim] Failed to save stats:", error);
    }
  }

  private getMemoryManager(): nsIMemoryReporterManager | null {
    try {
      return Cc[MEMORY_REPORTER_MANAGER_CONTRACT_ID].getService(
        Ci.nsIMemoryReporterManager,
      );
    } catch (error) {
      console.error("[IdleMemoryReclaim] Memory manager unavailable:", error);
      return null;
    }
  }

  private getIdleService(): nsIUserIdleService | null {
    try {
      return Cc[USER_IDLE_SERVICE_CONTRACT_ID].getService(
        Ci.nsIUserIdleService,
      );
    } catch (error) {
      console.error("[IdleMemoryReclaim] Idle service unavailable:", error);
      return null;
    }
  }

  /**
   * Reads resident memory for the whole browser.
   *
   * ChromeUtils.requestProcInfo() reports the parent and every content process,
   * so their sum is used. If that call fails we fall back to the parent-only
   * figure - a coarse gate still beats a feature that cannot fire.
   */
  private async readTotalResident(
    manager: nsIMemoryReporterManager,
  ): Promise<number> {
    try {
      const info = await ChromeUtils.requestProcInfo();
      const children = info.children ?? [];
      return children.reduce(
        (total, child) => total + (child.memory ?? 0),
        info.memory ?? 0,
      );
    } catch (error) {
      console.error(
        "[IdleMemoryReclaim] requestProcInfo failed; falling back to the",
        "parent-only figure:",
        error,
      );
      return manager.residentUnique;
    }
  }

  /** Takes a memory snapshot, or returns null when it cannot be read. */
  private async takeSnapshot(): Promise<MemorySnapshot | null> {
    const manager = this.getMemoryManager();
    if (!manager) {
      return null;
    }
    try {
      return {
        residentBytes: await this.readTotalResident(manager),
        ghostWindows: manager.ghostWindows,
      };
    } catch (error) {
      console.error("[IdleMemoryReclaim] Failed to read memory:", error);
      return null;
    }
  }

  /** Watches the pref and re-registers when the thresholds change. */
  private setupPrefObserver(): void {
    this.prefObserver = {
      observe: (_subject: nsISupports, topic: string, data: string) => {
        if (topic !== "nsPref:changed" || data !== IDLE_MEMORY_RECLAIM_PREF) {
          return;
        }
        this.settings = this.loadSettings();
        // A changed threshold or interval needs a fresh registration.
        this.setupIdleObserver();
        this.setupPollTimer();
      },
    };

    Services.prefs.addObserver(IDLE_MEMORY_RECLAIM_PREF, this.prefObserver);
  }

  /**
   * (Re-)registers the idle observer. Any existing registration is removed
   * first, then re-added at the current threshold.
   */
  private setupIdleObserver(): void {
    this.unregisterIdleObserver();

    if (!this.settings.enabled) {
      return;
    }

    const idleService = this.getIdleService();
    if (!idleService) {
      return;
    }

    const observer: nsIObserver = {
      observe: (_subject: nsISupports, topic: string, _data: string) => {
        // "idle" means the threshold was reached; RECLAIM_REQUEST_TOPIC is an
        // explicit request.
        if (topic === "idle" || topic === RECLAIM_REQUEST_TOPIC) {
          void this.reclaimIfNeeded();
        }
      },
    };

    try {
      const idleSec = Math.round(this.settings.idleThresholdSec);
      idleService.addIdleObserver(observer, idleSec);
      this.idleObserver = observer;
      this.registeredIdleSec = idleSec;
    } catch (error) {
      console.error("[IdleMemoryReclaim] Failed to add idle observer:", error);
      return;
    }

    // Let tests and other features request a reclaim explicitly.
    try {
      Services.obs.addObserver(observer, RECLAIM_REQUEST_TOPIC);
    } catch (error) {
      console.error(
        "[IdleMemoryReclaim] Failed to add request observer:",
        error,
      );
    }
  }

  private unregisterIdleObserver(): void {
    const observer = this.idleObserver;
    if (!observer) {
      return;
    }

    if (this.registeredIdleSec !== null) {
      try {
        this.getIdleService()?.removeIdleObserver(
          observer,
          this.registeredIdleSec,
        );
      } catch (error) {
        console.error(
          "[IdleMemoryReclaim] Failed to remove idle observer:",
          error,
        );
      }
    }

    try {
      Services.obs.removeObserver(observer, RECLAIM_REQUEST_TOPIC);
    } catch {
      // Not registered - nothing to do.
    }

    this.idleObserver = null;
    this.registeredIdleSec = null;
  }

  /**
   * (Re-)arms the timer that re-evaluates idle state.
   *
   * nsIUserIdleService fires "idle" once when the threshold is crossed and
   * never again while the user stays away. Relying on it alone would reclaim
   * once a minute after they left and then stop, so this poll fills the gap.
   */
  private setupPollTimer(): void {
    this.clearPollTimer();

    if (!this.settings.enabled) {
      return;
    }

    const intervalMs = Math.round(this.settings.pollIntervalSec) * 1000;
    this.pollTimerId = globalThis.setInterval(() => {
      const idleService = this.getIdleService();
      if (!idleService) {
        return;
      }
      try {
        // idleTime is milliseconds since the last user input.
        if (idleService.idleTime >= this.settings.idleThresholdSec * 1000) {
          void this.reclaimIfNeeded();
        }
      } catch (error) {
        console.error("[IdleMemoryReclaim] Idle poll failed:", error);
      }
    }, intervalMs);
  }

  private clearPollTimer(): void {
    if (this.pollTimerId !== null) {
      globalThis.clearInterval(this.pollTimerId);
      this.pollTimerId = null;
    }
  }

  /**
   * Runs a reclaim when the current state calls for one.
   *
   * The feature is instantiated per window, so lastRunAt in the stats pref acts
   * as a cross-window lock: claiming it before the work starts keeps concurrent
   * reclaims from piling up.
   */
  async reclaimIfNeeded(): Promise<void> {
    if (this.reclaiming) {
      return;
    }

    const snapshot = await this.takeSnapshot();
    if (!snapshot) {
      return;
    }

    const stats = this.loadStats();
    const now = Date.now();
    if (!shouldReclaim(this.settings, snapshot, stats, now)) {
      return;
    }

    this.reclaiming = true;
    // Claim the slot first so other windows bail out at their own check.
    this.saveStats({ ...stats, lastRunAt: now });

    try {
      await this.runMinimizeMemoryUsage();

      const after = await this.takeSnapshot();
      const freed = after ? snapshot.residentBytes - after.residentBytes : 0;

      this.saveStats({
        runCount: stats.runCount + 1,
        lastRunAt: now,
        lastFreedBytes: freed,
        // Never let a negative delta reduce the running total.
        totalFreedBytes: stats.totalFreedBytes + Math.max(0, freed),
      });

      const toMB = (bytes: number): string => (bytes / BYTES_PER_MB).toFixed(1);
      console.log(
        "[IdleMemoryReclaim] Reclaimed",
        `${toMB(freed)}MB`,
        `(resident ${toMB(snapshot.residentBytes)}MB ->`,
        `${toMB(after?.residentBytes ?? 0)}MB,`,
        `ghostWindows ${snapshot.ghostWindows} -> ${after?.ghostWindows ?? 0})`,
      );
    } catch (error) {
      console.error("[IdleMemoryReclaim] Reclaim failed:", error);
    } finally {
      this.reclaiming = false;
    }
  }

  /**
   * Performs the same work as "Minimize memory usage" in about:memory.
   *
   * Gecko cycles GC -> CC -> GC several times and sends memory-pressure to
   * every content process so jemalloc returns its dirty pages to the OS.
   */
  private runMinimizeMemoryUsage(): Promise<void> {
    return new Promise<void>((resolve, reject) => {
      const manager = this.getMemoryManager();
      if (!manager) {
        reject(new Error("nsIMemoryReporterManager is unavailable"));
        return;
      }

      try {
        manager.minimizeMemoryUsage({ run: () => resolve() });
      } catch (error) {
        reject(error);
      }
    });
  }

  /** Removes observers when the window goes away or on HMR. */
  private teardown(): void {
    this.clearPollTimer();
    this.unregisterIdleObserver();

    if (this.prefObserver) {
      try {
        Services.prefs.removeObserver(
          IDLE_MEMORY_RECLAIM_PREF,
          this.prefObserver,
        );
      } catch (error) {
        console.error(
          "[IdleMemoryReclaim] Failed to remove pref observer:",
          error,
        );
      }
      this.prefObserver = null;
    }
  }
}
