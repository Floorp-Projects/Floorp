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
  enabled: true,
  idleThresholdSec: 60,
  minIntervalSec: 300,
  minResidentMB: 400,
};

/**
 * Floors for the values this page offers.
 *
 * The feature clamps lower (15s / 30s) so that a hand-edited pref can never
 * destroy the machine. A reclaim stalls the main thread for a second or more,
 * so this page deliberately does not offer values that would fire that often.
 */
export const IDLE_THRESHOLD_SEC_MIN = 60;
export const MIN_INTERVAL_SEC_MIN = 60;
export const MIN_RESIDENT_MB_MIN = 0;

function readBoolean(value: unknown, fallback: boolean): boolean {
  return typeof value === "boolean" ? value : fallback;
}

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
