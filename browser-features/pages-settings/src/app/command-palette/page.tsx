import React from "react";
import { FormProvider, useForm, useWatch } from "react-hook-form";
import { useTranslation } from "react-i18next";
import { AppearanceSettings } from "./components/AppearanceSettings.tsx";
import { BasicSettings } from "./components/BasicSettings.tsx";
import { ContentSettings } from "./components/ContentSettings.tsx";
import { DynamicSearchSettings } from "./components/DynamicSearchSettings.tsx";
import { ResultLimitSettings } from "./components/ResultLimitSettings.tsx";
import { ShortcutList } from "./components/ShortcutList.tsx";
import {
  COMMAND_PALETTE_DEFAULT_VALUES,
  getCommandPaletteSettings,
  saveCommandPaletteSettings,
} from "./dataManager.ts";
import type { CommandPaletteFormData } from "@/types/pref.ts";

export default function Page() {
  const { t } = useTranslation();
  const methods = useForm<CommandPaletteFormData>({
    defaultValues: COMMAND_PALETTE_DEFAULT_VALUES,
  });

  const { control, setValue } = methods;
  const watchAll = useWatch({ control });

  // Skip saves until the initial pref load has populated the form, and
  // serialize saves so an older snapshot can never finish after a newer one.
  const initialLoadDoneRef = React.useRef(false);
  const saveChainRef = React.useRef<Promise<void>>(Promise.resolve());

  React.useEffect(() => {
    const fetchDefaultValues = async () => {
      try {
        const values = await getCommandPaletteSettings();
        if (!values) return;

        setValue("enabled", values.enabled, { shouldValidate: true });
        setValue("width", values.width, { shouldValidate: true });
        setValue("maxHeight", values.maxHeight, { shouldValidate: true });
        setValue("offsetTop", values.offsetTop, { shouldValidate: true });
        setValue("horizontalAlign", values.horizontalAlign, {
          shouldValidate: true,
        });
        setValue("fontSize", values.fontSize, { shouldValidate: true });
        setValue("showTabs", values.showTabs, { shouldValidate: true });
        setValue("showHistory", values.showHistory, { shouldValidate: true });
        setValue("showBookmarks", values.showBookmarks, {
          shouldValidate: true,
        });
        setValue("categoryPriority", values.categoryPriority, {
          shouldValidate: true,
        });
        setValue("maxResultsPerCategory", values.maxResultsPerCategory, {
          shouldValidate: true,
        });
        setValue("maxBookmarkSuggestions", values.maxBookmarkSuggestions, {
          shouldValidate: true,
        });
        setValue("maxHistorySuggestions", values.maxHistorySuggestions, {
          shouldValidate: true,
        });
        setValue("maxTabsResults", values.maxTabsResults, {
          shouldValidate: true,
        });
      } catch (error) {
        console.error("[command-palette] Failed to load settings:", error);
      } finally {
        initialLoadDoneRef.current = true;
      }
    };

    fetchDefaultValues();
    globalThis.addEventListener("focus", fetchDefaultValues);
    return () => {
      globalThis.removeEventListener("focus", fetchDefaultValues);
    };
  }, [setValue]);

  React.useEffect(() => {
    if (!initialLoadDoneRef.current) return;
    if (Object.keys(watchAll).length === 0) return;

    const snapshot = { ...watchAll };
    saveChainRef.current = saveChainRef.current
      .then(async () => {
        await saveCommandPaletteSettings(snapshot);
      })
      .catch((error: unknown) => {
        console.error("[command-palette] Failed to save settings:", error);
      });
  }, [watchAll]);

  return (
    <div className="p-6 space-y-3">
      <div className="flex flex-col items-start pl-6">
        <h1 className="text-3xl font-bold mb-2">
          {t("pages.commandPalette")}
        </h1>
        <p className="text-sm mb-8">{t("commandPalette.description")}</p>
      </div>

      <FormProvider {...methods}>
        <form
          className="space-y-3 pl-6"
          onSubmit={(e) => e.preventDefault()}
        >
          <BasicSettings />
          <ContentSettings />
          <DynamicSearchSettings />
          <ResultLimitSettings />
          <AppearanceSettings />
        </form>
      </FormProvider>

      <div className="pl-6">
        <ShortcutList />
      </div>
    </div>
  );
}
