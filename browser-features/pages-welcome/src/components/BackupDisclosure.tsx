import { type ChangeEvent, useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Switch } from "../../../../libs/ui/switch.tsx";
import { rpc } from "../lib/rpc/rpc.ts";
import styles from "../setup.module.css";

const BACKUP_CONFIG_PREF = "floorp.workspaces.backup.config";

interface BackupConfig {
  enabled: boolean;
  intervalMinutes: number;
  retentionDays: number;
  maxPreRestoreBackups: number;
}

// Keep in sync with BACKUP_LIMITS in pages-settings/src/app/backup/dataManager.ts
// and normalizeBackupConfig in workspaces-backup-helpers.ts. Out-of-range values
// clamp; non-finite values fall back to the default (0 is no longer valid).
const LIMITS = {
  intervalMinutes: { min: 1, max: 1440 },
  retentionDays: { min: 1, max: 365 },
  maxPreRestoreBackups: { min: 1, max: 50 },
} as const;

const DEFAULT_CONFIG: BackupConfig = {
  enabled: true,
  intervalMinutes: 15,
  retentionDays: 7,
  maxPreRestoreBackups: 10,
};

const clampInt = (
  value: unknown,
  limits: { readonly min: number; readonly max: number },
  fallback: number,
): number =>
  typeof value === "number" && Number.isFinite(value)
    ? Math.min(limits.max, Math.max(limits.min, Math.trunc(value)))
    : fallback;

const normalize = (raw: string | null): BackupConfig => {
  if (!raw) {
    return { ...DEFAULT_CONFIG };
  }
  let parsed: Partial<BackupConfig>;
  try {
    parsed = JSON.parse(raw) as Partial<BackupConfig>;
  } catch (error) {
    console.error("[Welcome] Failed to parse backup config", error);
    return { ...DEFAULT_CONFIG };
  }
  return {
    enabled: typeof parsed.enabled === "boolean"
      ? parsed.enabled
      : DEFAULT_CONFIG.enabled,
    intervalMinutes: clampInt(
      parsed.intervalMinutes,
      LIMITS.intervalMinutes,
      DEFAULT_CONFIG.intervalMinutes,
    ),
    retentionDays: clampInt(
      parsed.retentionDays,
      LIMITS.retentionDays,
      DEFAULT_CONFIG.retentionDays,
    ),
    maxPreRestoreBackups: clampInt(
      parsed.maxPreRestoreBackups,
      LIMITS.maxPreRestoreBackups,
      DEFAULT_CONFIG.maxPreRestoreBackups,
    ),
  };
};

/**
 * First-run disclosure for the automatic workspace/tab backup.
 *
 * The backup is enabled by default and stores tab URLs, titles and session
 * state under the profile directory, so the welcome flow explains what is
 * saved and lets the user turn it off before they start browsing.
 */
export function BackupDisclosure() {
  const { t } = useTranslation();
  const [config, setConfig] = useState<BackupConfig | null>(null);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(false);
  const [loadFailed, setLoadFailed] = useState(false);

  useEffect(() => {
    let active = true;
    void rpc.getStringPref(BACKUP_CONFIG_PREF).then((raw) => {
      if (active) {
        setConfig(normalize(raw));
      }
    }).catch((loadError) => {
      console.error("[Welcome] Failed to read backup config", loadError);
      // Do not fall back to defaults: writing them back would overwrite the
      // user's real settings. Leave the switch disabled instead.
      if (active) {
        setLoadFailed(true);
      }
    });
    return () => {
      active = false;
    };
  }, []);

  const toggle = async (next: boolean) => {
    if (!config) {
      return;
    }
    const previous = config;
    const updated = { ...config, enabled: next };
    setConfig(updated);
    setSaving(true);
    setError(false);
    try {
      await rpc.setStringPref(BACKUP_CONFIG_PREF, JSON.stringify(updated));
    } catch (saveError) {
      console.error("[Welcome] Failed to save backup config", saveError);
      setConfig(previous);
      setError(true);
    } finally {
      setSaving(false);
    }
  };

  return (
    <section className={styles.section}>
      <h3>{t("setupV5.backupTitle")}</h3>
      <p className={styles.muted}>{t("setupV5.backupDescription")}</p>
      <div
        style={{
          display: loadFailed ? "none" : "flex",
          alignItems: "center",
          justifyContent: "space-between",
          gap: "1rem",
        }}
      >
        <label htmlFor="welcome-backup-enabled">
          {t("setupV5.backupEnable")}
        </label>
        <Switch
          id="welcome-backup-enabled"
          checked={config?.enabled ?? false}
          disabled={config === null || loadFailed || saving}
          onChange={(event: ChangeEvent<HTMLInputElement>) =>
            void toggle(event.currentTarget.checked)}
        />
      </div>
      <p className={styles.muted}>{t("setupV5.backupDetails")}</p>
      {loadFailed && (
        <p role="alert" className={styles.error}>{t("ui.loadError")}</p>
      )}
      {error && <p role="alert" className={styles.error}>{t("ui.saveError")}
      </p>}
      <div className={styles.links}>
        {/* Backup settings live in the Floorp Hub, not Firefox preferences. */}
        <a
          href="about:hub#/features/backup"
          target="_blank"
          rel="noopener noreferrer"
        >
          {t("setupV5.backupOpenSettings")}
        </a>
      </div>
    </section>
  );
}
