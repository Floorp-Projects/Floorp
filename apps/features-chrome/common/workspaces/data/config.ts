/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Accessor, createEffect, createSignal, onCleanup, Setter } from "solid-js";
import { TWorkspacesServicesConfigs, zWorkspacesServicesConfigs } from "../utils/type.js";
import { getOldConfigs } from "./old-config";
import { createRootHMR } from "@nora/solid-xul";
import { createStore, SetStoreFunction, Store, unwrap } from "solid-js/store";
import { WORKSPACE_ENABLED_PREF_NAME, WORKSPACED_CONFIG_PREF_NAME } from "../utils/workspaces-static-names.js";

function createEnabled(): [Accessor<boolean>,Setter<boolean>] {
  const [enabled, setEnabled] = createSignal(
    Services.prefs.getBoolPref(WORKSPACE_ENABLED_PREF_NAME, true),
  );
  createEffect(() => {
    Services.prefs.setBoolPref(WORKSPACE_ENABLED_PREF_NAME, enabled());
  });

  const observer = () =>
    setEnabled(Services.prefs.getBoolPref(WORKSPACE_ENABLED_PREF_NAME));
  Services.prefs.addObserver(WORKSPACE_ENABLED_PREF_NAME, observer);
  onCleanup(()=>{
    Services.prefs.removeObserver(WORKSPACE_ENABLED_PREF_NAME, observer);
  })
  return [enabled,setEnabled]
}

 /** enable/disable workspaces */
export const [enabled,setEnabled] = createRootHMR(createEnabled,import.meta.hot);

function createConfig(): [Store<TWorkspacesServicesConfigs>,SetStoreFunction<TWorkspacesServicesConfigs>] {
  const [configStore, setConfigStore] = createStore(zWorkspacesServicesConfigs.parse(
    JSON.parse(
      Services.prefs.getStringPref(WORKSPACED_CONFIG_PREF_NAME, getOldConfigs),
    )
  ))

  createEffect(() => {
    Services.prefs.setStringPref(
      WORKSPACED_CONFIG_PREF_NAME,
      JSON.stringify(unwrap(configStore)),
    );
  });

  const observer = () => setConfigStore(
    zWorkspacesServicesConfigs.parse(
      JSON.parse(Services.prefs.getStringPref(WORKSPACED_CONFIG_PREF_NAME)),
    ),
  );
  Services.prefs.addObserver(WORKSPACED_CONFIG_PREF_NAME, observer);
  onCleanup(()=>{
    Services.prefs.removeObserver(WORKSPACED_CONFIG_PREF_NAME, observer);
  });
  return [configStore,setConfigStore]
}

/** Configs */
export const [configStore,setConfigStore] = createRootHMR(createConfig,import.meta.hot);
