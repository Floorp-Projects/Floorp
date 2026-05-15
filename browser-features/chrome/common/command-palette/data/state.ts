// SPDX-License-Identifier: MPL-2.0

import { createSignal } from "solid-js";
import type { PaletteCommand } from "../command-registry.ts";

export type PaletteMode = "command" | "input";

export function createPaletteState() {
  const [isVisible, setIsVisible] = createSignal(false);
  const [isAnimatingOut, setIsAnimatingOut] = createSignal(false);
  const [query, setQuery] = createSignal("");
  const [selectedIndex, setSelectedIndex] = createSignal(0);
  const [filteredCommands, setFilteredCommands] = createSignal<
    PaletteCommand[]
  >([]);

  // Multi-step input state
  const [mode, setMode] = createSignal<PaletteMode>("command");
  const [activeCommand, setActiveCommand] = createSignal<PaletteCommand | null>(
    null,
  );
  const [currentStepIndex, setCurrentStepIndex] = createSignal(0);
  const [stepInputs, setStepInputs] = createSignal<Record<string, string>>({});
  const [stepError, setStepError] = createSignal<string | null>(null);

  return {
    isVisible,
    setIsVisible,
    isAnimatingOut,
    setIsAnimatingOut,
    query,
    setQuery,
    selectedIndex,
    setSelectedIndex,
    filteredCommands,
    setFilteredCommands,
    mode,
    setMode,
    activeCommand,
    setActiveCommand,
    currentStepIndex,
    setCurrentStepIndex,
    stepInputs,
    setStepInputs,
    stepError,
    setStepError,
    reset() {
      setQuery("");
      setSelectedIndex(0);
      setFilteredCommands([]);
      setMode("command");
      setActiveCommand(null);
      setCurrentStepIndex(0);
      setStepInputs({});
      setStepError(null);
    },
  };
}

export type PaletteState = ReturnType<typeof createPaletteState>;
