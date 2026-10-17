/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { KeyboardShortcutConfig, ShortcutConfig } from "./type.ts";
import {
  createDefaultConfig,
  KEYBOARD_SHORTCUT_SCHEMA_VERSION,
  ZEN_MODE_ACTION,
} from "./defaults.ts";

const LEGACY_PREF = "floorp.browser.nora.csk.data";

interface LegacyModifiers {
  alt: boolean;
  ctrl: boolean;
  meta: boolean;
  shift: boolean;
}

interface LegacyShortcutConfig {
  key: string;
  modifiers: LegacyModifiers;
}

type LegacyConfig = Record<string, LegacyShortcutConfig>;

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function isShortcutShape(value: unknown): value is ShortcutConfig {
  if (!isRecord(value)) return false;
  return typeof value.key === "string" &&
    isRecord(value.modifiers) &&
    typeof value.modifiers.alt === "boolean" &&
    typeof value.modifiers.ctrl === "boolean" &&
    typeof value.modifiers.meta === "boolean" &&
    typeof value.modifiers.shift === "boolean" &&
    typeof value.action === "string";
}

function shortcutSignature(shortcut: ShortcutConfig): string {
  const key = shortcut.key.replace(/^(Key|Digit)/, "").toUpperCase();
  return JSON.stringify([shortcut.modifiers, key]);
}

function normalizeShortcutBindings(
  value: unknown,
  actionId: string,
): ShortcutConfig[] {
  const candidates = Array.isArray(value) ? value : [value];
  const normalized: ShortcutConfig[] = [];
  const seen = new Set<string>();

  for (const candidate of candidates) {
    if (!isShortcutShape(candidate)) continue;
    const shortcut: ShortcutConfig = {
      key: candidate.key,
      modifiers: {
        alt: candidate.modifiers.alt,
        ctrl: candidate.modifiers.ctrl,
        meta: candidate.modifiers.meta,
        shift: candidate.modifiers.shift,
      },
      action: candidate.action === ZEN_MODE_ACTION ? ZEN_MODE_ACTION : actionId,
    };
    const signature = shortcutSignature(shortcut);
    if (seen.has(signature)) continue;
    seen.add(signature);
    normalized.push(shortcut);
  }

  return normalized;
}

/**
 * Normalize the current JSON preference to schema version 3.
 *
 * Only schema-less/older configurations receive the Zen shortcut. A version 3
 * configuration without Zen is an intentional user deletion and is preserved.
 */
export function migrateConfigToV3(
  value: unknown,
  defaults: KeyboardShortcutConfig = createDefaultConfig(),
): KeyboardShortcutConfig {
  const source = isRecord(value) ? value : {};
  const sourceShortcuts = isRecord(source.shortcuts)
    ? source.shortcuts as Record<string, unknown>
    : null;
  const shortcuts: Record<string, ShortcutConfig[]> = {};

  if (sourceShortcuts) {
    for (const [actionId, binding] of Object.entries(sourceShortcuts)) {
      const bindings = normalizeShortcutBindings(binding, actionId);
      if (bindings.length > 0) shortcuts[actionId] = bindings;
    }
  } else {
    for (const [actionId, bindings] of Object.entries(defaults.shortcuts)) {
      shortcuts[actionId] = [...bindings];
    }
  }

  if (
    source.schemaVersion !== KEYBOARD_SHORTCUT_SCHEMA_VERSION &&
    !Object.values(shortcuts).some((bindings) =>
      bindings.some((shortcut) => shortcut.action === ZEN_MODE_ACTION)
    )
  ) {
    shortcuts[ZEN_MODE_ACTION] = [...defaults.shortcuts[ZEN_MODE_ACTION]];
  }

  return {
    ...source,
    schemaVersion: KEYBOARD_SHORTCUT_SCHEMA_VERSION,
    enabled: typeof source.enabled === "boolean"
      ? source.enabled
      : defaults.enabled,
    shortcuts,
  } as KeyboardShortcutConfig;
}

export function migrateLegacyConfig(): KeyboardShortcutConfig | null {
  try {
    if (!Services.prefs.prefHasUserValue(LEGACY_PREF)) {
      return null;
    }

    const legacyData = JSON.parse(
      Services.prefs.getStringPref(LEGACY_PREF, "{}"),
    ) as LegacyConfig;

    const shortcuts: Record<string, ShortcutConfig[]> = {};
    for (const [action, config] of Object.entries(legacyData)) {
      if (typeof config === "object" && config !== null) {
        const bindings = normalizeShortcutBindings({
          key: config.key,
          modifiers: config.modifiers,
          action,
        }, action);
        if (bindings.length > 0) shortcuts[action] = bindings;
      }
    }

    return {
      enabled: true,
      shortcuts,
    };
  } catch (e) {
    console.error("Failed to migrate legacy keyboard shortcut config:", e);
    return null;
  }
}

export function clearLegacyConfig() {
  try {
    if (Services.prefs.prefHasUserValue(LEGACY_PREF)) {
      Services.prefs.clearUserPref(LEGACY_PREF);
    }
  } catch (e) {
    console.error("Failed to clear legacy keyboard shortcut config:", e);
  }
}
