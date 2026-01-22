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

export interface NRSettingsParentFunctions {
  getBoolPref(prefName: string): Promise<boolean | null>;
  getIntPref(prefName: string): Promise<number | null>;
  getStringPref(prefName: string): Promise<string | null>;
  setBoolPref(prefName: string, prefValue: boolean): Promise<void>;
  setIntPref(prefName: string, prefValue: number): Promise<void>;
  setStringPref(prefName: string, prefValue: string): Promise<void>;
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
