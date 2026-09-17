import styles from "./design.module.css";
import { Button } from "../../../../../libs/ui/button.tsx";
import React from "react";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { useNavigate } from "react-router-dom";
import {
  getDesignSettings,
  saveDesignSettings,
} from "@/app/design/dataManager.ts";
import { Interface } from "@/app/design/components/Interface.tsx";
import { Tabbar } from "@/app/design/components/Tabbar.tsx";
import { Tab } from "@/app/design/components/Tab.tsx";
import { UICustomization } from "@/app/design/components/UICustomization.tsx";
import { TabSleepExclusion } from "@/app/design/components/TabSleepExclusion.tsx";
import { TabWindowBehavior } from "@/app/design/components/TabWindowBehavior.tsx";
import { TabStacks } from "@/app/design/components/TabStacks.tsx";
import type { DesignFormData } from "@/types/pref.ts";

export default function Page() {
  const { t } = useTranslation();
  const navigate = useNavigate();
  const methods = useForm<DesignFormData>({
    defaultValues: {} as DesignFormData,
  });
  const { control, reset } = methods;
  const watchAll = useWatch({ control });
  const [hasLoadedDefaults, setHasLoadedDefaults] = React.useState(false);
  const lastSavedSettingsRef = React.useRef<DesignFormData | null>(null);
  const lastRequestedSettingsRef = React.useRef<DesignFormData | null>(null);
  const saveQueueRef = React.useRef(Promise.resolve());
  const pendingRef = React.useRef(0);
  const mountedRef = React.useRef(false);
  const [loadError, setLoadError] = React.useState(false);
  const [saveError, setSaveError] = React.useState(false);
  const [saving, setSaving] = React.useState(false);
  const [retry, setRetry] = React.useState(0);

  const cloneDesignSettings = React.useCallback(
    (data: DesignFormData): DesignFormData => JSON.parse(JSON.stringify(data)),
    [],
  );

  React.useEffect(() => {
    let active = true;
    mountedRef.current = true;
    const fetchDefaultValues = async () => {
      // Preserve pending or failed edits when the window regains focus.
      if (
        pendingRef.current ||
        JSON.stringify(lastRequestedSettingsRef.current) !==
          JSON.stringify(lastSavedSettingsRef.current)
      ) return;
      try {
        const values = await getDesignSettings();
        if (!active) return;
        if (!values) throw new Error("Settings unavailable");
        if (
          pendingRef.current ||
          JSON.stringify(lastRequestedSettingsRef.current) !==
            JSON.stringify(lastSavedSettingsRef.current)
        ) return;
        lastSavedSettingsRef.current = cloneDesignSettings(values);
        lastRequestedSettingsRef.current = cloneDesignSettings(values);
        reset(values);
        setHasLoadedDefaults(true);
        setLoadError(false);
      } catch (error) {
        if (!active) return;
        console.error("[Settings:design] Load failed", error);
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
  }, [cloneDesignSettings, reset]);

  React.useEffect(() => {
    if (!hasLoadedDefaults) {
      return;
    }

    if (!watchAll || Object.keys(watchAll).length === 0) {
      return;
    }

    const currentSettings = watchAll as DesignFormData;
    const lastRequested = lastRequestedSettingsRef.current;

    if (
      lastRequested &&
      JSON.stringify(lastRequested) === JSON.stringify(currentSettings)
    ) {
      return;
    }

    const snapshot = cloneDesignSettings(currentSettings);
    lastRequestedSettingsRef.current = snapshot;
    pendingRef.current++;
    setSaving(true);
    setSaveError(false);
    saveQueueRef.current = saveQueueRef.current.then(async () => {
      try {
        const result = await saveDesignSettings(snapshot, {
          hasTabStyleChanged:
            lastSavedSettingsRef.current?.style !== snapshot.style,
        });
        if (result === null) throw new Error("Settings unavailable");
        lastSavedSettingsRef.current = snapshot;
        if (mountedRef.current) setSaveError(false);
      } catch (error) {
        console.error("[Settings:design] Save failed", error);
        if (mountedRef.current) setSaveError(true);
      } finally {
        pendingRef.current--;
        if (mountedRef.current) setSaving(pendingRef.current > 0);
      }
    });
  }, [cloneDesignSettings, hasLoadedDefaults, watchAll, retry]);

  const leptonSettingsButton = (
    <div className={styles.lepton}>
      <div className={styles.row}>
        <div>
          <h3 className="font-medium mb-1">
            {t("design.lepton-preferences.title")}
          </h3>
          <p className="text-sm text-muted-foreground">
            {t("design.lepton-preferences.description")}
          </p>
        </div>
        <Button
          type="button"
          onClick={() => navigate("/features/design/lepton")}
          variant="secondary"
        >
          {t("design.lepton-preferences.configureLepton")}
        </Button>
      </div>
    </div>
  );

  // Check if current design supports Lepton settings
  const isLeptonCompatible = watchAll &&
    watchAll.design &&
    ["protonfix", "photon", "lepton"].includes(watchAll.design);

  return (
    <div className={`floorp-settings-page ${styles.page}`}>
      <div className="floorp-page-header">
        <h1 className="floorp-page-heading">
          {t("design.tabAndAppearance")}
        </h1>
        <p className="floorp-page-description">
          {t("design.customizePositionOfToolbars")}
        </p>
      </div>

      <FormProvider {...methods}>
        {loadError && <p role="alert">{t("ui.loadError")}</p>}
        {!hasLoadedDefaults && !loadError && (
          <p role="status">{t("ui.loading")}</p>
        )}
        {saveError && (
          <div className="space-y-2">
            <p role="alert">{t("ui.saveError")}</p>
            <Button
              type="button"
              variant="secondary"
              disabled={saving}
              onClick={() => {
                lastRequestedSettingsRef.current = null;
                setRetry((value) => value + 1);
              }}
            >
              {t("ui.retry")}
            </Button>
          </div>
        )}
        <form onSubmit={(event) => event.preventDefault()}>
          <fieldset
            disabled={!hasLoadedDefaults}
            aria-busy={!hasLoadedDefaults || saving}
            className={styles.sections}
          >
            <Interface />
            {isLeptonCompatible && leptonSettingsButton}
            <Tabbar />
            <Tab />
            <TabWindowBehavior />
            <TabStacks />
            <TabSleepExclusion />
            <UICustomization />
          </fieldset>
        </form>
      </FormProvider>
    </div>
  );
}
