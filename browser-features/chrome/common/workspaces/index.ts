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

@noraComponent(import.meta.hot)
export default class Workspaces extends NoraComponentBase {
  static windowWorkspacesMap: WeakMap<Window, WorkspacesService> =
    new WeakMap();

  static getCtx(): WorkspacesService | null {
    if (!window || !enabled()) {
      return null;
    }
    return this.windowWorkspacesMap.get(window) || null;
  }

  init(): void {
    if (!enabled()) {
      return;
    }

    const owner = getOwner();
    migrateWorkspacesData().then(() => {
      const exec = () => {
        const iconCtx = new WorkspaceIcons();
        const dataManagerCtx = new WorkspacesDataManager();
        const tabCtx = new WorkspacesTabManager(iconCtx, dataManagerCtx);
        const ctx = new WorkspacesService(tabCtx, iconCtx, dataManagerCtx);
        Workspaces.windowWorkspacesMap.set(window, ctx);

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
      };
      if (owner) runWithOwner(owner, exec);
      else createRoot(exec);
    });
  }
}
