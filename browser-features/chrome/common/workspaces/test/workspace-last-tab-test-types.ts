// SPDX-License-Identifier: MPL-2.0

export type TestWindow = Window & {
  gBrowserInit?: { delayedStartupFinished: boolean };
  gBrowser: Omit<GBrowser, "removeTab" | "hideTab" | "addTabGroup"> & {
    removeTab(tab: XULElement, options: { animate: boolean }): void;
    hideTab(tab: XULElement, source?: string): void;
    addTabGroup(tabs: XULElement[]): { collapsed: boolean };
  };
  workspacesFuncs?: {
    createNoNameWorkspace(): string;
    getSelectedWorkspaceID(): string;
    changeWorkspace(id: string): void;
  };
};
