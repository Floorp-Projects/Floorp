// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type { PaletteCommand } from "../types.ts";
import { getJapaneseReadings } from "../utils/getJapaneseReadings.ts";
import { getEnglishStepCommandKeywords } from "#features-chrome/common/command-palette/utils/getEnglishKeywords.ts";
import { getSegmentedKeywordsFromI18nKeys } from "#features-chrome/common/command-palette/utils/budouxSegmenter.ts";
import { loadContainerChoices } from "#features-chrome/common/command-palette/utils/containerChoices.ts";
import {
  createTriggeringPrincipal,
  parseUserContextChoice,
  resolvePaletteTarget,
} from "#features-chrome/common/command-palette/utils/targetContext.ts";

const EXPLICIT_SCHEME_PATTERN = /^(?:[a-zA-Z][a-zA-Z0-9+.-]*:\/\/|about:)/;

function hasExplicitScheme(value: string): boolean {
  return EXPLICIT_SCHEME_PATTERN.test(value);
}

export const openUrlCommand: PaletteCommand = {
  id: "floorp-open-url",
  label: i18next.t("commandPalette.openUrl", { defaultValue: "Open URL" }),
  description: i18next.t("commandPalette.openUrlDescription", {
    defaultValue: "Open a URL in a new tab",
  }),
  category: "navigation",
  keywords: [
    "open url",
    "navigate",
    "go to",
    "open page",
    "url",
    ...getJapaneseReadings("floorp-open-url"),
    ...getEnglishStepCommandKeywords(
      "commandPalette.openUrl",
      "commandPalette.openUrlDescription",
    ),
    ...getSegmentedKeywordsFromI18nKeys(
      "commandPalette.openUrl",
      "commandPalette.openUrlDescription",
    ),
  ],
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
        // Accept scheme-prefixed URLs, domain-like patterns, and localhost
        const looksValid = hasExplicitScheme(trimmed) || // has scheme
          /^[^\s]+\.[a-z]{2,}/i.test(trimmed) || // domain-like
          /^localhost(:\d+)?$/i.test(trimmed); // localhost
        if (!looksValid) {
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
    {
      id: "container",
      label: i18next.t("commandPalette.openUrlContainerStepLabel", {
        defaultValue: "Open in container",
      }),
      placeholder: i18next.t("commandPalette.openUrlContainerStepPlaceholder", {
        defaultValue: "Choose a container...",
      }),
      choicesLoader: loadContainerChoices,
      shouldInclude: (inputs) => inputs.where !== "current-tab",
    },
  ],
  fn: (targetWindow: Window, args?: Record<string, string>) => {
    const url = args?.url?.trim();
    if (!url) return;

    const where = args?.where ?? "new-tab";
    if (where === "current-tab" && args?.container !== undefined) {
      console.error(
        "[command-palette] Open URL rejected a container override for the current tab",
      );
      return;
    }
    const navUrl = hasExplicitScheme(url) ? url : `https://${url}`;

    try {
      const target = resolvePaletteTarget(targetWindow);
      if (!target) {
        console.error("[command-palette] Open URL target is unavailable");
        return;
      }

      const containerChoice = args?.container ?? "workspace";
      const contextChoice = parseUserContextChoice(
        containerChoice,
        target.workspaces?.getCurrentWorkspaceUserContextId() ?? 0,
      );
      if (!contextChoice) {
        console.error("[command-palette] Open URL container is invalid");
        return;
      }

      const { explicit, userContextId } = contextChoice;
      const principal = where === "current-tab"
        ? target.principal
        : createTriggeringPrincipal(target, userContextId);
      const addTab = (inBackground: boolean): XULElement => {
        const createTab = () =>
          target.gBrowser.addTab(navUrl, {
            triggeringPrincipal: principal,
            inBackground,
            userContextId,
          });
        return explicit && target.workspaces
          ? target.workspaces.withExplicitTabUserContext(
            userContextId,
            createTab,
          )
          : createTab();
      };

      switch (where) {
        case "current-tab":
          target.browser.loadURI?.(Services.io.newURI(navUrl), {
            triggeringPrincipal: principal,
          });
          break;

        case "background-tab":
          addTab(true);
          break;

        case "new-tab":
        default: {
          const tab = addTab(false);
          target.gBrowser.selectedTab = tab;
          break;
        }
      }
    } catch (e) {
      console.error("[command-palette] Open URL failed", e);
    }
  },
};
