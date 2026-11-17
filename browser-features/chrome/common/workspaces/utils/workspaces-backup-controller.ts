/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createRoot } from "solid-js";
import type { WorkspacesService } from "../workspacesService.ts";
import { WorkspacesDataManager } from "../workspacesDataManagerBase.tsx";
import { WorkspacesBackupService } from "./workspaces-backup-service.ts";
import type {
  RestoreBackupResult,
  WorkspacesBackupHost,
} from "./workspaces-backup-service.ts";
import type { WorkspaceBackupFile } from "./backup-types.ts";
import type { WorkspaceBackupSummary } from "./backup-types.ts";
import type { TWorkspaceID } from "./type.ts";

/** Backup operations exposed to settings UI through the window actor. */
export interface WorkspacesBackupFuncs {
  backupNow(): Promise<WorkspaceBackupSummary>;
  listBackups(): Promise<WorkspaceBackupSummary[]>;
  restoreBackup(fileName: string): Promise<RestoreBackupResult>;
  deleteBackup(fileName: string): Promise<boolean>;
  deleteAllBackups(): Promise<boolean>;
  /**
   * True while a restore runs in this window. Other windows' schedulers read
   * it so they do not capture a half-restored store.
   */
  isRestoreInFlight(): boolean;
}

/**
 * Capture/restore host for the workspaces-disabled case. It reads the
 * persistent workspace store directly for metadata lookups, but never writes
 * to it: tabs-mode restore is handled by the service itself.
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

  public restoreWorkspaceState(_file: WorkspaceBackupFile): Promise<void> {
    // Workspaces-mode restores only run while the feature is enabled, so this
    // host must never touch the store.
    return Promise.reject(new Error("workspaces-disabled"));
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

  /**
   * Starts the backup service immediately.
   *
   * Used when workspaces are disabled: without this the detached service would
   * only exist after the Hub backup page was opened, so scheduled backups
   * would never run.
   */
  public ensureStarted(): void {
    this.resolveService();
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

  public restoreBackup(fileName: string): Promise<RestoreBackupResult> {
    return this.resolveService().restoreBackup(fileName);
  }

  public deleteBackup(fileName: string): Promise<boolean> {
    return this.resolveService().deleteBackup(fileName);
  }

  public deleteAllBackups(): Promise<boolean> {
    return this.resolveService().deleteAllBackups();
  }

  public isRestoreInFlight(): boolean {
    const service = this.getService();
    if (service) {
      return service.backupService.isRestoreInFlight();
    }
    return this.detachedService?.isRestoreInFlight() ?? false;
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
