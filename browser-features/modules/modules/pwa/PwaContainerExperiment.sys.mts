// SPDX-License-Identifier: MPL-2.0

const PWA_CONTAINER_EXPERIMENT = "pwa_container_support";

const readPwaContainerExperimentAssignment = (): boolean => {
  try {
    const { Experiments } = ChromeUtils.importESModule(
      "resource://noraneko/modules/experiments/Experiments.sys.mjs",
    );
    // getCachedEnrollment reads the persisted assignment directly, so this is
    // correct even before Experiments.init() completes during startup. A user
    // who disabled the experiment or opted out of participation must not get
    // container behavior even when their assigned variant is "enabled".
    const enrollment = Experiments.getCachedEnrollment(
      PWA_CONTAINER_EXPERIMENT,
    );
    return enrollment.variantId === "enabled" && !enrollment.disabled &&
      !enrollment.optedOut;
  } catch (error) {
    console.error(
      "[PwaContainerExperiment] Failed to check pwa_container_support experiment:",
      error,
    );
    return false;
  }
};

export const PwaContainerExperiment = {
  /**
   * Returns true when the user is enrolled in the pwa_container_support
   * experiment and PWA container assignment features should run.
   */
  isEnabled(): boolean {
    return readPwaContainerExperimentAssignment();
  },
} as const;
