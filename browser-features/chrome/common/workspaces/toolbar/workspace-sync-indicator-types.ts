// SPDX-License-Identifier: MPL-2.0

export interface WorkspaceSyncIndicator {
  state: "selected" | "attention" | undefined;
  tooltip: string;
}
