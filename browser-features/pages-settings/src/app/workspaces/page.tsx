import React from "react";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { BasicSettings } from "./components/BasicSettings.tsx";
import { getWorkspaceSettings, saveWorkspaceSettings } from "./dataManager.ts";
import type { WorkspacesFormData } from "@/types/pref.ts";

export default function Page() {
  const { t } = useTranslation();
  const methods = useForm<WorkspacesFormData>({});

  const { control, reset, getValues } = methods;
  const [ready, setReady] = React.useState(false);
  const [error, setError] = React.useState(false);
  const watchAll = useWatch({ control });

  React.useEffect(() => {
    const fetchDefaultValues = async () => {
      try {
        const values = await getWorkspaceSettings();
        if (!values) throw new Error("Settings unavailable");
        reset(values);
        setReady(true);
        setError(false);
      } catch (error) {
        console.error("[Settings:workspaces] Load failed", error);
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
    Promise.resolve().then(() => saveWorkspaceSettings(getValues())).catch(
      (error) => {
        console.error("[Settings:workspaces] Save failed", error);
        setError(true);
      },
    );
  }, [watchAll, ready, getValues]);

  return (
    <div className="space-y-6">
      <div>
        <h1 className="floorp-page-heading">
          {t("workspaces.workspaces")}
        </h1>
        <p className="floorp-page-description">
          {t("workspaces.workspacesDescription")}
        </p>
      </div>

      {error && <p role="alert">{t(ready ? "ui.saveError" : "ui.loadError")}</p>}
      {!ready && !error && <p role="status">{t("ui.loading")}</p>}
      <FormProvider {...methods}>
        <form
          className="space-y-6"
          onSubmit={(e) => e.preventDefault()}
        >
          <fieldset disabled={!ready} aria-busy={!ready} className="min-w-0">
            <BasicSettings />
            {/* <BackupSettings /> */}
          </fieldset>
        </form>
      </FormProvider>
    </div>
  );
}
