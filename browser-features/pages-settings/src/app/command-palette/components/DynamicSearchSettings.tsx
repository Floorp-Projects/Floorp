import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Seekbar } from "@/components/common/seekbar.tsx";
import { Separator } from "@/components/common/separator.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { cn } from "@/lib/utils";
import { Search } from "lucide-react";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import type { CommandPaletteFormData } from "@/types/pref.ts";

type DynamicSearchToggleKey = "showTabs" | "showHistory" | "showBookmarks";

export function DynamicSearchSettings() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext<CommandPaletteFormData>();
  const isDisabled = !getValues("enabled");

  const items: Array<{
    id: string;
    key: DynamicSearchToggleKey;
    label: string;
    desc: string;
  }> = [
    {
      id: "cp-show-tabs",
      key: "showTabs",
      label: t("commandPalette.showTabs"),
      desc: t("commandPalette.showTabsDescription"),
    },
    {
      id: "cp-show-history",
      key: "showHistory",
      label: t("commandPalette.showHistory"),
      desc: t("commandPalette.showHistoryDescription"),
    },
    {
      id: "cp-show-bookmarks",
      key: "showBookmarks",
      label: t("commandPalette.showBookmarks"),
      desc: t("commandPalette.showBookmarksDescription"),
    },
  ];

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Search className="size-5" />
          {t("commandPalette.dynamicSearch")}
        </CardTitle>
        <CardDescription>
          {t("commandPalette.dynamicSearchDescription")}
        </CardDescription>
      </CardHeader>
      <CardContent
        className={cn(
          "space-y-4 transition-opacity",
          isDisabled && "opacity-60",
        )}
      >
        {items.map((item) => (
          <div
            key={item.id}
            className="flex items-center justify-between gap-2"
          >
            <label htmlFor={item.id} className="flex flex-col gap-1.5">
              <span>{item.label}</span>
              <span className="font-normal text-sm text-base-content/70">
                {item.desc}
              </span>
            </label>
            <Switch
              id={item.id}
              checked={getValues(item.key)}
              disabled={isDisabled}
              onChange={(e) => setValue(item.key, e.target.checked)}
            />
          </div>
        ))}

        <Separator className="my-2" />

        {/*
         * Per-type result limits.
         * KEEP IN SYNC with the bounds in:
         * - browser-features/chrome/common/command-palette/config.ts
         * - browser-features/pages-settings/src/app/command-palette/dataManager.ts
         */}
        <Seekbar
          label={t("commandPalette.maxBookmarkSuggestions")}
          description={t("commandPalette.maxBookmarkSuggestionsDescription")}
          min={1}
          max={20}
          step={1}
          value={getValues("maxBookmarkSuggestions")}
          showValue
          showMinMax
          disabled={isDisabled || !getValues("showBookmarks")}
          onChange={(e) =>
            setValue("maxBookmarkSuggestions", Number(e.target.value))
          }
        />
        <Seekbar
          label={t("commandPalette.maxHistorySuggestions")}
          description={t("commandPalette.maxHistorySuggestionsDescription")}
          min={1}
          max={20}
          step={1}
          value={getValues("maxHistorySuggestions")}
          showValue
          showMinMax
          disabled={isDisabled || !getValues("showHistory")}
          onChange={(e) =>
            setValue("maxHistorySuggestions", Number(e.target.value))
          }
        />
        <Seekbar
          label={t("commandPalette.maxTabsResults")}
          description={t("commandPalette.maxTabsResultsDescription")}
          min={1}
          max={20}
          step={1}
          value={getValues("maxTabsResults")}
          showValue
          showMinMax
          disabled={isDisabled || !getValues("showTabs")}
          onChange={(e) => setValue("maxTabsResults", Number(e.target.value))}
        />
      </CardContent>
    </Card>
  );
}
