import { useState } from "react";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Button } from "@/components/common/button.tsx";
import { cn } from "@/lib/utils";
import { ListFilter } from "lucide-react";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import type { CommandPaletteFormData } from "@/types/pref.ts";
import { CategoryPriorityModal } from "./CategoryPriorityModal.tsx";

export function ContentSettings() {
  const { t } = useTranslation();
  const { getValues } = useFormContext<CommandPaletteFormData>();
  const isDisabled = !getValues("enabled");
  const [isPriorityModalOpen, setIsPriorityModalOpen] = useState(false);

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <ListFilter className="size-5" />
          {t("commandPalette.prioritySettings")}
        </CardTitle>
        <CardDescription>
          {t("commandPalette.prioritySettingsDescription")}
        </CardDescription>
      </CardHeader>
      <CardContent
        className={cn(
          "transition-opacity",
          isDisabled && "opacity-60",
        )}
      >
        <div className="flex items-center justify-end gap-2">
          <Button
            variant="secondary"
            size="sm"
            disabled={isDisabled}
            onClick={() => setIsPriorityModalOpen(true)}
          >
            {t("commandPalette.prioritySettingsButton")}
          </Button>
        </div>
      </CardContent>
      <CategoryPriorityModal
        isOpen={isPriorityModalOpen}
        onClose={() => setIsPriorityModalOpen(false)}
      />
    </Card>
  );
}
