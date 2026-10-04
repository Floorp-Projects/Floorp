// SPDX-License-Identifier: MPL-2.0

export const FLOORP_IP_PROTECTION_EXPERIMENT = "floorp_ip_protection";
export const FIREFOX_IP_PROTECTION_ENABLED_PREF =
  "browser.ipProtection.enabled";
export const FIREFOX_IP_PROTECTION_BLOCK_CALLOUTS_PREF =
  "browser.ipProtection.blockIPProtectionCallouts";

type ExperimentsLike = {
  getVariant(experimentId: string): string | null;
};

type PrefsLike = {
  setBoolPref(prefName: string, value: boolean): void;
};

export function isFloorpIPProtectionVariantEnabled(
  variant: string | null,
): boolean {
  return variant === "enabled";
}

export function shouldEnableFloorpIPProtection(
  experiments: ExperimentsLike,
): boolean {
  try {
    return isFloorpIPProtectionVariantEnabled(
      experiments.getVariant(FLOORP_IP_PROTECTION_EXPERIMENT),
    );
  } catch (error) {
    console.error(
      "[FloorpIPProtectionGate] Failed to check floorp_ip_protection Flasco:",
      error,
    );
    return false;
  }
}

export function resolveFloorpIPProtectionEnabled(
  experimentEnabled: boolean,
  runtimeReady: boolean,
): boolean {
  return experimentEnabled && runtimeReady;
}

export function applyFloorpIPProtectionPref(
  prefs: PrefsLike,
  enabled: boolean,
): void {
  prefs.setBoolPref(FIREFOX_IP_PROTECTION_ENABLED_PREF, enabled);
  prefs.setBoolPref(FIREFOX_IP_PROTECTION_BLOCK_CALLOUTS_PREF, true);
}

export const FloorpIPProtectionGate = {
  isEnabled(): boolean {
    const { Experiments } = ChromeUtils.importESModule(
      "resource://noraneko/modules/experiments/Experiments.sys.mjs",
    );
    return shouldEnableFloorpIPProtection(Experiments);
  },

  apply(runtimeReady = true): boolean {
    const enabled = resolveFloorpIPProtectionEnabled(
      runtimeReady ? this.isEnabled() : false,
      runtimeReady,
    );
    if (!runtimeReady) {
      console.error(
        "[FloorpIPProtectionGate] Runtime adapter was not ready; disabling IP Protection.",
      );
    }
    applyFloorpIPProtectionPref(Services.prefs, enabled);
    return enabled;
  },
} as const;
