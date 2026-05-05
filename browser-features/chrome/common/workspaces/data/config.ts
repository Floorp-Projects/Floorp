/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal, effect } from "@preact/signals";
import type { Signal } from "@preact/signals";
import {
  type TWorkspacesServicesConfigs,
  zWorkspacesServicesConfigs,
} from "../utils/type.js";
import { getOldConfigs } from "./old-config";
import { createRootHMR } from "#features-chrome/utils/base";
import {
  WORKSPACE_ENABLED_PREF_NAME,
  WORKSPACED_CONFIG_PREF_NAME,
} from "../utils/workspaces-static-names.js";
import { isRight } from "fp-ts/Either";

// ===== enabled =====

function createEnabled(): Signal<boolean> {
  const sig = signal(
    Services.prefs.getBoolPref(WORKSPACE_ENABLED_PREF_NAME, true),
  );

  effect(() => {
    const v = sig.value;
    Services.prefs.setBoolPref(WORKSPACE_ENABLED_PREF_NAME, v);
  });

  const observer = () =>
    (sig.value = Services.prefs.getBoolPref(WORKSPACE_ENABLED_PREF_NAME));
  Services.prefs.addObserver(WORKSPACE_ENABLED_PREF_NAME, observer);
  import.meta.hot?.dispose(() => {
    Services.prefs.removeObserver(WORKSPACE_ENABLED_PREF_NAME, observer);
  });

  return sig;
}

/** enable/disable workspaces */
export const enabled: Signal<boolean> = createRootHMR(
  createEnabled,
  import.meta.hot,
);
export const setEnabled = (v: boolean): void => {
  enabled.value = v;
};

// ===== configStore =====

function buildInitialConfig(): TWorkspacesServicesConfigs {
  const oldConfigs = JSON.parse(getOldConfigs);
  const loadedConfigs = JSON.parse(
    Services.prefs.getStringPref(WORKSPACED_CONFIG_PREF_NAME, getOldConfigs),
  );
  const mergedConfigs = { ...oldConfigs, ...loadedConfigs };
  const configResult = zWorkspacesServicesConfigs.decode(mergedConfigs);
  if (isRight(configResult)) {
    return configResult.right;
  }
  console.error(
    "Failed to decode workspace configuration, attempting partial recovery:",
    configResult.left,
  );
  return {
    manageOnBms:
      typeof mergedConfigs.manageOnBms === "boolean"
        ? mergedConfigs.manageOnBms
        : oldConfigs.manageOnBms,
    showWorkspaceNameOnToolbar:
      typeof mergedConfigs.showWorkspaceNameOnToolbar === "boolean"
        ? mergedConfigs.showWorkspaceNameOnToolbar
        : oldConfigs.showWorkspaceNameOnToolbar,
    closePopupAfterClick:
      typeof mergedConfigs.closePopupAfterClick === "boolean"
        ? mergedConfigs.closePopupAfterClick
        : oldConfigs.closePopupAfterClick,
    exitOnLastTabClose:
      typeof mergedConfigs.exitOnLastTabClose === "boolean"
        ? mergedConfigs.exitOnLastTabClose
        : oldConfigs.exitOnLastTabClose,
  };
}

function createConfig(): Signal<TWorkspacesServicesConfigs> {
  const oldConfigs = JSON.parse(getOldConfigs);
  const sig = signal<TWorkspacesServicesConfigs>(buildInitialConfig());

  effect(() => {
    const v = sig.value;
    Services.prefs.setStringPref(
      WORKSPACED_CONFIG_PREF_NAME,
      JSON.stringify(v),
    );
  });

  const observer = () => {
    try {
      const parsedConfig = JSON.parse(
        Services.prefs.getStringPref(WORKSPACED_CONFIG_PREF_NAME),
      );
      const merged = { ...oldConfigs, ...parsedConfig };
      const result = zWorkspacesServicesConfigs.decode(merged);
      if (isRight(result)) {
        sig.value = result.right;
      } else {
        console.error("Failed to decode workspace configuration:", result.left);
        sig.value = {
          manageOnBms:
            typeof merged.manageOnBms === "boolean"
              ? merged.manageOnBms
              : oldConfigs.manageOnBms,
          showWorkspaceNameOnToolbar:
            typeof merged.showWorkspaceNameOnToolbar === "boolean"
              ? merged.showWorkspaceNameOnToolbar
              : oldConfigs.showWorkspaceNameOnToolbar,
          closePopupAfterClick:
            typeof merged.closePopupAfterClick === "boolean"
              ? merged.closePopupAfterClick
              : oldConfigs.closePopupAfterClick,
          exitOnLastTabClose:
            typeof merged.exitOnLastTabClose === "boolean"
              ? merged.exitOnLastTabClose
              : oldConfigs.exitOnLastTabClose,
        };
      }
    } catch (e) {
      console.error(
        "Failed to parse workspace configuration from preferences:",
        e,
      );
    }
  };
  Services.prefs.addObserver(WORKSPACED_CONFIG_PREF_NAME, observer);
  import.meta.hot?.dispose(() => {
    Services.prefs.removeObserver(WORKSPACED_CONFIG_PREF_NAME, observer);
  });

  return sig;
}

/** Internal signal */
const _configSignal: Signal<TWorkspacesServicesConfigs> = createRootHMR(
  createConfig,
  import.meta.hot,
);

/**
 * Proxy that preserves configStore.manageOnBms etc. access patterns
 * while subscribing components/effects to the underlying signal.
 */
export const configStore: TWorkspacesServicesConfigs = new Proxy(
  {} as TWorkspacesServicesConfigs,
  {
    get(_target, prop: string) {
      return (_configSignal.value as Record<string, unknown>)[prop];
    },
  },
);

export const setConfigStore = (
  v:
    | TWorkspacesServicesConfigs
    | ((prev: TWorkspacesServicesConfigs) => TWorkspacesServicesConfigs),
): void => {
  _configSignal.value =
    typeof v === "function" ? v(_configSignal.value) : v;
};
