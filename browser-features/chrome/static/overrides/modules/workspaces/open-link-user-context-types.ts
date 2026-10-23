// SPDX-License-Identifier: MPL-2.0

export type OpenLinkUserContextInput = {
  userContextId?: number | null;
  targetBrowser?: unknown;
  [key: string]: unknown;
};

export type OpenLinkUserContextOptions =
  & Omit<
    OpenLinkUserContextInput,
    "userContextId"
  >
  & {
    userContextId?: number;
  };

export type WorkspaceOpenLinkUserContextResolution = {
  options: OpenLinkUserContextOptions;
  originalUserContextId: number | undefined;
  shouldApplyWorkspaceContainer: boolean;
};
