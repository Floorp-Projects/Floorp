// SPDX-License-Identifier: MPL-2.0

import { createSignal } from "solid-js";
import type { PaletteCommand } from "../command-registry.ts";

export function createPaletteState() {
  const [isVisible, setIsVisible] = createSignal(false);
  const [isAnimatingOut, setIsAnimatingOut] = createSignal(false);
  const [query, setQuery] = createSignal("");
  const [selectedIndex, setSelectedIndex] = createSignal(0);
  const [filteredCommands, setFilteredCommands] = createSignal<PaletteCommand[]>(
    [],
  );

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
    reset() {
      setQuery("");
      setSelectedIndex(0);
      setFilteredCommands([]);
    },
  };
}

export type PaletteState = ReturnType<typeof createPaletteState>;
