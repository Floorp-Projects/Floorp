import React from "react";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { BasicSettings } from "./components/BasicSettings.tsx";
import { PanelList } from "./components/PanelList.tsx";
import {
  getPanelSidebarSettings,
  savePanelSidebarSettings,
} from "./dataManager.ts";
import type { PanelSidebarFormData } from "@/types/pref.ts";

export default function Page() {
  const { t } = useTranslation();
  const methods = useForm({ defaultValues: {} });
  const { control, setValue } = methods;
  const watchAll = useWatch({ control });

  React.useEffect(() => {
    const fetchDefaultValues = async () => {
      const values = await getPanelSidebarSettings();
      if (!values) return;

      for (const key in values) {
        setValue(key as keyof PanelSidebarFormData, values[key]);
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
    savePanelSidebarSettings(watchAll);
  }, [watchAll]);

  return (
    <div className="p-6 space-y-6">
      <div className="flex flex-col items-start pl-6">
        <h1 className="text-3xl font-bold mb-2">{t("panelSidebar.title")}</h1>
        <p className="text-sm mb-8">{t("panelSidebar.description")}</p>
      </div>

      <div className="space-y-8 pl-6" />
      <FormProvider {...methods}>
        <form>
          <BasicSettings />
        </form>
      </FormProvider>
      <PanelList />
    </div>
  );
}
