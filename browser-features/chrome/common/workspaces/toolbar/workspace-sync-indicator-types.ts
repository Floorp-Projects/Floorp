// SPDX-License-Identifier: MPL-2.0

export interface WorkspaceSyncIndicator {
  state: "selected" | "attention" | undefined;
  status: string;
  error: string | null;
  tooltip: string;
}
