// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type {
  PaletteCommand,
  CommandStepChoice,
} from "../../command-registry.ts";

interface ChromeWindow extends Window {
  gBrowser?: {
    tabs?: { length: number };
  };
  document?: {
    title?: string;
  };
}

export async function loadWindows(): Promise<CommandStepChoice[]> {
  try {
    const windows = Services.wm.getEnumerator("navigator:browser");
    const currentWin = globalThis.window as unknown as ChromeWindow;
    const collected: ChromeWindow[] = [];

    while (windows.hasMoreElements()) {
      collected.push(windows.getNext() as ChromeWindow);
    }

    return collected.map((win, index) => {
      const tabCount = win.gBrowser?.tabs?.length ?? 0;
      const isCurrent = win === currentWin;
      const title = win.document?.title ?? "";

      return {
        label: isCurrent
          ? `${title} (${i18next.t("commandPalette.windowSwitcherCurrent", { defaultValue: "current" })})`
          : title,
        value: String(index),
        description: i18next.t("commandPalette.windowSwitcherTabCount", {
          defaultValue: "{{count}} tab(s)",
          count: tabCount,
        }),
      };
    });
  } catch (err) {
    console.error("[WindowSwitcher] Failed to load windows", err);
    return [];
  }
}

export const windowSwitcherCommand: PaletteCommand = {
  id: "floorp-window-switcher",
  label: i18next.t("commandPalette.windowSwitcher", {
    defaultValue: "Switch Window",
  }),
  description: i18next.t("commandPalette.windowSwitcherDescription", {
    defaultValue: "Switch to a different open window",
  }),
  category: "switcher",
  keywords: ["switch window", "window", "change window", "go to window"],
  steps: [
    {
      id: "window",
      label: i18next.t("commandPalette.windowSwitcherStepLabel", {
        defaultValue: "Select a window",
      }),
      placeholder: i18next.t("commandPalette.windowSwitcherStepPlaceholder", {
        defaultValue: "Choose a window...",
      }),
      choicesLoader: loadWindows,
    },
  ],
  fn: (_win: Window, args?: Record<string, string>) => {
    try {
      const windowIndex = Number(args?.window);
      if (isNaN(windowIndex)) return;

      const windows = Services.wm.getEnumerator("navigator:browser");
      let currentIndex = 0;

      while (windows.hasMoreElements()) {
        const win = windows.getNext() as ChromeWindow;
        if (currentIndex === windowIndex) {
          (win as Window).focus();
          return;
        }
        currentIndex++;
      }
    } catch (err) {
      console.error("[WindowSwitcher] Failed to switch window", err);
    }
  },
};
