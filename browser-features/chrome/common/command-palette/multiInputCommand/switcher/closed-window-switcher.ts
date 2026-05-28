// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type {
  PaletteCommand,
  CommandStepChoice,
} from "../../types.ts";
import { getJapaneseReadings } from "../../utils/getJapaneseReadings.ts";

export function loadClosedWindows(): Promise<CommandStepChoice[]> {
  try {
    const closedWindows = (
      globalThis.SessionStore as unknown as { getClosedWindowData(): unknown[] }
    ).getClosedWindowData();
    if (!Array.isArray(closedWindows) || closedWindows.length === 0) return Promise.resolve([]);

    return Promise.resolve(closedWindows.map((entry: unknown, index: number) => {
      const e = entry as {
        title?: string;
        tabs?: Array<{ title?: string }>;
      };
      const tabCount = e.tabs?.length ?? 0;
      return {
        label: e.title || "Untitled Window",
        value: String(index),
        description:
          tabCount === 0
            ? "0 tabs"
            : `${tabCount} tabs • ${e.tabs?.[0]?.title ?? ""}`,
      };
    }));
  } catch (e) {
    console.error("[ClosedWindowSwitcher] Failed to load closed windows", e);
    return Promise.resolve([]);
  }
}

export const closedWindowSwitcherCommand: PaletteCommand = {
  id: "floorp-closed-window-switcher",
  label: i18next.t("commandPalette.closedWindowSwitcher", {
    defaultValue: "Reopen Closed Window",
  }),
  description: i18next.t("commandPalette.closedWindowSwitcherDescription", {
    defaultValue: "Reopen a recently closed window",
  }),
  category: "switcher",
  keywords: [
    "closed window",
    "reopen window",
    "undo close window",
    "restore window",
    "recently closed",
    ...getJapaneseReadings("floorp-closed-window-switcher"),
  ],
  steps: [
    {
      id: "closedWindow",
      label: i18next.t("commandPalette.closedWindowSwitcherStepLabel", {
        defaultValue: "Select a closed window",
      }),
      placeholder: i18next.t(
        "commandPalette.closedWindowSwitcherStepPlaceholder",
        { defaultValue: "Choose a closed window..." },
      ),
      choicesLoader: loadClosedWindows,
    },
  ],
  fn: (_win: Window, args?: Record<string, string>) => {
    try {
      const index = Number(args?.closedWindow);
      if (isNaN(index)) return;
      if (!globalThis.SessionStore?.undoCloseWindow) return;
      globalThis.SessionStore.undoCloseWindow(index);
    } catch (e) {
      console.error("[ClosedWindowSwitcher] Failed to reopen window", e);
    }
  },
};
