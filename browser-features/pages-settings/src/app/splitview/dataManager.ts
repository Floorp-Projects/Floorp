import { useCallback, useEffect, useState } from "react";
import { rpc } from "../../lib/rpc/rpc.ts";
import type { SplitViewFormData } from "@/types/pref.ts";
import {
  createDefaultSplitViewSettings,
  defaultFlexRatiosForMaxPanes,
  isValidSplitViewFormData,
  parseSplitViewConfig,
  parseSplitViewPaneSizes,
  PREF_BROWSER_SPLIT_VIEW_ENABLED,
  PREF_SPLIT_VIEW_CONFIG,
  PREF_SPLIT_VIEW_ENABLED,
  PREF_SPLIT_VIEW_PANE_SIZES,
  resolveEnabledFromPrefs,
} from "./splitViewSettingsLogic.ts";

export {
  createDefaultSplitViewSettings,
  defaultFlexRatiosForMaxPanes,
  isValidSplitViewFormData,
  parseSplitViewConfig,
  parseSplitViewPaneSizes,
  PREF_BROWSER_SPLIT_VIEW_ENABLED,
  PREF_SPLIT_VIEW_CONFIG,
  PREF_SPLIT_VIEW_ENABLED,
  PREF_SPLIT_VIEW_PANE_SIZES,
  resolveEnabledFromPrefs,
} from "./splitViewSettingsLogic.ts";

export async function saveSplitViewSettings(
  data: Partial<SplitViewFormData>,
): Promise<boolean> {
  if (!isValidSplitViewFormData(data)) {
    return false;
  }

  const { enabled, layout, maxPanes, flexRatios, gridColRatio, gridRowRatio } =
    data;

  try {
    await Promise.all([
      rpc.setBoolPref(PREF_SPLIT_VIEW_ENABLED, enabled),
      rpc.setBoolPref(PREF_BROWSER_SPLIT_VIEW_ENABLED, enabled),
      rpc.setStringPref(
        PREF_SPLIT_VIEW_CONFIG,
        JSON.stringify({ layout, maxPanes }),
      ),
      rpc.setStringPref(
        PREF_SPLIT_VIEW_PANE_SIZES,
        JSON.stringify({ flexRatios, gridColRatio, gridRowRatio }),
      ),
    ]);
    return true;
  } catch (error) {
    console.error("[SplitViewSettings] Failed to save settings", error);
    return false;
  }
}

export async function getSplitViewSettings(): Promise<SplitViewFormData> {
  const [floorpEnabled, browserEnabled, configStr, paneSizesStr] = await Promise
    .all([
      rpc.getBoolPref(PREF_SPLIT_VIEW_ENABLED),
      rpc.getBoolPref(PREF_BROWSER_SPLIT_VIEW_ENABLED),
      rpc.getStringPref(PREF_SPLIT_VIEW_CONFIG),
      rpc.getStringPref(PREF_SPLIT_VIEW_PANE_SIZES),
    ]);

  const enabled = resolveEnabledFromPrefs(floorpEnabled, browserEnabled);

  return {
    enabled,
    ...parseSplitViewConfig(configStr),
    ...parseSplitViewPaneSizes(paneSizesStr),
  };
}

export function useSplitViewSettings() {
  const [settings, setSettings] = useState<SplitViewFormData>(
    createDefaultSplitViewSettings(),
  );
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const loadSettings = async () => {
      try {
        setLoading(true);
        const values = await getSplitViewSettings();
        setSettings(values);

        const [floorpEnabled, browserEnabled] = await Promise.all([
          rpc.getBoolPref(PREF_SPLIT_VIEW_ENABLED),
          rpc.getBoolPref(PREF_BROWSER_SPLIT_VIEW_ENABLED),
        ]);
        if (
          floorpEnabled !== values.enabled ||
          browserEnabled !== values.enabled
        ) {
          await saveSplitViewSettings(values);
        }
      } catch (error) {
        console.error("[SplitViewSettings] Failed to load settings", error);
      } finally {
        setLoading(false);
      }
    };

    loadSettings();
  }, []);

  const updateSettings = useCallback(
    async (partial: Partial<SplitViewFormData>) => {
      try {
        const next = { ...settings, ...partial };
        if (
          partial.maxPanes !== undefined &&
          partial.maxPanes !== settings.maxPanes &&
          partial.flexRatios === undefined
        ) {
          next.flexRatios = defaultFlexRatiosForMaxPanes(partial.maxPanes);
        }
        const saved = await saveSplitViewSettings(next);
        if (saved) {
          setSettings(next);
        }
        return saved;
      } catch (error) {
        console.error("[SplitViewSettings] Failed to update settings", error);
        return false;
      }
    },
    [settings],
  );

  const toggleEnabled = useCallback(() => {
    return updateSettings({ enabled: !settings.enabled });
  }, [settings.enabled, updateSettings]);

  return {
    settings,
    loading,
    updateSettings,
    toggleEnabled,
  };
}
