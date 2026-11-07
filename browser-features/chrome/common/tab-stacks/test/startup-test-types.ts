// SPDX-License-Identifier: MPL-2.0
import type { TabStacksChrome } from "../types.ts";
import type { StackGroup, StackTab, TabBrowser } from "../stack-bar.tsx";

export type StartupDeferred = {
  promise: Promise<unknown>;
  resolve(value?: unknown): void;
  reject(reason: unknown): void;
};

export type StartupLogs = {
  info: unknown[][];
  warn: unknown[][];
  error: unknown[][];
};

export type StartupMount = (chrome: TabStacksChrome) => void;

export interface SessionStackWindow extends Window {
  gBrowser: TabBrowser & {
    addTabGroup(tabs: StackTab[], options: { label: string }): StackGroup;
  };
  gBrowserInit?: { delayedStartupFinished: boolean };
  OpenBrowserWindow(): SessionStackWindow;
}

export type StackSessionWindowState = {
  tabs: Array<{ entries?: Array<{ url?: string }>; groupId?: string }>;
  groups?: Array<{ id: string; name?: string }>;
};

export type StackSessionStore = {
  getWindowState(win: Window): string | { windows: StackSessionWindowState[] };
  getClosedWindowData(): StackSessionWindowState[];
  undoCloseWindow(index: number): SessionStackWindow;
  forgetClosedWindow(index: number): void;
};
