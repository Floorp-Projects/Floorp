import React from "react";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { BasicSettings } from "./components/BasicSettings.tsx";
import { LayoutSettings } from "./components/LayoutSettings.tsx";
import { LearnButton } from "@/components/common/learn-button.tsx";
import {
  getSplitViewSettings,
  saveSplitViewSettings,
} from "./dataManager.ts";
import type { SplitViewFormData } from "@/types/pref.ts";

export default function Page() {
  const { t } = useTranslation();
  const methods = useForm<SplitViewFormData>({ defaultValues: {} });

  const { control, setValue } = methods;
  const watchAll = useWatch({ control });

  React.useEffect(() => {
    const fetchDefaultValues = async () => {
      const values = await getSplitViewSettings();
      if (!values) return;

      for (const key in values) {
        setValue(key as keyof SplitViewFormData, values[key as keyof SplitViewFormData]);
      }
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
    if (Object.keys(watchAll).length === 0) return;
    saveSplitViewSettings(watchAll as SplitViewFormData);
  }, [watchAll]);

  return (
    <div className="p-6">
      <div className="flex flex-col items-start pl-6">
        <header className="mb-8">
          <h1 className="text-3xl font-bold mb-2">
            {t("splitView.title")}
          </h1>
          <p className="text-sm mb-4">
            {t("splitView.description")}
          </p>
          <LearnButton tourId="splitView" />
        </header>
      </div>
      <div className="pl-6 space-y-6">
        <FormProvider {...methods}>
          <form className="space-y-6">
            <BasicSettings />
            <LayoutSettings />
          </form>
        </FormProvider>
      </div>
    </div>
  );
}
