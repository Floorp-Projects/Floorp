// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { searchHistory, isHistoryCommand } from "../history-provider.ts";

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

];

export function runAllTests() {
  runTests("providers.test.ts", rawTests);
}
