// SPDX-License-Identifier: MPL-2.0

export interface ContextMenuExperimentState {
  id: string;
  isActive: boolean;
  currentVariantId: string | null;
  enrollmentStatus: string;
  start?: string;
  end?: string;
}

export interface ContextMenuExperiments {
  readonly manifestAvailable: boolean;
  readonly initializing: boolean;
  getAllExperiments(): ContextMenuExperimentState[];
  ensureActiveAssignment(experimentId: string): boolean;
  subscribe(listener: () => void): () => void;
}

export interface ContextMenuRuntimePreferences {
  getBoolPref(name: string, fallback: boolean): boolean;
  getStringPref(name: string, fallback: string): string;
  addObserver(name: string, observer: nsIObserver): void;
  removeObserver(name: string, observer: nsIObserver): void;
}

export interface ContextMenuRuntimeController {
  attach(): void;
  destroy(): void;
}

export interface ContextMenuRuntimeOptions {
  window: Window;
  experiments?: ContextMenuExperiments;
  preferences?: ContextMenuRuntimePreferences;
  createController?: () => ContextMenuRuntimeController;
}
