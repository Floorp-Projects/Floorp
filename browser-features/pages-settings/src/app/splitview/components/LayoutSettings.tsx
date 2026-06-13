import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { useTranslation } from "react-i18next";
import type { SplitViewFormData } from "@/types/pref.ts";

type LayoutOption = {
  value: SplitViewFormData["layout"];
  labelKey: string;
  preview: React.ReactNode;
};

const layouts: LayoutOption[] = [
  {
    value: "horizontal",
    labelKey: "splitView.layoutHorizontal",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect
          x="1"
          y="1"
          width="18"
          height="26"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
        <rect
          x="21"
          y="1"
          width="18"
          height="26"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
      </svg>
    ),
  },
  {
    value: "vertical",
    labelKey: "splitView.layoutVertical",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect
          x="1"
          y="1"
          width="38"
          height="12"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
        <rect
          x="1"
          y="15"
          width="38"
          height="12"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
      </svg>
    ),
  },
  {
    value: "grid-2x2",
    labelKey: "splitView.layoutGrid2x2",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect
          x="1"
          y="1"
          width="18"
          height="12"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
        <rect
          x="21"
          y="1"
          width="18"
          height="12"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
        <rect
          x="1"
          y="15"
          width="18"
          height="12"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
        <rect
          x="21"
          y="15"
          width="18"
          height="12"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
      </svg>
    ),
  },
  {
    value: "grid-3pane-left-main",
    labelKey: "splitView.layout3PaneLeft",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect
          x="1"
          y="1"
          width="22"
          height="26"
          rx="2"
          fill="currentColor"
          fillOpacity="0.25"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
        <rect
          x="25"
          y="1"
          width="14"
          height="12"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
        <rect
          x="25"
          y="15"
          width="14"
          height="12"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
      </svg>
    ),
  },
  {
    value: "grid-3pane-right-main",
    labelKey: "splitView.layout3PaneRight",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect
          x="1"
          y="1"
          width="14"
          height="12"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
        <rect
          x="1"
          y="15"
          width="14"
          height="12"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
        <rect
          x="17"
          y="1"
          width="22"
          height="26"
          rx="2"
          fill="currentColor"
          fillOpacity="0.25"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
      </svg>
    ),
  },
  {
    value: "grid-3pane-top-main",
    labelKey: "splitView.layout3PaneTop",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect
          x="1"
          y="1"
          width="38"
          height="14"
          rx="2"
          fill="currentColor"
          fillOpacity="0.25"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
        <rect
          x="1"
          y="17"
          width="18"
          height="10"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
        <rect
          x="21"
          y="17"
          width="18"
          height="10"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
      </svg>
    ),
  },
  {
    value: "grid-3pane-bottom-main",
    labelKey: "splitView.layout3PaneBottom",
    preview: (
      <svg viewBox="0 0 40 28" className="size-8">
        <rect
          x="1"
          y="1"
          width="18"
          height="10"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
        <rect
          x="21"
          y="1"
          width="18"
          height="10"
          rx="2"
          fill="currentColor"
          fillOpacity="0.15"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
        <rect
          x="1"
          y="13"
          width="38"
          height="14"
          rx="2"
          fill="currentColor"
          fillOpacity="0.25"
          stroke="currentColor"
          strokeWidth="1.5"
          opacity="0.4"
        />
      </svg>
    ),
  },
];

interface LayoutSettingsProps {
  settings: SplitViewFormData;
  onLayoutChange: (layout: SplitViewFormData["layout"]) => Promise<boolean>;
}

export function LayoutSettings({
  settings,
  onLayoutChange,
}: LayoutSettingsProps) {
  const { t } = useTranslation();

  return (
    <Card className={!settings.enabled ? "opacity-60" : undefined}>
      <CardHeader>
        <CardTitle>{t("splitView.layoutSettings")}</CardTitle>
        <CardDescription>
          {t("splitView.layoutSettingsDescription")}
        </CardDescription>
      </CardHeader>
      <CardContent>
        <div className="grid grid-cols-2 sm:grid-cols-3 gap-3">
          {layouts.map((layout) => (
            <button
              key={layout.value}
              type="button"
              disabled={!settings.enabled}
              className={`flex flex-col items-center gap-2 p-3 rounded-lg border-2 transition-colors ${
                settings.layout === layout.value
                  ? "border-primary bg-primary/10"
                  : "border-base-300 hover:border-base-content/30"
              }`}
              onClick={() => {
                void onLayoutChange(layout.value);
              }}
            >
              <div
                className={settings.layout === layout.value
                  ? "text-primary"
                  : "text-base-content/60"}
              >
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
