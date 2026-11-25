// SPDX-License-Identifier: MPL-2.0

import type { HubPanelMenuStartupSelection } from "./HubPanelMenuStartupSelection.types.ts";

export const HUB_MENU_FEATURE_PREF_DEFAULT = false;
export const HUB_MENU_MANUAL_HOST_PREF_DEFAULT = false;

export function selectHubPanelMenuStartupMode(
  startupMode: string,
  featureEnabled: boolean,
  manualHostEnabled: boolean,
): HubPanelMenuStartupSelection {
  if (
    (startupMode !== "dev" && startupMode !== "test") || !featureEnabled
  ) {
    return "legacy";
  }
  return manualHostEnabled ? "manual" : "pinned";
}
