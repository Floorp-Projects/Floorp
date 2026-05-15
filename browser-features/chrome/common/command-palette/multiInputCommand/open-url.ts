// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type { PaletteCommand } from "../command-registry.ts";

export const openUrlCommand: PaletteCommand = {
  id: "floorp-open-url",
  label: i18next.t("commandPalette.openUrl", { defaultValue: "Open URL" }),
  description: i18next.t("commandPalette.openUrlDescription", {
    defaultValue: "Open a URL in a new tab",
  }),
  category: "navigation",
  keywords: ["open url", "navigate", "go to", "open page", "url"],
  steps: [
    {
      id: "url",
      label: i18next.t("commandPalette.openUrlStepLabel", {
        defaultValue: "Enter URL to open",
      }),
      placeholder: i18next.t("commandPalette.openUrlStepPlaceholder", {
        defaultValue: "https://example.com",
      }),
      validate: (input: string): boolean | string => {
        const trimmed = input.trim();
        if (!trimmed) {
          return i18next.t("commandPalette.openUrlValidationError", {
            defaultValue: "Please enter a valid URL",
          });
        }
        return true;
      },
    },
  ],
  fn: (_win: Window, args?: Record<string, string>) => {
    const url = args?.url?.trim();
    if (!url) return;
    const navUrl = url.includes("://") ? url : `https://${url}`;
    try {
      let principal = globalThis.gBrowser?.selectedBrowser?.contentPrincipal;
      let tab = globalThis.gBrowser?.addTab(navUrl, {
        triggeringPrincipal: principal,
        inBackground: false,
      });
      if (globalThis.gBrowser) globalThis.gBrowser.selectedTab = tab;
    } catch (e) {
      console.error("[command-palette] Open URL failed", e);
    }
  },
};
