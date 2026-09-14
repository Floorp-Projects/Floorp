import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import {
  getLocaleData,
  getNativeNames,
  installLangPack,
  setAppLocale,
} from "./dataManager.ts";
import type { LocaleData } from "./type.ts";
import { Button } from "../../../../../libs/ui/button.tsx";
import { DataList } from "@chakra-ui/react";
import {
  CurrentSetting,
  SetupInfo,
  SetupSelect,
} from "../../components/SetupControls.tsx";
import { Languages } from "lucide-react";
import styles from "../../setup.module.css";
export function LanguageSettings() {
  const { t, i18n } = useTranslation();
  const [attempt, setAttempt] = useState(0);
  const [data, setData] = useState<LocaleData | null>(null);
  const [names, setNames] = useState<Record<string, string>>({});
  const [selected, setSelected] = useState("");
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState(false);
  useEffect(() => {
    let active = true;
    setLoading(true);
    setError(false);
    async function load() {
      try {
        const result = await getLocaleData();
        if (!active) return;
        setData(result);
        setSelected(result.localeInfo.appLocaleRaw);
        const codes = result.availableLocales.map((pack) => pack.target_locale);
        const nativeNames = await getNativeNames(codes).catch((error) => {
          console.error("[Welcome] Native locale names unavailable", error);
          return codes;
        });
        if (active) {
          setNames(
            Object.fromEntries(
              codes.map((code, index) => [code, nativeNames[index] || code]),
            ),
          );
        }
      } catch (error) {
        console.error("[Welcome] Language loading failed", error);
        if (active) setError(true);
      } finally {
        if (active) setLoading(false);
      }
    }
    void load();
    return () => {
      active = false;
    };
  }, [attempt]);
  async function changeLocale(locale: string) {
    if (!data || saving || locale === selected) return;
    setSaving(true);
    setError(false);
    try {
      if (!data.installedLocales.includes(locale)) {
        const pack = data.availableLocales.find((pack) =>
          pack.target_locale === locale
        );
        if (!pack || !(await installLangPack(pack)).success) {
          throw new Error("Language pack installation failed");
        }
      }
      if (!(await setAppLocale(locale)).success) {
        throw new Error("Locale update rejected");
      }
      await i18n.changeLanguage(
        await globalThis.NRI18n.normalizeLocale(locale),
      );
      setSelected(locale);
      setData({
        ...data,
        installedLocales: [...new Set([...data.installedLocales, locale])],
      });
    } catch (error) {
      console.error("[Welcome] Language update failed", error);
      setError(true);
    } finally {
      setSaving(false);
    }
  }
  return (
    <div>
      {loading ? <p role="status">{t("ui.loading")}</p> : data && (
        <section>
          <CurrentSetting
            icon={<Languages size={32} />}
            label={t("localizationPage.floorpLanguage")}
            value={names[selected] || selected}
            detail={t("setupRich.languageActive")}
          />
          <DataList.Root unstyled className={styles.list}>
            <DataList.Item className={styles.listItem}>
              <DataList.ItemLabel className={styles.listLabel}>
                {t("localizationPage.systemLanguage")}
              </DataList.ItemLabel>
              <DataList.ItemValue>
                {data.localeInfo.displayNames?.systemLanguage ||
                  data.localeInfo.systemLocaleRaw}
              </DataList.ItemValue>
            </DataList.Item>
          </DataList.Root>
          <SetupSelect
            id="setup-language"
            icon={<Languages size={22} />}
            label={t("setupV5.languageLabel")}
            hint={t("setupV5.languageHint")}
            disabled={saving}
            value={selected}
            onChange={(value) => void changeLocale(value)}
            options={[
              ...new Set([
                selected,
                ...data.availableLocales.map((pack) => pack.target_locale),
              ]),
            ]
              .sort((a, b) => (names[a] || a).localeCompare(names[b] || b))
              .map((code) => ({ value: code, label: names[code] || code }))}
          />
          <Button
            variant="secondary"
            disabled={saving ||
              data.localeInfo.systemLocale.language === selected}
            onClick={() =>
              void changeLocale(data.localeInfo.systemLocale.language)}
          >
            {t("localizationPage.useSystemLanguage")}
          </Button>
          <SetupInfo>{t("setupV5.languageNotice")}</SetupInfo>
        </section>
      )}
      {saving && <p role="status">{t("ui.loading")}</p>}
      {error && (
        <p role="alert" className={styles.error}>
          {t(data ? "ui.saveError" : "ui.loadError")}
        </p>
      )}
      {error && !data && (
        <Button
          variant="secondary"
          onClick={() => setAttempt((value) => value + 1)}
        >
          {t("ui.retry", { defaultValue: "Try again" })}
        </Button>
      )}
    </div>
  );
}
