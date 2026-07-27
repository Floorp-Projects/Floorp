// SPDX-License-Identifier: MPL-2.0

const PWA_CONTAINER_EXPERIMENT = "pwa_container_support";

const readPwaContainerExperimentAssignment = (): boolean => {
  try {
    const { Experiments } = ChromeUtils.importESModule(
      "resource://noraneko/modules/experiments/Experiments.sys.mjs",
    );
    const variant = Experiments.getVariant(PWA_CONTAINER_EXPERIMENT);
    return variant !== null;
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
