// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type { PaletteCommand } from "../types.ts";
import { CommandItem } from "./CommandItem.tsx";
import { CategoryHeader } from "./CategoryHeader.tsx";

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

const HIDDEN_CATEGORIES = new Set(["navigation-suggestion", "search-suggestion"]);

export function CommandList(props: CommandListProps) {
  // Build category groups from props — recomputes on every render (props change
  // triggers parent re-render which passes new commands array).
  const grouped: CategorizedCommands[] = [];
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
    grouped.push({ category, commands });
  }

  const getGlobalIndex = (groupIdx: number, itemIdx: number): number => {
    let idx = 0;
    for (let g = 0; g < groupIdx; g++) {
      idx += grouped[g].commands.length;
    }
    return idx + itemIdx;
  };

  if (props.commands.length === 0) {
    return (
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
    );
  }

  return (
    <div class="command-palette-list" role="listbox">
      {grouped.map((group, groupIdx) => (
        <>
          {!HIDDEN_CATEGORIES.has(group.category) && (
            <CategoryHeader category={group.category} />
          )}
          {group.commands.map((cmd, itemIdx) => {
            const globalIdx = getGlobalIndex(groupIdx, itemIdx);
            return (
              <CommandItem
                command={cmd}
                isSelected={props.selectedIndex === globalIdx}
                query={props.query}
                onSelect={() => props.onCommandSelect(globalIdx)}
                onExecute={() => props.onCommandExecute(cmd)}
              />
            );
          })}
        </>
      ))}
    </div>
  );
}
