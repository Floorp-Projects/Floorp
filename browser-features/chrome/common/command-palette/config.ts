// SPDX-License-Identifier: MPL-2.0

import { signal } from "@preact/signals";
import type { Signal } from "@preact/signals";
import {
  createRootHMR,
  rootEffect,
  addDisposer,
} from "@nora/preact-xul/lifetime";

export const COMMAND_PALETTE_ENABLED_PREF = "floorp.commandPalette.enabled";
export const COMMAND_PALETTE_RECENT_PREF = "floorp.commandPalette.recentCommands";
export const COMMAND_PALETTE_FREQUENCY_PREF = "floorp.commandPalette.commandFrequency";

export interface CommandPaletteConfig {
  enabled: boolean;
  recentCommands: string[];
  maxRecentCommands: number;
}

export const defaultConfig: CommandPaletteConfig = {
  enabled: true,
  recentCommands: [],
  maxRecentCommands: 10,
};

const parseRecentCommands = (jsonStr: string): string[] => {
  try {
    const parsed = JSON.parse(jsonStr);
    if (
      Array.isArray(parsed) &&
      parsed.every((el): el is string => typeof el === "string")
    ) {
      return parsed;
    }
  } catch {
    // ignore
  }
  return [];
};

function createEnabledSignal(): Signal<boolean> {
  const sig = signal(
    Services.prefs.getBoolPref(
      COMMAND_PALETTE_ENABLED_PREF,
      defaultConfig.enabled,
    ),
  );

  rootEffect(() => {
    try {
      Services.prefs.setBoolPref(COMMAND_PALETTE_ENABLED_PREF, sig.value);
    } catch (e) {
      console.error("[command-palette] Failed to persist enabled pref", e);
    }
  });

  const enabledObserver = () => {
    sig.value = Services.prefs.getBoolPref(
      COMMAND_PALETTE_ENABLED_PREF,
      defaultConfig.enabled,
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_ENABLED_PREF, enabledObserver);
  addDisposer(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_ENABLED_PREF,
      enabledObserver,
    );
  });

  return sig;
}

function createRecentCommandsSignal(): Signal<string[]> {
  const sig = signal(
    parseRecentCommands(
      Services.prefs.getStringPref(COMMAND_PALETTE_RECENT_PREF, "[]"),
    ),
  );

  rootEffect(() => {
    try {
      Services.prefs.setStringPref(
        COMMAND_PALETTE_RECENT_PREF,
        JSON.stringify(sig.value),
      );
    } catch (e) {
      console.error("[command-palette] Failed to persist recent commands", e);
    }
  });

  const recentObserver = () => {
    sig.value = parseRecentCommands(
      Services.prefs.getStringPref(COMMAND_PALETTE_RECENT_PREF, "[]"),
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_RECENT_PREF, recentObserver);
  addDisposer(() => {
    Services.prefs.removeObserver(COMMAND_PALETTE_RECENT_PREF, recentObserver);
  });

  return sig;
}

export const _enabled = createRootHMR(createEnabledSignal, import.meta.hot);
export const _setEnabled = (value: boolean) => { _enabled.value = value; };
export const _recentCommands = createRootHMR(
  createRecentCommandsSignal,
  import.meta.hot,
);
export const _setRecentCommands = (value: string[]) => {
  _recentCommands.value = value;
};

export const isEnabled = () => _enabled.value;
export const setEnabled = (value: boolean) => { _enabled.value = value; };
export const getRecentCommands = () => _recentCommands.value;

export function addRecentCommand(id: string) {
  const current = _recentCommands.value.filter((c) => c !== id);
  current.unshift(id);
  _recentCommands.value = current.slice(0, defaultConfig.maxRecentCommands);
}

const parseFrequency = (jsonStr: string): Record<string, number> => {
  try {
    const parsed = JSON.parse(jsonStr);
    if (typeof parsed === "object" && parsed !== null) {
      return parsed as Record<string, number>;
    }
  } catch {
    // ignore
  }
  return {};
};

function createFrequencySignal(): Signal<Record<string, number>> {
  const sig = signal(
    parseFrequency(
      Services.prefs.getStringPref(COMMAND_PALETTE_FREQUENCY_PREF, "{}"),
    ),
  );

  rootEffect(() => {
    try {
      Services.prefs.setStringPref(
        COMMAND_PALETTE_FREQUENCY_PREF,
        JSON.stringify(sig.value),
      );
    } catch (e) {
      console.error("[command-palette] Failed to persist frequency", e);
    }
  });

  const freqObserver = () => {
    sig.value = parseFrequency(
      Services.prefs.getStringPref(COMMAND_PALETTE_FREQUENCY_PREF, "{}"),
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_FREQUENCY_PREF, freqObserver);
  addDisposer(() => {
    Services.prefs.removeObserver(COMMAND_PALETTE_FREQUENCY_PREF, freqObserver);
  });

  return sig;
}

export const _frequency = createRootHMR(createFrequencySignal, import.meta.hot);
export const _setFrequency = (value: Record<string, number>) => {
  _frequency.value = value;
};

export const getFrequencies = () => _frequency.value;

export function incrementFrequency(id: string) {
  const current = { ..._frequency.value };
  current[id] = (current[id] ?? 0) + 1;
  _frequency.value = current;
}
