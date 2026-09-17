import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import Navigation from "../../components/Navigation.tsx";
import {
  getDefaultEngine,
  getSearchEngines,
  setDefaultEngine,
} from "./dataManager.ts";
import type { SearchEngine } from "./types.ts";
import { useTheme } from "../../components/theme-provider.tsx";
import styles from "../../setup.module.css";
import { RadioCard } from "@chakra-ui/react";
import {
  CurrentSetting,
  SelectionBadge,
  SetupSelect,
} from "../../components/SetupControls.tsx";
import { Monitor, Moon, Search, Sun } from "lucide-react";
const themeIcons = { light: Sun, dark: Moon, system: Monitor };
export default function CustomizePage() {
  const { t } = useTranslation();
  const { theme, setTheme } = useTheme();
  const [engines, setEngines] = useState<SearchEngine[]>([]);
  const [engine, setEngine] = useState("");
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(false);
  useEffect(() => {
    let active = true;
    Promise.all([getSearchEngines(), getDefaultEngine()]).then(
      ([list, current]) => {
        if (active) {
          setEngines(list);
          setEngine(current.identifier);
        }
      },
    ).catch(() => {
      if (active) setError(true);
    }).finally(() => {
      if (active) setLoading(false);
    });
    return () => {
      active = false;
    };
  }, []);
  async function save(action: () => Promise<unknown>) {
    if (saving) return;
    setSaving(true);
    setError(false);
    try {
      await action();
    } catch (error) {
      console.error("[Welcome] Could not save customization", error);
      setError(true);
    } finally {
      setSaving(false);
    }
  }
  const ThemeIcon = themeIcons[theme];
  const selectedEngine = engines.find((item) => item.identifier === engine);
  return (
    <div>
      <h2>{t("setupV5.customizeTitle")}</h2>
      <RadioCard.Root
        unstyled
        value={theme}
        disabled={saving}
        onValueChange={({ value }) => {
          if (
            value === "light" || value === "dark" || value === "system"
          ) void save(() => setTheme(value));
        }}
      >
        <RadioCard.Label className={styles.fieldLabel}>
          {t("customize.appearance.title")}
        </RadioCard.Label>
        <div className={styles.choices}>
          {(["light", "dark", "system"] as const).map((value) => (
            <RadioCard.Item
              key={value}
              value={value}
              className={styles.radioItem}
            >
              <RadioCard.ItemHiddenInput checked={theme === value} />
              <span
                className={styles.themePreview}
                data-theme={value}
                aria-hidden="true"
              >
                <span className={styles.sampleType}>Aa</span>
                <span className={styles.sampleAction} />
              </span>
              <RadioCard.ItemControl className={styles.radioControl}>
                <RadioCard.ItemText>
                  {t(`customize.themes.${value}`)}
                </RadioCard.ItemText>
              </RadioCard.ItemControl>
              <span className={styles.choiceCaption}>
                {t("setupRich.palettePreview")}
              </span>
              {theme === value && <SelectionBadge />}
            </RadioCard.Item>
          ))}
        </div>
      </RadioCard.Root>
      <div className={styles.themeStatus} role="status">
        <ThemeIcon size={20} aria-hidden="true" />
        <span>{t(`customize.themeInfo.${theme}`)}</span>
      </div>
      {loading
        ? <p role="status">{t("ui.loading")}</p>
        : engines.length === 0
        ? <p>{t("customize.searchEngine.noEnginesFound")}</p>
        : (
          <SetupSelect
            id="setup-engine"
            icon={<Search size={22} />}
            label={t("customize.searchEngine.title")}
            hint={t("setupV5.searchHint")}
            value={engine}
            disabled={saving}
            options={engines.map((item) => ({
              value: item.identifier,
              label: item.name,
            }))}
            onChange={(value) =>
              void save(async () => {
                const result = await setDefaultEngine(value);
                if (!result.success) {
                  throw new Error("Search engine update rejected");
                }
                setEngine(value);
              })}
          />
        )}
      {!loading && selectedEngine && (
        <CurrentSetting
          icon={<Search size={26} />}
          label={t("setupRich.searchDestination")}
          value={selectedEngine.name}
          detail={t("setupV5.searchHint")}
        />
      )}
      {saving && (
        <p role="status" className={styles.status}>{t("ui.loading")}</p>
      )}
      {error && <p role="alert" className={styles.error}>{t("ui.saveError")}
      </p>}
      <div className={styles.links}>
        <a href="about:hub" target="_blank" rel="noopener noreferrer">
          {t("setupV5.openHub")}
        </a>
        <a href="about:preferences" target="_blank" rel="noopener noreferrer">
          {t("setupV5.openPreferences")}
        </a>
      </div>
      <Navigation />
    </div>
  );
}
