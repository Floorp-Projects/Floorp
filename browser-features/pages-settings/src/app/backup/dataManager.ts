import { rpc } from "../../lib/rpc/rpc.ts";
import type { BackupSettingsFormData } from "../../types/pref.ts";

const WORKSPACES_BACKUP_CONFIG_PREF = "floorp.workspaces.backup.config";

const DEFAULT_BACKUP_CONFIG = {
  enabled: true,
  intervalMinutes: 15,
  retentionDays: 7,
  maxPreRestoreBackups: 10,
};

interface WorkspaceBackupConfig {
  enabled: boolean;
  intervalMinutes: number;
  retentionDays: number;
  maxPreRestoreBackups: number;
}

export type WorkspaceBackupKind = "auto" | "manual" | "pre-restore";
export type WorkspaceBackupMode = "workspaces" | "tabs";

export interface WorkspaceBackupSummary {
  id: string;
  kind: WorkspaceBackupKind;
  mode: WorkspaceBackupMode;
  capturedAt: number;
  windowCount: number;
  workspaceCount: number;
  tabCount: number;
  fileName: string;
  fileSize: number;
}

export type BackupOperationResult<T> =
  | { success: true; data: T }
  | { success: false; error: string };

interface BackupBridgeResponse {
  requestId: string;
  success: boolean;
  error?: string;
  data?: unknown;
}

type BackupBridgeFunction = (
  ...args: unknown[]
) => Promise<BackupBridgeResponse>;

interface WorkspacesBridgeWindow extends Window {
  NRWorkspacesBackupNow?: BackupBridgeFunction;
  NRWorkspacesListBackups?: BackupBridgeFunction;
  NRWorkspacesRestoreBackup?: BackupBridgeFunction;
  NRWorkspacesDeleteBackup?: BackupBridgeFunction;
  NRWorkspacesDeleteAllBackups?: BackupBridgeFunction;
}

async function getBackupConfig(): Promise<WorkspaceBackupConfig> {
  const raw = await rpc.getStringPref(WORKSPACES_BACKUP_CONFIG_PREF);
  if (!raw) {
    return { ...DEFAULT_BACKUP_CONFIG };
  }
  try {
    const parsed = JSON.parse(raw) as Partial<WorkspaceBackupConfig>;
    return {
      enabled: typeof parsed.enabled === "boolean"
        ? parsed.enabled
        : DEFAULT_BACKUP_CONFIG.enabled,
      intervalMinutes: typeof parsed.intervalMinutes === "number" &&
          Number.isFinite(parsed.intervalMinutes) &&
          parsed.intervalMinutes > 0
        ? parsed.intervalMinutes
        : DEFAULT_BACKUP_CONFIG.intervalMinutes,
      retentionDays: typeof parsed.retentionDays === "number" &&
          Number.isFinite(parsed.retentionDays) &&
          parsed.retentionDays >= 0
        ? parsed.retentionDays
        : DEFAULT_BACKUP_CONFIG.retentionDays,
      maxPreRestoreBackups: typeof parsed.maxPreRestoreBackups === "number" &&
          Number.isFinite(parsed.maxPreRestoreBackups) &&
          parsed.maxPreRestoreBackups >= 0
        ? parsed.maxPreRestoreBackups
        : DEFAULT_BACKUP_CONFIG.maxPreRestoreBackups,
    };
  } catch (error) {
    console.error(
      "Failed to parse workspace backup configuration, falling back to defaults:",
      error,
    );
    return { ...DEFAULT_BACKUP_CONFIG };
  }
}

/**
 * Whether the workspaces feature is enabled. Restore semantics follow this
 * flag, not the mode of the backup file, so the UI needs it to describe what
 * the restore button will actually do.
 */
export async function getWorkspacesEnabled(): Promise<boolean> {
  try {
    const value = await rpc.getBoolPref("floorp.workspaces.enabled");
    return value ?? true;
  } catch (error) {
    console.error(
      "[Settings:backup] Failed to read workspaces enabled state",
      error,
    );
    return true;
  }
}

export async function getBackupSettings(): Promise<BackupSettingsFormData> {
  const config = await getBackupConfig();
  return {
    enabled: config.enabled,
    intervalMinutes: config.intervalMinutes,
    retentionDays: config.retentionDays,
  };
}

export async function saveBackupSettings(
  settings: BackupSettingsFormData,
): Promise<void> {
  if (Object.keys(settings).length === 0) {
    return;
  }

  const oldConfig = await getBackupConfig();

  // Only the backup config pref is written here; workspace v4 settings are
  // owned by the workspaces page.
  await rpc.setStringPref(
    WORKSPACES_BACKUP_CONFIG_PREF,
    JSON.stringify({
      ...oldConfig,
      enabled: settings.enabled,
      intervalMinutes: settings.intervalMinutes,
      retentionDays: settings.retentionDays,
    }),
  );
}

const getBackupBridge = (
  name: keyof WorkspacesBridgeWindow,
): BackupBridgeFunction | null => {
  const bridge = window as WorkspacesBridgeWindow;
  const fn = bridge[name];
  return typeof fn === "function" ? fn : null;
};

async function invokeBackupBridge<T>(
  name: keyof WorkspacesBridgeWindow,
  ...args: unknown[]
): Promise<BackupOperationResult<T>> {
  const fn = getBackupBridge(name);
  if (!fn) {
    return { success: false, error: "bridge-unavailable" };
  }
  try {
    const result = await fn(...args);
    if (!result?.success) {
      return { success: false, error: result?.error ?? "unknown" };
    }
    return { success: true, data: result.data as T };
  } catch (error) {
    console.error(`Workspace backup bridge call failed: ${name}`, error);
    return {
      success: false,
      error: error instanceof Error ? error.message : String(error),
    };
  }
}

export function backupNow(): Promise<
  BackupOperationResult<WorkspaceBackupSummary>
> {
  return invokeBackupBridge<WorkspaceBackupSummary>("NRWorkspacesBackupNow");
}

export function listBackups(): Promise<
  BackupOperationResult<WorkspaceBackupSummary[]>
> {
  return invokeBackupBridge<WorkspaceBackupSummary[]>(
    "NRWorkspacesListBackups",
  );
}

export async function restoreBackup(
  fileName: string,
): Promise<BackupOperationResult<null>> {
  const result = await invokeBackupBridge<{ success: boolean; error?: string }>(
    "NRWorkspacesRestoreBackup",
    fileName,
  );
  if (!result.success) {
    return result;
  }
  if (!result.data?.success) {
    return { success: false, error: result.data?.error ?? "restore-failed" };
  }
  return { success: true, data: null };
}

export async function deleteBackup(
  fileName: string,
): Promise<BackupOperationResult<null>> {
  const result = await invokeBackupBridge<boolean>(
    "NRWorkspacesDeleteBackup",
    fileName,
  );
  if (!result.success) {
    return result;
  }
  if (!result.data) {
    return { success: false, error: "delete-failed" };
  }
  return { success: true, data: null };
}

export async function deleteAllBackups(): Promise<BackupOperationResult<null>> {
  const result = await invokeBackupBridge<boolean>(
    "NRWorkspacesDeleteAllBackups",
  );
  if (!result.success) {
    return result;
  }
  if (!result.data) {
    return { success: false, error: "delete-failed" };
  }
  return { success: true, data: null };
}
