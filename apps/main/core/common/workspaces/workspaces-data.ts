/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import { zWorkspacesStoreData } from "./utils/type";
import { WorkspacesService as Ws } from "./utils/workspaces-services";

/** Workspaces data */
export const [workspaces, setWorkspaces] = createSignal(
  zWorkspacesStoreData.parse(
    getWorkspacesArrayData(
      Services.prefs.getStringPref(Ws.workspaceDataPrefName, "{}"),
    ),
  ),
);

createEffect(() => {
  Services.prefs.setStringPref(
    Ws.workspaceDataPrefName,
    JSON.stringify({ workspaces: workspaces() }),
  );
});

Services.prefs.addObserver("floorp.workspaces.v3.data", () =>
  setWorkspaces(
    zWorkspacesStoreData.parse(
      getWorkspacesArrayData(
        Services.prefs.getStringPref(Ws.workspaceDataPrefName, "{}"),
      ),
    ),
  ),
);

function getWorkspacesArrayData(stringData: string) {
  return JSON.parse(stringData).workspaces || [];
}
