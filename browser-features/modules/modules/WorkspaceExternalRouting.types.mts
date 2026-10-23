// SPDX-License-Identifier: MPL-2.0

export interface RoutingTab extends Element {
  userContextId?: number;
  linkedBrowser: RoutingBrowser;
}

export interface RoutingBrowser extends Element {
  browsingContext: { originAttributes?: { userContextId?: number } } | null;
  userContextId?: number;
  fixupAndLoadURIString(url: string, options: Record<string, unknown>): void;
}

export interface RoutingWindow extends Window {
  arguments?: unknown[];
  gNotificationBox?: {
    PRIORITY_WARNING_MEDIUM: number;
    appendNotification(
      id: string,
      options: { label: string; priority: number },
      buttons: Array<{ label: string; callback(): void }>,
    ): Promise<unknown>;
  };
  gBrowser?: {
    selectedTab: RoutingTab;
    selectedBrowser: RoutingBrowser;
    tabs: Iterable<RoutingTab>;
    addTab(url: string, options: Record<string, unknown>): RoutingTab;
    addAdjacentTab(
      tab: RoutingTab,
      url: string,
      options: Record<string, unknown>,
    ): RoutingTab;
    hideTab(tab: RoutingTab): void;
  };
}

export interface WorkspaceExternalRequest {
  readonly window: RoutingWindow | null;
  readonly workspaceId: string | null;
  readonly userContextId: number;
  readonly reuseTab: RoutingTab | null;
}

export interface InitialWindowOptions {
  userContextId?: unknown;
  tabArgument?: RoutingTab | null;
  openWindowInfo?: { originAttributes?: { userContextId?: number } } | null;
}

export interface InitialWindowSession {
  workspaceId: string | null;
  requestedUserContextId: number;
  userContextId: number;
}

export interface ExternalOpenOptions {
  where: number;
  triggeringPrincipal: unknown;
  policyContainer?: unknown;
  referrerInfo?: unknown;
  forceAllowDataURI?: boolean;
  request?: WorkspaceExternalRequest;
}
