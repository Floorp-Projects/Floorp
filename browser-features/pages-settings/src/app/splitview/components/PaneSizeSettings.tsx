import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Seekbar } from "@/components/common/seekbar.tsx";
import { useTranslation } from "react-i18next";
import type { SplitViewFormData, SplitViewLayout } from "@/types/pref.ts";

interface PaneSizeSettingsProps {
  settings: SplitViewFormData;
  onPaneSizesChange: (
    paneSizes: Pick<
      SplitViewFormData,
      "flexRatios" | "gridColRatio" | "gridRowRatio"
    >,
  ) => Promise<boolean>;
}

function isFlexLayout(layout: SplitViewLayout): boolean {
  return layout === "horizontal" || layout === "vertical";
}

function isGridLayout(layout: SplitViewLayout): boolean {
  return layout.startsWith("grid");
}

function normalizeFlexRatios(
  maxPanes: number,
  rawValues: number[],
): number[] {
  if (maxPanes === 2) {
    const first = clampRatio(rawValues[0] ?? 0.5);
    return [first, 1 - first];
  }

  if (maxPanes === 3) {
    const first = clampRatio(rawValues[0] ?? 0.33);
    const second = clampRatio(rawValues[1] ?? 0.33);
    const third = Math.max(0.05, 1 - first - second);
    const sum = first + second + third;
    return [first / sum, second / sum, third / sum];
  }

  const first = clampRatio(rawValues[0] ?? 0.25);
  const second = clampRatio(rawValues[1] ?? 0.25);
  const third = clampRatio(rawValues[2] ?? 0.25);
  const fourth = Math.max(0.05, 1 - first - second - third);
  const sum = first + second + third + fourth;
  return [first / sum, second / sum, third / sum, fourth / sum];
}

function clampRatio(value: number): number {
  return Math.max(0.1, Math.min(0.8, value));
}

function toPercent(ratio: number): number {
  return Math.round(ratio * 100);
}

function fromPercent(percent: number): number {
  return percent / 100;
}

export function PaneSizeSettings({
  settings,
  onPaneSizesChange,
}: PaneSizeSettingsProps) {
  const { t } = useTranslation();
  const disabled = !settings.enabled;
  const showFlex = isFlexLayout(settings.layout);
  const showGrid = isGridLayout(settings.layout);

  const updateFlexAt = (index: number, percent: number) => {
    const rawValues = [...settings.flexRatios];
    rawValues[index] = fromPercent(percent);
    void onPaneSizesChange({
      flexRatios: normalizeFlexRatios(settings.maxPanes, rawValues),
      gridColRatio: settings.gridColRatio,
      gridRowRatio: settings.gridRowRatio,
    });
  };

  const updateGridCol = (percent: number) => {
    void onPaneSizesChange({
      flexRatios: settings.flexRatios,
      gridColRatio: fromPercent(percent),
      gridRowRatio: settings.gridRowRatio,
    });
  };

  const updateGridRow = (percent: number) => {
    void onPaneSizesChange({
      flexRatios: settings.flexRatios,
      gridColRatio: settings.gridColRatio,
      gridRowRatio: fromPercent(percent),
    });
  };

  const flexSliders = showFlex
    ? Array.from(
      { length: Math.max(1, settings.maxPanes - 1) },
      (_, index) => ({
        index,
        value: toPercent(settings.flexRatios[index] ?? 0.5),
        label: settings.maxPanes === 2
          ? t("splitView.flexRatio")
          : t("splitView.flexRatioPane", { pane: index + 1 }),
      }),
    )
    : [];

  return (
    <Card className={disabled ? "opacity-60" : undefined}>
      <CardHeader>
        <CardTitle>{t("splitView.sizeSettings")}</CardTitle>
        <CardDescription>
          {t("splitView.sizeSettingsDescription")}
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-6">
        {showFlex && (
          <div className="space-y-4">
            {flexSliders.map((slider) => (
              <Seekbar
                key={slider.index}
                label={slider.label}
                min={10}
                max={80}
                step={5}
                value={slider.value}
                valueSuffix="%"
                minLabel="10%"
                maxLabel="80%"
                disabled={disabled}
                onChange={(event) => {
                  updateFlexAt(slider.index, Number(event.target.value));
                }}
              />
            ))}
          </div>
        )}

        {showGrid && (
          <div className="space-y-4">
            <Seekbar
              label={t("splitView.gridColRatio")}
              min={10}
              max={90}
              step={5}
              value={toPercent(settings.gridColRatio)}
              valueSuffix="%"
              minLabel="10%"
              maxLabel="90%"
              disabled={disabled}
              onChange={(event) => {
                updateGridCol(Number(event.target.value));
              }}
            />
            <Seekbar
              label={t("splitView.gridRowRatio")}
              min={10}
              max={90}
              step={5}
              value={toPercent(settings.gridRowRatio)}
              valueSuffix="%"
              minLabel="10%"
              maxLabel="90%"
              disabled={disabled}
              onChange={(event) => {
                updateGridRow(Number(event.target.value));
              }}
            />
          </div>
        )}
      </CardContent>
    </Card>
  );
}
