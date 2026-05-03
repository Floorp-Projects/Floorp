// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { fuzzyScore, fuzzySearch } from "../fuzzy.ts";
import type { FuzzyTarget } from "../fuzzy.ts";
import { getPaletteCommands, searchCommands, getCommand } from "../command-registry.ts";

const makeTarget = (label: string, desc = "", category = "test", keywords: string[] = []): FuzzyTarget => ({
  id: label.toLowerCase().replace(/\s+/g, "-"),
  label,
  description: desc,
  category,
  keywords,
});

const rawTests: TestCase[] = [
  // --- Fuzzy Search ---
  {
    name: "fuzzyScore: exact prefix match scores highest",
    fn() {
      const target = makeTarget("New Tab");
      const score = fuzzyScore("new", target);
      assert(score >= 100, `prefix score should be >= 100, got ${score}`);
    },
  },
  {
    name: "fuzzyScore: substring match scores lower than prefix",
    fn() {
      const target = makeTarget("Open New Tab");
      const prefixScore = fuzzyScore("open", target);
      const subScore = fuzzyScore("new", target);
      assert(prefixScore > subScore, `prefix (${prefixScore}) should score higher than substring (${subScore})`);
    },
  },
  {
    name: "fuzzyScore: no match returns 0",
    fn() {
      const target = makeTarget("Reload");
      const score = fuzzyScore("xyz", target);
      assertEquals(score, 0, "no match should return 0");
    },
  },
  {
    name: "fuzzyScore: keyword match scores",
    fn() {
      const target = makeTarget("Screenshot", "", "page", ["capture", "screen"]);
      const score = fuzzyScore("capture", target);
      assert(score > 0, `keyword match should score > 0, got ${score}`);
    },
  },
  {
    name: "fuzzySearch: returns filtered and sorted results",
    fn() {
      const items = [
        makeTarget("Open New Tab"),
        makeTarget("Close Tab"),
        makeTarget("New Window"),
        makeTarget("Reload"),
      ];
      const results = fuzzySearch("new", items);
      assert(results.length > 0, "should have results");
      // "New Window" has prefix match, "Open New Tab" has substring
      assert(results.length <= 3, "should have at most 3 matches");
    },
  },
  {
    name: "fuzzySearch: empty query returns all items",
    fn() {
      const items = [
        makeTarget("A"),
        makeTarget("B"),
        makeTarget("C"),
      ];
      const results = fuzzySearch("", items);
      assertEquals(results.length, 3, "empty query should return all items");
    },
  },
  {
    name: "fuzzySearch: respects limit parameter",
    fn() {
      const items = Array.from({ length: 100 }, (_, i) => makeTarget(`Item ${i}`));
      const results = fuzzySearch("item", items, 5);
      assertEquals(results.length, 5, "should respect limit parameter");
    },
  },

  // --- Command Registry ---
  {
    name: "getPaletteCommands returns non-empty array",
    fn() {
      const commands = getPaletteCommands();
      assert(Array.isArray(commands), "should return an array");
      assert(commands.length > 0, "should have registered commands");
    },
  },
  {
    name: "each palette command has required properties",
    fn() {
      const commands = getPaletteCommands();
      for (const cmd of commands) {
        assert(typeof cmd.id === "string" && cmd.id.length > 0, `id should be non-empty string, got: ${cmd.id}`);
        assert(typeof cmd.label === "string" && cmd.label.length > 0, `label should be non-empty string for ${cmd.id}`);
        assert(typeof cmd.category === "string", `category should be string for ${cmd.id}`);
        assert(typeof cmd.fn === "function", `fn should be function for ${cmd.id}`);
        assert(Array.isArray(cmd.keywords), `keywords should be array for ${cmd.id}`);
      }
    },
  },
  {
    name: "command IDs are unique",
    fn() {
      const commands = getPaletteCommands();
      const ids = commands.map((c) => c.id);
      const uniqueIds = new Set(ids);
      assertEquals(ids.length, uniqueIds.size, "all command IDs should be unique");
    },
  },
  {
    name: "getCommand returns correct command by id",
    fn() {
      const cmd = getCommand("gecko-open-new-tab");
      assert(cmd !== undefined, "should find gecko-open-new-tab");
      assertEquals(cmd.id, "gecko-open-new-tab", "should return correct command id");
    },
  },
  {
    name: "getCommand returns undefined for unknown id",
    fn() {
      const cmd = getCommand("nonexistent-action");
      assertEquals(cmd, undefined, "should return undefined for unknown id");
    },
  },
  {
    name: "searchCommands returns results for tab query",
    fn() {
      const results = searchCommands("tab");
      assert(results.length > 0, "should find tab-related commands");
      const labels = results.map((r) => r.label.toLowerCase());
      assert(labels.some((l) => l.includes("tab")), "results should contain tab-related items");
    },
  },
  {
    name: "all commands have a valid category",
    fn() {
      const validCategories = new Set([
        "navigation", "tabs", "zoom", "bookmarks", "page", "search",
        "sidebar", "scrolling", "history", "window", "tools", "downloads",
        "workspace", "floorp", "media",
      ]);
      const commands = getPaletteCommands();
      for (const cmd of commands) {
        assert(validCategories.has(cmd.category), `unknown category "${cmd.category}" for ${cmd.id}`);
      }
    },
  },
];

export function runAllTests() {
  runTests("commandPalette.test.ts", rawTests);
}
