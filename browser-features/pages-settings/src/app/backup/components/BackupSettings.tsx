import { useCallback, useEffect, useMemo, useState } from "react";
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
  backupNow,
  deleteAllBackups,
  deleteBackup,
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

const formatFileSize = (bytes: number): string => {
  if (bytes >= 1024 * 1024) {
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
  const [actionError, setActionError] = useState(false);
  const [busy, setBusy] = useState<string | null>(null);
  const [restoreTarget, setRestoreTarget] = useState<
    WorkspaceBackupSummary | null
  >(null);
  const [showDeleteAll, setShowDeleteAll] = useState(false);

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
    setLoading(false);
  }, []);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const lastBackup = backups[0] ?? null;

  const runAction = async (
    key: string,
    action: () => Promise<{ success: boolean; error?: string }>,
  ) => {
    setBusy(key);
    setActionError(false);
    try {
      const result = await action();
      if (!result.success) {
        console.error(
          "[Settings:backup] Backup action failed",
          result.error,
        );
        setActionError(true);
        return false;
      }
      await refresh();
      return true;
    } finally {
      setBusy(null);
    }
  };

  const handleBackupNow = () => runAction("backup-now", () => backupNow());

  const handleRestore = async () => {
    if (!restoreTarget) return;
    const target = restoreTarget;
    setRestoreTarget(null);
    await runAction("restore", () => restoreBackup(target.fileName));
  };

  const handleDelete = (backup: WorkspaceBackupSummary) =>
    runAction(`delete:${backup.fileName}`, () => deleteBackup(backup.fileName));

  const handleDeleteAll = async () => {
    setShowDeleteAll(false);
    await runAction("delete-all", () => deleteAllBackups());
  };

  const intervalLabel = (value: number, unit: "minutes" | "hours") =>
    unit === "minutes"
      ? t("workspaces.backupIntervalMinutes", { count: value })
      : t("workspaces.backupIntervalHours", { count: value / 60 });

  return (
    <>
      {restoreTarget
        ? (
          <ConfirmModal
            isOpen
            onClose={() => setRestoreTarget(null)}
            onConfirm={handleRestore}
            title={t("workspaces.backupRestoreConfirmTitle")}
            confirmText={t("workspaces.backupRestoreConfirmAction")}
            cancelText={t("workspaces.backupCancel")}
            confirmVariant="danger"
          >
            <p>{t("workspaces.backupRestoreConfirmDescription")}</p>
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
            {t("workspaces.backup")}
          </CardTitle>
        </CardHeader>
        <CardContent className="space-y-4">
          <p className="text-sm text-base-content/70">
            {t("workspaces.backupDescription")}
          </p>

          <div className={styles.row}>
            <label htmlFor="backup-enabled" className="flex flex-col gap-1.5">
              <span>{t("workspaces.backupAutoEnable")}</span>
              <span className="font-normal text-sm text-base-content/70">
                {t("workspaces.backupAutoEnableDescription")}
              </span>
            </label>
            <Switch
              id="backup-enabled"
              checked={!!enabled}
              onChange={(e) => setValue("enabled", e.target.checked)}
            />
          </div>

          <div className={styles.row}>
            <label htmlFor="backup-interval" className="flex flex-col gap-1.5">
              <span>{t("workspaces.backupInterval")}</span>
              <span className="font-normal text-sm text-base-content/70">
                {t("workspaces.backupIntervalDescription")}
              </span>
            </label>
            <Select
              id="backup-interval"
              value={String(intervalMinutes ?? 15)}
              onChange={(e) =>
                setValue("intervalMinutes", Number(e.target.value))}
            >
              {INTERVAL_OPTIONS.map((option) => (
                <option key={option.value} value={option.value}>
                  {intervalLabel(option.value, option.unit)}
                </option>
              ))}
            </Select>
          </div>

          <div className={styles.row}>
            <label htmlFor="backup-retention" className="flex flex-col gap-1.5">
              <span>{t("workspaces.backupRetention")}</span>
              <span className="font-normal text-sm text-base-content/70">
                {t("workspaces.backupRetentionDescription")}
              </span>
            </label>
            <div className="flex items-center gap-2">
              <Input
                id="backup-retention"
                type="number"
                min={1}
                max={365}
                className="w-24"
                value={retentionDays ?? 7}
                onChange={(e) => {
                  const parsed = Number.parseInt(e.target.value, 10);
                  setValue(
                    "retentionDays",
                    Number.isFinite(parsed) && parsed > 0 ? parsed : 7,
                  );
                }}
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
                {t("workspaces.backupActionError")}
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
                            {t(
                              backup.kind === "auto"
                                ? "workspaces.backupKindAuto"
                                : backup.kind === "manual"
                                ? "workspaces.backupKindManual"
                                : "workspaces.backupKindPreRestore",
                            )}
                          </span>
                        </div>
                        <p className="text-xs text-base-content/70">
                          {t("workspaces.backupWorkspaceCount", {
                            count: backup.workspaceCount,
                          })}
                          {" · "}
                          {t("workspaces.backupTabCount", {
                            count: backup.tabCount,
                          })}
                          {" · "}
                          {formatFileSize(backup.fileSize)}
                        </p>
                      </div>
                      <div className="flex items-center gap-2">
                        <Button
                          variant="secondary"
                          size="sm"
                          disabled={busy !== null}
                          onClick={() => setRestoreTarget(backup)}
                        >
                          {t("workspaces.backupRestore")}
                        </Button>
                        <Button
                          variant="ghost"
                          size="sm"
                          disabled={busy !== null}
                          onClick={() => void handleDelete(backup)}
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
