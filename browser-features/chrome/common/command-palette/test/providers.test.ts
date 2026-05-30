// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { searchHistory, isHistoryCommand } from "../history-provider.ts";
import { searchBookmarks, isBookmarkCommand } from "../bookmark-provider.ts";

const REQUIRED_COMMAND_FIELDS = [
  "id",
  "label",
  "description",
  "category",
  "keywords",
  "fn",
] as const;

function assertHasCommandFields(
  cmd: Record<string, unknown>,
  prefix: string,
): void {
  for (const field of REQUIRED_COMMAND_FIELDS) {
    assert(
      field in cmd,
      `${prefix} missing required field "${field}", got keys: ${Object.keys(cmd).join(", ")}`,
    );
  }
  assert(
    typeof cmd.fn === "function",
    `${prefix} field "fn" should be a function`,
  );
  assert(
    Array.isArray(cmd.keywords),
    `${prefix} field "keywords" should be an array`,
  );
}

const rawTests: TestCase[] = [
  // --- History Provider: isHistoryCommand ---
  {
    name: "isHistoryCommand returns true for history IDs",
    fn() {
      assert(
        isHistoryCommand("__history__abc"),
        'isHistoryCommand("__history__abc") should return true',
      );
      assert(
        isHistoryCommand("__history__"),
        'isHistoryCommand("__history__") should return true',
      );
    },
  },
  {
    name: "isHistoryCommand returns false for non-history IDs",
    fn() {
      assert(
        !isHistoryCommand("some-other-id"),
        'isHistoryCommand("some-other-id") should return false',
      );
      assert(
        !isHistoryCommand("__tab__something"),
        'isHistoryCommand("__tab__something") should return false',
      );
      assert(!isHistoryCommand(""), "isHistoryCommand('') should return false");
    },
  },

  // --- History Provider: searchHistory ---
  {
    name: "searchHistory returns PaletteCommand array with correct category",
    async fn() {
      const results = await searchHistory("test");
      for (const cmd of results) {
        assertEquals(
          cmd.category,
          "history-suggestions",
          `history command "${cmd.label}" should have category "history-suggestions"`,
        );
        assert(
          cmd.id.startsWith("__history__"),
          `history command id "${cmd.id}" should start with "__history__"`,
        );
      }
    },
  },
  {
    name: "searchHistory returns array for empty query",
    async fn() {
      const results = await searchHistory("");
      // Empty query may still return results via Places API — just verify array
      assert(Array.isArray(results), "searchHistory should return an array");
    },
  },
  {
    name: "searchHistory results have required PaletteCommand fields",
    async fn() {
      const results = await searchHistory("test");
      for (let i = 0; i < results.length; i++) {
        assertHasCommandFields(
          results[i] as unknown as Record<string, unknown>,
          `searchHistory result[${i}]`,
        );
      }
    },
  },

  // --- Bookmark Provider: isBookmarkCommand ---
  {
    name: "isBookmarkCommand returns true for bookmark IDs",
    fn() {
      assert(
        isBookmarkCommand("__bookmark__abc"),
        'isBookmarkCommand("__bookmark__abc") should return true',
      );
      assert(
        isBookmarkCommand("__bookmark__"),
        'isBookmarkCommand("__bookmark__") should return true',
      );
    },
  },
  {
    name: "isBookmarkCommand returns false for non-bookmark IDs",
    fn() {
      assert(
        !isBookmarkCommand("some-other-id"),
        'isBookmarkCommand("some-other-id") should return false',
      );
      assert(
        !isBookmarkCommand("__history__something"),
        'isBookmarkCommand("__history__something") should return false',
      );
      assert(
        !isBookmarkCommand(""),
        "isBookmarkCommand('') should return false",
      );
    },
  },

  // --- Bookmark Provider: searchBookmarks ---
  {
    name: "searchBookmarks returns PaletteCommand array with correct category",
    async fn() {
      const results = await searchBookmarks("test");
      for (const cmd of results) {
        assertEquals(
          cmd.category,
          "bookmark-suggestions",
          `bookmark command "${cmd.label}" should have category "bookmark-suggestions"`,
        );
        assert(
          cmd.id.startsWith("__bookmark__"),
          `bookmark command id "${cmd.id}" should start with "__bookmark__"`,
        );
      }
    },
  },
  {
    name: "searchBookmarks returns array for empty query",
    async fn() {
      const results = await searchBookmarks("");
      assert(Array.isArray(results), "searchBookmarks should return an array");
    },
  },
  {
    name: "searchBookmarks results have required PaletteCommand fields",
    async fn() {
      const results = await searchBookmarks("test");
      for (let i = 0; i < results.length; i++) {
        assertHasCommandFields(
          results[i] as unknown as Record<string, unknown>,
          `searchBookmarks result[${i}]`,
        );
      }
    },
  },

  // --- Bookmark Provider: result type safety ---
  {
    name: "searchBookmarks results have string labels (not URL objects)",
    async fn() {
      const results = await searchBookmarks("floorp");
      for (let i = 0; i < results.length; i++) {
        assert(
          typeof results[i].label === "string",
          `bookmark result[${i}] label should be string, got ${typeof results[i].label}`,
        );
        assert(
          typeof results[i].description === "string",
          `bookmark result[${i}] description should be string, got ${typeof results[i].description}`,
        );
      }
    },
  },
  {
    name: "searchBookmarks result IDs use __bookmark__ prefix",
    async fn() {
      const results = await searchBookmarks("floorp");
      for (const cmd of results) {
        assert(
          cmd.id.startsWith("__bookmark__"),
          `bookmark id "${cmd.id}" should start with "__bookmark__"`,
        );
        assert(
          isBookmarkCommand(cmd.id),
          `isBookmarkCommand should return true for "${cmd.id}"`,
        );
      }
    },
  },
  {
    name: "searchBookmarks does not return duplicate URLs",
    async fn() {
      const results = await searchBookmarks("floorp");
      const descriptions = results.map((c) => c.description);
      const uniqueDescriptions = new Set(descriptions);
      assertEquals(
        descriptions.length,
        uniqueDescriptions.size,
        "bookmark results should not contain duplicate URLs",
      );
    },
  },
  {
    name: "isBookmarkCommand and isHistoryCommand do not overlap",
    fn() {
      const bookmarkId = "__bookmark__https://example.com";
      const historyId = "__history__https://example.com";
      assert(
        isBookmarkCommand(bookmarkId) && !isHistoryCommand(bookmarkId),
        "bookmark ID should only match isBookmarkCommand",
      );
      assert(
        isHistoryCommand(historyId) && !isBookmarkCommand(historyId),
        "history ID should only match isHistoryCommand",
      );
    },
  },
  {
    name: "searchBookmarks keywords contain URL strings",
    async fn() {
      const results = await searchBookmarks("floorp");
      for (const cmd of results) {
        for (const kw of cmd.keywords) {
          assert(
            typeof kw === "string",
            `keyword "${kw}" should be a string, got ${typeof kw}`,
          );
        }
      }
    },
  },
];

export function runAllTests() {
  runTests("providers.test.ts", rawTests);
}
