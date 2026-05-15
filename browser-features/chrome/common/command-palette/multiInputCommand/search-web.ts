// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type { PaletteCommand, CommandStepChoice } from "../command-registry.ts";

async function loadSearchEngines(): Promise<CommandStepChoice[]> {
  try {
    const { SearchService } = ChromeUtils.importESModule(
      "moz-src:///toolkit/components/search/SearchService.sys.mjs",
    );

    const timeoutPromise = new Promise((_, reject) => {
      globalThis.setTimeout(
        () => reject(new Error("Search engine list timeout")),
        2000,
      );
    });

    const engines = (await Promise.race([
      SearchService.getEngines(),
      timeoutPromise,
    ])) as any[];

    let defaultEngine: any = null;
    try {
      defaultEngine = await SearchService.getDefault();
    } catch {
      // ignore
    }

    const defaultName = defaultEngine?.name ?? "";

    return (engines ?? []).map((engine: any) => ({
      label:
        engine.name +
        (engine.name === defaultName
          ? i18next.t("commandPalette.searchEngineDefault", {
              defaultValue: " (default)",
            })
          : ""),
      value: engine.name,
      description: engine.description ?? "",
    }));
  } catch (e) {
    console.error("[command-palette] Failed to load search engines:", e);
    return [];
  }
}

export const searchWebCommand: PaletteCommand = {
  id: "floorp-search-web",
  label: i18next.t("commandPalette.searchWeb", {
    defaultValue: "Search the Web",
  }),
  description: i18next.t("commandPalette.searchWebDescription", {
    defaultValue: "Search with your default search engine",
  }),
  category: "search",
  keywords: ["search", "web search", "find", "lookup", "google"],
  steps: [
    {
      id: "query",
      label: i18next.t("commandPalette.searchWebStepLabel", {
        defaultValue: "Enter search query",
      }),
      placeholder: i18next.t("commandPalette.searchWebStepPlaceholder", {
        defaultValue: "Search terms...",
      }),
      validate: (input: string): boolean | string => {
        if (!input.trim()) return "Please enter a search query";
        return true;
      },
    },
    {
      id: "engine",
      label: i18next.t("commandPalette.searchWebEngineLabel", {
        defaultValue: "Select search engine",
      }),
      placeholder: i18next.t("commandPalette.searchWebEnginePlaceholder", {
        defaultValue: "Select a search engine...",
      }),
      choicesLoader: loadSearchEngines,
    },
    {
      id: "where",
      label: i18next.t("commandPalette.searchWebWhereLabel", {
        defaultValue: "Where to open results",
      }),
      placeholder: i18next.t("commandPalette.searchWebWherePlaceholder", {
        defaultValue: "Select where to open...",
      }),
      choices: [
        {
          label: i18next.t("commandPalette.searchWebWhereNewTab", {
            defaultValue: "New Tab",
          }),
          value: "new-tab",
          description: i18next.t("commandPalette.searchWebWhereNewTabDesc", {
            defaultValue: "Open in a new foreground tab",
          }),
        },
        {
          label: i18next.t("commandPalette.searchWebWhereBackgroundTab", {
            defaultValue: "Background Tab",
          }),
          value: "background-tab",
          description: i18next.t(
            "commandPalette.searchWebWhereBackgroundTabDesc",
            {
              defaultValue: "Open in a new background tab",
            },
          ),
        },
        {
          label: i18next.t("commandPalette.searchWebWhereCurrentTab", {
            defaultValue: "Current Tab",
          }),
          value: "current-tab",
          description: i18next.t(
            "commandPalette.searchWebWhereCurrentTabDesc",
            {
              defaultValue: "Navigate the current tab",
            },
          ),
        },
      ],
    },
  ],
  fn: (_win: Window, args?: Record<string, string>) => {
    const query = args?.query?.trim();
    if (!query) return;

    const engineName = args?.engine;
    const where = args?.where ?? "new-tab";

    const { SearchService } = ChromeUtils.importESModule(
      "moz-src:///toolkit/components/search/SearchService.sys.mjs",
    );

    try {
      const timeoutPromise = new Promise((_, reject) => {
        globalThis.setTimeout(() => {
          reject(new Error("Search engine timeout"));
        }, 2000);
      });

      // getEngineByName is synchronous, getDefault is async
      const enginePromise = engineName
        ? Promise.resolve(
            SearchService.getEngineByName(engineName) ??
              SearchService.getDefault(),
          )
        : SearchService.getDefault();

      Promise.race([enginePromise, timeoutPromise])
        .then((engine) => {
          if (!engine) return;

          const sysPrincipal =
            globalThis.Services.scriptSecurityManager.getSystemPrincipal();
          const submission = engine.getSubmission(query);

          switch (where) {
            case "current-tab":
              globalThis.gBrowser?.loadURI(submission.uri, {
                triggeringPrincipal: sysPrincipal,
                postData: submission.postData,
              });
              break;

            case "background-tab": {
              globalThis.gBrowser?.addTab(submission.uri.spec, {
                triggeringPrincipal: sysPrincipal,
                inBackground: true,
                postData: submission.postData,
              });
              break;
            }

            case "new-tab":
            default: {
              const tab = globalThis.gBrowser?.addTab(submission.uri.spec, {
                triggeringPrincipal: sysPrincipal,
                inBackground: false,
                postData: submission.postData,
              });
              if (globalThis.gBrowser && tab) {
                globalThis.gBrowser.selectedTab = tab;
              }
              break;
            }
          }
        })
        .catch((e) => {
          console.error(
            "[command-palette] Search failed or timed out:",
            e.message,
          );
        });
    } catch (e) {
      console.error("[command-palette] Synchronous error:", e);
    }
  },
};
