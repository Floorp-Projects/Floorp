// SPDX-License-Identifier: MPL-2.0

import { createSignal } from "solid-js";
import { createRootHMR } from "@nora/solid-xul";
import type { PaletteCommand } from "../command-registry.ts";

function createPaletteState() {
  const [isVisible, setIsVisible] = createSignal(false);
  const [query, setQuery] = createSignal("");
  const [selectedIndex, setSelectedIndex] = createSignal(0);
  const [filteredCommands, setFilteredCommands] = createSignal<PaletteCommand[]>(
    [],
  );

  return {
    isVisible,
    setIsVisible,
    query,
    setQuery,
    selectedIndex,
    setSelectedIndex,
    filteredCommands,
    setFilteredCommands,
  };
}

export const state = createRootHMR(createPaletteState, import.meta.hot);

export function resetPaletteState() {
  state.setQuery("");
  state.setSelectedIndex(0);
  state.setFilteredCommands([]);
}
