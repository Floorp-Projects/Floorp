// SPDX-License-Identifier: MPL-2.0

/**
 * Clips rides a Flasco. This gate runs at startup, reads the assignment, and
 * leaves the answer in a plain preference. Everything else — the sidebar
 * panel list, the static panel table, the gesture/palette actions, the
 * settings entry — reads that pref when it loads, so the feature appears or
 * disappears as a whole on the next start, never half-way through a run.
 *
 * The experiment is defined on the Flasco manifest
 * (`floorp.experiments.manifestUrl`), roughly:
 *
 *   { "id": "floorp_clips", "rollout": 10,
 *     "variants": [ { "id": "enabled", "weight": 1 },
 *                   { "id": "control", "weight": 0 } ] }
 *
 * Without an assignment the pref is left alone, so a tester can still flip
 * `floorp.browser.clips.enabled` by hand.
 */

export const FLOORP_CLIPS_EXPERIMENT = "floorp_clips";
export const FLOORP_CLIPS_ENABLED_PREF = "floorp.browser.clips.enabled";

type ExperimentsLike = {
  getVariant(experimentId: string): string | null;
};

type PrefsLike = {
  getBoolPref(prefName: string, defaultValue: boolean): boolean;
  setBoolPref(prefName: string, value: boolean): void;
};

export function isFloorpClipsVariantEnabled(variant: string | null): boolean {
  return variant === "enabled";
}

/**
 * The pref's next value. No assignment means the Flasco has nothing to say,
 * so the current value stands.
 */
export function resolveFloorpClipsEnabled(
  variant: string | null,
  current: boolean,
): boolean {
  if (variant === null) return current;
  return isFloorpClipsVariantEnabled(variant);
}

export function readFloorpClipsVariant(
  experiments: ExperimentsLike,
): string | null {
  try {
    return experiments.getVariant(FLOORP_CLIPS_EXPERIMENT);
  } catch (error) {
    console.error("[FloorpClipsGate] Failed to check floorp_clips Flasco:", error);
    return null;
  }
}

export function applyFloorpClipsPref(prefs: PrefsLike, enabled: boolean): void {
  prefs.setBoolPref(FLOORP_CLIPS_ENABLED_PREF, enabled);
}

export const FloorpClipsGate = {
  apply(): boolean {
    const { Experiments } = ChromeUtils.importESModule(
      "resource://noraneko/modules/experiments/Experiments.sys.mjs",
    );
    const enabled = resolveFloorpClipsEnabled(
      readFloorpClipsVariant(Experiments),
      Services.prefs.getBoolPref(FLOORP_CLIPS_ENABLED_PREF, false),
    );
    applyFloorpClipsPref(Services.prefs, enabled);
    return enabled;
  },
} as const;
