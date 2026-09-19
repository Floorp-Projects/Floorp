// SPDX-License-Identifier: MPL-2.0
import type { StackGroup, StackTab, TabBrowser } from "../stack-bar.tsx";

export type NativeStackTab = StackTab & {
  splitview: NativeSplitView | null;
};

export type NativeStackTransfer = DataTransfer & {
  mozSetDataAt(format: string, value: unknown, index: number): void;
};

export type NativeSplitView = XULElement & {
  tabs: NativeStackTab[];
  unsplitTabs(): void;
};

export type NativeStackBrowser = TabBrowser & {
  selectedTab: NativeStackTab;
  tabContainer: XULElement & { verticalMode: boolean };
  addTabGroup(tabs: StackTab[], options: { label: string }): StackGroup;
  addToMultiSelectedTabs(tab: StackTab): void;
  addTabSplitView(
    tabs: StackTab[],
    options: { insertBefore: StackTab },
  ): NativeSplitView;
};

export type StackTestSidebar = {
  getUIState(): { launcherExpanded: boolean; launcherVisible: boolean };
  updateUIState(
    state: { launcherExpanded: boolean; launcherVisible: boolean },
  ): Promise<void>;
};

export type StackTestPopup = XULElement & {
  state: string;
  hidePopup(): void;
};
