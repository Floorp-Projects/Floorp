// SPDX-License-Identifier: MPL-2.0

import type { TWorkspace } from "./type.ts";

export interface WorkspaceSessionIdentity {
  userContextId: number;
  name: string;
  public?: boolean;
  l10nId?: string;
  floorpPrivateContainer?: boolean;
}

export interface WorkspaceSessionIdentityService {
  ensureDataReady(): void;
  getPublicIdentities(): WorkspaceSessionIdentity[];
  getUserContextLabel(userContextId: number): string;
  create(name: string, icon: string, color: string): WorkspaceSessionIdentity;
  remove(userContextId: number): boolean;
}

export interface ResolvedWorkspaceSession {
  userContextId: number;
  created: boolean;
}

export type WorkspaceSessionErrorReason = "invalid" | "missing" | "create";

export type WorkspaceCreationOptions = Pick<
  TWorkspace,
  "userContextId" | "icon"
>;

export type WorkspaceSessionReferenceWindow = Window & {
  gBrowser?: {
    tabs?: Iterable<{ userContextId?: number }>;
  };
};

export type WorkspaceOperationWindow = Window & {
  workspacesFuncs?: { getSelectedWorkspaceID?(): string };
  gBrowser?: { tabs: Iterable<XULElement> };
};

export type WorkspaceSessionTabBrowser =
  & Omit<GBrowser, "addTab" | "removeTab">
  & {
    addTab(url: string, options: Record<string, unknown>): XULElement;
    removeTab(tab: XULElement, options?: {
      animate?: boolean;
      skipSessionStore?: boolean;
    }): void;
  };
