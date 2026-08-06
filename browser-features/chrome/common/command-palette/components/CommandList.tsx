// SPDX-License-Identifier: MPL-2.0

import { createMemo, For, Show } from "solid-js";
import i18next from "i18next";
import type { PaletteCommand } from "../types.ts";
import { CommandItem } from "./CommandItem.tsx";
import { CategoryHeader } from "./CategoryHeader.tsx";
import { getCategoryPriority } from "../config.ts";
import { sortCategoriesByPriority } from "../category-priority.ts";

interface CommandListProps {
  commands: PaletteCommand[];
  selectedIndex: number;
  query: string;
  onCommandSelect: (index: number) => void;
  onCommandExecute: (command: PaletteCommand) => void;
}

interface CategorizedCommands {
  category: string;
  commands: PaletteCommand[];
}

const HIDDEN_CATEGORIES = new Set(["navigation-suggestion"]);

export function CommandList(props: CommandListProps) {
  const grouped = createMemo(() => {
    const groups: CategorizedCommands[] = [];
    const categoryMap = new Map<string, PaletteCommand[]>();

    for (const cmd of props.commands) {
      const list = categoryMap.get(cmd.category);
      if (list) {
        list.push(cmd);
      } else {
        categoryMap.set(cmd.category, [cmd]);
      }
    }

    for (const [category, commands] of categoryMap) {
      groups.push({ category, commands });
    }

    // Order groups by the user-defined category priority.
    //
    // - `recent` is a runtime pseudo-category that must always stay at the
    //   very top regardless of the priority list.
    // - `navigation-suggestion` is a pseudo-category with no header; its
    //   position is controller-driven (top of the flat list), so it is
    //   excluded from the priority sort to preserve that behavior.
    // - All other categories (including `search`, which has the lowest
    //   priority by default) are sorted via the user's priority list.
    const recentGroups = groups.filter((g) => g.category === "recent");
    const navSuggestionGroups = groups.filter(
      (g) => g.category === "navigation-suggestion",
    );
    const visibleGroups = groups.filter(
      (g) =>
        g.category !== "recent" &&
        g.category !== "navigation-suggestion",
    );

    const priorityList = getCategoryPriority();
    const sortedVisible = sortCategoriesByPriority(visibleGroups, priorityList);

    // NOTE: `recent` (only emitted for empty queries by buildInitialCommandList)
    // and `navigation-suggestion` (only emitted for URL-like queries) are mutually
    // exclusive in the controller's flat array, so the order of these two groups
    // relative to each other does not affect the getGlobalIndex invariant. If a
    // future change ever causes both to coexist, the flat-array order in
    // `controller.ts:applyPriorityTiebreak` (recent first) and the display order
    // here must be reconciled to keep arrow-key navigation correct.
    return [
      ...recentGroups,
      ...navSuggestionGroups,
      ...sortedVisible,
    ];
  });

  const getGlobalIndex = (groupIdx: number, itemIdx: number): number => {
    let idx = 0;
    const groups = grouped();
    for (let g = 0; g < groupIdx; g++) {
      idx += groups[g].commands.length;
    }
    return idx + itemIdx;
  };

  return (
    <Show
      when={props.commands.length > 0}
      fallback={
        <div class="command-palette-empty">
          <div class="command-palette-empty-title">
            {i18next.t("commandPalette.noResults", {
              defaultValue: "No commands found",
            })}
          </div>
          <div class="command-palette-empty-hint">
            {i18next.t("commandPalette.noResultsHint", {
              defaultValue: "Try a different search term",
            })}
          </div>
        </div>
      }
    >
      <div class="command-palette-list" role="listbox">
        <For each={grouped()}>
          {(group, groupIdx) => (
            <>
              <Show when={!HIDDEN_CATEGORIES.has(group.category)}>
                <CategoryHeader category={group.category} />
              </Show>
              <For each={group.commands}>
                {(cmd, itemIdx) => {
                  const globalIdx = () =>
                    getGlobalIndex(groupIdx(), itemIdx());
                  return (
                    <CommandItem
                      command={cmd}
                      isSelected={props.selectedIndex === globalIdx()}
                      query={props.query}
                      onSelect={() => props.onCommandSelect(globalIdx())}
                      onExecute={() => props.onCommandExecute(cmd)}
                    />
                  );
                }}
              </For>
            </>
          )}
        </For>
      </div>
    </Show>
  );
}
