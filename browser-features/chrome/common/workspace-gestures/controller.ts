// SPDX-License-Identifier: MPL-2.0

import type {
  ActiveWorkspaceGesture,
  WorkspaceGestureEnvironment,
  WorkspaceGestureInput,
  WorkspaceSwitchSnapshot,
  WorkspaceSwitchTarget,
} from "./types.ts";

export function workspaceSnapshotSignature(
  snapshot: WorkspaceSwitchSnapshot,
): string {
  return JSON.stringify([snapshot.currentId, snapshot.order]);
}

export function adjacentWorkspace(
  snapshot: WorkspaceSwitchSnapshot,
  offset: -1 | 1,
): WorkspaceSwitchTarget | null {
  const index = snapshot.order.findIndex((entry) =>
    entry.id === snapshot.currentId
  );
  if (index < 0) return null;
  return snapshot.order[index + offset] ?? null;
}

/** Receives genuine input phases; it never selects a tab during preview. */
export class WorkspaceGestureController {
  private active: ActiveWorkspaceGesture | null = null;
  private readonly completed = new Set<string>();

  constructor(private readonly environment: WorkspaceGestureEnvironment) {}

  get isActive(): boolean {
    return this.active !== null;
  }

  private completeKey(source: string, operationId: string): string {
    return `${source}:${operationId}`;
  }

  private remember(source: string, operationId: string): void {
    this.completed.add(this.completeKey(source, operationId));
    if (this.completed.size > 32) {
      const oldest = this.completed.values().next().value;
      if (oldest !== undefined) this.completed.delete(oldest);
    }
  }

  cancel(): void {
    if (this.active) this.remember(this.active.source, this.active.operationId);
    this.active = null;
    this.environment.preview(null);
  }

  validate(): void {
    if (!this.active) return;
    const current = this.environment.getSnapshot();
    if (
      !current || workspaceSnapshotSignature(current) !== this.active.signature
    ) {
      this.cancel();
    }
  }

  switchByOffset(offset: -1 | 1): boolean {
    this.cancel();
    const snapshot = this.environment.getSnapshot();
    if (!snapshot || snapshot.order.length < 2) return false;
    const target = adjacentWorkspace(snapshot, offset);
    if (!target) return false;
    this.environment.commit(target.id);
    this.environment.announce(target);
    return true;
  }

  handle(input: WorkspaceGestureInput): boolean {
    if (
      !input.operationId || !Number.isFinite(input.displacementX) ||
      !Number.isFinite(input.displacementY) || !Number.isFinite(input.time) ||
      (input.source === "native" && input.fingerCount !== 3)
    ) return false;
    const key = this.completeKey(input.source, input.operationId);
    if (this.completed.has(key)) return false;
    if (input.phase === "cancel") {
      if (
        this.active?.operationId === input.operationId &&
        this.active.source === input.source
      ) {
        this.cancel();
      }
      return false;
    }
    if (input.inertia) return false;
    if (input.phase === "start") {
      if (this.active) return false;
      const snapshot = this.environment.getSnapshot();
      if (!snapshot || snapshot.order.length < 2) return false;
      this.active = {
        source: input.source,
        operationId: input.operationId,
        snapshot,
        signature: workspaceSnapshotSignature(snapshot),
        offset: null,
        target: null,
        edge: false,
        lastX: input.displacementX,
        lastTime: input.time,
        velocity: 0,
      };
      return false;
    }
    const active = this.active;
    if (
      !active || active.source !== input.source ||
      active.operationId !== input.operationId
    ) return false;
    this.validate();
    if (this.active !== active) return false;
    const elapsed = input.time - active.lastTime;
    if (elapsed > 0 && input.displacementX !== active.lastX) {
      active.velocity = (input.displacementX - active.lastX) / elapsed;
    } else if (elapsed > 100) {
      active.velocity = 0;
    }
    active.lastX = input.displacementX;
    active.lastTime = input.time;
    const horizontal = Math.abs(input.displacementX);
    const vertical = Math.abs(input.displacementY);
    if (active.offset === null) {
      if (vertical >= 12 && vertical >= horizontal) {
        this.cancel();
        return false;
      }
      if (horizontal >= 12 && horizontal > vertical * 1.4) {
        active.offset = input.displacementX < 0 ? 1 : -1;
        active.target = adjacentWorkspace(active.snapshot, active.offset);
        active.edge = active.target === null;
      }
    }
    if (active.offset !== null) {
      const target = active.target ??
        active.snapshot.order.find((entry) =>
          entry.id === active.snapshot.currentId
        );
      if (target) {
        this.environment.preview({
          source: active.source,
          target,
          progress: Math.max(-1, Math.min(1, input.displacementX / 120)),
          edge: active.edge,
        });
      }
    }
    if (input.phase !== "end") return active.offset !== null;
    const sameDirection = active.offset === 1
      ? input.displacementX < 0
      : input.displacementX > 0;
    const velocityMatches = active.offset === 1
      ? active.velocity < 0
      : active.velocity > 0;
    const shouldCommit = active.target && sameDirection &&
      horizontal > vertical * 1.4 &&
      (horizontal >= 96 ||
        (horizontal >= 36 && velocityMatches &&
          Math.abs(active.velocity) >= 0.55));
    this.remember(active.source, active.operationId);
    this.active = null;
    this.environment.preview(null);
    if (!shouldCommit || !active.target) return active.offset !== null;
    // validate() above ran immediately before this synchronous commit.
    this.environment.commit(active.target.id);
    this.environment.announce(active.target);
    return true;
  }
}
