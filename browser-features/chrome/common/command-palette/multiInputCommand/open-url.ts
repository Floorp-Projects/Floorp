// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type {
  CommandStepChoice,
  PaletteCommand,
  StepChoicesResult,
} from "../types.ts";
import { getJapaneseReadings } from "../utils/getJapaneseReadings.ts";
import { getEnglishStepCommandKeywords } from "#features-chrome/common/command-palette/utils/getEnglishKeywords.ts";
import { getSegmentedKeywordsFromI18nKeys } from "#features-chrome/common/command-palette/utils/budouxSegmenter.ts";
import Workspaces from "#features-chrome/common/workspaces";

export async function loadContainersForOpenUrl(): Promise<StepChoicesResult> {
  try {
    const { ContextualIdentityService } = ChromeUtils.importESModule(
      "resource://gre/modules/ContextualIdentityService.sys.mjs",
    );

    ContextualIdentityService.ensureDataReady();

    const identities = await ContextualIdentityService.getPublicIdentities();

    const workspaceDefaultId = Workspaces.getCtx()
      ?.getCurrentWorkspaceUserContextId() ?? 0;

    const workspaceDisplayName = workspaceDefaultId > 0
      ? (ContextualIdentityService.getUserContextLabel(workspaceDefaultId) || "")
      : i18next.t("commandPalette.reopenInContainerNoContainer", {
        defaultValue: "No Container",
      });

    const workspaceDefault: CommandStepChoice = {
      label: i18next.t("commandPalette.openUrlContainerWorkspaceDefault", {
        defaultValue: "Workspace Default",
      }),
      value: "workspace",
      description: i18next.t(
        "commandPalette.openUrlContainerWorkspaceDefaultDesc",
        { defaultValue: "Uses the workspace's default context" },
      ) + (workspaceDisplayName ? ` (${workspaceDisplayName})` : ""),
    };

    const noContainer: CommandStepChoice = {
      label: i18next.t("commandPalette.reopenInContainerNoContainer", {
        defaultValue: "No Container",
      }),
      value: "0",
      description: i18next.t(
        "commandPalette.reopenInContainerNoContainerDesc",
        { defaultValue: "Open in the default context" },
      ),
    };

    const containerChoices: CommandStepChoice[] = identities
      .filter(
        (identity: unknown) =>
          !(identity as { floorpPrivateContainer?: boolean })
            .floorpPrivateContainer,
      )
      .map((container: unknown) => {
        const userContextId = (container as { userContextId: number })
          .userContextId;
        // getUserContextLabel handles both l10nId (built-in) and name (user-created)
        const label =
          ContextualIdentityService.getUserContextLabel(userContextId);
        return {
          label: label || "Unknown",
          value: String(userContextId),
          description: `${(container as { color: string }).color} • ${(container as { icon: string }).icon}`,
        };
      });

    return {
      choices: [workspaceDefault, noContainer, ...containerChoices],
      defaultIndex: 0,
    };
  } catch (e) {
    console.error("[command-palette] Failed to load containers:", e);
    return { choices: [], defaultIndex: 0 };
  }
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
    ...getEnglishStepCommandKeywords("commandPalette.openUrl", "commandPalette.openUrlDescription"),
    ...getSegmentedKeywordsFromI18nKeys("commandPalette.openUrl", "commandPalette.openUrlDescription"),
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
        const looksValid =
          /^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//.test(trimmed) ||      // has scheme
          /^[^\s]+\.[a-z]{2,}/i.test(trimmed) ||                  // domain-like
          /^localhost(:\d+)?$/i.test(trimmed) ||                   // localhost
          /^about:/.test(trimmed) ||                               // about: pages
          /^floorp:\/\//.test(trimmed);                            // floorp:// pages
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
      choicesLoader: loadContainersForOpenUrl,
    },
  ],
  fn: (_win: Window, args?: Record<string, string>) => {
    const url = args?.url?.trim();
    if (!url) return;

    const where = args?.where ?? "new-tab";
    const navUrl = url.includes("://") ? url : `https://${url}`;

    try {
      const principal = globalThis.gBrowser?.selectedBrowser?.contentPrincipal;
      const containerChoice = args?.container ?? "workspace";
      let userContextId: number;
      if (containerChoice === "workspace") {
        userContextId = Workspaces.getCtx()?.getCurrentWorkspaceUserContextId() ?? 0;
      } else {
        const parsed = Number.parseInt(containerChoice, 10);
        userContextId = Number.isNaN(parsed) ? 0 : parsed;
      }

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
            userContextId: userContextId > 0 ? userContextId : undefined,
          });
          break;

        case "new-tab":
        default: {
          const tab = globalThis.gBrowser?.addTab(navUrl, {
            triggeringPrincipal: principal,
            inBackground: false,
            userContextId: userContextId > 0 ? userContextId : undefined,
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
