// SPDX-License-Identifier: MPL-2.0

import Workspaces from "../workspaces/index.ts";
import { workspacesDataStore } from "../workspaces/data/data.ts";
import type { TWorkspaceID } from "../workspaces/utils/type.ts";
import { getWorkspaceSessionName } from "../workspaces/utils/workspace-sessions.ts";
import {
  adjacentWorkspace,
  type WorkspaceGestureController,
} from "./controller.ts";
import type { WorkspaceSwitchSnapshot } from "./types.ts";
import i18next from "i18next";

const controllers = new WeakMap<Window, WorkspaceGestureController>();

export function getWorkspaceSwitchSnapshot(
  win: Window,
): WorkspaceSwitchSnapshot | null {
  if (win.closed) return null;
  const context = Workspaces.getCtx(win);
  if (!context) return null;
  const currentId = context.getSelectedWorkspaceID();
  const order = workspacesDataStore.order.flatMap((id) => {
    const workspace = context.getRawWorkspace(id);
    if (!workspace) return [];
    return [{
      id,
      name: workspace.name,
      userContextId: workspace.userContextId,
      sessionName: workspace.userContextId === 0
        ? i18next.t("workspaces.modal.no-container", {
          defaultValue: "Standard (shared with other standard tabs)",
        })
        : getWorkspaceSessionName(workspace.userContextId) ??
          i18next.t("workspaceGestures.missingSession", {
            defaultValue: "Missing login environment",
          }),
    }];
  });
  return { currentId, order };
}

export function registerWorkspaceGestureController(
  win: Window,
  controller: WorkspaceGestureController,
): () => void {
  controllers.set(win, controller);
  return () => {
    if (controllers.get(win) === controller) controllers.delete(win);
    controller.cancel();
  };
}

export function commitWorkspaceSwitch(win: Window, id: string): void {
  const context = Workspaces.getCtx(win);
  if (!context || !context.isWorkspaceID(id)) return;
  context.changeWorkspace(id as TWorkspaceID);
}

/** End-stop actions are also usable by OS gesture-to-key mappings. */
export function switchWorkspaceByOffset(win: Window, offset: -1 | 1): boolean {
  const controller = controllers.get(win);
  if (controller) return controller.switchByOffset(offset);
  const snapshot = getWorkspaceSwitchSnapshot(win);
  if (!snapshot) return false;
  const target = adjacentWorkspace(snapshot, offset);
  if (!target) return false;
  commitWorkspaceSwitch(win, target.id);
  return true;
}
