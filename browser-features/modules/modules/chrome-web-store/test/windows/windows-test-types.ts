// SPDX-License-Identifier: MPL-2.0

export interface BoundsAddon {
  uninstall(): Promise<void>;
}

export interface BoundsSnapshot {
  id: number;
  left: number;
  top: number;
  width: number;
  height: number;
}

export interface BoundsChecksReport {
  phase: string;
  checks: string[];
  error?: string;
  requested?: { width: number; height: number };
  resized?: BoundsSnapshot;
  event?: BoundsSnapshot;
}

export interface BoundsWakeReport {
  phase: string;
  events: BoundsSnapshot[];
}

export interface BoundsPrivateWindow extends Window {
  gBrowserInit?: { delayedStartupFinished: boolean };
}

export interface BoundsBrowserWindow extends Window {
  OpenBrowserWindow(options?: { private?: boolean }): BoundsPrivateWindow;
}
