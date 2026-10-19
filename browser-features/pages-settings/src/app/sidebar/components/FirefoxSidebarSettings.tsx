import { useEffect, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import styles from "@/components/common/settings-sections.module.css";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { getChromeExtras, saveChromeExtras } from "../../design/dataManager.ts";
import type { FirefoxSidebarSettings as Settings } from "../types.ts";

export function FirefoxSidebarSettings() {
  const { t } = useTranslation();
  const [settings, setSettings] = useState<Settings>({
    sidebarOverlap: false,
    autohideSidebar: false,
  });
  const [ready, setReady] = useState(false);
  const [loadError, setLoadError] = useState(false);
  const [saveError, setSaveError] = useState(false);
  const mounted = useRef(false);
  const version = useRef(0);

  useEffect(() => {
    mounted.current = true;
    const load = async () => {
      const requestVersion = ++version.current;
      try {
        const value = await getChromeExtras();
        if (!mounted.current || requestVersion !== version.current) return;
        setSettings({
          sidebarOverlap: value.sidebarOverlap,
          autohideSidebar: value.autohideSidebar,
        });
        setReady(true);
        setLoadError(false);
      } catch (error) {
        if (!mounted.current || requestVersion !== version.current) return;
        console.error("[Settings:sidebar] Firefox settings load failed", error);
        setLoadError(true);
      }
    };
    void load();
    globalThis.addEventListener("focus", load);
    return () => {
      mounted.current = false;
      ++version.current;
      globalThis.removeEventListener("focus", load);
    };
  }, []);

  const update = (patch: Partial<Settings>) => {
    const requestVersion = ++version.current;
    setSettings((previous) => ({ ...previous, ...patch }));
    setSaveError(false);
    void saveChromeExtras(patch).catch((error) => {
      console.error("[Settings:sidebar] Firefox settings save failed", error);
      if (mounted.current && requestVersion === version.current) {
        setSaveError(true);
      }
    });
  };

  return (
    <Card className={styles.section}>
      <CardHeader>
        <CardTitle>{t("panelSidebar.firefoxTitle")}</CardTitle>
      </CardHeader>
      <CardContent>
        <p className="mb-4 text-sm text-base-content/70">
          {t("panelSidebar.firefoxDescription")}
        </p>
        {loadError && <p role="alert">{t("ui.loadError")}</p>}
        {saveError && <p role="alert">{t("ui.saveError")}</p>}
        <fieldset disabled={!ready} aria-busy={!ready} className="space-y-4">
          <div className={styles.row}>
            <div className="space-y-1">
              <label
                htmlFor="firefox-sidebar-overlay"
                className="text-sm font-medium"
              >
                {t("panelSidebar.overlay")}
              </label>
              <p className="text-sm text-base-content/70">
                {t("panelSidebar.overlayDescription")}
              </p>
            </div>
            <Switch
              id="firefox-sidebar-overlay"
              checked={settings.sidebarOverlap}
              onChange={(event) =>
                update(
                  event.target.checked
                    ? { sidebarOverlap: true }
                    : { sidebarOverlap: false, autohideSidebar: false },
                )}
            />
          </div>
          <div className={styles.row}>
            <div className="space-y-1">
              <label
                htmlFor="firefox-sidebar-hover"
                className="text-sm font-medium"
              >
                {t("panelSidebar.openOnHover")}
              </label>
              <p className="text-sm text-base-content/70">
                {t("panelSidebar.firefoxHoverDescription")}
              </p>
            </div>
            <Switch
              id="firefox-sidebar-hover"
              checked={settings.autohideSidebar}
              onChange={(event) =>
                update(
                  event.target.checked
                    ? { sidebarOverlap: true, autohideSidebar: true }
                    : { autohideSidebar: false },
                )}
            />
          </div>
        </fieldset>
      </CardContent>
    </Card>
  );
}
