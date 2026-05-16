import { rpc } from "../../lib/rpc/rpc.ts";
import type { SplitViewFormData } from "@/types/pref.ts";

export const DEFAULT_CONFIG = {
  layout: "horizontal" as const,
  maxPanes: 4,
};

export const DEFAULT_PANE_SIZES = {
  flexRatios: [0.5, 0.5],
  gridColRatio: 0.5,
  gridRowRatio: 0.5,
};

const VALID_LAYOUTS = new Set([
  "horizontal",
  "vertical",
  "grid-2x2",
  "grid-3pane-left-main",
  "grid-3pane-right-main",
  "grid-3pane-top-main",
  "grid-3pane-bottom-main",
]);

export async function saveSplitViewSettings(
  data: SplitViewFormData,
): Promise<void> {
  if (Object.keys(data).length === 0) return;

  const { enabled, ...configData } = data;

  await Promise.all([
    rpc.setBoolPref("floorp.splitView.enabled", enabled),
    rpc.setStringPref(
      "floorp.splitView.config",
      JSON.stringify({
        layout: configData.layout,
        maxPanes: configData.maxPanes,
      }),
    ),
    rpc.setStringPref(
      "floorp.splitView.paneSizes",
      JSON.stringify({
        flexRatios: configData.flexRatios,
        gridColRatio: configData.gridColRatio,
        gridRowRatio: configData.gridRowRatio,
      }),
    ),
  ]);
}

export async function getSplitViewSettings(): Promise<SplitViewFormData> {
  const [enabled, configStr, paneSizesStr] = await Promise.all([
    rpc.getBoolPref("floorp.splitView.enabled"),
    rpc.getStringPref("floorp.splitView.config"),
    rpc.getStringPref("floorp.splitView.paneSizes"),
  ]);

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

  return {
    enabled: enabled ?? true,
    ...config,
    ...paneSizes,
  };
}
