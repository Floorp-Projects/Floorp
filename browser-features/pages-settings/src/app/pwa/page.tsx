import styles from "@/components/common/settings-sections.module.css";
import React from "react";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { Preferences } from "./components/Preferences.tsx";
import { InstalledApps } from "./components/InstalledApps.tsx";
import { getPwaSettings, savePwaSettings } from "./dataManager.ts";
import type { TProgressiveWebAppFormData } from "@/types/pref.ts";

export default function Page() {
  const { t } = useTranslation();
  const methods = useForm<TProgressiveWebAppFormData>({ defaultValues: {} });
  const { control, reset } = methods;
  const [ready, setReady] = React.useState(false);
  const [error, setError] = React.useState(false);
  const watchAll = useWatch({ control });

  React.useEffect(() => {
    const fetchDefaultValues = async () => {
      try {
        const values = await getPwaSettings();
        reset(values);
        setReady(true);
        setError(false);
      } catch (error) {
        console.error("[Settings:pwa] Load failed", error);
        setError(true);
      }
    };
    fetchDefaultValues();
    globalThis.addEventListener("focus", fetchDefaultValues);
    return () => {
      globalThis.removeEventListener("focus", fetchDefaultValues);
    };
  }, [reset]);

  React.useEffect(() => {
    if (!ready) return;
    void savePwaSettings(watchAll as TProgressiveWebAppFormData).catch((error) => {
      console.error("[Settings:pwa] Save failed", error);
      setError(true);
    });
  }, [watchAll, ready]);

  return (
    <div className={`floorp-settings-page ${styles.page}`}>
      <header className="floorp-page-header">
        <h1 className="floorp-page-heading">{t("progressiveWebApp.title")}</h1>
        <p className="floorp-page-description">
          {t("progressiveWebApp.description")}
        </p>
      </header>
      {error && <p role="alert">{t(ready ? "ui.saveError" : "ui.loadError")}</p>}
      {!ready && !error && <p role="status">{t("ui.loading")}</p>}
      <FormProvider {...methods}>
        <form
          className={styles.sections}
          onSubmit={(event) => event.preventDefault()}
        >
          <fieldset disabled={!ready} aria-busy={!ready} className="min-w-0">
            <Preferences />
          </fieldset>
          <InstalledApps />
        </form>
      </FormProvider>
    </div>
  );
}
