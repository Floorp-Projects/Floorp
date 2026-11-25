import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { useFormContext, useWatch } from "react-hook-form";
import { SaveAll } from "lucide-react";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { Button } from "@/components/common/button.tsx";
import { Input } from "@/components/common/input.tsx";
import { Select } from "@/components/common/dropdown.tsx";
import { ConfirmModal } from "@/components/common/ConfirmModal.tsx";
import type { BackupSettingsFormData } from "@/types/pref.ts";
import {
  BACKUP_LIMITS,
  backupNow,
  clampBackupInt,
  deleteAllBackups,
  deleteBackup,
  getWorkspacesEnabled,
  listBackups,
  restoreBackup,
  type WorkspaceBackupSummary,
} from "../dataManager.ts";

import styles from "../backup.module.css";

const INTERVAL_OPTIONS = [
  { value: 5, unit: "minutes" as const },
  { value: 15, unit: "minutes" as const },
  { value: 30, unit: "minutes" as const },
  { value: 60, unit: "hours" as const },
  { value: 360, unit: "hours" as const },
  { value: 720, unit: "hours" as const },
  { value: 1440, unit: "hours" as const },
];

// The restore actor call can time out while the restore keeps running in the
// parent process, so the list is polled for a while before the UI is released.
const STILL_RUNNING_POLL_MS = 10_000;
const STILL_RUNNING_POLL_COUNT = 6;

const isTimeoutError = (code: string | undefined): boolean =>
  typeof code === "string" && /time-?d?\s?out/i.test(code);

const intervalUnit = (value: number): "minutes" | "hours" =>
  value >= 60 && value % 60 === 0 ? "hours" : "minutes";

const formatFileSize = (bytes: number): string => {
  // Compare the rounded KB value so 1048575 B shows as "1.0 MB", not "1024.0 KB".
  if (Number((bytes / 1024).toFixed(1)) >= 1024) {
    return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  }
  if (bytes >= 1024) {
    return `${(bytes / 1024).toFixed(1)} KB`;
  }
  return `${bytes} B`;
};

export function BackupSettings() {
  const { t, i18n } = useTranslation();
  const { control, setValue } = useFormContext<BackupSettingsFormData>();
  const enabled = useWatch({ control, name: "enabled" });
  const intervalMinutes = useWatch({ control, name: "intervalMinutes" });
  const retentionDays = useWatch({ control, name: "retentionDays" });

  const [backups, setBackups] = useState<WorkspaceBackupSummary[]>([]);
  const [loading, setLoading] = useState(true);
  const [loadError, setLoadError] = useState(false);
  // Holds the failure code so the UI can explain why the action was rejected.
  const [actionError, setActionError] = useState<string | null>(null);
  const [busy, setBusy] = useState<string | null>(null);
  const [workspacesEnabled, setWorkspacesEnabled] = useState(true);
  const [restoreTarget, setRestoreTarget] = useState<
    WorkspaceBackupSummary | null
  >(null);
  // Workspaces state captured when the confirm dialog opened. The service reads
  // the pref again at restore time, so the dialog must not rely on a value that
  // may be stale by the time the user confirms.
  const [restorePlanEnabled, setRestorePlanEnabled] = useState<
    boolean | null
  >(null);
  const [showDeleteAll, setShowDeleteAll] = useState(false);
  const [deleteTarget, setDeleteTarget] = useState<
    WorkspaceBackupSummary | null
  >(null);
  const [skippedCount, setSkippedCount] = useState(0);
  const [stillRunning, setStillRunning] = useState(false);
  const [retentionText, setRetentionText] = useState<string | null>(null);
  const pollTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const mountedRef = useRef(true);

  const dateFormatter = useMemo(
    () =>
      new Intl.DateTimeFormat(i18n.language, {
        dateStyle: "medium",
        timeStyle: "short",
      }),
    [i18n.language],
  );

  const refresh = useCallback(async () => {
    setLoading(true);
    try {
      const result = await listBackups();
      if (result.success) {
        setBackups(result.data);
        setLoadError(false);
      } else {
        console.error(
          "[Settings:backup] Failed to list backups",
          result.error,
        );
        setLoadError(true);
      }
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
      if (pollTimerRef.current !== null) {
        clearTimeout(pollTimerRef.current);
      }
    };
  }, []);

  // A restore that outlived the actor timeout is still running in the parent
  // process. Keep the UI busy and reload the list a few times before releasing.
  const pollWhileStillRunning = useCallback((remaining: number) => {
    pollTimerRef.current = setTimeout(async () => {
      if (!mountedRef.current) return;
      await refresh();
      if (!mountedRef.current) return;
      if (remaining > 1) {
        pollWhileStillRunning(remaining - 1);
      } else {
        pollTimerRef.current = null;
        setStillRunning(false);
        setBusy(null);
      }
    }, STILL_RUNNING_POLL_MS);
  }, [refresh]);

  useEffect(() => {
    let active = true;
    const refresh = () => {
      void getWorkspacesEnabled().then((value) => {
        if (active) {
          setWorkspacesEnabled(value);
        }
      });
    };
    refresh();
    // Another window can toggle the feature while this page stays open.
    globalThis.addEventListener("focus", refresh);
    return () => {
      active = false;
      globalThis.removeEventListener("focus", refresh);
    };
  }, []);

  const lastBackup = backups[0] ?? null;

  const runAction = async <T,>(
    key: string,
    action: () => Promise<
      { success: true; data: T } | { success: false; error?: string }
    >,
    onSuccess?: (data: T) => void,
  ) => {
    setBusy(key);
    setActionError(null);
    setSkippedCount(0);
    setStillRunning(false);
    let keepBusy = false;
    try {
      const result = await action();
      if (!result.success) {
        // Only a restore keeps running after the actor times out (its
        // timeout is long); other operations simply failed.
        if (key === "restore" && isTimeoutError(result.error)) {
          // The operation may still be running; do not release the UI yet.
          keepBusy = true;
          setStillRunning(true);
          pollWhileStillRunning(STILL_RUNNING_POLL_COUNT);
          return false;
        }
        console.error(
          "[Settings:backup] Backup action failed",
          result.error,
        );
        // The service reports an oversized file the same way for save and
        // restore; the message has to say which one was refused.
        setActionError(
          key === "restore" && result.error === "backup-too-large"
            ? "restore-too-large"
            : result.error ?? "unknown",
        );
        if (isTimeoutError(result.error)) {
          // The operation may still finish in the background; show its
          // result once it does instead of a stale list.
          void refresh();
        }
        return false;
      }
      onSuccess?.(result.data);
      await refresh();
      return true;
    } finally {
      if (!keepBusy) {
        setBusy(null);
      }
    }
  };

  const handleBackupNow = () => runAction("backup-now", () => backupNow());

  const openRestoreDialog = async (backup: WorkspaceBackupSummary) => {
    const current = await getWorkspacesEnabled();
    setWorkspacesEnabled(current);
    setRestorePlanEnabled(current);
    setRestoreTarget(backup);
  };

  const closeRestoreDialog = () => {
    setRestoreTarget(null);
    setRestorePlanEnabled(null);
  };

  const handleRestore = async () => {
    if (!restoreTarget || restorePlanEnabled === null) return;
    const target = restoreTarget;
    const planned = restorePlanEnabled;
    closeRestoreDialog();
    // runAction marks the page busy before the first await, so the pre-flight
    // re-check below cannot overlap another action.
    await runAction("restore", async () => {
      // Re-check immediately before the destructive call: if the feature was
      // toggled in another window the dialog wording no longer matches what
      // the service would do, so refuse instead of guessing.
      const current = await getWorkspacesEnabled();
      if (current !== planned) {
        setWorkspacesEnabled(current);
        return { success: false as const, error: "restore-state-changed" };
      }
      return restoreBackup(target.fileName);
    }, (data) => setSkippedCount(data.skippedCount));
  };

  const openDeleteDialog = (backup: WorkspaceBackupSummary) =>
    setDeleteTarget(backup);

  const handleDelete = async () => {
    if (!deleteTarget) return;
    const target = deleteTarget;
    setDeleteTarget(null);
    await runAction(
      `delete:${target.fileName}`,
      () => deleteBackup(target.fileName),
    );
  };

  const handleDeleteAll = async () => {
    setShowDeleteAll(false);
    await runAction("delete-all", () => deleteAllBackups());
  };

  /**
   * Maps a service failure code to a message. The service returns codes rather
   * than prose so the UI can explain what happened and what to do next.
   */
  const describeActionError = (code: string): string => {
    switch (code) {
      case "pre-restore-failed":
        return t("backup.errorPreRestoreFailed");
      case "restore-in-progress":
        return t("backup.errorRestoreInProgress");
      case "workspaces-disabled":
        // Workspaces were toggled without restarting Floorp.
        return t("backup.errorRestartRequired");
      case "empty-backup":
        return t("backup.errorEmptyBackup");
      case "nothing-to-back-up":
        return t("backup.errorNothingToBackUp");
      case "private-window-unsupported":
        return t("backup.errorPrivateWindowUnsupported");
      case "restore-state-changed":
        return t("backup.errorStateChanged");
      case "restore-partial-failure":
        return t("backup.errorPartialFailure");
      case "private-detection-failed":
        return t("backup.errorPrivateDetection");
      case "backup-not-found":
        return t("backup.errorNotFound");
      case "invalid-file-name":
        return t("backup.errorInvalidName");
      case "backup-capture-invalid":
        return t("backup.errorCaptureInvalid");
      case "backup-too-large":
        return t("backup.errorTooLarge");
      case "restore-too-large":
        return t("backup.errorRestoreTooLarge");
      default:
        return t("workspaces.backupActionError");
    }
  };

  // A stored value outside the presets (for example set by hand in about:config)
  // is added as an extra option so the select shows what is really configured.
  const intervalOptions = useMemo(() => {
    const current = intervalMinutes ?? 15;
    if (INTERVAL_OPTIONS.some((option) => option.value === current)) {
      return INTERVAL_OPTIONS;
    }
    return [
      ...INTERVAL_OPTIONS,
      { value: current, unit: intervalUnit(current) },
    ].sort((a, b) => a.value - b.value);
  }, [intervalMinutes]);

  const retentionValue = retentionText ?? String(retentionDays ?? 7);

  const commitRetention = () => {
    if (retentionText === null) return;
    const parsed = Number.parseInt(retentionText, 10);
    if (Number.isFinite(parsed)) {
      setValue(
        "retentionDays",
        clampBackupInt(parsed, BACKUP_LIMITS.retentionDays, 7),
      );
    }
    // Empty or invalid input reverts to the stored value instead of resetting.
    setRetentionText(null);
  };

  const intervalLabel = (value: number, unit: "minutes" | "hours") =>
    unit === "minutes"
      ? t("workspaces.backupIntervalMinutes", { count: value })
      : t("workspaces.backupIntervalHours", { count: value / 60 });

  const describeCounts = (backup: WorkspaceBackupSummary) => {
    const parts: string[] = [];
    if (backup.mode === "workspaces") {
      parts.push(
        t("workspaces.backupWorkspaceCount", { count: backup.workspaceCount }),
      );
    } else {
      parts.push(t("backup.windowCount", { count: backup.windowCount }));
    }
    parts.push(t("workspaces.backupTabCount", { count: backup.tabCount }));
    parts.push(formatFileSize(backup.fileSize));
    return parts.join(" · ");
  };

  const kindLabel = (backup: WorkspaceBackupSummary) =>
    t(
      backup.kind === "auto"
        ? "workspaces.backupKindAuto"
        : backup.kind === "manual"
        ? "workspaces.backupKindManual"
        : "workspaces.backupKindPreRestore",
    );

  const modeLabel = (backup: WorkspaceBackupSummary) =>
    backup.mode === "workspaces"
      ? t("backup.modeWorkspaces")
      : t("backup.modeTabs");

  const restoreWillReplace = (restorePlanEnabled ?? workspacesEnabled) &&
    restoreTarget?.mode === "workspaces";

  return (
    <>
      {restoreTarget
        ? (
          <ConfirmModal
            isOpen
            onClose={closeRestoreDialog}
            onConfirm={handleRestore}
            title={restoreWillReplace
              ? t("workspaces.backupRestoreConfirmTitle")
              : t("backup.tabsRestoreConfirmTitle")}
            confirmText={restoreWillReplace
              ? t("workspaces.backupRestoreConfirmAction")
              : t("backup.tabsRestoreConfirmAction")}
            cancelText={t("workspaces.backupCancel")}
            confirmVariant={restoreWillReplace ? "danger" : "primary"}
          >
            {restoreWillReplace
              ? <p>{t("workspaces.backupRestoreConfirmDescription")}</p>
              : (
                <p>
                  {t("backup.tabsRestoreConfirmDescription")}
                  {!workspacesEnabled && restoreTarget.mode === "workspaces"
                    ? ` ${t("backup.workspacesFileWarning")}`
                    : ""}
                </p>
              )}
          </ConfirmModal>
        )
        : null}
      {deleteTarget
        ? (
          <ConfirmModal
            isOpen
            onClose={() => setDeleteTarget(null)}
            onConfirm={handleDelete}
            title={t("backup.deleteConfirmTitle")}
            confirmText={t("backup.deleteConfirmAction")}
            cancelText={t("workspaces.backupCancel")}
            confirmVariant="danger"
          >
            <p>
              {t("backup.deleteConfirmDescription", {
                time: dateFormatter.format(new Date(deleteTarget.capturedAt)),
              })}
            </p>
          </ConfirmModal>
        )
        : null}
      {showDeleteAll
        ? (
          <ConfirmModal
            isOpen
            onClose={() => setShowDeleteAll(false)}
            onConfirm={handleDeleteAll}
            title={t("workspaces.backupDeleteAllConfirmTitle")}
            confirmText={t("workspaces.backupDeleteAllConfirmAction")}
            cancelText={t("workspaces.backupCancel")}
            confirmVariant="danger"
          >
            <p>{t("workspaces.backupDeleteAllConfirmDescription")}</p>
          </ConfirmModal>
        )
        : null}

      <Card className={styles.section}>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <SaveAll className="size-5" />
            {workspacesEnabled ? t("workspaces.backup") : t("backup.tabsTitle")}
          </CardTitle>
        </CardHeader>
        <CardContent className="space-y-4">
          <p className="text-sm text-base-content/70">
            {workspacesEnabled
              ? t("workspaces.backupDescription")
              : t("backup.tabsDescription")}
          </p>

          <div className={styles.row}>
            <label htmlFor="backup-enabled" className="flex flex-col gap-1.5">
              <span>{t("workspaces.backupAutoEnable")}</span>
              <span
                id="backup-enabled-description"
                className="font-normal text-sm text-base-content/70"
              >
                {t("workspaces.backupAutoEnableDescription")}
              </span>
            </label>
            <Switch
              id="backup-enabled"
              aria-describedby="backup-enabled-description"
              checked={!!enabled}
              onChange={(e) => setValue("enabled", e.target.checked)}
            />
          </div>

          <div className={styles.row}>
            <label htmlFor="backup-interval" className="flex flex-col gap-1.5">
              <span>{t("workspaces.backupInterval")}</span>
              <span
                id="backup-interval-description"
                className="font-normal text-sm text-base-content/70"
              >
                {t("workspaces.backupIntervalDescription")}
              </span>
            </label>
            <Select
              id="backup-interval"
              aria-describedby="backup-interval-description"
              value={String(intervalMinutes ?? 15)}
              onChange={(e) =>
                setValue("intervalMinutes", Number(e.target.value))}
            >
              {intervalOptions.map((option) => (
                <option key={option.value} value={option.value}>
                  {intervalLabel(option.value, option.unit)}
                </option>
              ))}
            </Select>
          </div>

          <div className={styles.row}>
            <label htmlFor="backup-retention" className="flex flex-col gap-1.5">
              <span>{t("workspaces.backupRetention")}</span>
              <span
                id="backup-retention-description"
                className="font-normal text-sm text-base-content/70"
              >
                {t("workspaces.backupRetentionDescription")}
              </span>
            </label>
            <div className="flex items-center gap-2">
              <Input
                id="backup-retention"
                aria-describedby="backup-retention-description"
                type="number"
                inputMode="numeric"
                min={BACKUP_LIMITS.retentionDays.min}
                max={BACKUP_LIMITS.retentionDays.max}
                className="w-24"
                value={retentionValue}
                onChange={(e) => setRetentionText(e.target.value)}
                onBlur={commitRetention}
              />
              <span className="text-sm">
                {t("workspaces.backupRetentionUnit")}
              </span>
            </div>
          </div>

          <div className={styles.row}>
            <div className="space-y-1">
              <p className="text-sm">
                {lastBackup
                  ? t("workspaces.backupLastBackup", {
                    time: dateFormatter.format(new Date(lastBackup.capturedAt)),
                  })
                  : t("workspaces.backupNever")}
              </p>
            </div>
            <Button
              variant="secondary"
              disabled={busy !== null}
              onClick={handleBackupNow}
            >
              {t("workspaces.backupNow")}
            </Button>
          </div>

          {actionError
            ? (
              <p
                role="alert"
                className="text-sm text-red-600 dark:text-red-400"
              >
                {describeActionError(actionError)}
              </p>
            )
            : null}

          {stillRunning
            ? (
              <p role="status" className="text-sm">
                {t("backup.restoreStillRunning")}
              </p>
            )
            : null}
          {skippedCount > 0
            ? (
              <p role="status" className="text-sm">
                {t("backup.restoreSkipped", { count: skippedCount })}
              </p>
            )
            : null}

          <div className="space-y-2">
            <div className="flex items-center justify-between">
              <h3 className="text-base font-medium">
                {t("workspaces.backupListTitle")}
              </h3>
              <Button
                variant="danger"
                size="sm"
                disabled={busy !== null || backups.length === 0}
                onClick={() => setShowDeleteAll(true)}
              >
                {t("workspaces.backupDeleteAll")}
              </Button>
            </div>

            {loading
              ? <p role="status">{t("workspaces.backupLoading")}</p>
              : loadError
              ? (
                <div className="space-y-2">
                  <p role="alert">{t("workspaces.backupError")}</p>
                  <Button variant="secondary" onClick={() => void refresh()}>
                    {t("ui.retry")}
                  </Button>
                </div>
              )
              : backups.length === 0
              ? (
                <p className="text-sm text-base-content/70">
                  {t("workspaces.backupEmpty")}
                </p>
              )
              : (
                <ul className="space-y-2">
                  {backups.map((backup) => (
                    <li
                      key={backup.fileName}
                      className="flex flex-wrap items-center justify-between gap-3 p-3 bg-base-200 rounded-lg"
                    >
                      <div className="space-y-1">
                        <div className="flex items-center gap-2">
                          <span className="text-sm font-medium">
                            {dateFormatter.format(
                              new Date(backup.capturedAt),
                            )}
                          </span>
                          <span className="rounded px-2 py-0.5 text-xs bg-base-300">
                            {kindLabel(backup)}
                          </span>
                          <span className="rounded px-2 py-0.5 text-xs bg-base-300">
                            {modeLabel(backup)}
                          </span>
                        </div>
                        <p
                          id={`backup-desc-${backup.id}`}
                          className="text-xs text-base-content/70"
                        >
                          {describeCounts(backup)}
                        </p>
                      </div>
                      <div className="flex items-center gap-2">
                        <Button
                          variant="secondary"
                          size="sm"
                          disabled={busy !== null}
                          aria-label={t("backup.restoreAriaLabel", {
                            kind: kindLabel(backup),
                            time: dateFormatter.format(
                              new Date(backup.capturedAt),
                            ),
                          })}
                          aria-describedby={`backup-desc-${backup.id}`}
                          onClick={() => void openRestoreDialog(backup)}
                        >
                          {workspacesEnabled
                            ? t("workspaces.backupRestore")
                            : t("backup.tabsRestore")}
                        </Button>
                        <Button
                          variant="ghost"
                          size="sm"
                          disabled={busy !== null}
                          aria-label={t("backup.deleteAriaLabel", {
                            kind: kindLabel(backup),
                            time: dateFormatter.format(
                              new Date(backup.capturedAt),
                            ),
                          })}
                          aria-describedby={`backup-desc-${backup.id}`}
                          onClick={() => openDeleteDialog(backup)}
                        >
                          {t("workspaces.backupDelete")}
                        </Button>
                      </div>
                    </li>
                  ))}
                </ul>
              )}
          </div>
        </CardContent>
      </Card>
    </>
  );
}
