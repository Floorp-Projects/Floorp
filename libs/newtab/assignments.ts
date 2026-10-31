// SPDX-License-Identifier: MPL-2.0
import { isRecord } from "./origin.ts";
/** Drop unchanged obsolete assignments, but reject a newly selected stale target. */
export function validWorkspaceAssignments(
  proposed: Record<string, string>,
  current: Record<string, string>,
  workspaceIds: string[],
  groupGuids: string[],
): Record<string, string> {
  if (!isRecord(proposed)) {
    throw Object.assign(new Error("Invalid workspace assignments."), {
      code: "VALIDATION",
    });
  }
  const workspaces = new Set(workspaceIds), groups = new Set(groupGuids);
  const result: Record<string, string> = {};
  for (const [workspace, group] of Object.entries(proposed)) {
    if (workspaces.has(workspace) && groups.has(group)) {
      result[workspace] = group;
    } else if (current[workspace] !== group) {
      throw Object.assign(
        new Error(
          "The workspace or group is no longer available. Reload the settings.",
        ),
        { code: "CONFLICT" },
      );
    }
  }
  return result;
}
