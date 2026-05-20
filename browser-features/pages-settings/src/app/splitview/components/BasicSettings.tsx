import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
  CardDescription,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import { ExternalLink, LayoutGrid } from "lucide-react";
import type { SplitViewFormData } from "@/types/pref.ts";

export function BasicSettings() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext<SplitViewFormData>();

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <LayoutGrid className="size-5" />
          {t("splitView.basicSettings")}
        </CardTitle>
        <CardDescription>{t("splitView.basicSettingsDescription")}</CardDescription>
      </CardHeader>
      <CardContent className="space-y-4">
        <div className="flex items-center justify-between gap-2">
          <div className="space-y-1">
            <label htmlFor="enable-splitview" className="font-medium">
              {t("splitView.enableSplitView")}
            </label>
            <p className="text-sm text-base-content/70">
              {t("splitView.enableSplitViewDescription")}
            </p>
          </div>
          <Switch
            id="enable-splitview"
            checked={!!getValues("enabled")}
            onChange={(e) => setValue("enabled", e.target.checked)}
          />
        </div>

        <div className="flex items-center justify-between gap-2">
          <div className="space-y-1">
            <label htmlFor="max-panes" className="font-medium">
              {t("splitView.maxPanes")}
            </label>
            <p className="text-sm text-base-content/70">
              {t("splitView.maxPanesDescription")}
            </p>
          </div>
          <select
            id="max-panes"
            className="select select-bordered select-sm w-24"
            value={getValues("maxPanes")}
            onChange={(e) => setValue("maxPanes", Number(e.target.value))}
          >
            <option value={2}>2</option>
            <option value={3}>3</option>
            <option value={4}>4</option>
          </select>
        </div>

        <div>
          <a
            href="https://docs.floorp.app/docs/features/split-view"
            target="_blank"
            className="text-[var(--link-text-color)] hover:underline inline-flex items-center gap-2"
          >
            {t("splitView.learnMore")}
            <ExternalLink className="size-4" />
          </a>
        </div>
      </CardContent>
    </Card>
  );
}
