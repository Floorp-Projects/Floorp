// SPDX-License-Identifier: MPL-2.0

import {
  MAC_APP_SHIM_EXPERIMENT_ID,
  MAC_APP_SHIM_PREF,
  type MacAppShimEnrollment,
  resolveMacAppShimEnabled,
} from "#libs/pwa/macAppShimExperiment.ts";

export const MacAppShimExperiment = {
  isEnabled(): boolean {
    const os = Services.appinfo.OS;
    if (os !== "Darwin") return false;

    try {
      const userPreference = Services.prefs.prefHasUserValue(MAC_APP_SHIM_PREF)
        ? Services.prefs.getBoolPref(MAC_APP_SHIM_PREF, false)
        : null;
      if (userPreference !== null) {
        return resolveMacAppShimEnabled(os, userPreference, null);
      }

      const { Experiments } = ChromeUtils.importESModule(
        "resource://noraneko/modules/experiments/Experiments.sys.mjs",
      ) as {
        Experiments: { getAllExperiments(): MacAppShimEnrollment[] };
      };
      const enrollment = Experiments.getAllExperiments().find(
        (experiment) => experiment.id === MAC_APP_SHIM_EXPERIMENT_ID,
      ) ?? null;
      return resolveMacAppShimEnabled(os, null, enrollment);
    } catch (error) {
      console.error(
        "[MacAppShimExperiment] Failed to check pwa_mac_app_shim Flasco:",
        error,
      );
      return false;
    }
  },
} as const;
