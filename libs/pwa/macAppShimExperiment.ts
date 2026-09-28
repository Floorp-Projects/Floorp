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

export function resolveMacAppShimEnabled(
  os: string,
  userPreference: boolean | null,
  enrollment: MacAppShimEnrollment | null,
): boolean {
  if (os !== "Darwin") return false;
  if (userPreference !== null) return userPreference;
  return enrollment?.isActive === true &&
    enrollment.currentVariantId === "enabled" &&
    (enrollment.enrollmentStatus === "enrolled" ||
      enrollment.enrollmentStatus === "force_enrolled");
}
