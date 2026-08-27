/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Settings for the idle memory reclaim feature.
 *
 * Gecko keeps freed heap pages mapped rather than returning them to the OS, so
 * a long-running browser holds more resident memory than it actually uses. This
 * feature runs an explicit reclaim (GC + CC + jemalloc purge) to bring that back
 * down, but only while the user is away so the pause never lands mid-interaction.
 */
export interface IdleMemoryReclaimSettings {
  /** Whether the feature is enabled at all. */
  enabled: boolean;
  /** Seconds of inactivity before a reclaim is considered. */
  idleThresholdSec: number;
  /**
   * How often idle state is re-evaluated, in seconds.
   *
   * nsIUserIdleService fires "idle" once when the threshold is crossed and then
   * stays silent until the user returns, so this periodic check is what keeps
   * an unattended browser reclaiming. The tick itself only reads idleTime;
   * whether a reclaim actually runs is decided by the minIntervalSec throttle
   * and the minResidentMB floor.
   */
  pollIntervalSec: number;
  /** Minimum seconds between reclaims. Throttle to avoid burning CPU. */
  minIntervalSec: number;
  /** Skip the reclaim while resident memory is below this many megabytes. */
  minResidentMB: number;
  /** Reclaim regardless of minResidentMB when ghost windows are present. */
  reclaimOnGhostWindows: boolean;
}

/**
 * Statistics about past reclaims.
 *
 * The feature is instantiated per browser window, so the stats live in a string
 * pref that every window shares. lastRunAt doubles as the throttle's lock.
 */
export interface IdleMemoryReclaimStats {
  /** How many times a reclaim has run. */
  runCount: number;
  /** When the last reclaim started, in epoch milliseconds. */
  lastRunAt: number;
  /** Bytes freed by the last reclaim; negative means memory grew instead. */
  lastFreedBytes: number;
  /** Bytes freed across all reclaims. */
  totalFreedBytes: number;
}

export const IDLE_MEMORY_RECLAIM_PREF = "floorp.memory.idleReclaim";
export const IDLE_MEMORY_RECLAIM_STATS_PREF = "floorp.memory.idleReclaim.stats";

export const DEFAULT_SETTINGS: IdleMemoryReclaimSettings = {
  enabled: true,
  idleThresholdSec: 60,
  pollIntervalSec: 60,
  minIntervalSec: 300,
  minResidentMB: 400,
  reclaimOnGhostWindows: true,
};

export const DEFAULT_STATS: IdleMemoryReclaimStats = {
  runCount: 0,
  lastRunAt: 0,
  lastFreedBytes: 0,
  totalFreedBytes: 0,
};

export const MEMORY_REPORTER_MANAGER_CONTRACT_ID =
  "@mozilla.org/memory-reporter-manager;1";

export const USER_IDLE_SERVICE_CONTRACT_ID =
  "@mozilla.org/widget/useridleservice;1";

/**
 * Floors for the settings above, so a hand-edited pref cannot drive the reclaim
 * at an excessive rate.
 */
export const MIN_IDLE_THRESHOLD_SEC = 15;
export const MIN_RECLAIM_INTERVAL_SEC = 30;
export const MIN_POLL_INTERVAL_SEC = 15;

/**
 * Ceiling for pollIntervalSec.
 *
 * The value is turned into milliseconds and handed to setInterval, whose
 * timeout is a signed 32-bit integer. Past this it wraps negative, and the
 * timer implementation clamps a negative delay to 0ms — so asking for a very
 * infrequent poll would produce the busiest one possible. Cap it at the largest
 * whole second that still fits, roughly 24.8 days.
 */
export const MAX_POLL_INTERVAL_SEC = Math.floor(0x7fffffff / 1000);

export const BYTES_PER_MB = 1024 * 1024;
