// SPDX-License-Identifier: MPL-2.0

import type { MouseGestureConfig } from "../../types/pref.ts";
import {
  DEFAULT_WHEEL_ACTIONS,
  normalizeWheelActions,
} from "#features-chrome/common/mouse-gesture/wheel-action-policy.ts";

export const MOUSE_GESTURE_ENABLED_PREF = "floorp.mousegesture.enabled";
export const MOUSE_GESTURE_CONFIG_PREF = "floorp.mousegesture.config";

const MIN_CONTEXT_MENU_DISTANCE = 5;
const GESTURE_DIRECTIONS = new Set([
  "up",
  "down",
  "left",
  "right",
  "upRight",
  "upLeft",
  "downRight",
  "downLeft",
]);

type PersistedMouseGestureConfig = Omit<MouseGestureConfig, "enabled">;

export type MouseGestureConfigUpdate =
  | Partial<PersistedMouseGestureConfig>
  | ((
    current: Readonly<MouseGestureConfig>,
  ) => Partial<PersistedMouseGestureConfig>);

export interface GestureConfigWriters {
  writeEnabled(enabled: boolean): Promise<void>;
  writeConfig(serializedConfig: string): Promise<void>;
}

export interface GestureConfigPersistenceSnapshot {
  config: MouseGestureConfig;
  pending: boolean;
  error: string | null;
}

export interface GestureConfigPersistence {
  getSnapshot(): GestureConfigPersistenceSnapshot;
  subscribe(
    listener: (snapshot: GestureConfigPersistenceSnapshot) => void,
  ): () => void;
  updateEnabled(
    update: boolean | ((currentEnabled: boolean) => boolean),
  ): Promise<boolean>;
  updateConfig(update: MouseGestureConfigUpdate): Promise<boolean>;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function booleanOr(value: unknown, fallback: boolean): boolean {
  return typeof value === "boolean" ? value : fallback;
}

function finiteNumberOr(value: unknown, fallback: number): number {
  return typeof value === "number" && Number.isFinite(value) ? value : fallback;
}

function stringOr(value: unknown, fallback: string): string {
  return typeof value === "string" ? value : fallback;
}

function normalizeActions(
  value: unknown,
  fallback: MouseGestureConfig["actions"],
): MouseGestureConfig["actions"] {
  if (!Array.isArray(value)) return fallback.map((action) => ({ ...action }));

  return value.flatMap((candidate) => {
    if (!isRecord(candidate) || typeof candidate.action !== "string") return [];
    if (
      !Array.isArray(candidate.pattern) ||
      !candidate.pattern.every((direction) =>
        typeof direction === "string" && GESTURE_DIRECTIONS.has(direction)
      )
    ) return [];

    return [{
      pattern: [
        ...candidate.pattern,
      ] as MouseGestureConfig["actions"][number]["pattern"],
      action: candidate.action,
    }];
  });
}

export function createDefaultMouseGestureConfig(
  enabled: boolean,
): MouseGestureConfig {
  return {
    enabled,
    rockerGesturesEnabled: true,
    wheelGesturesEnabled: true,
    sensitivity: 40,
    showTrail: true,
    showLabel: true,
    trailColor: "#37ff00",
    trailWidth: 6,
    contextMenu: {
      minDistance: 12,
      preventionTimeout: 200,
    },
    actions: [],
    rockerActions: {
      leftRight: "gecko-forward",
      rightLeft: "gecko-back",
    },
    wheelActions: { ...DEFAULT_WHEEL_ACTIONS },
  };
}

export function normalizeMouseGestureConfig(
  value: unknown,
  enabled: boolean,
): MouseGestureConfig {
  const defaults = createDefaultMouseGestureConfig(enabled);
  const source = isRecord(value) ? value : {};
  const contextMenu = isRecord(source.contextMenu) ? source.contextMenu : {};
  const rockerActions = isRecord(source.rockerActions)
    ? source.rockerActions
    : {};

  return {
    enabled,
    rockerGesturesEnabled: booleanOr(
      source.rockerGesturesEnabled,
      defaults.rockerGesturesEnabled,
    ),
    wheelGesturesEnabled: booleanOr(
      source.wheelGesturesEnabled,
      defaults.wheelGesturesEnabled,
    ),
    sensitivity: Math.min(
      100,
      Math.max(1, finiteNumberOr(source.sensitivity, defaults.sensitivity)),
    ),
    showTrail: booleanOr(source.showTrail, defaults.showTrail),
    showLabel: booleanOr(source.showLabel, defaults.showLabel),
    trailColor: stringOr(source.trailColor, defaults.trailColor),
    trailWidth: finiteNumberOr(source.trailWidth, defaults.trailWidth),
    contextMenu: {
      minDistance: Math.max(
        MIN_CONTEXT_MENU_DISTANCE,
        finiteNumberOr(
          contextMenu.minDistance,
          defaults.contextMenu.minDistance,
        ),
      ),
      preventionTimeout: Math.max(
        0,
        finiteNumberOr(
          contextMenu.preventionTimeout,
          defaults.contextMenu.preventionTimeout,
        ),
      ),
    },
    actions: normalizeActions(source.actions, defaults.actions),
    rockerActions: {
      leftRight: stringOr(
        rockerActions.leftRight,
        defaults.rockerActions.leftRight,
      ),
      rightLeft: stringOr(
        rockerActions.rightLeft,
        defaults.rockerActions.rightLeft,
      ),
    },
    wheelActions: normalizeWheelActions(source.wheelActions),
  };
}

export function parseMouseGestureConfig(
  serializedConfig: string | null,
  enabled: boolean,
): MouseGestureConfig {
  if (!serializedConfig) return createDefaultMouseGestureConfig(enabled);

  try {
    return normalizeMouseGestureConfig(JSON.parse(serializedConfig), enabled);
  } catch (error) {
    console.error("Failed to parse mouse gesture configuration", error);
    return createDefaultMouseGestureConfig(enabled);
  }
}

export function serializeMouseGestureConfig(
  config: MouseGestureConfig,
): string {
  const { enabled: _, ...configWithoutEnabled } = config;
  return JSON.stringify(configWithoutEnabled);
}

function errorToMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

export function createGestureConfigPersistence(
  initialConfig: MouseGestureConfig,
  writers: GestureConfigWriters,
): GestureConfigPersistence {
  let snapshot: GestureConfigPersistenceSnapshot = {
    config: normalizeMouseGestureConfig(initialConfig, initialConfig.enabled),
    pending: false,
    error: null,
  };
  let pendingCount = 0;
  let queueTail: Promise<void> = Promise.resolve();
  const listeners = new Set<
    (currentSnapshot: GestureConfigPersistenceSnapshot) => void
  >();

  const notify = () => {
    const currentSnapshot = { ...snapshot };
    for (const listener of listeners) listener(currentSnapshot);
  };

  const setPendingCount = (nextPendingCount: number) => {
    pendingCount = nextPendingCount;
    snapshot = { ...snapshot, pending: pendingCount > 0 };
    notify();
  };

  const enqueue = (
    operation: () => Promise<MouseGestureConfig>,
  ): Promise<boolean> => {
    setPendingCount(pendingCount + 1);

    const result = queueTail.then(async () => {
      try {
        const committedConfig = await operation();
        snapshot = {
          config: committedConfig,
          pending: snapshot.pending,
          error: null,
        };
        return true;
      } catch (error) {
        snapshot = {
          ...snapshot,
          error: errorToMessage(error),
        };
        return false;
      } finally {
        setPendingCount(pendingCount - 1);
      }
    });

    queueTail = result.then(() => undefined, () => undefined);
    return result;
  };

  return {
    getSnapshot: () => ({ ...snapshot }),
    subscribe: (listener) => {
      listeners.add(listener);
      listener({ ...snapshot });
      return () => listeners.delete(listener);
    },
    updateEnabled: (update) =>
      enqueue(async () => {
        const current = snapshot.config;
        const enabled = typeof update === "function"
          ? update(current.enabled)
          : update;
        await writers.writeEnabled(enabled);
        return { ...current, enabled };
      }),
    updateConfig: (update) =>
      enqueue(async () => {
        const current = snapshot.config;
        const partial = typeof update === "function" ? update(current) : update;
        const nextConfig = normalizeMouseGestureConfig(
          { ...current, ...partial },
          current.enabled,
        );
        await writers.writeConfig(serializeMouseGestureConfig(nextConfig));
        return nextConfig;
      }),
  };
}
