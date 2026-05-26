// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type {
  PaletteCommand,
  CommandStepChoice,
} from "../../command-registry.ts";

/**
 * Get hiragana reading keywords for a given action/command ID from i18n.
 * Returns an empty array for non-Japanese locales or if no readings are defined.
 */
function getJapaneseReadings(id: string): string[] {
  try {
    const readings: unknown = i18next.t(`commandPaletteReadings.${id}`, {
      defaultValue: [] as string[],
      returnObjects: true,
    });
    if (Array.isArray(readings)) {
      return readings.filter((r): r is string => typeof r === "string");
    }
    return [];
  } catch {
    return [];
  }
}

export async function loadClosedTabs(): Promise<CommandStepChoice[]> {
  try {
    const entries = globalThis.SessionStore.getClosedTabData(globalThis.window);
    if (!Array.isArray(entries) || entries.length === 0) return [];

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

    return choices;
  } catch (err) {
    console.error("[ClosedTabSwitcher] Failed to load closed tabs", err);
    return [];
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
