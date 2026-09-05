// SPDX-License-Identifier: MPL-2.0

import {
  CONTEXT_MENU_SCHEMA_VERSION,
  type ContextMenuConfig,
  type ContextMenuLevelOverride,
  type ContextMenuProfileOverride,
  type ContextMenuSurfaceConfig,
  type EffectiveContextMenuLevelOverride,
} from "./types.ts";

export const DEFAULT_CONTEXT_MENU_CONFIG: ContextMenuConfig = {
  schemaVersion: CONTEXT_MENU_SCHEMA_VERSION,
  surfaces: {},
};

export type ContextMenuConfigParseStatus =
  | "ok"
  | "empty"
  | "invalid"
  | "unsupported-version";

export interface ContextMenuConfigParseResult {
  status: ContextMenuConfigParseStatus;
  config: ContextMenuConfig;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function normalizeStringArray(value: unknown): string[] | undefined {
  if (!Array.isArray(value)) return undefined;

  const result: string[] = [];
  const seen = new Set<string>();
  for (const candidate of value) {
    if (typeof candidate !== "string" || candidate.length === 0) continue;
    if (seen.has(candidate)) continue;
    seen.add(candidate);
    result.push(candidate);
  }
  return result;
}

function normalizeLevelOverride(
  value: unknown,
): ContextMenuLevelOverride | null {
  if (!isRecord(value)) return null;

  const order = normalizeStringArray(value.order);
  const hidden = normalizeStringArray(value.hidden);
  const result: ContextMenuLevelOverride = {};
  if (order !== undefined) result.order = order;
  if (hidden !== undefined) result.hidden = hidden;
  return result;
}

function normalizeLevelMap(
  value: unknown,
): Record<string, ContextMenuLevelOverride> {
  if (!isRecord(value)) return {};

  const entries: Array<[string, ContextMenuLevelOverride]> = [];
  for (const [key, candidate] of Object.entries(value)) {
    if (key.length === 0) continue;
    const normalized = normalizeLevelOverride(candidate);
    if (normalized) entries.push([key, normalized]);
  }
  return Object.fromEntries(entries);
}

function normalizeProfileOverride(
  value: unknown,
): ContextMenuProfileOverride | null {
  if (!isRecord(value)) return null;
  return {
    independent: value.independent === true,
    containers: normalizeLevelMap(value.containers),
  };
}

function normalizeSurfaceConfig(
  value: unknown,
): ContextMenuSurfaceConfig | null {
  if (!isRecord(value)) return null;

  const profiles: Array<[string, ContextMenuProfileOverride]> = [];
  if (isRecord(value.profiles)) {
    for (const [key, candidate] of Object.entries(value.profiles)) {
      if (key.length === 0) continue;
      const normalized = normalizeProfileOverride(candidate);
      if (normalized) profiles.push([key, normalized]);
    }
  }

  return {
    base: normalizeLevelMap(value.base),
    profiles: Object.fromEntries(profiles),
  };
}

export function normalizeContextMenuConfig(value: unknown): ContextMenuConfig {
  if (!isRecord(value) || value.schemaVersion !== CONTEXT_MENU_SCHEMA_VERSION) {
    return cloneContextMenuConfig(DEFAULT_CONTEXT_MENU_CONFIG);
  }

  const surfaces: Array<[string, ContextMenuSurfaceConfig]> = [];
  if (isRecord(value.surfaces)) {
    for (const [key, candidate] of Object.entries(value.surfaces)) {
      if (key.length === 0) continue;
      const normalized = normalizeSurfaceConfig(candidate);
      if (normalized) surfaces.push([key, normalized]);
    }
  }

  return {
    schemaVersion: CONTEXT_MENU_SCHEMA_VERSION,
    surfaces: Object.fromEntries(surfaces),
  };
}

export function parseContextMenuConfigWithStatus(
  serialized: string | null,
): ContextMenuConfigParseResult {
  if (!serialized || serialized.trim().length === 0) {
    return {
      status: "empty",
      config: cloneContextMenuConfig(DEFAULT_CONTEXT_MENU_CONFIG),
    };
  }

  try {
    const parsed: unknown = JSON.parse(serialized);
    if (!isRecord(parsed)) {
      return {
        status: "invalid",
        config: cloneContextMenuConfig(DEFAULT_CONTEXT_MENU_CONFIG),
      };
    }
    if (parsed.schemaVersion !== CONTEXT_MENU_SCHEMA_VERSION) {
      return {
        status: "unsupported-version",
        config: cloneContextMenuConfig(DEFAULT_CONTEXT_MENU_CONFIG),
      };
    }
    if (!isRecord(parsed.surfaces)) {
      return {
        status: "invalid",
        config: cloneContextMenuConfig(DEFAULT_CONTEXT_MENU_CONFIG),
      };
    }
    return { status: "ok", config: normalizeContextMenuConfig(parsed) };
  } catch (error) {
    console.error(
      "[ContextMenuCustomizer] Failed to parse configuration",
      error,
    );
    return {
      status: "invalid",
      config: cloneContextMenuConfig(DEFAULT_CONTEXT_MENU_CONFIG),
    };
  }
}

export function parseContextMenuConfig(
  serialized: string | null,
): ContextMenuConfig {
  return parseContextMenuConfigWithStatus(serialized).config;
}

export function serializeContextMenuConfig(config: ContextMenuConfig): string {
  return JSON.stringify(normalizeContextMenuConfig(config));
}

export function cloneContextMenuConfig(
  config: ContextMenuConfig,
): ContextMenuConfig {
  return normalizeContextMenuConfig({
    schemaVersion: config.schemaVersion,
    surfaces: config.surfaces,
  });
}

function hasLevelChanges(level: ContextMenuLevelOverride): boolean {
  return (level.hidden?.length ?? 0) > 0 || (level.order?.length ?? 0) > 0;
}

export function isContextMenuConfigEmpty(config: ContextMenuConfig): boolean {
  for (const surface of Object.values(config.surfaces)) {
    if (Object.values(surface.base).some(hasLevelChanges)) return false;
    for (const profile of Object.values(surface.profiles)) {
      if (Object.values(profile.containers).some(hasLevelChanges)) return false;
    }
  }
  return true;
}

/** Resolve the active base/profile override without forcing a native-hidden item visible. */
export function resolveContextMenuLevelOverride(
  config: ContextMenuConfig,
  surfaceKey: string,
  profileKey: string,
  containerKey: string,
): EffectiveContextMenuLevelOverride | null {
  const surface = config.surfaces[surfaceKey];
  if (!surface) return null;

  const profile = surface.profiles[profileKey];
  const base = surface.base[containerKey];
  const contextual = profile?.containers[containerKey];

  if (profile?.independent) {
    if (!contextual) return null;
    return {
      order: [...(contextual.order ?? [])],
      hidden: [...(contextual.hidden ?? [])],
    };
  }

  // Profile containers remain persisted but dormant until independent mode is
  // enabled. This makes the mode switch reversible and gives base one source
  // of truth for all inherited profiles.
  if (!base) return null;
  return {
    order: [...(base.order ?? [])],
    hidden: [...(base.hidden ?? [])],
  };
}
