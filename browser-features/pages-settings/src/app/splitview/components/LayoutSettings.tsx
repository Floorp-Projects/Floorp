import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
  CardDescription,
} from "@/components/common/card.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext, useWatch } from "react-hook-form";
import type { SplitViewFormData } from "@/types/pref.ts";

type LayoutOption = {
  value: string;
  labelKey: string;
  preview: React.ReactNode;
};

const layouts: LayoutOption[] = [
  {
    value: "horizontal",
    labelKey: "splitView.layoutHorizontal",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect x="1" y="1" width="18" height="26" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
        <rect x="21" y="1" width="18" height="26" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
      </svg>
    ),
  },
  {
    value: "vertical",
    labelKey: "splitView.layoutVertical",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect x="1" y="1" width="38" height="12" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
        <rect x="1" y="15" width="38" height="12" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
      </svg>
    ),
  },
  {
    value: "grid-2x2",
    labelKey: "splitView.layoutGrid2x2",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect x="1" y="1" width="18" height="12" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
        <rect x="21" y="1" width="18" height="12" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
        <rect x="1" y="15" width="18" height="12" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
        <rect x="21" y="15" width="18" height="12" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
      </svg>
    ),
  },
  {
    value: "grid-3pane-left-main",
    labelKey: "splitView.layout3PaneLeft",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect x="1" y="1" width="22" height="26" rx="2" fill="currentColor" opacity="0.25" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
        <rect x="25" y="1" width="14" height="12" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
        <rect x="25" y="15" width="14" height="12" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
      </svg>
    ),
  },
  {
    value: "grid-3pane-right-main",
    labelKey: "splitView.layout3PaneRight",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect x="1" y="1" width="14" height="12" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
        <rect x="1" y="15" width="14" height="12" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
        <rect x="17" y="1" width="22" height="26" rx="2" fill="currentColor" opacity="0.25" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
      </svg>
    ),
  },
  {
    value: "grid-3pane-top-main",
    labelKey: "splitView.layout3PaneTop",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect x="1" y="1" width="38" height="14" rx="2" fill="currentColor" opacity="0.25" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
        <rect x="1" y="17" width="18" height="10" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
        <rect x="21" y="17" width="18" height="10" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
      </svg>
    ),
  },
  {
    value: "grid-3pane-bottom-main",
    labelKey: "splitView.layout3PaneBottom",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect x="1" y="1" width="18" height="10" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
        <rect x="21" y="1" width="18" height="10" rx="2" fill="currentColor" opacity="0.15" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
        <rect x="1" y="13" width="38" height="14" rx="2" fill="currentColor" opacity="0.25" stroke="currentColor" strokeWidth="1.5" opacity="0.4" />
      </svg>
    ),
  },
];

export function LayoutSettings() {
  const { t } = useTranslation();
  const { setValue } = useFormContext<SplitViewFormData>();
  const currentLayout = useWatch<SplitViewFormData>({ name: "layout" });

  return (
    <Card>
      <CardHeader>
        <CardTitle>{t("splitView.layoutSettings")}</CardTitle>
        <CardDescription>{t("splitView.layoutSettingsDescription")}</CardDescription>
      </CardHeader>
      <CardContent>
        <div className="grid grid-cols-2 sm:grid-cols-3 gap-3">
          {layouts.map((layout) => (
            <button
              key={layout.value}
              type="button"
              className={`flex flex-col items-center gap-2 p-3 rounded-lg border-2 transition-colors ${
                currentLayout === layout.value
                  ? "border-primary bg-primary/10"
                  : "border-base-300 hover:border-base-content/30"
              }`}
              onClick={() => setValue("layout", layout.value as SplitViewFormData["layout"])}
            >
              <div className={currentLayout === layout.value ? "text-primary" : "text-base-content/60"}>
                {layout.preview}
              </div>
              <span className="text-xs font-medium">{t(layout.labelKey)}</span>
            </button>
          ))}
        </div>
      </CardContent>
    </Card>
  );
}
