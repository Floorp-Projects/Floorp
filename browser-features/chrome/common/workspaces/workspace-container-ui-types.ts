// SPDX-License-Identifier: MPL-2.0

import type {
  WorkspaceContainerDefinition,
  WorkspaceContainerRef,
} from "#modules/common/workspace-container-types.ts";
import type {
  LocalWorkspaceContainer,
  WorkspaceContainerResolution,
} from "#modules/common/workspace-container-bindings.ts";

export interface WorkspaceContainerBindingAPI {
  isReady(): boolean;
  captureReference(
    localId: number,
    preferredRef?: WorkspaceContainerRef,
  ): { ref: WorkspaceContainerRef; definition?: WorkspaceContainerDefinition };
  resolve(ref: WorkspaceContainerRef): Promise<WorkspaceContainerResolution>;
  lookup(ref: WorkspaceContainerRef): WorkspaceContainerResolution;
  listLocalContainers(): LocalWorkspaceContainer[];
  bind(id: string, localId: number, expectedEpoch?: number): Promise<unknown>;
  create(id: string, expectedEpoch?: number): Promise<unknown>;
  getEpoch(): number;
  isCurrentEpoch(epoch: number): boolean;
  acquirePrompt(): boolean;
  releasePrompt(expectedEpoch?: number): void;
}

export interface WorkspaceContainerSetupActions {
  isCurrent(): boolean;
  unsubscribe(): Promise<void>;
  retry(): Promise<void>;
  capture(): Promise<void>;
  resolveLegacy(
    target: number | "new",
    scope: "default" | "tabs" | "all",
  ): Promise<void>;
}

export interface WorkspaceContainerPrompt {
  confirm(title: string, message: string): boolean;
  select(title: string, message: string, items: string[]): number | null;
  alert(title: string, message: string): void;
}
