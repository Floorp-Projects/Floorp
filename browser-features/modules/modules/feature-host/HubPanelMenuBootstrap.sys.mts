// SPDX-License-Identifier: MPL-2.0

import { M0FeatureHost } from "./FeatureHost.sys.mts";
import {
  type AppMenuAdapter,
  type AppMenuAdapterConnection,
  getSharedAppMenuRegistry,
} from "./AppMenuRegistry.sys.mts";
import { isCandidateFeatureWindow } from "./WindowIdentity.sys.mts";

const STARTUP_MODE = Services.prefs.getStringPref("nora.startup.mode", "");
const HUB_MENU_SELECTED = (STARTUP_MODE === "dev" || STARTUP_MODE === "test") &&
  Services.prefs.getBoolPref("nora.dev.hub_menu_feature", false);

let hostStartup: Promise<void> | null = null;

export function initializeHubPanelMenuFeature(): void {
  if (!HUB_MENU_SELECTED || hostStartup) return;
  hostStartup = M0FeatureHost.openHubPanelMenu(true).then(
    () => {},
    (error: unknown) => {
      console.error("[HubPanelMenuBootstrap] feature startup failed", error);
    },
  );
}

export function usesExperimentalHubPanelMenu(window: Window): boolean {
  return HUB_MENU_SELECTED && isCandidateFeatureWindow(window);
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
