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

// `navigation-suggestion` renders inline (the URL itself is the label), and
// `shortcut` (@prefix results) each carry their own `@prefix` label — neither
// needs a category header.
const HIDDEN_CATEGORIES = new Set(["navigation-suggestion", "shortcut"]);

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
    // - `shortcut` (@prefix results), `recent`, and `navigation-suggestion` are
    //   pseudo-categories whose positions are controller-driven. They are pinned
    //   to the top and excluded from the priority sort (see the controller's
    //   `doUpdateSearch` push order: shortcut → navigation-suggestion → recent →
    //   others). `shortcut` is intentionally NOT in `DEFAULT_CATEGORY_PRIORITY`
    //   (doing so would break the `length === 18` invariant in
    //   `category-priority.test.ts`), so without pinning it here
    //   `getCategoryPriorityIndex("shortcut")` returns `MAX_SAFE_INTEGER` and
    //   shortcut results would sink to the bottom — diverging from the flat
    //   array where they sit at the top.
    // - All other categories (including `search`, which has the lowest priority
    //   by default) are sorted via the user's priority list.
    const shortcutGroups = groups.filter((g) => g.category === "shortcut");
    const navSuggestionGroups = groups.filter(
      (g) => g.category === "navigation-suggestion",
    );
    const recentGroups = groups.filter((g) => g.category === "recent");
    const visibleGroups = groups.filter(
      (g) =>
        g.category !== "shortcut" &&
        g.category !== "navigation-suggestion" &&
        g.category !== "recent",
    );

    const priorityList = getCategoryPriority();
    const sortedVisible = sortCategoriesByPriority(visibleGroups, priorityList);

    // NOTE: the order here mirrors `controller.ts:doUpdateSearch`'s flat-array
    // push order exactly (shortcut → navigation-suggestion → recent → others).
    // `recent` (only emitted for empty queries) and `navigation-suggestion`
    // (only emitted for URL-like queries) are mutually exclusive in practice,
    // but `shortcut` can coexist with both — e.g. `@g` yields shortcut +
    // fuzzy-search results, and `@foo.com` yields shortcut + navigation-
    // suggestion — so the full controller order is reproduced here to keep the
    // `getGlobalIndex` (display space) ↔ `selectedIndex` (flat-array space)
    // invariant sound and prevent highlight/execute drift.
    return [
      ...shortcutGroups,
      ...navSuggestionGroups,
      ...recentGroups,
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
