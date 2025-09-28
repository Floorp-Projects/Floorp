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
    document.documentElement.addEventListener("onfocus", fetchDefaultValues);
    return () => {
      document.documentElement.removeEventListener(
        "onfocus",
        fetchDefaultValues,
      );
    };
  }, [setValue]);

  React.useEffect(() => {
    savePwaSettings(watchAll as TProgressiveWebAppFormData);
  }, [watchAll]);

  return (
    <div className="p-6">
      <div className="flex flex-col items-start pl-6">
        <header className="mb-8">
          <h1 className="text-3xl font-bold mb-2">
            {t("progressiveWebApp.title")}
          </h1>
          <p className="text-sm">
            {t("progressiveWebApp.description")}
          </p>
        </header>
        <FormProvider {...methods}>
          <form className="space-y-3 w-full">
            <Preferences />
            <InstalledApps />
          </form>
        </FormProvider>
      </div>
    </div>
  );
}
