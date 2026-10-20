// SPDX-License-Identifier: MPL-2.0
export interface WorkArea {
  left: number;
  top: number;
  right: number;
  bottom: number;
}
export interface ChromeOSSnapshot {
  ownedChromeWindows: {
    workArea: WorkArea | null;
    dpi: number;
    processId: number;
  }[];
  authoredUi?: {
    matches?: unknown[];
    error?: string;
    observerExitCode?: number | null;
  };
}
export interface PublicNotificationResult {
  runtimeId: string;
  state: {
    ready: boolean;
    created: string | null;
    events: { id: string; byUser: boolean; at: number }[];
    steps: { method: string; value: unknown; at: number }[];
    errors: { method: string; name: string; message: string; at: number }[];
  };
}
