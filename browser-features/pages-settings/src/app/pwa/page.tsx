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
  const { control, setValue } = methods;
  const watchAll = useWatch({ control });

  React.useEffect(() => {
    const fetchDefaultValues = async () => {
      const values = await getPwaSettings();
      Object.entries(values).forEach(([key, value]) => {
        setValue(key as keyof TProgressiveWebAppFormData, value);
      });
    };
    fetchDefaultValues();
    globalThis.addEventListener("focus", fetchDefaultValues);
    return () => {
      globalThis.removeEventListener("focus", fetchDefaultValues);
    };
  }, [setValue]);

  React.useEffect(() => {
    savePwaSettings(watchAll as TProgressiveWebAppFormData);
  }, [watchAll]);

  return (
    <div className={`floorp-settings-page ${styles.page}`}>
      <header className="floorp-page-header">
        <h1 className="floorp-page-heading">{t("progressiveWebApp.title")}</h1>
        <p className="floorp-page-description">
          {t("progressiveWebApp.description")}
        </p>
      </header>
      <FormProvider {...methods}>
        <form
          className={styles.sections}
          onSubmit={(event) => event.preventDefault()}
        >
          <Preferences />
          <InstalledApps />
        </form>
      </FormProvider>
    </div>
  );
}
