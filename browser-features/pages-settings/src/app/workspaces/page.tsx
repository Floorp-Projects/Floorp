import React from "react";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { BasicSettings } from "./components/BasicSettings.tsx";
import { getWorkspaceSettings, saveWorkspaceSettings } from "./dataManager.ts";
import type { WorkspacesFormData } from "@/types/pref.ts";
import { Button } from "@/components/common/button.tsx";
import styles from "./workspace.module.css";

export default function Page() {
  const { t } = useTranslation();
  const methods = useForm<WorkspacesFormData>({});

  const { control, reset, getValues } = methods;
  const [ready, setReady] = React.useState(false);
  const [loadError, setLoadError] = React.useState(false);
  const [saveError, setSaveError] = React.useState(false);
  const [saving, setSaving] = React.useState(false);
  const [loadRetry, setLoadRetry] = React.useState(0);
  const [saveRetry, setSaveRetry] = React.useState(0);
  const savedRef = React.useRef<WorkspacesFormData | null>(null);
  const requestedRef = React.useRef<WorkspacesFormData | null>(null);
  const pendingRef = React.useRef(0);
  const saveQueueRef = React.useRef(Promise.resolve());
  const mountedRef = React.useRef(false);
  const watchAll = useWatch({ control });

  React.useEffect(() => {
    let active = true;
    let requestId = 0;
    mountedRef.current = true;
    const hasUnsavedChanges = () =>
      pendingRef.current > 0 ||
      (savedRef.current !== null &&
        JSON.stringify(getValues()) !== JSON.stringify(savedRef.current));
    const fetchDefaultValues = async () => {
      // Refocusing must not discard edits while saving or after a failure.
      if (hasUnsavedChanges()) return;
      const currentRequest = ++requestId;
      const requestedAtStart = requestedRef.current;
      const isStale = () =>
        !active || currentRequest !== requestId ||
        requestedAtStart !== requestedRef.current || hasUnsavedChanges();
      try {
        const values = await getWorkspaceSettings();
        if (isStale()) return;
        if (!values) throw new Error("Settings unavailable");
        savedRef.current = { ...values };
        requestedRef.current = { ...values };
        reset(values);
        setReady(true);
        setLoadError(false);
      } catch (error) {
        if (isStale()) return;
        console.error("[Settings:workspaces] Load failed", error);
        setLoadError(true);
      }
    };

    fetchDefaultValues();
    globalThis.addEventListener("focus", fetchDefaultValues);
    return () => {
      active = false;
      mountedRef.current = false;
      globalThis.removeEventListener("focus", fetchDefaultValues);
    };
  }, [getValues, reset, loadRetry]);

  React.useEffect(() => {
    if (!ready) return;
    const snapshot = { ...getValues() };
    if (
      JSON.stringify(snapshot) === JSON.stringify(requestedRef.current)
    ) return;
    requestedRef.current = snapshot;
    pendingRef.current++;
    setSaving(true);
    setSaveError(false);
    saveQueueRef.current = saveQueueRef.current.then(async () => {
      try {
        const result = await saveWorkspaceSettings(snapshot);
        if (result === null) throw new Error("Settings unavailable");
        savedRef.current = snapshot;
        if (mountedRef.current) setSaveError(false);
      } catch (error) {
        console.error("[Settings:workspaces] Save failed", error);
        if (mountedRef.current) setSaveError(true);
      } finally {
        pendingRef.current--;
        if (mountedRef.current) setSaving(pendingRef.current > 0);
      }
    });
  }, [watchAll, ready, getValues, saveRetry]);

  return (
    <div className={`floorp-settings-page ${styles.page}`}>
      <div className="floorp-page-header">
        <h1 className="floorp-page-heading">
          {t("workspaces.workspaces")}
        </h1>
        <p className="floorp-page-description">
          {t("workspaces.workspacesDescription")}
        </p>
      </div>

      {loadError && (
        <div className="space-y-2">
          <p role="alert">{t("ui.loadError")}</p>
          <Button
            variant="secondary"
            onClick={() => setLoadRetry((n) => n + 1)}
          >
            {t("ui.retry")}
          </Button>
        </div>
      )}
      {saveError && (
        <div className="space-y-2">
          <p role="alert">{t("ui.saveError")}</p>
          <Button
            variant="secondary"
            disabled={saving}
            onClick={() => {
              requestedRef.current = null;
              setSaveRetry((n) => n + 1);
            }}
          >
            {t("ui.retry")}
          </Button>
        </div>
      )}
      {!ready && !loadError && <p role="status">{t("ui.loading")}</p>}
      <FormProvider {...methods}>
        <form
          className="space-y-6"
          onSubmit={(e) => e.preventDefault()}
        >
          <fieldset
            disabled={!ready}
            aria-busy={!ready || saving}
            className={styles.sections}
          >
            <BasicSettings />
            {/* <BackupSettings /> */}
          </fieldset>
        </form>
      </FormProvider>
    </div>
  );
}
