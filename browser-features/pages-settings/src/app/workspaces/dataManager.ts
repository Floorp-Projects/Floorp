import { rpc } from "../../lib/rpc/rpc.ts";
import type { WorkspacesFormData } from "../../types/pref.ts";

const WORKSPACES_INIT_TOPIC = "floorp.workspaces.initialize";
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

export interface WorkspaceBackupSummary {
  id: string;
  kind: WorkspaceBackupKind;
  capturedAt: number;
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

type InitializeResult = {
  success: boolean;
  error?: string | null;
};

export async function initializeWorkspaces(reason?: string): Promise<boolean> {
  const globalWindow = window as unknown as {
    NRInitializeWorkspaces?: (
      reason?: string | null,
    ) => Promise<InitializeResult> | InitializeResult;
    Services?: {
      obs?: {
        notifyObservers: (
          subject: unknown,
          topic: string,
          data?: string | null,
        ) => void;
      };
    };
  };

  if (typeof globalWindow.NRInitializeWorkspaces === "function") {
    try {
      const result = await globalWindow.NRInitializeWorkspaces(reason ?? null);
      if (result?.success) {
        return true;
      }

      console.error(
        "initializeWorkspaces: actor reported failure",
        result?.error,
      );
      return false;
    } catch (error) {
      console.error(
        "initializeWorkspaces: actor call threw an exception",
        error,
      );
      return false;
    }
  }

  try {
    const observers = globalWindow.Services?.obs;
    if (typeof observers?.notifyObservers === "function") {
      observers.notifyObservers(null, WORKSPACES_INIT_TOPIC, reason ?? null);
      return true;
    }
  } catch (error) {
    console.error("initializeWorkspaces: fallback notification failed", error);
  }

  console.error("initializeWorkspaces: no available transport to initialize");
  return false;
}

export async function saveWorkspaceSettings(
  settings: WorkspacesFormData,
): Promise<null | void> {
  if (Object.keys(settings).length === 0) {
    return;
  }

  const oldConfig = await getWorkspacesConfigsExcludeEnabled();
  const oldBackupConfig = await getWorkspacesBackupConfig();

  const newConfigs = {
    ...oldConfig,
    manageOnBms: settings.manageOnBms,
    showWorkspaceNameOnToolbar: settings.showWorkspaceNameOnToolbar,
    closePopupAfterClick: settings.closePopupAfterClick,
    exitOnLastTabClose: settings.exitOnLastTabClose,
  };

  // Finish each write before reporting success or failure to the save queue.
  await rpc.setStringPref(
    "floorp.workspaces.v4.config",
    JSON.stringify(newConfigs),
  );
  await rpc.setStringPref(
    WORKSPACES_BACKUP_CONFIG_PREF,
    JSON.stringify({
      ...oldBackupConfig,
      enabled: settings.backupEnabled,
      intervalMinutes: settings.backupIntervalMinutes,
      retentionDays: settings.backupRetentionDays,
    }),
  );
  await rpc.setBoolPref("floorp.workspaces.enabled", Boolean(settings.enabled));
}

export async function getWorkspaceSettings(): Promise<
  WorkspacesFormData | null
> {
  const [enabled, configs] = await Promise.all([
    getWorkspacesEnabled(),
    getWorkspacesConfigsExcludeEnabled(),
  ]);
  const backupConfig = await getWorkspacesBackupConfig();

  if (enabled === null || configs === null) {
    return null;
  }

  return {
    enabled,
    ...configs,
    backupEnabled: backupConfig.enabled,
    backupIntervalMinutes: backupConfig.intervalMinutes,
    backupRetentionDays: backupConfig.retentionDays,
  };
}

async function getWorkspacesBackupConfig(): Promise<WorkspaceBackupConfig> {
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

async function getWorkspacesEnabled(): Promise<boolean | null> {
  return await rpc.getBoolPref("floorp.workspaces.enabled");
}

async function getWorkspacesConfigsExcludeEnabled(): Promise<
  Omit<
    WorkspacesFormData,
    | "enabled"
    | "backupEnabled"
    | "backupIntervalMinutes"
    | "backupRetentionDays"
  > | null
> {
  const defaultConfigs = {
    manageOnBms: false,
    showWorkspaceNameOnToolbar: false,
    closePopupAfterClick: true,
    exitOnLastTabClose: false,
  };

  const result = await rpc.getStringPref("floorp.workspaces.v4.config");
  if (!result) {
    return defaultConfigs;
  }

  try {
    const userConfigs = JSON.parse(result);
    return { ...defaultConfigs, ...userConfigs };
  } catch (e) {
    console.error(
      "Failed to parse workspace configuration, falling back to defaults:",
      e,
    );
    return defaultConfigs;
  }
}
