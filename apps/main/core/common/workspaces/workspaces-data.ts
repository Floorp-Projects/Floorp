/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import { zWorkspacesStoreData } from "./utils/type";
import { WorkspacesStaticNames } from "./utils/workspaces-static-names";

/** Workspaces data */
export const [workspaces, setWorkspaces] = createSignal(
  zWorkspacesStoreData.parse(
    getWorkspacesArrayData(
      Services.prefs.getStringPref(
        WorkspacesStaticNames.workspaceDataPrefName,
        "{}",
      ),
    ),
  ),
);

createEffect(() => {
  Services.prefs.setStringPref(
    WorkspacesStaticNames.workspaceDataPrefName,
    JSON.stringify({ workspaces: workspaces() }),
  );
});

Services.prefs.addObserver("floorp.workspaces.v3.data", () =>
  setWorkspaces(
    zWorkspacesStoreData.parse(
      getWorkspacesArrayData(
        Services.prefs.getStringPref(
          WorkspacesStaticNames.workspaceDataPrefName,
          "{}",
        ),
      ),
    ),
  ),
);

function getWorkspacesArrayData(stringData: string) {
  return JSON.parse(stringData).workspaces || [];
}
