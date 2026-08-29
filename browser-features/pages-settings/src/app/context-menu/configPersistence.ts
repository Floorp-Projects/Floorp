/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { ContextMenuConfig } from "#features-chrome/common/context-menu/types.ts";
import {
  parseContextMenuConfigWithStatus,
  serializeContextMenuConfig,
} from "#features-chrome/common/context-menu/config.ts";

export type ContextMenuPersistenceError =
  | { kind: "conflict" }
  | {
    kind: "unsafe-config";
    status: "invalid" | "unsupported-version";
    currentValue: string | null;
  }
  | {
    kind: "preference-type-mismatch";
    preference: "enabled" | "config";
  }
  | { kind: "write"; message: string };

export interface ContextMenuPersistenceState {
  enabled: boolean;
  config: ContextMenuConfig;
}

export interface ContextMenuPersistenceSnapshot {
  committed: ContextMenuPersistenceState;
  projected: ContextMenuPersistenceState;
  pending: boolean;
  error: ContextMenuPersistenceError | null;
  blockingErrors: {
    enabled: ContextMenuPersistenceError | null;
    config: ContextMenuPersistenceError | null;
  };
}

export interface ContextMenuCompareAndSetResult<T> {
  updated: boolean;
  currentValue: T | null;
  typeMismatch?: boolean;
}

export interface ContextMenuPersistenceWriters {
  compareAndSetEnabled(
    expectedValue: boolean | null,
    enabled: boolean,
  ): Promise<ContextMenuCompareAndSetResult<boolean>>;
  compareAndSetConfig(
    expectedValue: string | null,
    serializedConfig: string,
  ): Promise<ContextMenuCompareAndSetResult<string>>;
}

export interface ContextMenuPersistenceVersions {
  enabled: boolean | null;
  config: string | null;
}

export interface ContextMenuPersistence {
  getSnapshot(): ContextMenuPersistenceSnapshot;
  getVersions(): ContextMenuPersistenceVersions;
  subscribe(
    listener: (snapshot: ContextMenuPersistenceSnapshot) => void,
  ): () => void;
  updateEnabled(
    update: boolean | ((current: boolean) => boolean),
  ): Promise<boolean>;
  updateConfig(
    update: (current: Readonly<ContextMenuConfig>) => ContextMenuConfig,
  ): Promise<boolean>;
}

interface QueuedOperation {
  preference: "enabled" | "config";
  apply(current: ContextMenuPersistenceState): ContextMenuPersistenceState;
  write(
    target: ContextMenuPersistenceState,
    writers: ContextMenuPersistenceWriters,
  ): Promise<{
    saved: boolean;
    conflict: boolean;
    conflictError?: ContextMenuPersistenceError;
    state: ContextMenuPersistenceState;
  }>;
  resolve(saved: boolean): void;
}

function errorToMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

export function createContextMenuPersistence(
  initial: ContextMenuPersistenceState,
  writers: ContextMenuPersistenceWriters,
  initialVersions?: ContextMenuPersistenceVersions,
): ContextMenuPersistence {
  let committed = initial;
  let enabledVersion = initialVersions
    ? initialVersions.enabled
    : initial.enabled;
  let configVersion = initialVersions
    ? initialVersions.config
    : serializeContextMenuConfig(initial.config);
  let queue: QueuedOperation[] = [];
  let processing = false;
  let error: ContextMenuPersistenceError | null = null;
  let blockingErrors: ContextMenuPersistenceSnapshot["blockingErrors"] = {
    enabled: null,
    config: null,
  };
  const listeners = new Set<
    (snapshot: ContextMenuPersistenceSnapshot) => void
  >();

  const calculateProjected = (): ContextMenuPersistenceState =>
    queue.reduce(
      (state, operation) => operation.apply(state),
      committed,
    );

  let snapshot: ContextMenuPersistenceSnapshot = {
    committed,
    projected: committed,
    pending: false,
    error,
    blockingErrors,
  };

  const publish = () => {
    snapshot = {
      committed,
      projected: calculateProjected(),
      pending: queue.length > 0,
      error,
      blockingErrors,
    };
    for (const listener of listeners) listener(snapshot);
  };

  const processQueue = async () => {
    if (processing) return;
    processing = true;

    while (queue.length > 0) {
      const operation = queue[0];
      try {
        const target = operation.apply(committed);
        const result = await operation.write(target, writers);
        committed = result.state;
        queue = queue.slice(1);
        operation.resolve(result.saved);
        if (result.conflict) {
          error = result.conflictError ?? { kind: "conflict" };
          blockingErrors = {
            ...blockingErrors,
            [operation.preference]: result.conflictError ?? null,
          };
          const staleOperations = queue;
          queue = [];
          for (const staleOperation of staleOperations) {
            staleOperation.resolve(false);
          }
          publish();
          break;
        }
        blockingErrors = {
          ...blockingErrors,
          [operation.preference]: null,
        };
        error = null;
      } catch (writeError) {
        error = { kind: "write", message: errorToMessage(writeError) };
        queue = queue.slice(1);
        operation.resolve(false);
      }
      // Remaining functional/key operations are projected again from the last
      // successfully committed state. This is also the failure recovery path.
      publish();
    }

    processing = false;
  };

  const enqueue = (
    operationWithoutResolve: Omit<QueuedOperation, "resolve">,
  ): Promise<boolean> => {
    const result = new Promise<boolean>((resolve) => {
      queue = [...queue, { ...operationWithoutResolve, resolve }];
    });
    publish();
    void processQueue();
    return result;
  };

  return {
    getSnapshot: () => snapshot,
    getVersions: () => ({ enabled: enabledVersion, config: configVersion }),
    subscribe: (listener) => {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    updateEnabled: (update) =>
      enqueue({
        preference: "enabled",
        apply: (current) => ({
          ...current,
          enabled: typeof update === "function"
            ? update(current.enabled)
            : update,
        }),
        write: async (target, persistenceWriters) => {
          const result = await persistenceWriters.compareAndSetEnabled(
            enabledVersion,
            target.enabled,
          );
          if (result.typeMismatch) {
            return {
              saved: false,
              conflict: true,
              conflictError: {
                kind: "preference-type-mismatch",
                preference: "enabled",
              },
              state: committed,
            };
          }
          if (result.updated || result.currentValue === target.enabled) {
            enabledVersion = target.enabled;
            return { saved: true, conflict: false, state: target };
          }
          enabledVersion = result.currentValue;
          return {
            saved: false,
            conflict: true,
            state: {
              ...target,
              enabled: result.currentValue ?? true,
            },
          };
        },
      }),
    updateConfig: (update) =>
      enqueue({
        preference: "config",
        apply: (current) => ({
          ...current,
          config: update(current.config),
        }),
        write: async (target, persistenceWriters) => {
          const serializedConfig = serializeContextMenuConfig(target.config);
          const result = await persistenceWriters.compareAndSetConfig(
            configVersion,
            serializedConfig,
          );
          if (result.typeMismatch) {
            return {
              saved: false,
              conflict: true,
              conflictError: {
                kind: "preference-type-mismatch",
                preference: "config",
              },
              state: committed,
            };
          }
          if (result.updated || result.currentValue === serializedConfig) {
            configVersion = serializedConfig;
            return { saved: true, conflict: false, state: target };
          }

          const parsed = parseContextMenuConfigWithStatus(result.currentValue);
          if (
            parsed.status === "invalid" ||
            parsed.status === "unsupported-version"
          ) {
            return {
              saved: false,
              conflict: true,
              conflictError: {
                kind: "unsafe-config",
                status: parsed.status,
                currentValue: result.currentValue,
              },
              state: committed,
            };
          }
          configVersion = result.currentValue;
          return {
            saved: false,
            conflict: true,
            state: { ...target, config: parsed.config },
          };
        },
      }),
  };
}
