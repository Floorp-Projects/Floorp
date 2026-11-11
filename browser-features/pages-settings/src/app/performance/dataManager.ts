/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { rpc } from "../../lib/rpc/rpc.ts";

export const IDLE_MEMORY_RECLAIM_PREF = "floorp.memory.idleReclaim";

export interface IdleMemoryReclaimSettings {
  enabled: boolean;
  idleThresholdSec: number;
  minIntervalSec: number;
  minResidentMB: number;
}

export const DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS: IdleMemoryReclaimSettings = {
  enabled: false,
  idleThresholdSec: 60,
  minIntervalSec: 300,
  minResidentMB: 400,
};

/**
 * Floors for the values this page offers.
 *
 * The feature clamps lower (15s / 30s). Reclaim can pause the browser,
 * so this page offers more conservative minimum intervals.
 */
export const IDLE_THRESHOLD_SEC_MIN = 60;
export const MIN_INTERVAL_SEC_MIN = 60;
export const MIN_RESIDENT_MB_MIN = 0;

/** Returns the value when it is a boolean, otherwise the fallback. */
function readBoolean(value: unknown, fallback: boolean): boolean {
  return typeof value === "boolean" ? value : fallback;
}

/** Returns a finite number clamped to `min`, otherwise the fallback. */
function readNumber(value: unknown, fallback: number, min: number): number {
  if (typeof value !== "number" || !Number.isFinite(value)) {
    return fallback;
  }
  return Math.max(min, value);
}

/** Normalizes untrusted pref JSON into the values this page exposes. */
export function normalizeIdleMemoryReclaimSettings(
  raw: unknown,
): IdleMemoryReclaimSettings {
  if (typeof raw !== "object" || raw === null) {
    return { ...DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS };
  }

  const source = raw as Record<string, unknown>;
  return {
    enabled: readBoolean(
      source.enabled,
      DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.enabled,
    ),
    idleThresholdSec: readNumber(
      source.idleThresholdSec,
      DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.idleThresholdSec,
      IDLE_THRESHOLD_SEC_MIN,
    ),
    minIntervalSec: readNumber(
      source.minIntervalSec,
      DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.minIntervalSec,
      MIN_INTERVAL_SEC_MIN,
    ),
    minResidentMB: readNumber(
      source.minResidentMB,
      DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS.minResidentMB,
      MIN_RESIDENT_MB_MIN,
    ),
  };
}

/** Parses stored pref JSON, tolerating empty or malformed values. */
function parseSettingsJson(raw: string | null): Record<string, unknown> {
  if (!raw) {
    return {};
  }

  try {
    const parsed: unknown = JSON.parse(raw);
    if (typeof parsed !== "object" || parsed === null) {
      return {};
    }
    return parsed as Record<string, unknown>;
  } catch {
    return {};
  }
}

/**
 * Merges the exposed settings into the pref JSON.
 *
 * The pref carries more keys than this page shows (pollIntervalSec and
 * reclaimOnGhostWindows), so the existing object is kept and only the exposed
 * keys are replaced. Writing just this page's fields would silently reset a
 * hand-edited pollIntervalSec back to its default.
 */
export function mergeIdleMemoryReclaimSettings(
  existing: string | null,
  settings: IdleMemoryReclaimSettings,
): string {
  const normalized = normalizeIdleMemoryReclaimSettings(settings);
  return JSON.stringify({
    ...parseSettingsJson(existing),
    enabled: normalized.enabled,
    idleThresholdSec: normalized.idleThresholdSec,
    minIntervalSec: normalized.minIntervalSec,
    minResidentMB: normalized.minResidentMB,
  });
}

export async function getIdleMemoryReclaimSettings(): Promise<
  IdleMemoryReclaimSettings
> {
  try {
    const raw = await rpc.getStringPref(IDLE_MEMORY_RECLAIM_PREF);
    return normalizeIdleMemoryReclaimSettings(parseSettingsJson(raw));
  } catch (error) {
    console.error("[Performance] Failed to load idle reclaim settings:", error);
    return { ...DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS };
  }
}

export async function saveIdleMemoryReclaimSettings(
  settings: IdleMemoryReclaimSettings,
): Promise<void> {
  const existing = await rpc.getStringPref(IDLE_MEMORY_RECLAIM_PREF);
  await rpc.setStringPref(
    IDLE_MEMORY_RECLAIM_PREF,
    mergeIdleMemoryReclaimSettings(existing, settings),
  );
}

export const TAB_AUTO_UNLOAD_PREF = "floorp.tabs.autoUnload";
export const LEGACY_TAB_SLEEP_EXCLUSION_PREF = "floorp.tabs.sleep.exclusion";

export interface TabAutoUnloadSettings {
  enabled: boolean;
  memoryThresholdMB: number;
  minInactiveMinutes: number;
  excludePatterns: string[];
}

export const DEFAULT_TAB_AUTO_UNLOAD_SETTINGS: TabAutoUnloadSettings = {
  enabled: false,
  memoryThresholdMB: 2048,
  minInactiveMinutes: 10,
  excludePatterns: [],
};

export const MEMORY_THRESHOLD_MB_MIN = 0;
export const MIN_INACTIVE_MINUTES_MIN = 1;

/** Builds defaults with a fresh pattern array the caller can mutate. */
function createDefaultTabAutoUnloadSettings(): TabAutoUnloadSettings {
  return {
    ...DEFAULT_TAB_AUTO_UNLOAD_SETTINGS,
    excludePatterns: [],
  };
}

/**
 * Trims, drops empty entries, and removes duplicate exclusion patterns
 * while keeping the order the user entered.
 */
function normalizeExcludePatterns(raw: unknown): string[] {
  if (!Array.isArray(raw)) {
    return [];
  }

  const seen = new Set<string>();
  const patterns: string[] = [];
  for (const value of raw) {
    if (typeof value !== "string") {
      continue;
    }
    const trimmed = value.trim();
    if (!trimmed || seen.has(trimmed)) {
      continue;
    }
    seen.add(trimmed);
    patterns.push(trimmed);
  }
  return patterns;
}

/** Normalizes untrusted pref JSON into the values this page exposes. */
export function normalizeTabAutoUnloadSettings(
  raw: unknown,
): TabAutoUnloadSettings {
  if (typeof raw !== "object" || raw === null) {
    return createDefaultTabAutoUnloadSettings();
  }

  const source = raw as Record<string, unknown>;
  return {
    enabled: readBoolean(
      source.enabled,
      DEFAULT_TAB_AUTO_UNLOAD_SETTINGS.enabled,
    ),
    memoryThresholdMB: readNumber(
      source.memoryThresholdMB,
      DEFAULT_TAB_AUTO_UNLOAD_SETTINGS.memoryThresholdMB,
      MEMORY_THRESHOLD_MB_MIN,
    ),
    minInactiveMinutes: readNumber(
      source.minInactiveMinutes,
      DEFAULT_TAB_AUTO_UNLOAD_SETTINGS.minInactiveMinutes,
      MIN_INACTIVE_MINUTES_MIN,
    ),
    excludePatterns: normalizeExcludePatterns(source.excludePatterns),
  };
}

/**
 * Merges the exposed settings into the pref JSON.
 *
 * Unknown keys in the stored object are kept so a hand-edited pref is not
 * silently reset.
 */
export function mergeTabAutoUnloadSettings(
  existing: string | null,
  settings: TabAutoUnloadSettings,
): string {
  const normalized = normalizeTabAutoUnloadSettings(settings);
  return JSON.stringify({
    ...parseSettingsJson(existing),
    enabled: normalized.enabled,
    memoryThresholdMB: normalized.memoryThresholdMB,
    minInactiveMinutes: normalized.minInactiveMinutes,
    excludePatterns: normalized.excludePatterns,
  });
}

/**
 * Loads the settings, falling back to the legacy pref's patterns.
 *
 * Nothing is written back, so the old pref stays untouched until the user
 * saves on this page.
 */
export async function getTabAutoUnloadSettings(): Promise<
  TabAutoUnloadSettings
> {
  try {
    const raw = await rpc.getStringPref(TAB_AUTO_UNLOAD_PREF);
    if (raw) {
      return normalizeTabAutoUnloadSettings(parseSettingsJson(raw));
    }

    // Migrate the patterns from the old tab sleep exclusion pref without
    // writing anything back. The rest of the settings keep their defaults.
    const legacyRaw = await rpc.getStringPref(LEGACY_TAB_SLEEP_EXCLUSION_PREF);
    if (!legacyRaw) {
      return createDefaultTabAutoUnloadSettings();
    }

    const legacy = parseSettingsJson(legacyRaw);
    return {
      ...createDefaultTabAutoUnloadSettings(),
      excludePatterns: normalizeExcludePatterns(legacy.patterns),
    };
  } catch (error) {
    console.error(
      "[Performance] Failed to load tab auto unload settings:",
      error,
    );
    return createDefaultTabAutoUnloadSettings();
  }
}

/** Merges the page's settings into the stored pref JSON. */
export async function saveTabAutoUnloadSettings(
  settings: TabAutoUnloadSettings,
): Promise<void> {
  const existing = await rpc.getStringPref(TAB_AUTO_UNLOAD_PREF);
  await rpc.setStringPref(
    TAB_AUTO_UNLOAD_PREF,
    mergeTabAutoUnloadSettings(existing, settings),
  );
}
