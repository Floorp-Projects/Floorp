// SPDX-License-Identifier: MPL-2.0

/**
 * NMA State Management
 *
 * Implements a singleton state container for the NMA system.
 * This holds the runtime state of loaded modules, verification results,
 * and configuration.
 *
 * Ported from noraneko: bridge/loader-features/loader/nma/state.ts
 * TODO(Floorp): Update NMA_PATHS.FILE_PATTERN when the archive naming
 *              convention for Floorp is decided (currently noraneko pattern).
 * TODO(Floorp): Update DEFAULT_NMA_TRUSTED_CONFIG.allowedRepositories to
 *              include the Floorp repository (e.g. "nyanrus/Floorp") before
 *              enabling signature verification in production.
 * TODO(Floorp): When porting core.ts, rename PREF_HASH_STATE from
 *              "noraneko.nma.hash_state" to "floorp.nma.hash_state" to
 *              avoid pref key collision with noraneko.
 */

import {
  NMALoaderState,
  NMATrustedConfig,
  UpdateChannel,
} from "./types.ts";

// ============================================================================
// Defaults & Constants
// ============================================================================

export const DEFAULT_NMA_TRUSTED_CONFIG: NMATrustedConfig = {
  allowedIssuers: ["https://token.actions.githubusercontent.com"],
  // TODO(Floorp): Add Floorp repository here before enabling NMA in production
  allowedRepositories: ["f3liz-dev/noraneko", "noraneko-browser/noraneko"],
  allowedWorkflows: [
    ".github/workflows/package*.yml",
    ".github/workflows/build*.yml",
    ".github/workflows/nma*.yml",
  ],
  allowUnsignedInDev: true,
};

// NMA Naming Convention: <type>_<version>_noraneko.nma.zip
// TODO(Floorp): Update FILE_PATTERN to match Floorp archive naming convention
export const NMA_PATHS = {
  FILE_PATTERN: /^([a-z0-9-]+)_([a-z0-9.-]+)_noraneko\.nma\.zip$/i,
  EXTRACTED_DIR: "noraneko-modules",
} as const;

// ============================================================================
// State Container
// ============================================================================

class StateContainer {
  loader: NMALoaderState = {
    currentNMA: null,
    nmaPath: null,
    isActive: false,
    loadedModules: [],
    lastVerification: null,
    moduleVersions: new Map(),
  };

  currentChannel: UpdateChannel = UpdateChannel.DEFAULT;

  nmaTrustedConfig: NMATrustedConfig = DEFAULT_NMA_TRUSTED_CONFIG;

  // Event listeners
  listeners: Map<string, Array<(data: unknown) => void>> = new Map();
}

/** Global singleton state instance */
export const state = new StateContainer();

// ============================================================================
// State Accessors (Getters/Setters)
// ============================================================================

export const getLoaderState = (): NMALoaderState => ({ ...state.loader });

export const resetLoaderState = (): void => {
  state.loader = {
    currentNMA: null,
    nmaPath: null,
    isActive: false,
    loadedModules: [],
    lastVerification: null,
    moduleVersions: new Map(),
  };
};

export const getNMATrustedConfig = (): NMATrustedConfig =>
  state.nmaTrustedConfig;
export const setNMATrustedConfig = (cfg: NMATrustedConfig): void => {
  state.nmaTrustedConfig = cfg;
};

// ============================================================================
// Event Management
// ============================================================================

export const isNMAActive = (): boolean => state.loader.isActive;

export const getCurrentNMAManifest = (): typeof state.loader.currentNMA =>
  state.loader.currentNMA;

export const emitEvent = (event: string, data: unknown): void => {
  const listeners = state.listeners.get(event) || [];
  for (const listener of listeners) {
    try {
      listener(data);
    } catch (e) {
      console.error(`[NMA] Event listener error for ${event}:`, e);
    }
  }
};

export const onEvent = (
  event: string,
  listener: (data: unknown) => void,
): void => {
  const listeners = state.listeners.get(event) || [];
  listeners.push(listener);
  state.listeners.set(event, listeners);
};

export const offEvent = (
  event: string,
  listener: (data: unknown) => void,
): void => {
  const listeners = state.listeners.get(event) || [];
  const index = listeners.indexOf(listener);
  if (index !== -1) {
    listeners.splice(index, 1);
    state.listeners.set(event, listeners);
  }
};
