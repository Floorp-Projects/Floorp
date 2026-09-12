// SPDX-License-Identifier: MPL-2.0
import type { Experiment } from "#modules/modules/experiments/Experiments.sys.mts";

export interface PrefGetParams {
  prefName: string;
  prefType: "string" | "boolean" | "number";
}

export interface PrefSetParams {
  prefName: string;
  prefType: "string" | "boolean" | "number";
  prefValue: string | boolean | number;
}

export interface ActiveExperiment {
  id: string;
  variantId: string;
  variantName: string;
  assignedAt: string | null;
  experimentData: Experiment;
  disabled: boolean;
}

export interface AvailableExperiment {
  id: string;
  name: string | undefined;
  description: string | undefined;
  rollout: number;
  start: string | undefined;
  end: string | undefined;
  isActive: boolean;
  enrollmentStatus:
    | "enrolled"
    | "not_in_rollout"
    | "force_enrolled"
    | "disabled"
    | "control";
  currentVariantId: string | null;
  experimentData: Experiment;
}

export interface ActiveTabInfo {
  title: string;
  url: string;
}

export interface NRSettingsParentFunctions {
  getBoolPref(prefName: string): Promise<boolean | null>;
  getIntPref(prefName: string): Promise<number | null>;
  getStringPref(prefName: string): Promise<string | null>;
  setBoolPref(prefName: string, prefValue: boolean): Promise<void>;
  setIntPref(prefName: string, prefValue: number): Promise<void>;
  setStringPref(prefName: string, prefValue: string): Promise<void>;
}

/**
 * What the Clips page needs from the parent process: the tab it should offer
 * to clip, and the few OS-side things a clip can point at.
 */
export interface NRClipsParentFunctions {
  /** The active tab of the browser window this page belongs to. */
  getActiveTabInfo(): Promise<ActiveTabInfo | null>;
  /** Open a web URL in a normal tab. Non-web schemes are refused. */
  openLinkInTab(url: string): Promise<void>;
  /** The plain text on the system clipboard, for clipboard-history mode. */
  readClipboardText(): Promise<string | null>;
  /** Whether the path a clip remembers still points at something. */
  fileExists(path: string): Promise<boolean>;
  /** Show the file in the OS file manager, selected. */
  revealFile(path: string): Promise<boolean>;
  /** Open the file with whatever the OS uses for it. */
  launchFile(path: string): Promise<boolean>;
  /** When this browser session started; used by "clear on exit". */
  getSessionStartTime(): Promise<number>;
}

export interface NRExperimemmtParentFunctions {
  getActiveExperiments(): Promise<ActiveExperiment[]>;
  getAllExperiments(): Promise<AvailableExperiment[]>;
  disableExperiment(
    experimentId: string,
  ): Promise<{ success: boolean; error?: string }>;
  enableExperiment(
    experimentId: string,
  ): Promise<{ success: boolean; error?: string }>;
  forceEnrollExperiment(
    experimentId: string,
  ): Promise<{ success: boolean; error?: string }>;
  removeForceEnrollment(
    experimentId: string,
  ): Promise<{ success: boolean; error?: string }>;
  clearExperimentCache(): Promise<{ success: boolean; error?: string }>;
  reinitializeExperiments(): Promise<{ success: boolean; error?: string }>;
}
