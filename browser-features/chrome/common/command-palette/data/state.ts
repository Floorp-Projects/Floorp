// SPDX-License-Identifier: MPL-2.0

import { signal } from "@preact/signals";
import type { PaletteCommand, CommandStepChoice } from "../types.ts";

export type PaletteMode = "command" | "input";

export function createPaletteState() {
  // Internal preact signals. Exposed via accessor functions to stay compatible
  // with controller.ts and StepIndicator.tsx, which call state.xxx() as functions.
  // Reading .value inside an accessor called during preact render IS tracked by
  // @preact/signals, so reactivity is fully preserved.
  const _isVisible = signal(false);
  const _isAnimatingOut = signal(false);
  const _query = signal("");
  const _selectedIndex = signal(0);
  const _filteredCommands = signal<PaletteCommand[]>([]);

  // Multi-step input state
  const _mode = signal<PaletteMode>("command");
  const _activeCommand = signal<PaletteCommand | null>(null);
  const _currentStepIndex = signal(0);
  const _stepInputs = signal<Record<string, string>>({});
  const _stepError = signal<string | null>(null);
  const _filteredStepChoices = signal<CommandStepChoice[]>([]);
  const _selectedChoiceIndex = signal(0);
  const _stepChoicesLoading = signal(false);
  const _stepChoicesBase = signal<CommandStepChoice[]>([]);
  const _hasMoreChoices = signal(false);
  const _loadMoreCallback = signal<
    (() => Promise<{ choices: CommandStepChoice[]; hasMore: boolean }>) | null
  >(null);
  const _loadingMore = signal(false);

  return {
    isVisible: () => _isVisible.value,
    setIsVisible: (v: boolean) => { _isVisible.value = v; },
    isAnimatingOut: () => _isAnimatingOut.value,
    setIsAnimatingOut: (v: boolean) => { _isAnimatingOut.value = v; },
    query: () => _query.value,
    setQuery: (v: string) => { _query.value = v; },
    selectedIndex: () => _selectedIndex.value,
    setSelectedIndex: (v: number) => { _selectedIndex.value = v; },
    filteredCommands: () => _filteredCommands.value,
    setFilteredCommands: (v: PaletteCommand[]) => { _filteredCommands.value = v; },
    mode: () => _mode.value,
    setMode: (v: PaletteMode) => { _mode.value = v; },
    activeCommand: () => _activeCommand.value,
    setActiveCommand: (v: PaletteCommand | null) => { _activeCommand.value = v; },
    currentStepIndex: () => _currentStepIndex.value,
    setCurrentStepIndex: (v: number) => { _currentStepIndex.value = v; },
    stepInputs: () => _stepInputs.value,
    setStepInputs: (v: Record<string, string>) => { _stepInputs.value = v; },
    stepError: () => _stepError.value,
    setStepError: (v: string | null) => { _stepError.value = v; },
    filteredStepChoices: () => _filteredStepChoices.value,
    setFilteredStepChoices: (v: CommandStepChoice[]) => { _filteredStepChoices.value = v; },
    selectedChoiceIndex: () => _selectedChoiceIndex.value,
    setSelectedChoiceIndex: (v: number) => { _selectedChoiceIndex.value = v; },
    stepChoicesLoading: () => _stepChoicesLoading.value,
    setStepChoicesLoading: (v: boolean) => { _stepChoicesLoading.value = v; },
    stepChoicesBase: () => _stepChoicesBase.value,
    setStepChoicesBase: (v: CommandStepChoice[]) => { _stepChoicesBase.value = v; },
    hasMoreChoices: () => _hasMoreChoices.value,
    setHasMoreChoices: (v: boolean) => { _hasMoreChoices.value = v; },
    loadMoreCallback: () => _loadMoreCallback.value,
    setLoadMoreCallback: (
      v: (() => Promise<{ choices: CommandStepChoice[]; hasMore: boolean }>) | null,
    ) => { _loadMoreCallback.value = v; },
    loadingMore: () => _loadingMore.value,
    setLoadingMore: (v: boolean) => { _loadingMore.value = v; },
    reset() {
      _query.value = "";
      _selectedIndex.value = 0;
      _filteredCommands.value = [];
      _mode.value = "command";
      _activeCommand.value = null;
      _currentStepIndex.value = 0;
      _stepInputs.value = {};
      _stepError.value = null;
      _filteredStepChoices.value = [];
      _selectedChoiceIndex.value = 0;
      _stepChoicesLoading.value = false;
      _stepChoicesBase.value = [];
      _hasMoreChoices.value = false;
      _loadMoreCallback.value = null;
      _loadingMore.value = false;
    },
  };
}

export type PaletteState = ReturnType<typeof createPaletteState>;
