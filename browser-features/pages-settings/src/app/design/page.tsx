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
import type { DesignFormData } from "@/types/pref.ts";

export default function Page() {
  const { t } = useTranslation();
  const navigate = useNavigate();
  const methods = useForm<DesignFormData>({
    defaultValues: {} as DesignFormData,
  });
  const { control, setValue } = methods;
  const watchAll = useWatch({ control });
  const [hasLoadedDefaults, setHasLoadedDefaults] = React.useState(false);
  const lastSavedSettingsRef = React.useRef<DesignFormData | null>(null);

  const cloneDesignSettings = React.useCallback(
    (data: DesignFormData): DesignFormData => JSON.parse(JSON.stringify(data)),
    [],
  );

  React.useEffect(() => {
    const fetchDefaultValues = async () => {
      const values = await getDesignSettings();
      if (values) {
        lastSavedSettingsRef.current = cloneDesignSettings(values);
        Object.entries(values).forEach(([key, value]) => {
          setValue(key as keyof DesignFormData, value, {
            shouldDirty: false,
            shouldTouch: false,
          });
        });
      }

      setHasLoadedDefaults(true);
    };
    fetchDefaultValues();
    document.documentElement.addEventListener("onfocus", fetchDefaultValues);
    return () => {
      document.documentElement.removeEventListener(
        "onfocus",
        fetchDefaultValues,
      );
    };
  }, [cloneDesignSettings, setValue]);

  React.useEffect(() => {
    if (!hasLoadedDefaults) {
      return;
    }

    if (!watchAll || Object.keys(watchAll).length === 0) {
      return;
    }

    const currentSettings = watchAll as DesignFormData;
    const lastSaved = lastSavedSettingsRef.current;

    if (
      lastSaved &&
      JSON.stringify(lastSaved) === JSON.stringify(currentSettings)
    ) {
      return;
    }

    const styleChanged = lastSaved?.style !== currentSettings.style;
    const snapshot = cloneDesignSettings(currentSettings);
    lastSavedSettingsRef.current = snapshot;

    void saveDesignSettings(snapshot, {
      hasTabStyleChanged: styleChanged,
    });
  }, [cloneDesignSettings, hasLoadedDefaults, watchAll]);

  const LeptonSettingsButton = () => {
    return (
      <div className="bg-muted/50 p-4 rounded-lg">
        <div className="flex items-center justify-between">
          <div>
            <h3 className="font-medium mb-1">
              {t("design.lepton-preferences.title")}
            </h3>
            <p className="text-sm text-muted-foreground">
              {t("design.lepton-preferences.description")}
            </p>
          </div>
          <button
            type="button"
            onClick={() => navigate("/features/design/lepton")}
            className="px-4 py-2 bg-primary text-primary-foreground rounded-md hover:bg-primary/80"
          >
            {t("design.lepton-preferences.configureLepton")}
          </button>
        </div>
      </div>
    );
  };

  // Check if current design supports Lepton settings
  const isLeptonCompatible = watchAll &&
    watchAll.design &&
    ["protonfix", "photon", "lepton"].includes(watchAll.design);

  return (
    <div className="p-6 space-y-3">
      <div className="flex flex-col items-start pl-6">
        <h1 className="text-3xl font-bold mb-2">
          {t("design.tabAndAppearance")}
        </h1>
        <p className="text-sm mb-8">
          {t("design.customizePositionOfToolbars")}
        </p>
      </div>

      <FormProvider {...methods}>
        <form className="space-y-3 pl-6">
          <Interface />
          {isLeptonCompatible && <LeptonSettingsButton />}
          <Tabbar />
          <Tab />
          <TabSleepExclusion />
          <UICustomization />
        </form>
      </FormProvider>
    </div>
  );
}
