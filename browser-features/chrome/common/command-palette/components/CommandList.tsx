// SPDX-License-Identifier: MPL-2.0

import { createMemo, For, Show } from "solid-js";
import i18next from "i18next";
import type { PaletteCommand } from "../command-registry.ts";
import { CommandItem } from "./CommandItem.tsx";
import { CategoryHeader } from "./CategoryHeader.tsx";

interface CommandListProps {
  commands: PaletteCommand[];
  selectedIndex: number;
  onCommandSelect: (index: number) => void;
  onCommandExecute: (command: PaletteCommand) => void;
}

interface CategorizedCommands {
  category: string;
  commands: PaletteCommand[];
}

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

    return groups;
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
          {i18next.t("commandPalette.noResults", { defaultValue: "No commands found" })}
        </div>
      }
    >
      <div class="command-palette-list" role="listbox">
        <For each={grouped()}>
          {(group, groupIdx) => (
            <>
              <CategoryHeader category={group.category} />
              <For each={group.commands}>
                {(cmd, itemIdx) => {
                  const globalIdx = () =>
                    getGlobalIndex(groupIdx(), itemIdx());
                  return (
                    <CommandItem
                      command={cmd}
                      isSelected={props.selectedIndex === globalIdx()}
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
