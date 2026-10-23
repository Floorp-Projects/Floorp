// SPDX-License-Identifier: MPL-2.0

export type IndicatorTestTab = XULElement & {
  linkedPanel: string;
  pinned: boolean;
};

export type IndicatorTestSplitView = XULElement & {
  unsplitTabs(): void;
};

export type IndicatorTestBrowser =
  & Omit<
    GBrowser,
    "addTab" | "selectedTab" | "tabs" | "tabContainer"
  >
  & {
    selectedTab: IndicatorTestTab;
    tabs: IndicatorTestTab[];
    tabContainer: XULElement & { verticalMode: boolean };
    addTab(url: string, options: Record<string, unknown>): IndicatorTestTab;
    unpinTab(tab: IndicatorTestTab): void;
    addTabSplitView?(
      tabs: IndicatorTestTab[],
      options: { insertBefore: IndicatorTestTab },
    ): IndicatorTestSplitView | null;
    showSplitViewPanels?(tabs: IndicatorTestTab[]): void;
  };

export interface IndicatorTestSidebar {
  getUIState(): { launcherExpanded: boolean; launcherVisible: boolean };
  updateUIState(state: {
    launcherExpanded: boolean;
    launcherVisible: boolean;
  }): Promise<void>;
}
