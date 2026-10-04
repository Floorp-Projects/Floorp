// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { effect } from "@preact/signals";
import { setWorkspacesDataStore, workspacesDataStore } from "../data/data.ts";
import type {
  TWorkspace,
  TWorkspaceID,
  TWorkspacesStoreData,
} from "../utils/type.ts";
import { WORKSPACE_DATA_PREF_NAME } from "../utils/workspaces-static-names.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

async function testPersistedShapeAndMapUpdater(): Promise<void> {
  const original = { ...workspacesDataStore };
  const originalPref = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
  const id = "99999999-0000-4000-8000-000000000001" as TWorkspaceID;
  const workspace: TWorkspace = {
    name: "Persistence fixture",
    userContextId: 0,
    isSelected: null,
    isDefault: null,
    icon: undefined,
  };
  const getMap = () =>
    workspacesDataStore.data as unknown as Map<TWorkspaceID, TWorkspace>;
  let observations = 0;
  const dispose = effect(() => {
    void workspacesDataStore.data;
    observations++;
  });
  try {
    const data = new Map(getMap());
    data.set(id, workspace);
    setWorkspacesDataStore({
      data: data as unknown as TWorkspacesStoreData["data"],
    });
    assert(
      !Object.hasOwn(getMap().get(id)!, "icon"),
      "an own undefined icon is omitted after persistence",
    );
    const serialized = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);

    // A semantically identical write does not trigger a Gecko pref observer.
    setWorkspacesDataStore("data", (previous) => {
      const next = new Map(
        previous as unknown as Map<TWorkspaceID, TWorkspace>,
      );
      next.set(id, { ...next.get(id)!, icon: undefined });
      return next as unknown as TWorkspacesStoreData["data"];
    });
    assertEquals(
      Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME),
      serialized,
      "repeated undefined writes leave persisted JSON unchanged",
    );
    assert(
      !Object.hasOwn(getMap().get(id)!, "icon"),
      "normalization cannot depend on a changed preference",
    );

    const before = observations;
    setWorkspacesDataStore("data", (previous) => {
      const map = previous as unknown as Map<TWorkspaceID, TWorkspace>;
      map.set(id, { ...map.get(id)!, name: "Updated", icon: null });
      return previous;
    });
    assertEquals(
      observations,
      before + 1,
      "an in-place map updater notifies once without a pref echo",
    );
    assertEquals(
      getMap().get(id)?.name,
      "Updated",
      "map updater changes reach consumers",
    );
    assertEquals(
      getMap().get(id)?.icon,
      null,
      "explicit null stays distinct from undefined",
    );
    assertEquals(
      workspacesDataStore.defaultID,
      original.defaultID,
      "partial updates retain other fields",
    );
    assertEquals(
      JSON.stringify(workspacesDataStore.order),
      JSON.stringify(original.order),
      "partial updates retain workspace order",
    );
    await Promise.resolve();
  } finally {
    dispose();
    setWorkspacesDataStore(original);
    Services.prefs.setStringPref(WORKSPACE_DATA_PREF_NAME, originalPref);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("dataPersistence.test.ts", [{
    name:
      "workspace setters preserve persisted shape and map updater notifications",
    fn: testPersistedShapeAndMapUpdater,
  }]);
}
