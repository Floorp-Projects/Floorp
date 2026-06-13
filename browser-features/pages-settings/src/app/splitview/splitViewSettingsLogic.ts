import type { SplitViewFormData, SplitViewLayout } from "@/types/pref.ts";

export const PREF_SPLIT_VIEW_ENABLED = "floorp.splitView.enabled";
export const PREF_BROWSER_SPLIT_VIEW_ENABLED = "browser.tabs.splitView.enabled";
export const PREF_SPLIT_VIEW_CONFIG = "floorp.splitView.config";
export const PREF_SPLIT_VIEW_PANE_SIZES = "floorp.splitView.paneSizes";

export const DEFAULT_CONFIG = {
  layout: "horizontal" as const,
  maxPanes: 4,
};

export const DEFAULT_PANE_SIZES = {
  flexRatios: [0.5, 0.5],
  gridColRatio: 0.5,
  gridRowRatio: 0.5,
};

const VALID_LAYOUTS = new Set<SplitViewLayout>([
  "horizontal",
  "vertical",
  "grid-2x2",
  "grid-3pane-left-main",
  "grid-3pane-right-main",
  "grid-3pane-top-main",
  "grid-3pane-bottom-main",
]);

export function isValidSplitViewFormData(
  data: Partial<SplitViewFormData>,
): data is SplitViewFormData {
  if (typeof data.enabled !== "boolean") return false;
  if (!data.layout || !VALID_LAYOUTS.has(data.layout)) return false;
  if (
    typeof data.maxPanes !== "number" ||
    !Number.isInteger(data.maxPanes) ||
    data.maxPanes < 2 ||
    data.maxPanes > 4
  ) {
    return false;
  }
  if (
    !Array.isArray(data.flexRatios) ||
    data.flexRatios.length === 0 ||
    !data.flexRatios.every((ratio) => typeof ratio === "number")
  ) {
    return false;
  }
  if (
    typeof data.gridColRatio !== "number" || !Number.isFinite(data.gridColRatio)
  ) {
    return false;
  }
  if (
    typeof data.gridRowRatio !== "number" || !Number.isFinite(data.gridRowRatio)
  ) {
    return false;
  }
  return true;
}

export function resolveEnabledFromPrefs(
  floorpEnabled: boolean | null,
  browserEnabled: boolean | null,
): boolean {
  const floorp = floorpEnabled ?? true;
  const browser = browserEnabled ?? false;
  return floorp && browser;
}

export function parseSplitViewConfig(
  configStr: string | null,
): Pick<SplitViewFormData, "layout" | "maxPanes"> {
  let config = { ...DEFAULT_CONFIG };
  try {
    const raw = JSON.parse(configStr || "{}");
    if (VALID_LAYOUTS.has(raw.layout)) {
      config.layout = raw.layout;
    }
    if (
      typeof raw.maxPanes === "number" &&
      Number.isInteger(raw.maxPanes) &&
      raw.maxPanes >= 2 &&
      raw.maxPanes <= 4
    ) {
      config.maxPanes = raw.maxPanes;
    }
  } catch {
    // use defaults
  }
  return config;
}

export function parseSplitViewPaneSizes(
  paneSizesStr: string | null,
): Pick<SplitViewFormData, "flexRatios" | "gridColRatio" | "gridRowRatio"> {
  let paneSizes = { ...DEFAULT_PANE_SIZES };
  try {
    const raw = JSON.parse(paneSizesStr || "{}");
    if (Array.isArray(raw.flexRatios) && raw.flexRatios.length > 0) {
      paneSizes.flexRatios = raw.flexRatios;
    }
    if (typeof raw.gridColRatio === "number") {
      paneSizes.gridColRatio = raw.gridColRatio;
    }
    if (typeof raw.gridRowRatio === "number") {
      paneSizes.gridRowRatio = raw.gridRowRatio;
    }
  } catch {
    // use defaults
  }
  return paneSizes;
}

export function defaultFlexRatiosForMaxPanes(maxPanes: number): number[] {
  const ratio = 1 / maxPanes;
  return Array.from({ length: maxPanes }, () => ratio);
}

export function createDefaultSplitViewSettings(): SplitViewFormData {
  return {
    enabled: true,
    ...DEFAULT_CONFIG,
    ...DEFAULT_PANE_SIZES,
  };
}
