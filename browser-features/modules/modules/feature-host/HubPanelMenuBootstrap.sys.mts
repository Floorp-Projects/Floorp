// SPDX-License-Identifier: MPL-2.0

import { M0FeatureHost } from "./FeatureHost.sys.mts";
import {
  type AppMenuAdapter,
  type AppMenuAdapterConnection,
  getSharedAppMenuRegistry,
} from "./AppMenuRegistry.sys.mts";
import {
  HUB_MENU_FEATURE_PREF_DEFAULT,
  HUB_MENU_MANUAL_HOST_PREF_DEFAULT,
  selectHubPanelMenuStartupMode,
} from "./HubPanelMenuStartupSelection.ts";
import { isCandidateFeatureWindow } from "./WindowIdentity.sys.mts";

const STARTUP_SELECTION = selectHubPanelMenuStartupMode(
  Services.prefs.getStringPref("nora.startup.mode", ""),
  Services.prefs.getBoolPref(
    "nora.dev.hub_menu_feature",
    HUB_MENU_FEATURE_PREF_DEFAULT,
  ),
  Services.prefs.getBoolPref(
    "nora.dev.hub_menu_manual_host",
    HUB_MENU_MANUAL_HOST_PREF_DEFAULT,
  ),
);

let hostStartup: Promise<void> | null = null;

export function initializeHubPanelMenuFeature(): void {
  if (STARTUP_SELECTION !== "pinned" || hostStartup) return;
  hostStartup = M0FeatureHost.openHubPanelMenu(true).then(
    () => {},
    (error: unknown) => {
      console.error("[HubPanelMenuBootstrap] feature startup failed", error);
    },
  );
}

export function usesExperimentalHubPanelMenu(window: Window): boolean {
  return STARTUP_SELECTION !== "legacy" && isCandidateFeatureWindow(window);
}

export function registerHubPanelMenuAdapter(
  window: Window,
  adapter: AppMenuAdapter,
): AppMenuAdapterConnection | null {
  if (!usesExperimentalHubPanelMenu(window)) return null;
  initializeHubPanelMenuFeature();
  return getSharedAppMenuRegistry().registerAdapter(window, adapter);
}

export interface HubPanelMenuBootstrapModule {
  usesExperimentalHubPanelMenu: typeof usesExperimentalHubPanelMenu;
  registerHubPanelMenuAdapter: typeof registerHubPanelMenuAdapter;
}
