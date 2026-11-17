import { createRoot } from "solid-js";
import type { WorkspacesService } from "../workspacesService.ts";
import { WorkspacesDataManager } from "../workspacesDataManagerBase.tsx";
import { WorkspacesBackupService } from "./workspaces-backup-service.ts";
import type { WorkspacesBackupHost } from "./workspaces-backup-service.ts";
import { collectTabsForWorkspacelessRestore } from "./workspaces-backup-helpers.ts";
import { restoreSnapshotTabs } from "./workspace-snapshot.ts";
import type { WorkspaceBackupFile } from "./backup-types.ts";
import type { WorkspaceBackupSummary } from "./backup-types.ts";
import type { TWorkspaceID } from "./type.ts";

/** Backup operations exposed to settings UI through the window actor. */
export interface WorkspacesBackupFuncs {
  backupNow(): Promise<WorkspaceBackupSummary>;
  listBackups(): Promise<WorkspaceBackupSummary[]>;
  restoreBackup(fileName: string): Promise<{
    success: boolean;
    error?: string;
  }>;
  deleteBackup(fileName: string): Promise<boolean>;
  deleteAllBackups(): Promise<boolean>;
}

/**
 * Capture/restore host for the workspaces-disabled case. It reads the
 * persistent workspace store directly and restores tabs without assigning a
 * workspace or touching the store.
 */
class DetachedWorkspacesBackupHost implements WorkspacesBackupHost {
  private readonly dataManager = new WorkspacesDataManager();

  public isWorkspaceID(id: string): id is TWorkspaceID {
    return this.dataManager.isWorkspaceID(id);
  }

  public getRawWorkspace(id: TWorkspaceID): ReturnType<
    WorkspacesDataManager["getRawWorkspace"]
  > {
    return this.dataManager.getRawWorkspace(id);
  }

  public async restoreFromBackupFile(file: WorkspaceBackupFile): Promise<void> {
    const tabs = collectTabsForWorkspacelessRestore(file);
    if (tabs.length === 0) {
      throw new Error("empty-backup");
    }
    const existingTabs = [...globalThis.gBrowser.tabs];
    await restoreSnapshotTabs(tabs, { assignWorkspaceId: null });
    // Restore is a full replacement. Add the restored tabs first so the
    // window never has to close its last tab, then remove the old tabs.
    for (const tab of existingTabs) {
      if (!tab.isConnected) {
        continue;
      }
      try {
        globalThis.gBrowser.removeTab(tab);
      } catch (error) {
        console.error(
          "WorkspacesBackupController: failed to close replaced tab",
          error,
        );
      }
    }
  }
}

/**
 * Keeps backup operations available independently of `Workspaces.init()`'s
 * enabled gate. While workspaces are enabled it delegates to the window's
 * WorkspacesService; otherwise it lazily creates one detached backup service
 * per browser window.
 */
export class WorkspacesBackupController implements WorkspacesBackupFuncs {
  private readonly getService: () => WorkspacesService | null;
  private detachedService: WorkspacesBackupService | null = null;
  private detachedDisposer: (() => void) | null = null;

  constructor(getService: () => WorkspacesService | null) {
    this.getService = getService;
  }

  private resolveService(): WorkspacesBackupService {
    const service = this.getService();
    if (service) {
      this.disposeDetached();
      return service.backupService;
    }
    if (!this.detachedService) {
      this.detachedDisposer = createRoot((dispose) => {
        this.detachedService = new WorkspacesBackupService(
          new DetachedWorkspacesBackupHost(),
        );
        return dispose;
      });
    }
    return this.detachedService as WorkspacesBackupService;
  }

  public backupNow(): Promise<WorkspaceBackupSummary> {
    return this.resolveService().backupNow("manual");
  }

  public listBackups(): Promise<WorkspaceBackupSummary[]> {
    return this.resolveService().listBackups();
  }

  public restoreBackup(fileName: string): Promise<{
    success: boolean;
    error?: string;
  }> {
    return this.resolveService().restoreBackup(fileName);
  }

  public deleteBackup(fileName: string): Promise<boolean> {
    return this.resolveService().deleteBackup(fileName);
  }

  public deleteAllBackups(): Promise<boolean> {
    return this.resolveService().deleteAllBackups();
  }

  public dispose(): void {
    this.disposeDetached();
  }

  private disposeDetached(): void {
    this.detachedDisposer?.();
    this.detachedDisposer = null;
    this.detachedService = null;
  }
}
