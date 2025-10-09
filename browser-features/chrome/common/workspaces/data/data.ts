/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal, onCleanup } from "solid-js";
import type { Accessor, Setter } from "solid-js";
import {
  type TWorkspaceID,
  type TWorkspacesStoreData,
  zWorkspaceID,
  zWorkspacesServicesStoreData,
} from "../utils/type.ts";
import { WORKSPACE_DATA_PREF_NAME } from "../utils/workspaces-static-names.ts";
import { createRootHMR } from "@nora/solid-xul";
import {
  createStore,
  type SetStoreFunction,
  type Store,
  unwrap,
} from "solid-js/store";
import { trackStore } from "@solid-primitives/deep";
import { isRight } from "fp-ts/Either";

function getDefaultStore() {
  const result = zWorkspacesServicesStoreData.decode(
    JSON.parse(
      Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME, "{}"),
      (k, v) => (k == "data" ? new Map(v) : v),
    ),
  );
  if (isRight(result)) {
    return result.right;
  } else {
    const stubIDResult = zWorkspaceID.decode(
      "00000000-0000-0000-0000-000000000000",
    );
    const stubID = isRight(stubIDResult)
      ? stubIDResult.right
      : ("00000000-0000-0000-0000-000000000000" as TWorkspaceID);
    return {
      defaultID: stubID,
      data: new Map(),
      order: [],
    } satisfies TWorkspacesStoreData;
  }
}

function createWorkspacesData(): [
  Store<TWorkspacesStoreData>,
  SetStoreFunction<TWorkspacesStoreData>,
] {
  const [workspacesDataStore, setWorkspacesDataStore] =
    createStore(getDefaultStore());

  createEffect(() => {
    trackStore(workspacesDataStore);
    Services.prefs.setStringPref(
      WORKSPACE_DATA_PREF_NAME,
      JSON.stringify(unwrap(workspacesDataStore), (k, v) =>
        k == "data" ? [...v] : v,
      ),
    );
  });

  const observer = () => {
    const result = zWorkspacesServicesStoreData.decode(
      JSON.parse(
        Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME, "{}"),
        (k, v) => (k == "data" ? new Map(v) : v),
      ),
    );
    if (isRight(result)) {
      const _storedData = result.right;
      setWorkspacesDataStore("data", _storedData.data);
      setWorkspacesDataStore("defaultID", _storedData.defaultID);
      setWorkspacesDataStore("order", _storedData.order);
    }
  };
  Services.prefs.addObserver(WORKSPACE_DATA_PREF_NAME, observer);
  onCleanup(() => {
    Services.prefs.removeObserver(WORKSPACE_DATA_PREF_NAME, observer);
  });
  return [workspacesDataStore, setWorkspacesDataStore];
}

/** WorkspacesServices data */
export const [workspacesDataStore, setWorkspacesDataStore] = createRootHMR(
  createWorkspacesData,
  import.meta.hot,
);

/**
 * A Signal that holds the selected workspace ID.
 * Selected Workspace ID should not be stored in the store,
 * a Window should have only one selected workspace ID. Not need to track this value in Preferences.
 */
function createSelectedWorkspaceID(): [
  Accessor<TWorkspaceID | null>,
  Setter<TWorkspaceID | null>,
] {
  const [selectedWorkspaceID, setSelectedWorkspaceID] =
    createSignal<TWorkspaceID | null>(null);
  return [selectedWorkspaceID, setSelectedWorkspaceID];
}

/** Selected workspace ID */
export const [selectedWorkspaceID, setSelectedWorkspaceID] = createRootHMR(
  createSelectedWorkspaceID,
  import.meta.hot,
);
