// SPDX-License-Identifier: MPL-2.0

export type TestWindow = Window & {
  __NORA_TEST_FEATURES_READY__?: boolean;
  gBrowserInit?: { delayedStartupFinished: boolean };
  gBrowser: Omit<GBrowser, "removeTab" | "hideTab" | "addTabGroup"> & {
    browsers: Array<GBrowser["selectedBrowser"]>;
    getTabForBrowser(browser: GBrowser["selectedBrowser"]): XULElement;
    removeTab(tab: XULElement, options: { animate: boolean }): void;
    hideTab(tab: XULElement, source?: string): void;
    addTabGroup(tabs: XULElement[]): { collapsed: boolean };
    insertBrowser(tab: XULElement): void;
  };
  workspacesFuncs?: {
    createNoNameWorkspace(): string;
    getSelectedWorkspaceID(): string;
    changeWorkspace(id: string): void;
  };
};

export interface TestWindowClosure {
  wait(): Promise<void>;
  dispose(): void;
}
