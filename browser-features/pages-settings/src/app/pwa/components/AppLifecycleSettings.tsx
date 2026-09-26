// SPDX-License-Identifier: MPL-2.0

import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Switch } from "@/components/common/switch.tsx";
import styles from "@/components/common/settings-sections.module.css";
import { rpc } from "@/lib/rpc/rpc.ts";
import { KEEP_WEB_APPS_RUNNING_PREF } from "#libs/pwa/appLifecycle.ts";
import type { AppLifecycleSettings as LifecycleSettings } from "#libs/pwa/appLifecycleTypes.ts";

/** Hidden until the parent confirms that native lifecycle control is available. */
export function AppLifecycleSettings() {
  const { t } = useTranslation();
  const [settings, setSettings] = useState<LifecycleSettings | null>(null);
  const [saving, setSaving] = useState(false);
  const [saveError, setSaveError] = useState(false);

  useEffect(() => {
    let active = true;
    let requestId = 0;
    const load = async () => {
      const currentRequest = ++requestId;
      try {
        const value = await rpc.getWebAppLifecycleSettings();
        if (active && currentRequest === requestId) setSettings(value);
      } catch (error) {
        console.error(
          "[Settings:pwa] Lifecycle capability query failed",
          error,
        );
        if (active && currentRequest === requestId) setSettings(null);
      }
    };
    void load();
    globalThis.addEventListener("focus", load);
    return () => {
      active = false;
      globalThis.removeEventListener("focus", load);
    };
  }, []);

  async function changeKeepRunning(value: boolean) {
    setSaving(true);
    setSaveError(false);
    try {
      await rpc.setBoolPref(KEEP_WEB_APPS_RUNNING_PREF, value);
      setSettings(await rpc.getWebAppLifecycleSettings());
    } catch (error) {
      console.error("[Settings:pwa] Lifecycle setting save failed", error);
      setSaveError(true);
    } finally {
      setSaving(false);
    }
  }

  if (!settings?.supported) return null;

  return (
    <div>
      <div className={styles.row}>
        <div className="space-y-1">
          <label htmlFor="keep-web-apps-running" className="font-medium">
            {t("progressiveWebApp.keepRunningAfterBrowserQuit")}
          </label>
          <p
            id="keep-web-apps-running-description"
            className="text-sm text-base-content/70"
          >
            {t("progressiveWebApp.keepRunningAfterBrowserQuitDescription")}
          </p>
        </div>
        <Switch
          id="keep-web-apps-running"
          aria-describedby="keep-web-apps-running-description"
          checked={settings.keepRunningAfterBrowserQuit}
          disabled={saving}
          onChange={(event) => void changeKeepRunning(event.target.checked)}
        />
      </div>
      {saveError ? <p role="alert">{t("ui.saveError")}</p> : null}
    </div>
  );
}
