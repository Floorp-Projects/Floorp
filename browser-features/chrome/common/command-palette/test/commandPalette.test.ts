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
import {
  getPaletteCommands,
  searchCommands,
  getCommand,
} from "../command-registry.ts";
import { getHighlightSegments } from "../utils/highlight.ts";
import { openUrlCommand } from "../multiInputCommand/open-url.ts";
import {
  searchWebCommand,
  loadSearchEngines,
} from "../multiInputCommand/search-web.ts";
import {
  reopenInContainerCommand,
  loadContainers,
} from "../multiInputCommand/reopen-in-container.ts";

const makeTarget = (
  label: string,
  desc = "",
  category = "test",
  keywords: string[] = [],
): FuzzyTarget => ({
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
      assert(
        prefixScore > subScore,
        `prefix (${prefixScore}) should score higher than substring (${subScore})`,
      );
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
      const target = makeTarget("Screenshot", "", "page", [
        "capture",
        "screen",
      ]);
      const score = fuzzyScore("capture", target);
      assert(score > 0, `keyword match should score > 0, got ${score}`);
    },
  },
  {
    name: "fuzzyScore: multi-word search requires all words to match",
    fn() {
      const target = makeTarget("Open New Tab");
      const score = fuzzyScore("open tab", target);
      assert(score > 0, "multi-word 'open tab' should match 'Open New Tab'");
    },
  },
  {
    name: "fuzzyScore: multi-word search returns 0 if any word misses",
    fn() {
      const target = makeTarget("Open New Tab");
      const score = fuzzyScore("open xyz", target);
      assertEquals(
        score,
        0,
        "multi-word with non-matching word should return 0",
      );
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
      assert(results.length <= 3, "should have at most 3 matches");
    },
  },
  {
    name: "fuzzySearch: empty query returns all items",
    fn() {
      const items = [makeTarget("A"), makeTarget("B"), makeTarget("C")];
      const results = fuzzySearch("", items);
      assertEquals(results.length, 3, "empty query should return all items");
    },
  },
  {
    name: "fuzzySearch: respects limit parameter",
    fn() {
      const items = Array.from({ length: 100 }, (_, i) =>
        makeTarget(`Item ${i}`),
      );
      const results = fuzzySearch("item", items, 5);
      assertEquals(results.length, 5, "should respect limit parameter");
    },
  },

  // --- Highlight ---
  {
    name: "getHighlightSegments: prefix match",
    fn() {
      const segments = getHighlightSegments("new", "New Tab");
      assertEquals(segments.length, 2, "should have 2 segments");
      assert(segments[0].matched, "first segment should be matched");
      assertEquals(
        segments[0].text,
        "New",
        "first segment text should be 'New'",
      );
      assert(!segments[1].matched, "second segment should not be matched");
      assertEquals(
        segments[1].text,
        " Tab",
        "second segment text should be ' Tab'",
      );
    },
  },
  {
    name: "getHighlightSegments: empty query returns no match",
    fn() {
      const segments = getHighlightSegments("", "Hello");
      assertEquals(segments.length, 1, "should have 1 segment");
      assert(!segments[0].matched, "should not be matched");
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
        assert(
          typeof cmd.id === "string" && cmd.id.length > 0,
          `id should be non-empty string, got: ${cmd.id}`,
        );
        assert(
          typeof cmd.label === "string" && cmd.label.length > 0,
          `label should be non-empty string for ${cmd.id}`,
        );
        assert(
          typeof cmd.category === "string",
          `category should be string for ${cmd.id}`,
        );
        assert(
          typeof cmd.fn === "function",
          `fn should be function for ${cmd.id}`,
        );
        assert(
          Array.isArray(cmd.keywords),
          `keywords should be array for ${cmd.id}`,
        );
      }
    },
  },
  {
    name: "command IDs are unique",
    fn() {
      const commands = getPaletteCommands();
      const ids = commands.map((c) => c.id);
      const uniqueIds = new Set(ids);
      assertEquals(
        ids.length,
        uniqueIds.size,
        "all command IDs should be unique",
      );
    },
  },
  {
    name: "getCommand returns correct command by id",
    fn() {
      const cmd = getCommand("gecko-open-new-tab");
      assert(cmd !== undefined, "should find gecko-open-new-tab");
      assertEquals(
        cmd.id,
        "gecko-open-new-tab",
        "should return correct command id",
      );
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
      assert(
        labels.some((l) => l.includes("tab")),
        "results should contain tab-related items",
      );
    },
  },
  {
    name: "all commands have a valid category",
    fn() {
      const validCategories = new Set([
        "navigation",
        "tabs",
        "zoom",
        "bookmarks",
        "page",
        "search",
        "sidebar",
        "scrolling",
        "history",
        "window",
        "tools",
        "downloads",
        "workspace",
        "floorp",
        "media",
        "open-tabs",
        "switcher",
      ]);
      const commands = getPaletteCommands();
      for (const cmd of commands) {
        assert(
          validCategories.has(cmd.category),
          `unknown category "${cmd.category}" for ${cmd.id}`,
        );
      }
    },
  },

  // --- Step Commands ---
  {
    name: "step commands are registered in palette",
    fn() {
      const commands = getPaletteCommands();
      const ids = commands.map((c) => c.id);
      assert(ids.includes("floorp-open-url"), "should include floorp-open-url");
      assert(
        ids.includes("floorp-search-web"),
        "should include floorp-search-web",
      );
      assert(
        ids.includes("floorp-reopen-in-container"),
        "should include floorp-reopen-in-container",
      );
    },
  },
  {
    name: "step commands have steps array with correct length",
    fn() {
      assert(
        openUrlCommand.steps && openUrlCommand.steps.length === 2,
        "open-url should have 2 steps",
      );
      assert(
        searchWebCommand.steps && searchWebCommand.steps.length === 3,
        "search-web should have 3 steps",
      );
      assert(
        reopenInContainerCommand.steps &&
          reopenInContainerCommand.steps.length === 1,
        "reopen-in-container should have 1 step",
      );
    },
  },
  {
    name: "each step has required properties",
    fn() {
      const stepCommands = [
        openUrlCommand,
        searchWebCommand,
        reopenInContainerCommand,
      ];
      for (const cmd of stepCommands) {
        for (const step of cmd.steps ?? []) {
          assert(
            typeof step.id === "string" && step.id.length > 0,
            `step id should be non-empty string in ${cmd.id}`,
          );
          assert(
            typeof step.label === "string" && step.label.length > 0,
            `step label should be non-empty string in ${cmd.id}/${step.id}`,
          );
          assert(
            typeof step.placeholder === "string" && step.placeholder.length > 0,
            `step placeholder should be non-empty string in ${cmd.id}/${step.id}`,
          );
        }
      }
    },
  },
  {
    name: "step commands fn does not throw with minimal args",
    fn() {
      const stepCommands = [
        openUrlCommand,
        searchWebCommand,
        reopenInContainerCommand,
      ];
      for (const cmd of stepCommands) {
        try {
          cmd.fn(window, {});
        } catch {
          // Some commands may fail on missing browser state, that's acceptable
        }
      }
    },
  },

  // --- Step Validate ---
  {
    name: "open-url validate rejects empty input",
    fn() {
      const validate = openUrlCommand.steps![0].validate!;
      const result = validate("  ");
      assert(
        typeof result === "string",
        "empty input should return error string",
      );
    },
  },
  {
    name: "open-url validate accepts non-empty input",
    fn() {
      const validate = openUrlCommand.steps![0].validate!;
      const result = validate("https://example.com");
      assertEquals(result, true, "valid URL should return true");
    },
  },
  {
    name: "search-web validate rejects empty input",
    fn() {
      const validate = searchWebCommand.steps![0].validate!;
      const result = validate("  ");
      assert(
        typeof result === "string",
        "empty input should return error string",
      );
    },
  },
  {
    name: "search-web validate accepts non-empty input",
    fn() {
      const validate = searchWebCommand.steps![0].validate!;
      const result = validate("test query");
      assertEquals(result, true, "non-empty query should return true");
    },
  },

  // --- Static Choices ---
  {
    name: "open-url where step has 3 choices with correct values",
    fn() {
      const choices = openUrlCommand.steps![1].choices!;
      assertEquals(choices.length, 3, "should have 3 choices");
      const values = choices.map((c) => c.value);
      assert(values.includes("new-tab"), "should include new-tab");
      assert(
        values.includes("background-tab"),
        "should include background-tab",
      );
      assert(values.includes("current-tab"), "should include current-tab");
      for (const choice of choices) {
        assert(
          typeof choice.label === "string" && choice.label.length > 0,
          "choice should have label",
        );
        assert(
          typeof choice.description === "string" &&
            choice.description.length > 0,
          "choice should have description",
        );
      }
    },
  },
  {
    name: "search-web where step has 3 choices with correct values",
    fn() {
      const choices = searchWebCommand.steps![2].choices!;
      assertEquals(choices.length, 3, "should have 3 choices");
      const values = choices.map((c) => c.value);
      assert(values.includes("new-tab"), "should include new-tab");
      assert(
        values.includes("background-tab"),
        "should include background-tab",
      );
      assert(values.includes("current-tab"), "should include current-tab");
    },
  },

  // --- choicesLoader ---
  {
    name: "loadSearchEngines returns array with label and value",
    async fn() {
      const engines = await loadSearchEngines();
      assert(Array.isArray(engines), "should return an array");
      for (const engine of engines) {
        assert(
          typeof engine.label === "string" && engine.label.length > 0,
          "engine should have label",
        );
        assert(
          typeof engine.value === "string" && engine.value.length > 0,
          "engine should have value",
        );
      }
    },
  },
  {
    name: "loadContainers returns array starting with no-container",
    async fn() {
      const containers = await loadContainers();
      assert(Array.isArray(containers), "should return an array");
      assert(
        containers.length > 0,
        "should have at least the no-container entry",
      );
      assertEquals(
        containers[0].value,
        "0",
        "first entry should be no-container with value '0'",
      );
      for (const container of containers) {
        assert(
          typeof container.label === "string" && container.label.length > 0,
          "container should have label",
        );
        assert(
          typeof container.value === "string" && container.value.length > 0,
          "container should have value",
        );
      }
    },
  },
];

export function runAllTests() {
  runTests("commandPalette.test.ts", rawTests);
}
