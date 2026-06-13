import React from "react";
import { useTranslation } from "react-i18next";
import { BasicSettings } from "./components/BasicSettings.tsx";
import { LayoutSettings } from "./components/LayoutSettings.tsx";
import { PaneSizeSettings } from "./components/PaneSizeSettings.tsx";
import { useSplitViewSettings } from "./dataManager.ts";

export default function Page() {
  const { t } = useTranslation();
  const { settings, loading, updateSettings, toggleEnabled } =
    useSplitViewSettings();

  if (loading) {
    return <div className="py-6 text-center">{t("loading")}...</div>;
  }

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
        </header>
      </div>
      <div className="pl-6 space-y-6">
        <BasicSettings
          settings={settings}
          onToggleEnabled={toggleEnabled}
          onMaxPanesChange={(maxPanes) => updateSettings({ maxPanes })}
        />
        <LayoutSettings
          settings={settings}
          onLayoutChange={(layout) => updateSettings({ layout })}
        />
        <PaneSizeSettings
          settings={settings}
          onPaneSizesChange={(paneSizes) => updateSettings(paneSizes)}
        />
      </div>
    </div>
  );
}
