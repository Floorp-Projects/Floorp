import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Seekbar } from "@/components/common/seekbar.tsx";
import { cn } from "@/lib/utils";
import { SlidersHorizontal } from "lucide-react";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import type { CommandPaletteFormData } from "@/types/pref.ts";

export function ResultLimitSettings() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext<CommandPaletteFormData>();
  const isDisabled = !getValues("enabled");

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <SlidersHorizontal className="size-5" />
          {t("commandPalette.resultLimit")}
        </CardTitle>
        <CardDescription>
          {t("commandPalette.resultLimitDescription")}
        </CardDescription>
      </CardHeader>
      <CardContent
        className={cn(
          "transition-opacity",
          isDisabled && "opacity-60",
        )}
      >
        {/*
         * KEEP IN SYNC with the bounds in:
         * - browser-features/chrome/common/command-palette/config.ts
         * - browser-features/pages-settings/src/app/command-palette/dataManager.ts
         */}
        <Seekbar
          label={t("commandPalette.maxResultsPerCategory")}
          description={t("commandPalette.maxResultsPerCategoryDescription")}
          min={1}
          max={20}
          step={1}
          value={getValues("maxResultsPerCategory")}
          showValue
          showMinMax
          disabled={isDisabled}
          onChange={(e) =>
            setValue(
              "maxResultsPerCategory",
              Number(e.target.value),
            )
          }
        />
      </CardContent>
    </Card>
  );
}
