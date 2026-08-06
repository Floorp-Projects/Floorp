// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";
import { CommandPaletteController } from "../controller.ts";
import type { PaletteCommand, CommandStep } from "#features-chrome/common/command-palette/types.ts";
import {
  getPaletteCommands,
  isTabCommand,
} from "#features-chrome/common/command-palette/command-registry.ts";
import { setShortcuts } from "#features-chrome/common/command-palette/config.ts";

function makeStepCommand(steps: CommandStep[], fn?: PaletteCommand["fn"]): PaletteCommand {
  return {
    id: "__test-step-command__",
    label: "Test Command",
    description: "Test command for controller",
    category: "test",
    keywords: [],
    fn: fn ?? ((_win: Window, _args?: Record<string, string>) => {}),
    steps,
  };
}

const STEP_COMMAND_NO_VALIDATE_2: PaletteCommand = makeStepCommand(
  [
    { id: "step1", label: "Step 1", placeholder: "Enter step 1" },
    { id: "step2", label: "Step 2", placeholder: "Enter step 2" },
  ],
);

const STEP_COMMAND_WITH_VALIDATE: PaletteCommand = makeStepCommand([
  {
    id: "validated",
    label: "Validated",
    placeholder: "Enter value",
    validate: (input: string): boolean | string =>
      input.trim() ? true : "Input required",
  },
]);

const STEP_COMMAND_WITH_CHOICES: PaletteCommand = makeStepCommand([
  {
    id: "choiceStep",
    label: "Choose",
    placeholder: "Pick one",
    choices: [
      { label: "Alpha", value: "a" },
      { label: "Beta", value: "b" },
      { label: "Gamma", value: "g" },
    ],
  },
]);

const STEP_COMMAND_WITH_LOADER: PaletteCommand = makeStepCommand([
  {
    id: "loaderStep",
    label: "Load",
    placeholder: "Loading...",
    choicesLoader: () =>
      Promise.resolve([
        { label: "Loaded A", value: "la" },
        { label: "Loaded B", value: "lb" },
      ]),
  },
]);

let capturedArgs: Record<string, string> | undefined;
const STEP_COMMAND_CAPTURE_ARGS: PaletteCommand = makeStepCommand(
  [
    { id: "input", label: "Input", placeholder: "Type" },
  ],
  (_win: Window, args?: Record<string, string>) => {
    capturedArgs = args;
  },
);

function createController(): CommandPaletteController {
  return new CommandPaletteController(window);
}

// ---------------------------------------------------------------------------
// @prefix shortcut tests
// ---------------------------------------------------------------------------
//
// The shortcut feature aliases a user-chosen `@prefix` to an existing palette
// command id (stored in the `floorp.commandPalette.shortcuts` pref). Typing
// `@` lists every shortcut at the top of the palette; `@xxx` filters by
// exact-prefix > starts-with > includes. Each shortcut renders as a pseudo
// `PaletteCommand` with `category: "shortcut"` and a synthetic id
// `__shortcut:<prefix>:<commandId>`. Shortcuts whose target command no longer
// exists are silently dropped, and duplicate prefixes are deduped (first
// declared wins).
//
// `updateSearch("@")` routes through a 30ms debounce (non-empty query), so
// every shortcut test must await a short tick before asserting on
// `filteredCommands()`. Cleanup via `setShortcuts([])` in `finally` keeps the
// shared pref hermetic across the suite.

/**
 * Resolves two stable, non-tab command ids from the live registry. Prefers
 * `floorp-open-settings` / `floorp-open-hub` (top-level Floorp actions
 * registered unconditionally); otherwise falls back to the first two non-tab
 * commands discovered at module load. Used so the shortcut tests can build
 * shortcuts whose targets definitely resolve in the test window.
 */
function resolveKnownCommandIds(): [string, string] {
  const preferred = ["floorp-open-settings", "floorp-open-hub"];
  const present = new Set(getPaletteCommands(window).map((c) => c.id));
  const found = preferred.filter((id) => present.has(id));
  if (found.length >= 2) return [found[0], found[1]];
  const fallback = getPaletteCommands(window)
    .filter((c) => !isTabCommand(c.id))
    .map((c) => c.id);
  return [fallback[0] ?? "__no-command__", fallback[1] ?? "__no-command-2__"];
}

const [KNOWN_ID, KNOWN_ID_2] = resolveKnownCommandIds();

/** Wait long enough for the 30ms debounced updateSearch to flush. */
function flushDebounce(): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, 60));
}

/** Returns only the pseudo shortcut rows from a filtered list. */
function shortcutRows(commands: PaletteCommand[]): PaletteCommand[] {
  return commands.filter((c) => c.category === "shortcut");
}

const shortcutTests: TestCase[] = [
  // --- "@" alone lists every shortcut in declaration order ---
  {
    name: "@ alone lists all shortcuts in declaration order",
    async fn() {
      setShortcuts([
        { prefix: "gh", commandId: KNOWN_ID },
        { prefix: "gp", commandId: KNOWN_ID_2 },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(rows.length, 2, "should list both declared shortcuts");
        assertEquals(
          rows[0].id,
          `__shortcut:gh:${KNOWN_ID}`,
          "first row should be 'gh' (declaration order)",
        );
        assertEquals(
          rows[1].id,
          `__shortcut:gp:${KNOWN_ID_2}`,
          "second row should be 'gp' (declaration order)",
        );
      } finally {
        setShortcuts([]);
      }
    },
  },

  // --- "@gh" exact match pins to top ---
  {
    name: "@<exact> pins the exact-prefix shortcut to the top",
    async fn() {
      setShortcuts([
        { prefix: "gh", commandId: KNOWN_ID },
        { prefix: "gp", commandId: KNOWN_ID_2 },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@gh");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assert(rows.length >= 1, "should have at least one shortcut row");
        assertEquals(
          rows[0].id,
          `__shortcut:gh:${KNOWN_ID}`,
          "exact 'gh' match should be first",
        );
      } finally {
        setShortcuts([]);
      }
    },
  },

  // --- "@g" prefix match ranks both gh and gp (exact-first, then startsWith) ---
  {
    name: "@<partial> ranks exact > startsWith for prefix matches",
    async fn() {
      setShortcuts([
        { prefix: "gh", commandId: KNOWN_ID },
        { prefix: "gp", commandId: KNOWN_ID_2 },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@g");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(rows.length, 2, "both 'gh' and 'gp' start with 'g'");
        assertEquals(
          rows[0].id,
          `__shortcut:gh:${KNOWN_ID}`,
          "'gh' should rank before 'gp' (declaration order among startsWith)",
        );
        assertEquals(
          rows[1].id,
          `__shortcut:gp:${KNOWN_ID_2}`,
          "'gp' should come second",
        );
      } finally {
        setShortcuts([]);
      }
    },
  },

  // --- "@xyz" with no match yields zero shortcut rows ---
  {
    name: "@<no-match> yields zero shortcut rows",
    async fn() {
      setShortcuts([
        { prefix: "gh", commandId: KNOWN_ID },
        { prefix: "gp", commandId: KNOWN_ID_2 },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@xyz");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(rows.length, 0, "no shortcut prefix matches 'xyz'");
      } finally {
        setShortcuts([]);
      }
    },
  },

  // --- Critical #1 regression: shortcut pinned to list head ---
  {
    name: "CRITICAL#1: shortcut is pinned to filteredCommands[0] when present",
    async fn() {
      setShortcuts([
        { prefix: "gh", commandId: KNOWN_ID },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@");
        await flushDebounce();
        const filtered = ctrl.state.filteredCommands();
        assert(filtered.length > 0, "filtered list should be non-empty");
        assertEquals(
          filtered[0].category,
          "shortcut",
          "shortcut must occupy index 0 (list-head pinning)",
        );
      } finally {
        setShortcuts([]);
      }
    },
  },

  // --- Major #4 regression: duplicate prefix dedups (first declared wins) ---
  {
    name: "MAJOR#4: duplicate prefix dedups to first declared (1 row)",
    async fn() {
      setShortcuts([
        { prefix: "gh", commandId: KNOWN_ID },
        { prefix: "gh", commandId: KNOWN_ID_2 },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(
          rows.length,
          1,
          "duplicate prefix should collapse to a single row",
        );
        assertEquals(
          rows[0].id,
          `__shortcut:gh:${KNOWN_ID}`,
          "first-declared commandId should win",
        );
      } finally {
        setShortcuts([]);
      }
    },
  },

  // --- Minor #5 regression: dead commandId is filtered out ---
  {
    name: "MINOR#5: shortcut with non-existent commandId is dropped",
    async fn() {
      setShortcuts([
        { prefix: "dead", commandId: "__nonexistent_command__" },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@dead");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(
          rows.length,
          0,
          "shortcut whose target does not resolve must be omitted",
        );
      } finally {
        setShortcuts([]);
      }
    },
  },

  // --- non-@ query regression: shortcuts never leak into normal search ---
  {
    name: "non-@ query yields zero shortcut rows (normal search untouched)",
    async fn() {
      setShortcuts([
        { prefix: "gh", commandId: KNOWN_ID },
        { prefix: "gp", commandId: KNOWN_ID_2 },
      ]);
      try {
        const ctrl = createController();
        // Use a fragment that is unlikely to coincide with a prefix so the
        // assertion isolates the shortcut-leak guard.
        ctrl.updateSearch("tab");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(
          rows.length,
          0,
          "no shortcut rows should appear for a non-@ query",
        );
      } finally {
        setShortcuts([]);
      }
    },
  },

  // --- Args-bearing shortcut: "@s hello" generates args candidate ---
  {
    name: "@s hello generates args-bearing shortcut candidate pinned to top",
    async fn() {
      setShortcuts([
        { prefix: "s", commandId: "floorp-search-web" },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@s hello");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(rows.length, 1, "should have exactly 1 shortcut row");
        assertEquals(
          rows[0].id,
          "__shortcut:s:floorp-search-web:args",
          "id should end with :args",
        );
        assertEquals(
          rows[0].category,
          "shortcut",
          "category should be shortcut",
        );
        // Must be pinned to top of all filtered results
        const filtered = ctrl.state.filteredCommands();
        assert(filtered.length > 0, "filtered list should be non-empty");
        assertEquals(
          filtered[0].category,
          "shortcut",
          "args shortcut must be pinned to list head",
        );
      } finally {
        setShortcuts([]);
      }
    },
  },

  // --- Args-bearing shortcut: multi-word args "@s hello world" ---
  {
    name: "@s hello world preserves multi-word args in label",
    async fn() {
      setShortcuts([
        { prefix: "s", commandId: "floorp-search-web" },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@s hello world");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(rows.length, 1, "should have exactly 1 shortcut row");
        assertEquals(
          rows[0].id,
          "__shortcut:s:floorp-search-web:args",
          "id should be args variant",
        );
        assert(
          rows[0].label.includes("hello world"),
          `label "${rows[0].label}" should contain "hello world"`,
        );
      } finally {
        setShortcuts([]);
      }
    },
  },

  // --- "@s" alone (no space) uses plain ranking, not args mode ---
  {
    name: "@s alone (no args) uses plain shortcut, not args mode",
    async fn() {
      setShortcuts([
        { prefix: "s", commandId: "floorp-search-web" },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@s");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(rows.length, 1, "should have exactly 1 shortcut row");
        assert(
          !rows[0].id.endsWith(":args"),
          `id "${rows[0].id}" should NOT end with :args for plain shortcut`,
        );
        assertEquals(
          rows[0].id,
          "__shortcut:s:floorp-search-web",
          "should be plain shortcut id",
        );
      } finally {
        setShortcuts([]);
      }
    },
  },

  // --- "@x foo" with non-existent prefix yields zero shortcuts ---
  {
    name: "@x foo with non-existent prefix yields zero shortcut rows",
    async fn() {
      setShortcuts([
        { prefix: "s", commandId: "floorp-search-web" },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@x foo");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(
          rows.length,
          0,
          "no shortcut prefix 'x' exists, so 0 rows",
        );
      } finally {
        setShortcuts([]);
      }
    },
  },

  // --- Non-step command with args falls back to plain shortcut ---
  {
    name: "@gh hello on non-step command falls back to plain shortcut",
    async fn() {
      setShortcuts([
        { prefix: "gh", commandId: KNOWN_ID },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@gh hello");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        // KNOWN_ID (floorp-open-settings or similar) has no steps, so the
        // controller falls back to a plain (args-less) shortcut candidate.
        assert(rows.length >= 1, "should have at least 1 shortcut row");
        assert(
          !rows[0].id.endsWith(":args"),
          `id "${rows[0].id}" should NOT have :args suffix for non-step command`,
        );
      } finally {
        setShortcuts([]);
      }
    },
  },
];

const rawTests: TestCase[] = [
  // --- Controller instantiation ---
  {
    name: "controller constructs with window",
    fn() {
      const ctrl = createController();
      assert(ctrl !== null, "controller should be created");
      assertEquals(ctrl.state.mode(), "command", "initial mode should be command");
      assertEquals(ctrl.state.isVisible(), false, "initial visibility should be false");
    },
  },

  // --- enterInputMode via executeCommand ---
  {
    name: "executeCommand with steps enters input mode",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_NO_VALIDATE_2);
      assertEquals(ctrl.state.mode(), "input", "mode should be input");
      assertEquals(ctrl.state.activeCommand()?.id, "__test-step-command__", "active command should be set");
      assertEquals(ctrl.state.currentStepIndex(), 0, "step index should be 0");
      assertEquals(ctrl.state.stepInputs(), {}, "step inputs should be empty");
      assertEquals(ctrl.state.stepError(), null, "step error should be null");
    },
  },

  // --- advanceStep progression ---
  {
    name: "advanceStep progresses through steps",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_NO_VALIDATE_2);
      ctrl.updateSearch("value1");
      ctrl.advanceStep();
      assertEquals(ctrl.state.currentStepIndex(), 1, "should advance to step 1");
      assertEquals(ctrl.state.stepInputs().step1, "value1", "step1 input should be saved");
    },
  },
  {
    name: "advanceStep at last step executes fn with collected args",
    fn() {
      capturedArgs = undefined;
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_CAPTURE_ARGS);
      ctrl.updateSearch("final value");
      ctrl.advanceStep();
      assertEquals((capturedArgs as unknown as Record<string, string>)?.input, "final value", "fn should receive collected args");
    },
  },

  // --- advanceStep with validation ---
  {
    name: "advanceStep with validation failure sets error and stays",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_VALIDATE);
      ctrl.updateSearch("   ");
      ctrl.advanceStep();
      assertEquals(typeof ctrl.state.stepError(), "string", "should have error string");
      assertEquals(ctrl.state.currentStepIndex(), 0, "should stay on step 0");
    },
  },
  {
    name: "advanceStep with validation pass clears error",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_VALIDATE);
      ctrl.updateSearch("   ");
      ctrl.advanceStep();
      assert(ctrl.state.stepError() !== null, "should have error after empty input");
      ctrl.updateSearch("valid");
      ctrl.advanceStep();
      assertEquals(ctrl.state.stepError(), null, "error should be cleared after valid input");
    },
  },

  // --- goBackStep ---
  {
    name: "goBackStep returns to previous step",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_NO_VALIDATE_2);
      ctrl.updateSearch("value1");
      ctrl.advanceStep();
      assertEquals(ctrl.state.currentStepIndex(), 1, "should be on step 1");
      ctrl.goBackStep();
      assertEquals(ctrl.state.currentStepIndex(), 0, "should return to step 0");
    },
  },
  {
    name: "goBackStep at step 0 exits to command mode",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_NO_VALIDATE_2);
      assertEquals(ctrl.state.mode(), "input", "should be in input mode");
      ctrl.goBackStep();
      assertEquals(ctrl.state.mode(), "command", "should return to command mode");
      assertEquals(ctrl.state.activeCommand(), null, "active command should be null");
    },
  },

  // --- loadStepChoices with static choices ---
  {
    name: "enterInputMode with choices populates filteredStepChoices",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_CHOICES);
      assertEquals(ctrl.state.filteredStepChoices().length, 3, "should have 3 choices");
      assertEquals(ctrl.state.filteredStepChoices()[0].value, "a", "first choice should be Alpha");
    },
  },

  // --- loadStepChoices with choicesLoader ---
  {
    name: "enterInputMode with choicesLoader loads async choices",
    async fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_LOADER);
      assertEquals(ctrl.state.stepChoicesLoading(), true, "should be loading");
      // Wait for choicesLoader promise to resolve
      await new Promise((resolve) => setTimeout(resolve, 100));
      assertEquals(ctrl.state.stepChoicesLoading(), false, "loading should be done");
      assertEquals(ctrl.state.filteredStepChoices().length, 2, "should have 2 loaded choices");
      assertEquals(ctrl.state.filteredStepChoices()[0].value, "la", "first loaded choice value");
    },
  },

  // --- updateStepChoices filtering ---
  {
    name: "updateSearch filters choices in input mode",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_CHOICES);
      assertEquals(ctrl.state.filteredStepChoices().length, 3, "should start with 3 choices");
      ctrl.updateSearch("alp");
      assertEquals(ctrl.state.filteredStepChoices().length, 1, "should filter to 1 choice");
      assertEquals(ctrl.state.filteredStepChoices()[0].value, "a", "filtered choice should be Alpha");
    },
  },
  {
    name: "empty query restores all choices",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_CHOICES);
      ctrl.updateSearch("alp");
      assertEquals(ctrl.state.filteredStepChoices().length, 1, "should be filtered");
      ctrl.updateSearch("");
      assertEquals(ctrl.state.filteredStepChoices().length, 3, "should restore all choices");
    },
  },
];

export function runAllTests(): void {
  runTests("commandPaletteController.test.ts", [...rawTests, ...shortcutTests]);
}
