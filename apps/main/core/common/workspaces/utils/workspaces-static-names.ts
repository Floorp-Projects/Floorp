/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *
 * Workspaces static names. Pref names, attribute names, etc.
 * @workspaceDataPrefName is the name of the preference that stores the workspaces data.
 * @workspaceLastShowId is the attribute name of tabs that stores the workspace id.
 * @workspaceLastShowId is the attribute name of the workspace that stores the last shown workspace id.
 */
export namespace WorkspacesStaticNames {
  /*
   * workspaceDataPrefName is the name of the preference that stores the workspaces data.
   * workspaceLastShowId is the attribute name of tabs that stores the workspace id.
   * workspaceLastShowId is the attribute name of the workspace that stores the last shown workspace id.
   */
  export const workspaceDataPrefName = "floorp.workspaces.v3.data";
  export const workspacesTabAttributionId = "floorpWorkspaceId";
  export const workspaceLastShowId = "floorpWorkspaceLastShowId";
}
