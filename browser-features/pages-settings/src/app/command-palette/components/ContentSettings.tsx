import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { cn } from "@/lib/utils";
import { ListFilter } from "lucide-react";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import type { CommandPaletteFormData } from "@/types/pref.ts";

type ContentToggleKey = "showTabs" | "showHistory" | "showBookmarks";

export function ContentSettings() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext<CommandPaletteFormData>();
  const isDisabled = !getValues("enabled");

  const items: Array<{
    id: string;
    key: ContentToggleKey;
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
          <ListFilter className="size-5" />
          {t("commandPalette.contentSettings")}
        </CardTitle>
        <CardDescription>
          {t("commandPalette.contentSettingsDescription")}
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
      </CardContent>
    </Card>
  );
}
