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
  const methods = useForm<PanelSidebarFormData>({ defaultValues: {} });
  const { control, reset, getValues } = methods;
  const [ready, setReady] = React.useState(false);
  const [error, setError] = React.useState(false);
  const watchAll = useWatch({ control });

  React.useEffect(() => {
    const fetchDefaultValues = async () => {
      try {
        const values = await getPanelSidebarSettings();
        if (!values) throw new Error("Settings unavailable");
        reset(values);
        setReady(true);
        setError(false);
      } catch (error) {
        console.error("[Settings:sidebar] Load failed", error);
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
    Promise.resolve().then(() => savePanelSidebarSettings(getValues())).catch(
      (error) => {
        console.error("[Settings:sidebar] Save failed", error);
        setError(true);
      },
    );
  }, [watchAll, ready, getValues]);

  return (
    <div className="floorp-settings-page">
      <div className="floorp-page-header">
        <h1 className="floorp-page-heading">{t("panelSidebar.title")}</h1>
        <p className="floorp-page-description">
          {t("panelSidebar.description")}
        </p>
      </div>

      {error && <p role="alert">{t(ready ? "ui.saveError" : "ui.loadError")}</p>}
      {!ready && !error && <p role="status">{t("ui.loading")}</p>}
      <FormProvider {...methods}>
        <form onSubmit={(event) => event.preventDefault()}>
          <fieldset disabled={!ready} aria-busy={!ready} className="min-w-0">
            <BasicSettings />
          </fieldset>
        </form>
      </FormProvider>
      <PanelList />
    </div>
  );
}
