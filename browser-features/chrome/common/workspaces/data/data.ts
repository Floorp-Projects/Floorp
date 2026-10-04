/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal } from "@preact/signals";
import { rootEffect } from "@nora/preact-xul/lifetime";
import type { Signal } from "@preact/signals";
import {
  type TWorkspaceID,
  type TWorkspacesStoreData,
  zWorkspaceID,
  zWorkspacesServicesStoreData,
} from "../utils/type.ts";
import { WORKSPACE_DATA_PREF_NAME } from "../utils/workspaces-static-names.ts";
import { addDisposer, createRootHMR } from "#features-chrome/utils/base";
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
      data: new Map() as unknown as TWorkspacesStoreData["data"],
      order: [],
    } satisfies TWorkspacesStoreData;
  }
}

function createWorkspacesData(): Signal<TWorkspacesStoreData> {
  const store = signal<TWorkspacesStoreData>(getDefaultStore());

  rootEffect(() => {
    const data = store.value;
    Services.prefs.setStringPref(
      WORKSPACE_DATA_PREF_NAME,
      JSON.stringify(data, (k, v) => (k == "data" ? [...v] : v)),
    );
  });

  const observer = () => {
    const result = zWorkspacesServicesStoreData.decode(
      JSON.parse(
        Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME, "{}"),
        (k, v) => (k == "data" ? new Map(v) : v),
      ),
    );
    if (
      isRight(result) &&
      JSON.stringify(
          store.peek(),
          (key, value) => key === "data" ? [...value] : value,
        ) !==
        JSON.stringify(
          result.right,
          (key, value) => key === "data" ? [...value] : value,
        )
    ) {
      store.value = result.right;
    }
  };
  Services.prefs.addObserver(WORKSPACE_DATA_PREF_NAME, observer);
  addDisposer(() => {
    Services.prefs.removeObserver(WORKSPACE_DATA_PREF_NAME, observer);
  });

  return store;
}

/** Internal signal — access via workspacesDataStore proxy */
const _workspacesDataSignal: Signal<TWorkspacesStoreData> = createRootHMR(
  createWorkspacesData,
  import.meta.hot,
);

/**
 * WorkspacesServices data.
 * Proxy object that reads from the underlying signal so that preact
 * components and effects automatically subscribe when they access
 * .data / .order / .defaultID.
 */
export const workspacesDataStore: {
  readonly data: TWorkspacesStoreData["data"];
  readonly order: TWorkspacesStoreData["order"];
  readonly defaultID: TWorkspacesStoreData["defaultID"];
} = {
  get data() {
    return _workspacesDataSignal.value.data;
  },
  get order() {
    return _workspacesDataSignal.value.order;
  },
  get defaultID() {
    return _workspacesDataSignal.value.defaultID;
  },
};

/**
 * Compatibility setter that mirrors Solid's SetStoreFunction path API.
 * setWorkspacesDataStore("order", prev => [...prev, id]) still works.
 */
type WorkspaceUpdate =
  | Partial<TWorkspacesStoreData>
  | ((previous: TWorkspacesStoreData) => Partial<TWorkspacesStoreData>);
export function setWorkspacesDataStore(update: WorkspaceUpdate): void;
export function setWorkspacesDataStore<K extends keyof TWorkspacesStoreData>(
  key: K,
  updater:
    | TWorkspacesStoreData[K]
    | ((previous: TWorkspacesStoreData[K]) => TWorkspacesStoreData[K]),
): void;
export function setWorkspacesDataStore<K extends keyof TWorkspacesStoreData>(
  key: K | WorkspaceUpdate,
  updater?:
    | TWorkspacesStoreData[K]
    | ((previous: TWorkspacesStoreData[K]) => TWorkspacesStoreData[K]),
): void {
  const current = _workspacesDataSignal.peek();
  if (typeof key !== "string") {
    _workspacesDataSignal.value = {
      ...current,
      ...(typeof key === "function" ? key(current) : key),
    };
    return;
  }
  if (updater === undefined) return;
  const value = typeof updater === "function" ? updater(current[key]) : updater;
  _workspacesDataSignal.value = { ...current, [key]: value };
}

/**
 * A Signal that holds the selected workspace ID.
 * Selected Workspace ID should not be stored in the store;
 * a Window should have only one selected workspace ID.
 * Not tracked in Preferences.
 */
export const selectedWorkspaceID: Signal<TWorkspaceID | null> = createRootHMR(
  () => signal<TWorkspaceID | null>(null),
  import.meta.hot,
);

/** Setter for selectedWorkspaceID */
export const setSelectedWorkspaceID = (v: TWorkspaceID | null): void => {
  selectedWorkspaceID.value = v;
};
