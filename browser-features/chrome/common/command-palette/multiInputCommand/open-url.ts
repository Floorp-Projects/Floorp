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
    {
      id: "where",
      label: i18next.t("commandPalette.openUrlWhereLabel", {
        defaultValue: "Where to open",
      }),
      placeholder: i18next.t("commandPalette.openUrlWherePlaceholder", {
        defaultValue: "Select where to open...",
      }),
      choices: [
        {
          label: i18next.t("commandPalette.openUrlWhereNewTab", {
            defaultValue: "New Tab",
          }),
          value: "new-tab",
          description: i18next.t("commandPalette.openUrlWhereNewTabDesc", {
            defaultValue: "Open in a new foreground tab",
          }),
        },
        {
          label: i18next.t("commandPalette.openUrlWhereBackgroundTab", {
            defaultValue: "Background Tab",
          }),
          value: "background-tab",
          description: i18next.t(
            "commandPalette.openUrlWhereBackgroundTabDesc",
            {
              defaultValue: "Open in a new background tab",
            },
          ),
        },
        {
          label: i18next.t("commandPalette.openUrlWhereCurrentTab", {
            defaultValue: "Current Tab",
          }),
          value: "current-tab",
          description: i18next.t("commandPalette.openUrlWhereCurrentTabDesc", {
            defaultValue: "Navigate the current tab",
          }),
        },
      ],
    },
  ],
  fn: (_win: Window, args?: Record<string, string>) => {
    const url = args?.url?.trim();
    if (!url) return;

    const where = args?.where ?? "new-tab";
    const navUrl = url.includes("://") ? url : `https://${url}`;

    try {
      const principal = globalThis.gBrowser?.selectedBrowser?.contentPrincipal;

      switch (where) {
        case "current-tab":
          globalThis.gBrowser?.loadURI?.(Services.io.newURI(navUrl), {
            triggeringPrincipal: principal,
          });
          break;

        case "background-tab":
          globalThis.gBrowser?.addTab(navUrl, {
            triggeringPrincipal: principal,
            inBackground: true,
          });
          break;

        case "new-tab":
        default: {
          const tab = globalThis.gBrowser?.addTab(navUrl, {
            triggeringPrincipal: principal,
            inBackground: false,
          });
          if (globalThis.gBrowser && tab) {
            globalThis.gBrowser.selectedTab = tab;
          }
          break;
        }
      }
    } catch (e) {
      console.error("[command-palette] Open URL failed", e);
    }
  },
};
