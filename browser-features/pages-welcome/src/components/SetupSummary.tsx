import { DataList } from "@chakra-ui/react";
import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Link } from "react-router-dom";
import { getDefaultEngine } from "../app/customize/dataManager.ts";
import {
  getLocaleData,
  getNativeNames,
} from "../app/localization/dataManager.ts";
import { useTheme } from "./theme-provider.tsx";
import { useSetupReleaseNotesChoice } from "./ReleaseNotesChoice.tsx";
import { Button } from "../../../../libs/ui/button.tsx";
import styles from "../setup.module.css";
import type { SetupSummaryData } from "./types.ts";
import { Languages, Monitor, Newspaper, Pencil, Search } from "lucide-react";

export function SetupSummary() {
  const { t } = useTranslation();
  const { theme } = useTheme();
  const { choice } = useSetupReleaseNotesChoice();
  const [data, setData] = useState<SetupSummaryData | null>(null);
  const [error, setError] = useState(false);
  const [attempt, setAttempt] = useState(0);
  useEffect(() => {
    let active = true;
    setError(false);
    Promise.all([getLocaleData(), getDefaultEngine()]).then(
      async ([locale, engine]) => {
        const code = locale.localeInfo.appLocaleRaw;
        const names = await getNativeNames([code]).catch(() => [code]);
        if (active) {
          setData({ language: names[0] || code, engine: engine.name });
        }
      },
    ).catch((error) => {
      console.error("[Welcome] Could not load setup summary", error);
      if (active) setError(true);
    });
    return () => {
      active = false;
    };
  }, [attempt]);
  if (error) {
    return (
      <div>
        <p role="alert" className={styles.error}>{t("ui.loadError")}</p>
        <Button
          variant="secondary"
          onClick={() => setAttempt((value) => value + 1)}
        >
          {t("ui.retry", { defaultValue: "Try again" })}
        </Button>
      </div>
    );
  }
  if (!data) return <p role="status">{t("ui.loading")}</p>;
  const rows = [
    {
      label: t("setupV5.languageTitle"),
      value: data.language,
      path: "/localization",
      icon: Languages,
    },
    {
      label: t("customize.appearance.title"),
      value: t(`customize.themes.${theme}`),
      path: "/customize",
      icon: Monitor,
    },
    {
      label: t("customize.searchEngine.title"),
      value: data.engine,
      path: "/customize",
      icon: Search,
    },
    {
      label: t("setupV5.supportTitle"),
      value: t(`setupV5.supportLabels.${choice.mode}`),
      path: "/support",
      icon: Newspaper,
    },
  ];
  return (
    <DataList.Root unstyled className={styles.list}>
      {rows.map((row) => (
        <DataList.Item key={row.label} className={styles.listItem}>
          <DataList.ItemLabel className={styles.listLabel}>
            <row.icon size={20} aria-hidden="true" />
            {row.label}
          </DataList.ItemLabel>
          <DataList.ItemValue className={styles.listValue}>
            <span>{row.value}</span>
            <Link
              to={row.path}
              aria-label={row.label + ": " + t("setupV5.change")}
            >
              {t("setupV5.change")}
              <Pencil size={14} aria-hidden="true" />
            </Link>
          </DataList.ItemValue>
        </DataList.Item>
      ))}
    </DataList.Root>
  );
}
