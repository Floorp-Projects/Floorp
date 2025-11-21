const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

const LINUX_TASKBAR_EXPERIMENT = "pwa_taskbar_integration_linux";

const readLinuxExperimentAssignment = (): boolean => {
  try {
    const { Experiments } = ChromeUtils.importESModule(
      "resource://noraneko/modules/experiments/Experiments.sys.mjs",
    );
    const variant = Experiments.getVariant(LINUX_TASKBAR_EXPERIMENT);
    return variant !== null;
  } catch (error) {
    console.error(
      "[TaskbarExperiment] Failed to check pwa_taskbar_integration_linux experiment:",
      error,
    );
    // If experiments system fails, default to disabled for safety
    return false;
  }
};

export const TaskbarExperiment = {
  /**
   * Returns true when taskbar integration should run on Linux.
   * This ignores the current platform and only checks the experiment state.
   */
  isEnabledForLinux(): boolean {
    return readLinuxExperimentAssignment();
  },

  /**
   * Returns true when taskbar integration should run on the current platform.
   * Linux is guarded by the experiment rollout, other platforms always allow it.
   */
  isEnabledForCurrentPlatform(): boolean {
    if (AppConstants.platform !== "linux") {
      return true;
    }
    return readLinuxExperimentAssignment();
  },
} as const;
