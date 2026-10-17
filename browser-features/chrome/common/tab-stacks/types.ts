// SPDX-License-Identifier: MPL-2.0
import type { StackTab, TabBrowser } from "./stack-bar.tsx";

export type TabStacksChrome = {
  gb: TabBrowser;
  head: Element;
  toolbox: Element;
  navBar: Element;
};

export type TabStacksStartupWindow = EventTarget & {
  closed: boolean;
  delayedStartupPromise?: Promise<unknown>;
  gBrowserInit?: {
    delayedStartupPromise?: Promise<unknown>;
    delayedStartupFinished?: boolean;
  };
};

export type TabStacksStartupOptions = {
  window: TabStacksStartupWindow;
  getChrome(): TabStacksChrome | null;
  initialize(chrome: TabStacksChrome): void;
  logger: Pick<ConsoleInstance, "warn" | "error" | "info">;
};

export type StackSplitView = XULElement & {
  tabs: StackTab[];
};

export type StackDragController = {
  startTabDrag(
    event: DragEvent,
    tab: StackTab,
    options: { fromTabList: boolean },
  ): void;
  handle_dragend(event: DragEvent): void;
  getDropEffectForTabDrag(event: DragEvent): string;
  finishMoveTogetherSelectedTabs(tab: StackTab): void;
  finishAnimateTabMove(): void;
  _resetTabsAfterDrop(tab: StackTab): void;
};

export type ProxyDragTransaction = {
  tab: StackTab;
  source: Element;
  controller: StackDragController;
  data: object;
  sawSession: boolean;
  dropped: boolean;
};

export type ProxyDragEnvironment = {
  readSession(): unknown;
  schedule(callback: () => void): number;
  cancel(timer: number): void;
  finished(): void;
};

// The runtime takes the source window; the generated Gecko type predates it.
export type ProxyDragService = {
  getCurrentSession(window: Window): nsIDragSession | null;
};

export type StackDataTransfer = DataTransfer & {
  mozGetDataAt(format: string, index: number): unknown;
};
