// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type { PaletteCommand } from "../command-registry.ts";

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
  ],
  fn: (_win: Window, args?: Record<string, string>) => {
    const query = args?.query?.trim();
    console.log("Search query:", query);
    if (!query) return;

    const { SearchService } = ChromeUtils.importESModule(
      "moz-src:///toolkit/components/search/SearchService.sys.mjs",
    );

    try {
      // 1. タイムアウト用のPromiseを作成（2000ミリ秒でエラーを投げる）
      const timeoutPromise = new Promise((_, reject) => {
        globalThis.setTimeout(() => {
          reject(new Error("Search engine timeout"));
        }, 2000);
      });

      // 2. Promise.race で「エンジンの取得」と「タイムアウト」の早い方を取る
      Promise.race([SearchService.getDefault(), timeoutPromise])
        .then((engine) => {
          console.log("Search engine:", engine?.name);

          if (engine) {
            console.log("Submitting search query to engine:", engine.name);

            let sysPrincipal =
              globalThis.Services.scriptSecurityManager.getSystemPrincipal();
            const submission = engine.getSubmission(query);

            let tab = globalThis.gBrowser?.addTab(submission.uri.spec, {
              triggeringPrincipal: sysPrincipal,
              inBackground: false,
              postData: submission.postData,
            });

            if (globalThis.gBrowser && tab) {
              globalThis.gBrowser.selectedTab = tab;
            }
          }
        })
        .catch((e) => {
          // タイムアウトしたか、エンジン取得自体がエラーになった場合の処理
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
