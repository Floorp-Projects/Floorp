/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { WorkspacesTabManager } from "./workspacesTabManager.tsx";
import { WorkspaceIcons } from "./utils/workspace-icons.ts";
import { WorkspacesService } from "./workspacesService.ts";
import { WorkspaceManageModal } from "./workspace-modal.tsx";
import { WorkspacesToolbarButton } from "./toolbar/toolbar-element.tsx";
import { WorkspacesPopupContextMenu } from "./contextMenu/popupSet.tsx";
import { WorkspacesDataManager } from "./workspacesDataManagerBase.tsx";
import { enabled } from "./data/config.ts";
import { WorkspacesTabContextMenu } from "./tabContextMenu.tsx";
import { migrateWorkspacesData } from "./data/migrate/migration.ts";
import { createRoot, getOwner, onCleanup, runWithOwner } from "solid-js";
import { WORKSPACES_INIT_OBSERVER_TOPIC } from "./utils/workspaces-static-names.ts";
import { WorkspacesLinkContextMenu } from "./link-context-menu.tsx";
import { WorkspacesSyncController } from "./workspacesSync.ts";
import {
  WorkspacesBackupController,
  type WorkspacesBackupFuncs,
} from "./utils/workspaces-backup-controller.ts";
import {
  canInitializeWorkspace,
  waitForWorkspaceWindow,
} from "./window-ready.ts";

@noraComponent(import.meta.hot)
export default class Workspaces extends NoraComponentBase {
  static windowWorkspacesMap: WeakMap<Window, WorkspacesService> =
    new WeakMap();

  static getCtx(targetWindow?: Window): WorkspacesService | null {
    const requestedWindow = targetWindow ??
      (typeof window !== "undefined" ? window : null);
    if (!requestedWindow || !enabled()) {
      return null;
    }
    return this.windowWorkspacesMap.get(requestedWindow) || null;
  }

  init(): void {
    const owner = getOwner();
    // Backup must keep working while workspaces are disabled, so the backup
    // controller is initialized before the feature's enabled gate. It
    // delegates to WorkspacesService once that exists and otherwise captures/
    // restores tabs through a detached backup host.
    const backupController = new WorkspacesBackupController(() =>
      Workspaces.windowWorkspacesMap.get(window) ?? null
    );
    const backupFuncsHost = globalThis as typeof globalThis & {
      workspacesBackupFuncs?: WorkspacesBackupFuncs;
    };
    backupFuncsHost.workspacesBackupFuncs = {
      backupNow: () => backupController.backupNow(),
      listBackups: () => backupController.listBackups(),
      restoreBackup: (fileName: string) =>
        backupController.restoreBackup(fileName),
      deleteBackup: (fileName: string) =>
        backupController.deleteBackup(fileName),
      deleteAllBackups: () => backupController.deleteAllBackups(),
      isRestoreInFlight: () => backupController.isRestoreInFlight(),
    };
    onCleanup(() => backupController.dispose());

    if (!enabled()) {
      // Workspaces are disabled, but backup must still run. Start the detached
      // service now so the scheduler does not depend on the Hub page being
      // opened first.
      backupController.ensureStarted();
      return;
    }

    const startup = new AbortController();
    onCleanup(() => startup.abort());
    migrateWorkspacesData().then(async () => {
      const ready = await waitForWorkspaceWindow(
        window,
        Services.obs,
        startup.signal,
      );
      if (!canInitializeWorkspace(ready, window, startup.signal)) {
        if (!ready && !startup.signal.aborted && !window.closed) {
          console.error(
            "[Workspaces] Window startup finished without required browser chrome",
          );
        }
        return;
      }
      if (!enabled()) return;
      const exec = () => {
        const iconCtx = new WorkspaceIcons();
        const dataManagerCtx = new WorkspacesDataManager();
        const tabCtx = new WorkspacesTabManager(iconCtx, dataManagerCtx);
        const ctx = new WorkspacesService(tabCtx, iconCtx, dataManagerCtx);
        Workspaces.windowWorkspacesMap.set(window, ctx);
        // A Hub request that arrived before migration finished may have
        // started a detached backup service; hand over to the real one now
        // so two schedulers and observers do not run in this window.
        backupController.ensureStarted();
        ctx.syncController = new WorkspacesSyncController(ctx);

        const observer = {
          observe: (_subject: unknown, topic: string) => {
            if (topic !== WORKSPACES_INIT_OBSERVER_TOPIC) {
              return;
            }
            try {
              ctx.resetWorkspaces();
            } catch (error) {
              console.error("Workspaces: failed to reset workspaces", error);
            }
          },
        };

        Services.obs.addObserver(observer, WORKSPACES_INIT_OBSERVER_TOPIC);

        onCleanup(() => {
          try {
            Services.obs.removeObserver(
              observer,
              WORKSPACES_INIT_OBSERVER_TOPIC,
            );
          } catch (error) {
            console.debug(
              "Workspaces: failed to remove initialization observer",
              error,
            );
          }
          Workspaces.windowWorkspacesMap.delete(window);
        });

        new WorkspaceManageModal(ctx, iconCtx);
        new WorkspacesToolbarButton(ctx);
        new WorkspacesPopupContextMenu(ctx);
        new WorkspacesTabContextMenu(ctx);
        new WorkspacesLinkContextMenu(ctx);
      };
      if (owner) runWithOwner(owner, exec);
      else createRoot(exec);
    }).catch((error: unknown) => {
      if (!startup.signal.aborted && !window.closed) {
        console.error("[Workspaces] Failed to initialize:", error);
      }
    });
  }
}
