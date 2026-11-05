// SPDX-License-Identifier: MPL-2.0

export type AdoptionTab = XULElement & {
  linkedBrowser: { contentTitle: string; currentURI: { spec: string } };
};

export type AdoptionGroup = XULElement & { tabs: AdoptionTab[] };

export type AdoptionBrowser = {
  tabs: AdoptionTab[];
  tabGroups: AdoptionGroup[];
  selectedTab: AdoptionTab;
  addTab(url: string, options: Record<string, unknown>): AdoptionTab;
  addTabGroup(tabs: AdoptionTab[], options: { label: string }): AdoptionGroup;
  addTabSplitView(
    tabs: AdoptionTab[],
    options: { insertBefore: AdoptionTab },
  ): void;
  replaceGroupWithWindow(group: AdoptionGroup): AdoptionWindow;
  adoptTabGroup(
    group: AdoptionGroup,
    options: { tabIndex: number },
  ): AdoptionGroup;
  removeTab(tab: AdoptionTab, options: { animate: boolean }): void;
};

export type AdoptionWindow = Window & { gBrowser: AdoptionBrowser };
