// SPDX-License-Identifier: MPL-2.0

export const MAC_APP_SHIM_EXPERIMENT_ID = "pwa_mac_app_shim";
export const MAC_APP_SHIM_PREF = "floorp.browser.nativeApp.appShim.enabled";

export interface MacAppShimEnrollment {
  id: string;
  isActive: boolean;
  currentVariantId: string | null;
  enrollmentStatus:
    | "enrolled"
    | "force_enrolled"
    | "not_in_rollout"
    | "disabled"
    | "control";
}

export interface MacAppShimCachedEnrollment {
  variantId: string | null;
  disabled: boolean;
  optedOut: boolean;
}

export function resolveMacAppShimEnabled(
  os: string,
  userPreference: boolean | null,
  enrollment: MacAppShimEnrollment | null,
  cachedEnrollment: MacAppShimCachedEnrollment | null = null,
): boolean {
  if (os !== "Darwin") return false;
  if (userPreference !== null) return userPreference;
  if (!enrollment) {
    return cachedEnrollment?.variantId === "enabled" &&
      !cachedEnrollment.disabled && !cachedEnrollment.optedOut;
  }
  return enrollment?.isActive === true &&
    enrollment.currentVariantId === "enabled" &&
    (enrollment.enrollmentStatus === "enrolled" ||
      enrollment.enrollmentStatus === "force_enrolled");
}
