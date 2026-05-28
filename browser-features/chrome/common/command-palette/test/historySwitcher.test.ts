// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { getPaletteCommands } from "../command-registry.ts";
import type { CommandStepChoice } from "../command-registry.ts";
import {
  historySwitcherCommand,
  loadHistory,
} from "../multiInputCommand/switcher/history-switcher.ts";

const rawTests: TestCase[] = [
  {
    name: "historySwitcherCommand is registered in palette",
    fn() {
      const commands = getPaletteCommands();
      const ids = commands.map((c) => c.id);
      assert(
        ids.includes("floorp-history-switcher"),
        "should include floorp-history-switcher",
      );
    },
  },
  {
    name: "historySwitcherCommand has required properties",
    fn() {
      assert(
        typeof historySwitcherCommand.id === "string" &&
          historySwitcherCommand.id.length > 0,
        "id should be non-empty string",
      );
      assert(
        typeof historySwitcherCommand.label === "string" &&
          historySwitcherCommand.label.length > 0,
        "label should be non-empty string",
      );
      assert(
        typeof historySwitcherCommand.description === "string" &&
          historySwitcherCommand.description.length > 0,
        "description should be non-empty string",
      );
      assertEquals(
        historySwitcherCommand.category,
        "switcher",
        "category should be 'switcher'",
      );
      assert(
        Array.isArray(historySwitcherCommand.keywords) &&
          historySwitcherCommand.keywords.length > 0,
        "keywords should be non-empty array",
      );
      assert(
        typeof historySwitcherCommand.fn === "function",
        "fn should be a function",
      );
    },
  },
  {
    name: "historySwitcherCommand has one step with choicesLoader",
    fn() {
      assert(
        historySwitcherCommand.steps &&
          historySwitcherCommand.steps.length === 1,
        "should have exactly 1 step",
      );
      const step = historySwitcherCommand.steps![0];
      assert(
        typeof step.id === "string" && step.id.length > 0,
        "step id should be non-empty string",
      );
      assert(
        typeof step.label === "string" && step.label.length > 0,
        "step label should be non-empty string",
      );
      assert(
        typeof step.placeholder === "string" && step.placeholder.length > 0,
        "step placeholder should be non-empty string",
      );
      assert(
        typeof step.choicesLoader === "function",
        "step should have choicesLoader function",
      );
    },
  },
  {
    name: "loadHistory returns StepChoicesResult with pagination metadata",
    async fn() {
      const result = await loadHistory();

      // Should be a StepChoicesResult object (has "choices" property)
      const isResultObject =
        typeof result === "object" && result !== null && "choices" in result;
      assert(isResultObject, "result should be a StepChoicesResult object");

      if (isResultObject) {
        const stepResult = result as {
          choices: Array<{
            label: string;
            value: string;
            description?: string;
          }>;
          hasMore?: boolean;
          loadMore?: unknown;
        };

        assert(Array.isArray(stepResult.choices), "choices should be an array");

        // Validate each choice has required properties
        for (const choice of stepResult.choices) {
          assert(
            typeof choice.label === "string" && choice.label.length > 0,
            "choice label should be non-empty string",
          );
          assert(
            typeof choice.value === "string" && choice.value.length > 0,
            "choice value should be non-empty string",
          );
        }

        // Validate hasMore is a boolean if present
        if (stepResult.hasMore !== undefined) {
          assert(
            typeof stepResult.hasMore === "boolean",
            "hasMore should be boolean",
          );
        }

        // Validate loadMore is a function when hasMore is true
        if (stepResult.hasMore) {
          assert(
            typeof stepResult.loadMore === "function",
            "loadMore should be a function when hasMore is true",
          );
        }
      }
    },
  },
  {
    name: "loadHistory choices have valid URLs",
    async fn() {
      const result = await loadHistory();
      const isResultObject =
        typeof result === "object" && result !== null && "choices" in result;
      if (!isResultObject) return;

      const stepResult = result as {
        choices: Array<{ value: string }>;
      };

      for (const choice of stepResult.choices) {
        assert(
          choice.value.startsWith("http://") ||
            choice.value.startsWith("https://"),
          `choice value should be a URL, got: ${choice.value}`,
        );
      }
    },
  },
  {
    name: "historySwitcherCommand fn does not throw with empty args",
    fn() {
      historySwitcherCommand.fn(window, {});
    },
  },
  {
    name: "historySwitcherCommand fn does not throw with valid URL args",
    fn() {
      historySwitcherCommand.fn(window, {
        history: "https://example.com",
      });
    },
  },
  {
    name: "loadMore returns additional results when hasMore is true",
    async fn() {
      const result = await loadHistory();
      const isResultObject =
        typeof result === "object" && result !== null && "choices" in result;
      if (!isResultObject) return;

      const stepResult = result as {
        choices: CommandStepChoice[];
        hasMore?: boolean;
        loadMore?: () => Promise<{
          choices: CommandStepChoice[];
          hasMore: boolean;
        }>;
      };

      if (stepResult.hasMore && stepResult.loadMore) {
        const { choices: moreChoices, hasMore: updatedHasMore } =
          await stepResult.loadMore();
        assert(
          Array.isArray(moreChoices),
          "loadMore should return choices array",
        );
        assert(
          typeof updatedHasMore === "boolean",
          "loadMore should return hasMore boolean",
        );

        // Verify no overlap with initial choices
        const initialUrls = new Set(stepResult.choices.map((c) => c.value));
        for (const choice of moreChoices) {
          assert(
            !initialUrls.has(choice.value),
            `loadMore result should not overlap with initial: ${choice.value}`,
          );
        }

        // Validate choice structure
        for (const choice of moreChoices) {
          assert(
            typeof choice.label === "string" && choice.label.length > 0,
            "loadMore choice label should be non-empty string",
          );
          assert(
            typeof choice.value === "string" && choice.value.length > 0,
            "loadMore choice value should be non-empty string",
          );
        }
      }
    },
  },
];

export function runAllTests() {
  runTests("historySwitcher.test.ts", rawTests);
}
