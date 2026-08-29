/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { ContextMenuConfig } from "#features-chrome/common/context-menu/types.ts";
import { serializeContextMenuConfig } from "#features-chrome/common/context-menu/config.ts";

export interface ContextMenuPersistenceState {
  enabled: boolean;
  config: ContextMenuConfig;
}

export interface ContextMenuPersistenceSnapshot {
  committed: ContextMenuPersistenceState;
  projected: ContextMenuPersistenceState;
  pending: boolean;
  error: string | null;
}

export interface ContextMenuPersistenceWriters {
  writeEnabled(enabled: boolean): Promise<void>;
  writeConfig(serializedConfig: string): Promise<void>;
}

export interface ContextMenuPersistence {
  getSnapshot(): ContextMenuPersistenceSnapshot;
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
  apply(current: ContextMenuPersistenceState): ContextMenuPersistenceState;
  write(
    target: ContextMenuPersistenceState,
    writers: ContextMenuPersistenceWriters,
  ): Promise<void>;
  resolve(saved: boolean): void;
}

function errorToMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

export function createContextMenuPersistence(
  initial: ContextMenuPersistenceState,
  writers: ContextMenuPersistenceWriters,
): ContextMenuPersistence {
  let committed = initial;
  let queue: QueuedOperation[] = [];
  let processing = false;
  let error: string | null = null;
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
  };

  const publish = () => {
    snapshot = {
      committed,
      projected: calculateProjected(),
      pending: queue.length > 0,
      error,
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
        await operation.write(target, writers);
        committed = target;
        error = null;
        queue = queue.slice(1);
        operation.resolve(true);
      } catch (writeError) {
        error = errorToMessage(writeError);
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
    subscribe: (listener) => {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    updateEnabled: (update) =>
      enqueue({
        apply: (current) => ({
          ...current,
          enabled: typeof update === "function"
            ? update(current.enabled)
            : update,
        }),
        write: (target, persistenceWriters) =>
          persistenceWriters.writeEnabled(target.enabled),
      }),
    updateConfig: (update) =>
      enqueue({
        apply: (current) => ({
          ...current,
          config: update(current.config),
        }),
        write: (target, persistenceWriters) =>
          persistenceWriters.writeConfig(
            serializeContextMenuConfig(target.config),
          ),
      }),
  };
}
