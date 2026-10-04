// SPDX-License-Identifier: MPL-2.0

import type { MemorySnapshot } from "../index.ts";
import type {
  IdleMemoryReclaimSettings,
  IdleMemoryReclaimStats,
} from "../types.ts";

/** Test access to instance state without registering live browser observers. */
export interface ReclaimTestInstance {
  settings: IdleMemoryReclaimSettings;
  reclaiming: boolean;
  disposed: boolean;
  pollTimerId: ReturnType<typeof setInterval> | null;
  idleObserver: nsIObserver | null;
  prefObserver: nsIObserver | null;
  registeredIdleSec: number | null;
  takeSnapshot: () => Promise<MemorySnapshot | null>;
  isStillIdle: () => boolean;
  loadStats: () => IdleMemoryReclaimStats;
  saveStats: (stats: IdleMemoryReclaimStats) => void;
  runMinimizeMemoryUsage: () => Promise<void>;
  reclaimIfNeeded: (verifyIdle?: boolean) => Promise<void>;
  teardown: () => void;
}

export interface SharedReclaimStats {
  value: IdleMemoryReclaimStats;
}
