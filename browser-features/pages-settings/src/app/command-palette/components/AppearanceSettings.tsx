import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { DropDown, type DropDownOption } from "@/components/common/dropdown.tsx";
import { Seekbar } from "@/components/common/seekbar.tsx";
import { SlidersHorizontal } from "lucide-react";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import type { CommandPaletteFormData } from "@/types/pref.ts";

export function AppearanceSettings() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext<CommandPaletteFormData>();

  const alignOptions: DropDownOption[] = [
    { value: "center", label: t("commandPalette.alignCenter") },
    { value: "left", label: t("commandPalette.alignLeft") },
    { value: "right", label: t("commandPalette.alignRight") },
  ];

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <SlidersHorizontal className="size-5" />
          {t("commandPalette.appearanceSettings")}
        </CardTitle>
        <CardDescription>
          {t("commandPalette.appearanceSettingsDescription")}
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-4">
        <Seekbar
          label={t("commandPalette.width")}
          description={t("commandPalette.widthDescription")}
          min={400}
          max={1000}
          step={20}
          valueSuffix="px"
          value={getValues("width")}
          onChange={(e) => setValue("width", Number(e.target.value))}
        />

        <Seekbar
          label={t("commandPalette.maxHeight")}
          description={t("commandPalette.maxHeightDescription")}
          min={300}
          max={800}
          step={20}
          valueSuffix="px"
          value={getValues("maxHeight")}
          onChange={(e) => setValue("maxHeight", Number(e.target.value))}
        />

        <Seekbar
          label={t("commandPalette.offsetTop")}
          description={t("commandPalette.offsetTopDescription")}
          min={0}
          max={60}
          step={5}
          valueSuffix="vh"
          value={getValues("offsetTop")}
          onChange={(e) => setValue("offsetTop", Number(e.target.value))}
        />

        <div>
          <div className="mb-1">
            <label className="text-base-content/90 text-sm font-medium">
              {t("commandPalette.horizontalAlign")}
            </label>
            <p className="text-sm text-base-content/60 mt-0.5">
              {t("commandPalette.horizontalAlignDescription")}
            </p>
          </div>
          <DropDown
            className="max-w-xs"
            value={getValues("horizontalAlign")}
            options={alignOptions}
            onChange={(e) => setValue("horizontalAlign", e.target.value)}
          />
        </div>
      </CardContent>
    </Card>
  );
}
