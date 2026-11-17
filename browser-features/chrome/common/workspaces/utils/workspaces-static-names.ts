/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * the name of the preference that stores the workspaces data.
 */
export const WORKSPACE_DATA_PREF_NAME = "floorp.workspaces.v4.store";
/**
 * the attribute name of tabs that stores the workspace id.
 */
export const WORKSPACE_TAB_ATTRIBUTION_ID = "floorpWorkspaceId";
/** Emitted only when the user explicitly moves a tab between workspaces. */
export const WORKSPACE_TAB_MOVED_EVENT = "floorp-workspace-tab-moved";
/**
 * the attribute name of the workspace that stores the last shown workspace id.
 */
export const WORKSPACE_LAST_SHOW_ID = "floorpWorkspaceLastShowId";

export const WORKSPACE_ENABLED_PREF_NAME = "floorp.workspaces.enabled";

/**
 * Sentinel ID for tabs that have no workspace attribution.
 *
 * This ID is only valid inside backup snapshots: it must never be written to
 * the workspace store or to a real tab attribute. Consumers must check it
 * with `isUnassignedWorkspaceID()` before treating a snapshot ID as real.
 */
export const UNASSIGNED_WORKSPACE_ID = "__floorp_unassigned__";
export type TUnassignedWorkspaceID = typeof UNASSIGNED_WORKSPACE_ID;

export const isUnassignedWorkspaceID = (
  id: string | null | undefined,
): id is TUnassignedWorkspaceID => id === UNASSIGNED_WORKSPACE_ID;

/** Name stored in the virtual unassigned-workspace backup snapshot. */
export const UNASSIGNED_WORKSPACE_NAME = "Unassigned";

export const WORKSPACED_CONFIG_PREF_NAME = "floorp.workspaces.v4.config";

/** JSON backup configuration, kept separate from the strict v4 config codec. */
export const WORKSPACE_BACKUP_CONFIG_PREF_NAME =
  "floorp.workspaces.backup.config";

/** Epoch milliseconds of the last automatic/manual backup run. */
export const WORKSPACE_BACKUP_LAST_RUN_PREF_NAME =
  "floorp.workspaces.backup.lastRun";

/**
 * Observer topic used to request workspace initialization.
 */
export const WORKSPACES_INIT_OBSERVER_TOPIC = "floorp.workspaces.initialize";

/**
 * Observer topic notified when the current workspace changes.
 */
export const WORKSPACES_CHANGED_OBSERVER_TOPIC = "floorp.workspaces.changed";

/**
 * One-shot pref to mark that the browser exited due to an empty workspace,
 * used to collapse duplicated startup "new" tabs on next launch.
 */
export const WORKSPACE_PENDING_EXIT_PREF_NAME =
  "floorp.workspaces.pending-exit-from-workspace-empty";
