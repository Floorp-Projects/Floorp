import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { ConfirmModal } from "@/components/common/ConfirmModal.tsx";
import { Button } from "@/components/common/button.tsx";
import { DropDown, type DropDownOption } from "@/components/common/dropdown.tsx";
import { Seekbar } from "@/components/common/seekbar.tsx";
import { cn } from "@/lib/utils";
import { RotateCcw, SlidersHorizontal } from "lucide-react";
import { useState } from "react";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import type { CommandPaletteFormData } from "@/types/pref.ts";
import { COMMAND_PALETTE_APPEARANCE_DEFAULTS } from "../dataManager.ts";

export function AppearanceSettings() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext<CommandPaletteFormData>();
  const [showResetModal, setShowResetModal] = useState(false);
  const isDisabled = !getValues("enabled");

  const alignOptions: DropDownOption[] = [
    { value: "center", label: t("commandPalette.alignCenter") },
    { value: "left", label: t("commandPalette.alignLeft") },
    { value: "right", label: t("commandPalette.alignRight") },
  ];

  const handleReset = () => {
    setValue("width", COMMAND_PALETTE_APPEARANCE_DEFAULTS.width, {
      shouldDirty: true,
    });
    setValue("maxHeight", COMMAND_PALETTE_APPEARANCE_DEFAULTS.maxHeight, {
      shouldDirty: true,
    });
    setValue("offsetTop", COMMAND_PALETTE_APPEARANCE_DEFAULTS.offsetTop, {
      shouldDirty: true,
    });
    setValue(
      "horizontalAlign",
      COMMAND_PALETTE_APPEARANCE_DEFAULTS.horizontalAlign,
      { shouldDirty: true },
    );
    setValue("fontSize", COMMAND_PALETTE_APPEARANCE_DEFAULTS.fontSize, {
      shouldDirty: true,
    });
  };

  return (
    <>
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
        <CardContent className={cn("space-y-4 transition-opacity", isDisabled && "opacity-60")}>
          <Seekbar
            label={t("commandPalette.width")}
            description={t("commandPalette.widthDescription")}
            min={400}
            max={1000}
            step={20}
            valueSuffix="px"
            value={getValues("width")}
            onChange={(e) => setValue("width", Number(e.target.value))}
            disabled={isDisabled}
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
            disabled={isDisabled}
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
            disabled={isDisabled}
          />

          <Seekbar
            label={t("commandPalette.fontSize")}
            description={t("commandPalette.fontSizeDescription")}
            min={11}
            max={22}
            step={1}
            valueSuffix="px"
            value={getValues("fontSize")}
            onChange={(e) => setValue("fontSize", Number(e.target.value))}
            disabled={isDisabled}
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
              onChange={(e) =>
                setValue(
                  "horizontalAlign",
                  e.target.value as CommandPaletteFormData["horizontalAlign"],
                )}
              disabled={isDisabled}
            />
          </div>

          <div className="flex justify-end pt-2 border-t border-base-300/40">
            <Button
              variant="ghost"
              size="sm"
              onClick={() => setShowResetModal(true)}
              disabled={isDisabled}
            >
              <RotateCcw className="size-4 mr-1" />
              {t("commandPalette.resetToDefaults")}
            </Button>
          </div>
        </CardContent>
      </Card>

      <ConfirmModal
        isOpen={showResetModal}
        onClose={() => setShowResetModal(false)}
        onConfirm={handleReset}
        title={t("commandPalette.resetToDefaultsTitle")}
        confirmText={t("commandPalette.reset")}
        confirmVariant="danger"
      >
        <p>{t("commandPalette.resetToDefaultsConfirm")}</p>
      </ConfirmModal>
    </>
  );
}
