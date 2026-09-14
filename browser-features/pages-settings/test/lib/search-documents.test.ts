// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { createInstance } from "i18next";
import {
  buildSearchDocuments,
  normalizeSearchText,
} from "../../src/lib/search/index.tsx";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

const tests: TestCase[] = [
  {
    name:
      "Search includes translated and fallback setting labels without rendering pages",
    fn: async () => {
      const i18n = createInstance();
      await i18n.init({
        lng: "ja-JP",
        fallbackLng: "en-US",
        initImmediate: false,
        defaultNS: "translations",
        ns: ["translations"],
        resources: {
          "en-US": {
            translations: {
              pages: { workspaces: "Workspaces" },
              workspaces: {
                title: "Workspaces",
                toolbar: "Show workspace name on toolbar",
                showWorkspaceNameOnToolbar:
                  "Show workspace name next to the icon",
              },
            },
          },
          "ja-JP": {
            translations: {
              pages: { workspaces: "ワークスペース" },
              workspaces: { title: "作業環境" },
            },
          },
        },
      });
      const documents = buildSearchDocuments(i18n);
      const workspace = documents.find((document) =>
        document.id === "workspaces"
      );
      assert(workspace, "Workspace search document exists");
      assertEquals(workspace!.title, "ワークスペース", "Localized title");
      assert(
        workspace!.textContent.includes("作業環境"),
        "Localized setting labels are indexed",
      );
      assert(
        workspace!.textContent.includes("Show workspace name on toolbar"),
        "Missing locale leaves use the UI fallback",
      );
      assertEquals(
        workspace!.route,
        "/features/workspaces",
        "Existing deep link remains stable",
      );
      const field = documents.find((document) =>
        document.route === "/features/workspaces?setting=show-name"
      );
      assert(field, "A stable input destination is indexed");
      assertEquals(
        field!.title,
        "Show workspace name next to the icon",
        "Field title uses fallback translation",
      );
      assertEquals(
        new Set(documents.map((document) => document.id)).size,
        documents.length,
        "Document IDs are unique",
      );
    },
  },
  {
    name: "Search normalizes case, width and diacritics",
    fn: () => {
      assertEquals(
        normalizeSearchText("  ＦＬＯＯＲＰ  Café "),
        "floorp cafe",
        "Normalized query",
      );
    },
  },
];
export async function runAllTests(): Promise<void> {
  await runTests("search-documents.test.ts", tests);
}
