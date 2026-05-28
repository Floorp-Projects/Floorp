// SPDX-License-Identifier: MPL-2.0

import { createSignal } from "solid-js";
import type { PaletteCommand, CommandStepChoice } from "../types.ts";

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
  const [filteredStepChoices, setFilteredStepChoices] = createSignal<
    CommandStepChoice[]
  >([]);
  const [selectedChoiceIndex, setSelectedChoiceIndex] = createSignal(0);
  const [stepChoicesLoading, setStepChoicesLoading] = createSignal(false);
  const [stepChoicesBase, setStepChoicesBase] = createSignal<
    CommandStepChoice[]
  >([]);
  const [hasMoreChoices, setHasMoreChoices] = createSignal(false);
  const [loadMoreCallback, setLoadMoreCallback] = createSignal<
    (() => Promise<{ choices: CommandStepChoice[]; hasMore: boolean }>) | null
  >(null);
  const [loadingMore, setLoadingMore] = createSignal(false);

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
    filteredStepChoices,
    setFilteredStepChoices,
    selectedChoiceIndex,
    setSelectedChoiceIndex,
    stepChoicesLoading,
    setStepChoicesLoading,
    stepChoicesBase,
    setStepChoicesBase,
    hasMoreChoices,
    setHasMoreChoices,
    loadMoreCallback,
    setLoadMoreCallback,
    loadingMore,
    setLoadingMore,
    reset() {
      setQuery("");
      setSelectedIndex(0);
      setFilteredCommands([]);
      setMode("command");
      setActiveCommand(null);
      setCurrentStepIndex(0);
      setStepInputs({});
      setStepError(null);
      setFilteredStepChoices([]);
      setSelectedChoiceIndex(0);
      setStepChoicesLoading(false);
      setStepChoicesBase([]);
      setHasMoreChoices(false);
      setLoadMoreCallback(null);
      setLoadingMore(false);
    },
  };
}

export type PaletteState = ReturnType<typeof createPaletteState>;
