// SPDX-License-Identifier: MPL-2.0

import type {
  WorkspaceContainerDefinition,
  WorkspaceContainerRef,
} from "./workspace-container-types.ts";

function object(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function uuid(value: unknown): value is string {
  return typeof value === "string" &&
    /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i.test(
      value,
    );
}

/** Missing and unsupported references must never become the default container. */
export function parseWorkspaceContainerRef(
  value: unknown,
): WorkspaceContainerRef {
  if (!object(value)) throw new Error("Missing workspace container reference");
  if (value.kind === "none" || value.kind === "legacy-unresolved") {
    if (Object.keys(value).length !== 1) {
      throw new Error("Ambiguous workspace container reference");
    }
    return { kind: value.kind };
  }
  if (
    value.kind === "container" && uuid(value.containerId) &&
    Object.keys(value).length === 2
  ) {
    return { kind: "container", containerId: value.containerId };
  }
  throw new Error("Invalid workspace container reference");
}

export function sameWorkspaceContainerRef(
  a: WorkspaceContainerRef | undefined,
  b: WorkspaceContainerRef | undefined,
): boolean {
  if (!a || !b) return a === b;
  return a.kind === b.kind &&
    (a.kind !== "container" ||
      (b.kind === "container" && a.containerId === b.containerId));
}

export function parseWorkspaceContainerDefinition(
  value: unknown,
): WorkspaceContainerDefinition {
  if (
    !object(value) || value.version !== 2 || value.kind !== "container" ||
    !uuid(value.containerId) || !uuid(value.modifiedBy) ||
    typeof value.modifiedAt !== "number" ||
    !Number.isSafeInteger(value.modifiedAt) || value.modifiedAt < 0 ||
    typeof value.name !== "string" || value.name.length > 256 ||
    !value.name.isWellFormed() ||
    typeof value.color !== "string" ||
    !/^[a-z][a-z0-9-]{0,63}$/.test(value.color) ||
    typeof value.icon !== "string" ||
    !/^[a-z][a-z0-9-]{0,63}$/.test(value.icon)
  ) throw new Error("Invalid workspace container definition");
  // Reconstruct in a fixed order; local IDs, site data and unknown fields stay local.
  return {
    version: 2,
    kind: "container",
    containerId: value.containerId,
    name: value.name,
    color: value.color,
    icon: value.icon,
    modifiedAt: value.modifiedAt,
    modifiedBy: value.modifiedBy,
  };
}

export function mergeWorkspaceContainerDefinitions(
  a: WorkspaceContainerDefinition,
  b: WorkspaceContainerDefinition,
): WorkspaceContainerDefinition {
  const left = parseWorkspaceContainerDefinition(a);
  const right = parseWorkspaceContainerDefinition(b);
  if (left.containerId !== right.containerId) {
    throw new Error("Workspace container ID mismatch");
  }
  if (left.modifiedAt !== right.modifiedAt) {
    return left.modifiedAt > right.modifiedAt ? left : right;
  }
  if (left.modifiedBy !== right.modifiedBy) {
    return left.modifiedBy > right.modifiedBy ? left : right;
  }
  return JSON.stringify(left) >= JSON.stringify(right) ? left : right;
}
