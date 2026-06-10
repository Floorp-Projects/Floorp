// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type {
  PaletteCommand,
  CommandStepChoice,
} from "../../types.ts";
import { getJapaneseReadings } from "../../utils/getJapaneseReadings.ts";
import { getEnglishStepCommandKeywords } from "#features-chrome/common/command-palette/utils/getEnglishKeywords.ts";
import { getSegmentedKeywordsFromI18nKeys } from "#features-chrome/common/command-palette/utils/budouxSegmenter.ts";

export function loadClosedTabs(): Promise<CommandStepChoice[]> {
  try {
    const entries = globalThis.SessionStore.getClosedTabData(globalThis.window);
    if (!Array.isArray(entries) || entries.length === 0) return Promise.resolve([]);

    const choices: CommandStepChoice[] = entries.map(
      (entry: unknown, index: number) => {
        const e = entry as { title?: string; url?: string };
        return {
          label: e.title || e.url || "Untitled",
          value: String(index),
          description: e.url || "",
        };
      },
    );

    return Promise.resolve(choices);
  } catch (err) {
    console.error("[ClosedTabSwitcher] Failed to load closed tabs", err);
    return Promise.resolve([]);
  }
}

export const closedTabSwitcherCommand: PaletteCommand = {
  id: "floorp-closed-tab-switcher",
  label: i18next.t("commandPalette.closedTabSwitcher", {
    defaultValue: "Reopen Closed Tab",
  }),
  description: i18next.t("commandPalette.closedTabSwitcherDescription", {
    defaultValue: "Reopen a recently closed tab",
  }),
  category: "switcher",
  keywords: [
    "closed tab",
    "reopen tab",
    "undo close tab",
    "restore tab",
    "recently closed",
    ...getEnglishStepCommandKeywords("commandPalette.closedTabSwitcher", "commandPalette.closedTabSwitcherDescription"),
    ...getSegmentedKeywordsFromI18nKeys("commandPalette.closedTabSwitcher", "commandPalette.closedTabSwitcherDescription"),
    ...getJapaneseReadings("floorp-closed-tab-switcher"),
  ],
  steps: [
    {
      id: "closedTab",
      label: i18next.t("commandPalette.closedTabSwitcherStepLabel", {
        defaultValue: "Select a closed tab",
      }),
      placeholder: i18next.t(
        "commandPalette.closedTabSwitcherStepPlaceholder",
        {
          defaultValue: "Choose a closed tab...",
        },
      ),
      choicesLoader: loadClosedTabs,
    },
  ],
  fn: (_win: Window, args?: Record<string, string>) => {
    try {
      const index = Number(args?.closedTab);
      if (isNaN(index)) return;

      (
        globalThis.SessionStore as unknown as {
          undoCloseTab(aSource: Window, aIndex: number): void;
        }
      ).undoCloseTab(globalThis.window, index);
    } catch (err) {
      console.error("[ClosedTabSwitcher] Failed to reopen closed tab", err);
    }
  },
};
