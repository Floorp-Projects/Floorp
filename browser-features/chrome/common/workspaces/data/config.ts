/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  type Accessor,
  createEffect,
  createSignal,
  onCleanup,
  type Setter,
} from "solid-js";
import {
  type TWorkspacesServicesConfigs,
  zWorkspacesServicesConfigs,
} from "../utils/type.js";
import { getOldConfigs } from "./old-config";
import { createRootHMR } from "@nora/solid-xul";
import {
  createStore,
  type SetStoreFunction,
  type Store,
  unwrap,
} from "solid-js/store";
import {
  WORKSPACE_ENABLED_PREF_NAME,
  WORKSPACED_CONFIG_PREF_NAME,
} from "../utils/workspaces-static-names.js";
import { isRight } from "fp-ts/Either";

function createEnabled(): [Accessor<boolean>, Setter<boolean>] {
  const [enabled, setEnabled] = createSignal(
    Services.prefs.getBoolPref(WORKSPACE_ENABLED_PREF_NAME, true),
  );
  createEffect(() => {
    Services.prefs.setBoolPref(WORKSPACE_ENABLED_PREF_NAME, enabled());
  });

  const observer = () =>
    setEnabled(Services.prefs.getBoolPref(WORKSPACE_ENABLED_PREF_NAME));
  Services.prefs.addObserver(WORKSPACE_ENABLED_PREF_NAME, observer);
  onCleanup(() => {
    Services.prefs.removeObserver(WORKSPACE_ENABLED_PREF_NAME, observer);
  });
  return [enabled, setEnabled];
}

/** enable/disable workspaces */
export const [enabled, setEnabled] = createRootHMR(
  createEnabled,
  import.meta.hot,
);

function createConfig(): [
  Store<TWorkspacesServicesConfigs>,
  SetStoreFunction<TWorkspacesServicesConfigs>,
] {
  const oldConfigs = JSON.parse(getOldConfigs);
  const loadedConfigs = JSON.parse(
    Services.prefs.getStringPref(WORKSPACED_CONFIG_PREF_NAME, getOldConfigs),
  );

  const mergedConfigs = { ...oldConfigs, ...loadedConfigs };

  const configResult = zWorkspacesServicesConfigs.decode(mergedConfigs);
  let finalConfig: TWorkspacesServicesConfigs;
  if (isRight(configResult)) {
    finalConfig = configResult.right;
  } else {
    // Preserve valid fields from mergedConfigs, fallback to oldConfigs for invalid ones
    console.error(
      "Failed to decode workspace configuration, attempting partial recovery:",
      configResult.left,
    );
    finalConfig = {
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

  const [configStore, setConfigStore] = createStore(finalConfig);

  createEffect(() => {
    Services.prefs.setStringPref(
      WORKSPACED_CONFIG_PREF_NAME,
      JSON.stringify(unwrap(configStore)),
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
        setConfigStore(result.right);
      } else {
        console.error("Failed to decode workspace configuration:", result.left);
        // Preserve valid fields from merged config
        setConfigStore({
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
        });
      }
    } catch (e) {
      console.error(
        "Failed to parse workspace configuration from preferences:",
        e,
      );
    }
  };
  Services.prefs.addObserver(WORKSPACED_CONFIG_PREF_NAME, observer);
  onCleanup(() => {
    Services.prefs.removeObserver(WORKSPACED_CONFIG_PREF_NAME, observer);
  });
  return [configStore, setConfigStore];
}

/** Configs */
export const [configStore, setConfigStore] = createRootHMR(
  createConfig,
  import.meta.hot,
);
