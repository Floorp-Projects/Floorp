// SPDX-License-Identifier: MPL-2.0

import {
  type Accessor,
  createEffect,
  createSignal,
  onCleanup,
  type Setter,
} from "solid-js";
import { createRootHMR } from "@nora/solid-xul";

export const COMMAND_PALETTE_ENABLED_PREF = "floorp.commandPalette.enabled";
export const COMMAND_PALETTE_RECENT_PREF = "floorp.commandPalette.recentCommands";

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

function createEnabled(): [Accessor<boolean>, Setter<boolean>] {
  const [enabled, setEnabled] = createSignal(
    Services.prefs.getBoolPref(
      COMMAND_PALETTE_ENABLED_PREF,
      defaultConfig.enabled,
    ),
  );

  createEffect(() => {
    Services.prefs.setBoolPref(COMMAND_PALETTE_ENABLED_PREF, enabled());
  });

  const enabledObserver = () => {
    setEnabled(
      Services.prefs.getBoolPref(
        COMMAND_PALETTE_ENABLED_PREF,
        defaultConfig.enabled,
      ),
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_ENABLED_PREF, enabledObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_ENABLED_PREF,
      enabledObserver,
    );
  });

  return [enabled, setEnabled];
}

function createRecentCommands(): [
  Accessor<string[]>,
  Setter<string[]>,
] {
  const [recent, setRecent] = createSignal(
    parseRecentCommands(
      Services.prefs.getStringPref(
        COMMAND_PALETTE_RECENT_PREF,
        "[]",
      ),
    ),
  );

  createEffect(() => {
    Services.prefs.setStringPref(
      COMMAND_PALETTE_RECENT_PREF,
      JSON.stringify(recent()),
    );
  });

  const recentObserver = () => {
    setRecent(
      parseRecentCommands(
        Services.prefs.getStringPref(
          COMMAND_PALETTE_RECENT_PREF,
          "[]",
        ),
      ),
    );
  };

  Services.prefs.addObserver(COMMAND_PALETTE_RECENT_PREF, recentObserver);
  onCleanup(() => {
    Services.prefs.removeObserver(
      COMMAND_PALETTE_RECENT_PREF,
      recentObserver,
    );
  });

  return [recent, setRecent];
}

export const [_enabled, _setEnabled] = createRootHMR(
  createEnabled,
  import.meta.hot,
);
export const [_recentCommands, _setRecentCommands] = createRootHMR(
  createRecentCommands,
  import.meta.hot,
);

export const isEnabled = () => _enabled();
export const setEnabled = (value: boolean) => _setEnabled(value);
export const getRecentCommands = () => _recentCommands();

export function addRecentCommand(id: string) {
  const current = _recentCommands().filter((c) => c !== id);
  current.unshift(id);
  _setRecentCommands(current.slice(0, defaultConfig.maxRecentCommands));
}
