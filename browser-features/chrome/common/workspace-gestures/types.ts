// SPDX-License-Identifier: MPL-2.0

export type WorkspaceGestureSource = "sidebar" | "native";
export type WorkspaceGesturePhase = "start" | "update" | "end" | "cancel";

export interface WorkspaceSwitchTarget {
  id: string;
  name: string;
  sessionName: string;
  userContextId: number;
}

export interface WorkspaceSwitchSnapshot {
  currentId: string;
  order: readonly WorkspaceSwitchTarget[];
}

export interface WorkspaceGestureInput {
  source: WorkspaceGestureSource;
  operationId: string;
  phase: WorkspaceGesturePhase;
  displacementX: number;
  displacementY: number;
  time: number;
  inertia?: boolean;
  fingerCount?: number;
}

export interface WorkspaceGesturePreview {
  source: WorkspaceGestureSource;
  target: WorkspaceSwitchTarget;
  progress: number;
  edge: boolean;
}

export interface WorkspaceGestureEnvironment {
  getSnapshot(): WorkspaceSwitchSnapshot | null;
  commit(id: string): void;
  preview(value: WorkspaceGesturePreview | null): void;
  announce(target: WorkspaceSwitchTarget): void;
}

export interface ActiveWorkspaceGesture {
  source: WorkspaceGestureSource;
  operationId: string;
  snapshot: WorkspaceSwitchSnapshot;
  signature: string;
  offset: -1 | 1 | null;
  target: WorkspaceSwitchTarget | null;
  edge: boolean;
  lastX: number;
  lastTime: number;
  velocity: number;
}

export type NativeWorkspaceGestureStatus =
  | "available"
  | "unverified"
  | "runtime-required"
  | "unsupported";

export interface NativeWorkspaceGestureCapabilities {
  status: NativeWorkspaceGestureStatus;
  platform: "linux" | "macos" | "windows" | "other";
}

export interface NativeWorkspaceSwipeEvent extends Event {
  fingerCount?: number;
  phase?: string;
  operationId?: number;
  delta?: number;
  deltaY?: number;
}

export interface WorkspaceGestureWindow extends Window {
  SimpleGestureEvent?: { prototype: object };
}

export interface SidebarWheelBurst {
  operationId: string;
  x: number;
  y: number;
  startTarget: Element;
  blocked: boolean;
  lastTime: number;
}
