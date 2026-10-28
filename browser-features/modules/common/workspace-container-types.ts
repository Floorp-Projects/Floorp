// SPDX-License-Identifier: MPL-2.0

/** A wire identity, never a profile's numeric userContextId. */
export type WorkspaceContainerRef =
  | { kind: "none" }
  | { kind: "container"; containerId: string }
  | { kind: "legacy-unresolved" };

/** Creation metadata; rebinding does not publish local container edits. */
export interface WorkspaceContainerDefinition {
  version: 2;
  kind: "container";
  containerId: string;
  name: string;
  color: string;
  icon: string;
  modifiedAt: number;
  modifiedBy: string;
}

export type ContainerRef = WorkspaceContainerRef;
export type SyncedContainerDefinition = WorkspaceContainerDefinition;
