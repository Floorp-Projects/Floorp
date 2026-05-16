import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
  CardDescription,
} from "@/components/common/card.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import { Columns2, Rows2, LayoutGrid } from "lucide-react";
import type { SplitViewFormData } from "@/types/pref.ts";

type LayoutOption = {
  value: string;
  labelKey: string;
  icon: React.ReactNode;
};

const layouts: LayoutOption[] = [
  {
    value: "horizontal",
    labelKey: "splitView.layoutHorizontal",
    icon: <Columns2 className="size-5" />,
  },
  {
    value: "vertical",
    labelKey: "splitView.layoutVertical",
    icon: <Rows2 className="size-5" />,
  },
  {
    value: "grid-2x2",
    labelKey: "splitView.layoutGrid2x2",
    icon: <LayoutGrid className="size-5" />,
  },
  {
    value: "grid-3pane-left-main",
    labelKey: "splitView.layout3PaneLeft",
    icon: <LayoutGrid className="size-5" />,
  },
  {
    value: "grid-3pane-right-main",
    labelKey: "splitView.layout3PaneRight",
    icon: <LayoutGrid className="size-5" />,
  },
  {
    value: "grid-3pane-top-main",
    labelKey: "splitView.layout3PaneTop",
    icon: <LayoutGrid className="size-5" />,
  },
  {
    value: "grid-3pane-bottom-main",
    labelKey: "splitView.layout3PaneBottom",
    icon: <LayoutGrid className="size-5" />,
  },
];

export function LayoutSettings() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext<SplitViewFormData>();
  const currentLayout = getValues("layout");

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <LayoutGrid className="size-5" />
          {t("splitView.layoutSettings")}
        </CardTitle>
        <CardDescription>{t("splitView.layoutSettingsDescription")}</CardDescription>
      </CardHeader>
      <CardContent>
        <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
          {layouts.map((layout) => (
            <button
              key={layout.value}
              type="button"
              className={`flex items-center gap-3 p-3 rounded-lg border-2 transition-colors ${
                currentLayout === layout.value
                  ? "border-primary bg-primary/10"
                  : "border-base-300 hover:border-base-content/30"
              }`}
              onClick={() => setValue("layout", layout.value as SplitViewFormData["layout"])}
            >
              <div className={currentLayout === layout.value ? "text-primary" : "text-base-content/60"}>
                {layout.icon}
              </div>
              <span className="text-sm font-medium">{t(layout.labelKey)}</span>
            </button>
          ))}
        </div>
      </CardContent>
    </Card>
  );
}
