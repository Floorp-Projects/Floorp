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
  getCommand,
  getPaletteCommands,
  isTabCommand,
} from "#features-chrome/common/command-palette/command-registry.ts";
import { setShortcuts } from "#features-chrome/common/command-palette/config.ts";
import { getTabCommands } from "#features-chrome/common/command-palette/tab-provider.ts";

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
// `filteredCommands()`. Every test snapshots the `floorp.commandPalette.shortcuts`
// pref before mutating it and restores the snapshot in `finally` (see
// `snapshotShortcutsPref` / `restoreShortcutsPref` above), keeping the shared
// pref hermetic across the suite — and across real dev profiles.

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

// ---------------------------------------------------------------------------
// Shortcuts-pref snapshot / restore
// ---------------------------------------------------------------------------
//
// `setShortcuts(...)` persists through the config module's effect, which writes
// the new value into the `floorp.commandPalette.shortcuts` pref. A test that
// only calls `setShortcuts([])` in its `finally` therefore leaves the pref
// overwritten with "[]" — destroying a real user's shortcuts in a dev profile.
// Every test that mutates the shortcuts pref therefore snapshots the raw pref
// value BEFORE mutating and restores it in `finally`. Setting (or clearing) the
// pref fires the config module's pref observer, which resyncs the in-memory
// shortcuts signal, so the restore fully undoes the mutation.

const SHORTCUTS_PREF = "floorp.commandPalette.shortcuts";

interface ShortcutsPrefSnapshot {
  hadUserValue: boolean;
  value: string | null;
}

function snapshotShortcutsPref(): ShortcutsPrefSnapshot {
  const hadUserValue = Services.prefs.prefHasUserValue(SHORTCUTS_PREF);
  return {
    hadUserValue,
    value: hadUserValue ? Services.prefs.getStringPref(SHORTCUTS_PREF) : null,
  };
}

function restoreShortcutsPref(snapshot: ShortcutsPrefSnapshot): void {
  if (snapshot.hadUserValue && snapshot.value !== null) {
    Services.prefs.setStringPref(SHORTCUTS_PREF, snapshot.value);
  } else if (Services.prefs.prefHasUserValue(SHORTCUTS_PREF)) {
    Services.prefs.clearUserPref(SHORTCUTS_PREF);
  }
}

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
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
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
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@gh" exact match pins to top ---
  {
    name: "@<exact> pins the exact-prefix shortcut to the top",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
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
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@g" prefix match ranks both gh and gp (exact-first, then startsWith) ---
  {
    name: "@<partial> ranks exact > startsWith for prefix matches",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
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
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@xyz" with no match yields zero shortcut rows ---
  {
    name: "@<no-match> yields zero shortcut rows",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
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
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- Critical #1 regression: shortcut pinned to list head ---
  {
    name: "CRITICAL#1: shortcut is pinned to filteredCommands[0] when present",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
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
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- Major #4 regression: duplicate prefix dedups (first declared wins) ---
  {
    name: "MAJOR#4: duplicate prefix dedups to first declared (1 row)",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
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
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- Minor #5 regression: dead commandId is filtered out ---
  {
    name: "MINOR#5: shortcut with non-existent commandId is dropped",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
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
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- non-@ query regression: shortcuts never leak into normal search ---
  {
    name: "non-@ query yields zero shortcut rows (normal search untouched)",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
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
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- Args-bearing shortcut: "@s hello" generates args candidate ---
  {
    name: "@s hello generates args-bearing shortcut candidate pinned to top",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
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
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- Args-bearing shortcut: multi-word args "@s hello world" ---
  {
    name: "@s hello world preserves multi-word args in label",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
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
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@s" alone (no space) uses plain ranking, not args mode ---
  {
    name: "@s alone (no args) uses plain shortcut, not args mode",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
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
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@x foo" with non-existent prefix yields zero shortcuts ---
  {
    name: "@x foo with non-existent prefix yields zero shortcut rows",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
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
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- Non-step command with args falls back to plain shortcut ---
  {
    name: "@gh hello on non-step command falls back to plain shortcut",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
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
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- @s built-in web search (reserved prefix, independent of the pref) ---
  //
  // "@s" is a BUILT-IN reserved prefix (see RESERVED_SHORTCUT_PREFIXES in
  // config.ts): the controller resolves `floorp-search-web` directly, so it
  // works even when the shortcuts pref is empty, and it always beats any
  // user-defined "s" shortcut (e.g. a stale entry persisted before the prefix
  // was reserved). These tests are skipped when `floorp-search-web` is not
  // registered in the test window.
  {
    name: "@s lists floorp-search-web even with an empty shortcuts pref",
    async fn() {
      if (!getCommand("floorp-search-web", window)) {
        return; // floorp-search-web not registered in this environment — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@s");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(
          rows.length,
          1,
          "built-in @s should yield exactly one shortcut row",
        );
        assertEquals(
          rows[0].id,
          "__shortcut:s:floorp-search-web",
          "built-in @s should resolve to floorp-search-web",
        );
        assertEquals(
          rows[0].category,
          "shortcut",
          "category should be shortcut",
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  {
    name: "@s hello enters args mode with an empty shortcuts pref (built-in)",
    async fn() {
      if (!getCommand("floorp-search-web", window)) {
        return; // floorp-search-web not registered in this environment — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@s hello");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(
          rows.length,
          1,
          "built-in @s with args should yield exactly one shortcut row",
        );
        assertEquals(
          rows[0].id,
          "__shortcut:s:floorp-search-web:args",
          "@s <query> should produce the args-bearing row",
        );
        assertEquals(
          ctrl.state.highlightQuery(),
          "hello",
          "@s <query> should highlight only the args part",
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  {
    name: "@s built-in wins over a stale user-defined 's' shortcut",
    async fn() {
      if (!getCommand("floorp-search-web", window)) {
        return; // floorp-search-web not registered in this environment — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      // A stale user "s" entry pointing at a DIFFERENT command must be ignored.
      setShortcuts([{ prefix: "s", commandId: KNOWN_ID }]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@s");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(
          rows.length,
          1,
          "built-in @s should yield exactly one shortcut row",
        );
        assertEquals(
          rows[0].id,
          "__shortcut:s:floorp-search-web",
          "built-in @s must beat a stale user 's' shortcut",
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },
];

// ---------------------------------------------------------------------------
// @t tab-search tests
// ---------------------------------------------------------------------------
//
// "@t" is the built-in open-tabs search mode: typing "@t" lists every open tab
// (id `__tab__<id>`, category "open-tabs"), and "@t <query>" fuzzy-filters them
// by title/URL with no per-category limit. The "t" prefix is reserved
// (RESERVED_SHORTCUT_PREFIXES in config.ts) and always wins over user-defined
// shortcuts, so @t must work even when NO user shortcuts exist — every test
// therefore pins `setShortcuts([])` to prove the built-in path is independent
// of user shortcuts (and restores the real pref via `restoreShortcutsPref`).
//
// `updateSearch("@t ...")` routes through the 30ms debounce (non-empty query),
// so every test awaits `flushDebounce()` before asserting on
// `filteredCommands()`. Tab titles/URLs are environment-dependent, so queries
// are derived dynamically from `getTabCommands(window)` instead of being
// hard-coded.

const tabSearchTests: TestCase[] = [
  // --- "@t" alone lists every open tab, and nothing else ---
  {
    name: "@t lists only open-tab commands (all categories 'open-tabs')",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@t");
        await flushDebounce();
        const results = ctrl.state.filteredCommands();
        const expectedCount = getTabCommands(window).length;
        assertEquals(
          results.length,
          expectedCount,
          "@t should list every open tab, no more no less",
        );
        for (const cmd of results) {
          assert(
            isTabCommand(cmd.id),
            `id "${cmd.id}" should be a tab command`,
          );
          assertEquals(cmd.category, "open-tabs", "category should be open-tabs");
        }
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@t <query>" fuzzy-filters tabs by title/URL ---
  {
    name: "@t <query> fuzzy-filters tabs by title/URL",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const tabs = getTabCommands(window);
        if (tabs.length === 0) return; // nothing to filter — trivially passes
        // Derive the query dynamically from the first tab that has any
        // searchable text (title preferred, URL as fallback).
        const first = tabs.find((t) => t.label.trim().length > 0) ??
          tabs.find((t) => t.description.trim().length > 0);
        if (!first) return; // all tabs have empty label+URL — cannot filter
        const source = first.label.trim() || first.description;
        const query = source.slice(0, Math.min(6, source.length));
        const originalIds = new Set(tabs.map((t) => t.id));

        const ctrl = createController();
        ctrl.updateSearch(`@t ${query}`);
        await flushDebounce();
        const results = ctrl.state.filteredCommands();

        assert(results.length > 0, "filtered tab list should be non-empty");
        for (const cmd of results) {
          assert(
            originalIds.has(cmd.id),
            `result id "${cmd.id}" must be one of the open tabs`,
          );
        }
        assert(
          results.some((c) => c.id === first.id),
          "the tab matching the query must be present in the results",
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@t <no-match>" yields an empty list ---
  {
    name: "@t <no-match> returns an empty list",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@t zzqxv-not-a-real-tab-zzqxv");
        await flushDebounce();
        const results = ctrl.state.filteredCommands();
        // With zero open tabs this is trivially empty; with one or more tabs it
        // proves the fuzzy filter rejects queries matching no title/URL AND that
        // @t early-returns (no command-search / search-engine fallback rows leak
        // into the list).
        assertEquals(
          results.length,
          0,
          "non-matching @t query should yield an empty list",
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- highlightQuery: @t highlights only the args part ---
  {
    name: "@t mode sets highlightQuery to the args part (normal queries sync)",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@t foo");
        await flushDebounce();
        assertEquals(
          ctrl.state.highlightQuery(),
          "foo",
          "@t <query> should highlight only the args part",
        );
        ctrl.updateSearch("@t");
        await flushDebounce();
        assertEquals(
          ctrl.state.highlightQuery(),
          "",
          "@t alone should highlight nothing",
        );
        ctrl.updateSearch("hello");
        await flushDebounce();
        assertEquals(
          ctrl.state.highlightQuery(),
          "hello",
          "normal queries keep highlightQuery in sync with the full query",
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- Enter on an @t result switches to that tab ---
  {
    name: "executing an @t result switches to that tab (restored afterwards)",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      const gb = (window as unknown as { gBrowser: GBrowser }).gBrowser;
      if (!gb || gb.tabs.length < 2) return; // need a non-selected tab to switch to
      const originalTab = gb.selectedTab;
      const targetTab = gb.tabs.find((t) => t !== originalTab);
      assert(targetTab !== undefined, "a non-selected tab must exist");
      try {
        const ctrl = createController();
        ctrl.updateSearch("@t");
        await flushDebounce();
        const filtered = ctrl.state.filteredCommands();
        // Match the command id the same way tab-provider builds it
        // (`tab.tabId ?? tab._tPos`).
        const tabId = (targetTab as unknown as { tabId?: number }).tabId ??
          (targetTab as unknown as { _tPos?: number })._tPos;
        const targetCmd = filtered.find((c) => c.id === `__tab__${tabId}`);
        assert(
          targetCmd !== undefined,
          `tab command "__tab__${tabId}" should be in the @t list`,
        );
        ctrl.executeCommand(targetCmd);
        assertEquals(
          gb.selectedTab,
          targetTab,
          "executing the tab command should select the target tab",
        );
      } finally {
        // Side effect is heavy: always restore the originally selected tab.
        gb.selectedTab = originalTab;
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- @t vs a user-defined "t" shortcut: the built-in always wins ---
  {
    name: "@t wins over a user-defined 't' shortcut (reserved prefix)",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      // Even with a user "t" shortcut pointing at a different command, the
      // built-in @t branch runs first and the user shortcut must never appear.
      setShortcuts([{ prefix: "t", commandId: KNOWN_ID }]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@t");
        await flushDebounce();
        const results = ctrl.state.filteredCommands();
        assertEquals(
          shortcutRows(results).length,
          0,
          "user-defined 't' shortcut must be ignored by built-in @t",
        );
        for (const cmd of results) {
          assert(
            isTabCommand(cmd.id),
            `id "${cmd.id}" should be a tab command (built-in @t wins)`,
          );
        }
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@t " (trailing space) still lists every open tab ---
  {
    name: "@t with a trailing space lists all open tabs",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@t ");
        await flushDebounce();
        const results = ctrl.state.filteredCommands();
        const expectedCount = getTabCommands(window).length;
        assertEquals(
          results.length,
          expectedCount,
          "@t<space> should list every open tab, no more no less",
        );
        for (const cmd of results) {
          assert(
            isTabCommand(cmd.id),
            `id "${cmd.id}" should be a tab command`,
          );
        }
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@T" (uppercase) is NOT tab mode; it falls through to shortcuts ---
  //
  // The built-in @t branch only triggers on the lowercase prefix ("t"). Prefix
  // matching for user shortcuts is case-sensitive (`s.prefix === prefixPart`),
  // so a user-defined "T" shortcut stays reachable via "@T" — proving the @t
  // branch was skipped entirely.
  {
    name: "@T (uppercase) does not enter tab mode (falls through to shortcuts)",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([{ prefix: "T", commandId: KNOWN_ID }]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@T");
        await flushDebounce();
        const results = ctrl.state.filteredCommands();
        // In @t mode highlightQuery would be the args part ("" here); the
        // fall-through path syncs it to the full trimmed query instead.
        assertEquals(
          ctrl.state.highlightQuery(),
          "@T",
          "uppercase @T must NOT enter tab mode (highlightQuery stays the full query)",
        );
        const rows = shortcutRows(results);
        assert(
          rows.some((r) => r.id === `__shortcut:T:${KNOWN_ID}`),
          `user "T" shortcut row "__shortcut:T:${KNOWN_ID}" should appear via the shortcut mechanism`,
        );
        // Shortcut rows are pushed first in the @-prefix path, so the list head
        // is the "T" shortcut row (not a tab command).
        assert(
          results.length > 0 && results[0].category === "shortcut",
          "the 'T' shortcut row should be pinned to the top of the results",
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },
];

// ---------------------------------------------------------------------------
// Reserved built-in shortcut list tests ("@" alone)
// ---------------------------------------------------------------------------
//
// "@" alone shows the reserved built-in shortcut rows (`__reserved:s` for web
// search, `__reserved:t` for tab search) at the very top of the candidate list
// — above any user-defined shortcuts, and even when the shortcuts pref is
// empty. `__reserved:s` is only present when `floorp-search-web` is registered
// in the test window (same guard as the existing @s tests). Selecting a
// reserved row transitions into its mode by replacing the query (`@s ` /
// `@t `) WITHOUT hiding the palette: `mode` stays "command" and the palette
// stays visible, so the user lands directly in the next mode instead of
// re-opening the palette.
//
// `updateSearch("@")` routes through the 30ms debounce (non-empty query), so
// every test awaits `flushDebounce()` before asserting on
// `filteredCommands()`. All tests snapshot/restore the shortcuts pref to stay
// hermetic. `setIsVisible(true)` simulates an open palette so the
// "stays visible" invariant is meaningful (the palette starts hidden).

const reservedListTests: TestCase[] = [
  // --- "@" alone shows the reserved rows even with an empty shortcuts pref ---
  {
    name: "@ alone lists reserved rows (__reserved:s/__reserved:t) with empty pref",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        const hasSearchWeb =
          getCommand("floorp-search-web", window) !== undefined;
        const reservedS = rows.find((r) => r.id === "__reserved:s");
        const reservedT = rows.find((r) => r.id === "__reserved:t");
        if (hasSearchWeb) {
          assert(
            reservedS !== undefined,
            "__reserved:s row should be present when floorp-search-web is registered",
          );
        }
        assert(
          reservedT !== undefined,
          "__reserved:t row should always be present",
        );
        for (const row of rows) {
          if (row.id.startsWith("__reserved:")) {
            assertEquals(
              row.category,
              "shortcut",
              `reserved row "${row.id}" should have category "shortcut"`,
            );
          }
        }
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@" alone pins the reserved rows ABOVE user-defined shortcuts ---
  {
    name: "@ alone pins reserved rows above user-defined shortcuts",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([{ prefix: "gh", commandId: KNOWN_ID }]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@");
        await flushDebounce();
        const filtered = ctrl.state.filteredCommands();
        assert(filtered.length > 0, "filtered list should be non-empty");
        const hasSearchWeb =
          getCommand("floorp-search-web", window) !== undefined;
        const reservedCount = hasSearchWeb ? 2 : 1;
        for (let i = 0; i < reservedCount; i++) {
          assert(
            filtered[i].id.startsWith("__reserved:"),
            `filtered[${i}] should be a reserved row (got "${filtered[i].id}")`,
          );
        }
        if (hasSearchWeb) {
          assertEquals(
            filtered[0].id,
            "__reserved:s",
            "first row should be __reserved:s",
          );
          assertEquals(
            filtered[1].id,
            "__reserved:t",
            "second row should be __reserved:t",
          );
        } else {
          assertEquals(
            filtered[0].id,
            "__reserved:t",
            "first row should be __reserved:t",
          );
        }
        // The user-defined shortcut row follows right after the reserved rows.
        const userShortcut = filtered[reservedCount];
        assert(
          userShortcut !== undefined &&
            userShortcut.id === `__shortcut:gh:${KNOWN_ID}`,
          `row after reserved rows should be the user 'gh' shortcut (got "${
            userShortcut ? userShortcut.id : "undefined"
          }")`,
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@" alone excludes stale reserved-prefix entries from user shortcuts ---
  //
  // A stale "s" entry (persisted before the prefix was reserved) must be
  // filtered out of the user shortcut list by buildShortcutCommands, otherwise
  // it would duplicate the built-in __reserved:s row. A normal user-defined
  // shortcut ("gh") must be unaffected. The __reserved:s assertion is skipped
  // when floorp-search-web is not registered in the test window (same guard as
  // the other reserved tests).
  {
    name: "@ alone excludes stale reserved-prefix entries from user shortcuts",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([
        { prefix: "s", commandId: KNOWN_ID },
        { prefix: "gh", commandId: KNOWN_ID },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        const hasSearchWeb =
          getCommand("floorp-search-web", window) !== undefined;
        if (hasSearchWeb) {
          assert(
            rows.some((r) => r.id === "__reserved:s"),
            "__reserved:s row should be present when floorp-search-web is registered",
          );
        }
        assertEquals(
          rows.filter((r) => r.id.startsWith("__shortcut:s:")).length,
          0,
          "stale 's' shortcut entry must be excluded from the @ list",
        );
        assert(
          rows.some((r) => r.id === `__shortcut:gh:${KNOWN_ID}`),
          `user 'gh' shortcut "__shortcut:gh:${KNOWN_ID}" should still be listed`,
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- Selecting __reserved:s transitions into @s mode without hiding the palette ---
  {
    name: "executing __reserved:s transitions to @s mode (palette stays open)",
    async fn() {
      if (!getCommand("floorp-search-web", window)) {
        return; // floorp-search-web not registered in this environment — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        const reservedS = rows.find((r) => r.id === "__reserved:s");
        assert(
          reservedS !== undefined,
          "__reserved:s row should be in the @ list",
        );
        // Simulate an open palette so the "stays visible" invariant is meaningful.
        ctrl.state.setIsVisible(true);
        ctrl.executeCommand(reservedS);
        assertEquals(
          ctrl.state.mode(),
          "command",
          "mode should stay 'command' (no input mode, no close)",
        );
        assertEquals(
          ctrl.state.isVisible(),
          true,
          "palette should stay visible (reserved rows must not hide it)",
        );
        assertEquals(
          ctrl.state.query(),
          "@s ",
          "query should transition to '@s '",
        );
        // The reserved fn re-runs updateSearch (debounced) — flush it and check
        // the resolved @s shortcut row appears (args-less plain form).
        await flushDebounce();
        const filtered = ctrl.state.filteredCommands();
        const sRow = filtered.find(
          (c) => c.id === "__shortcut:s:floorp-search-web",
        );
        assert(
          sRow !== undefined,
          "resolved '__shortcut:s:floorp-search-web' row should be listed after the transition",
        );
        assert(
          !sRow.id.endsWith(":args"),
          `id "${sRow.id}" should be the plain (args-less) shortcut row`,
        );
        assertEquals(sRow.category, "shortcut", "category should be shortcut");
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- Selecting __reserved:t transitions into @t tab mode without hiding the palette ---
  {
    name: "executing __reserved:t transitions to @t tab mode (palette stays open)",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      const expectedTabs = getTabCommands(window);
      if (expectedTabs.length === 0) return; // nothing to list — trivially passes
      try {
        const ctrl = createController();
        ctrl.updateSearch("@");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        const reservedT = rows.find((r) => r.id === "__reserved:t");
        assert(
          reservedT !== undefined,
          "__reserved:t row should be in the @ list",
        );
        ctrl.state.setIsVisible(true);
        ctrl.executeCommand(reservedT);
        assertEquals(
          ctrl.state.mode(),
          "command",
          "mode should stay 'command' (no input mode, no close)",
        );
        assertEquals(
          ctrl.state.isVisible(),
          true,
          "palette should stay visible (reserved rows must not hide it)",
        );
        assertEquals(
          ctrl.state.query(),
          "@t ",
          "query should transition to '@t '",
        );
        // The reserved fn re-runs updateSearch (debounced) — flush it and check
        // every open tab is listed, exactly like a direct "@t" query.
        await flushDebounce();
        const filtered = ctrl.state.filteredCommands();
        assertEquals(
          filtered.length,
          expectedTabs.length,
          "@t after transition should list every open tab, no more no less",
        );
        for (const cmd of filtered) {
          assert(
            isTabCommand(cmd.id),
            `id "${cmd.id}" should be a tab command`,
          );
        }
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- Selecting a reserved row mirrors the query into the DOM input ---
  //
  // The SolidJS controlled value binding is unreliable in Firefox/XUL, so the
  // reserved fn also writes the query directly into `#command-palette-search`
  // via DOM manipulation (see buildReservedShortcutCommands): the fn receives
  // the controller's target window and sets
  // `win.document.getElementById("command-palette-search").value`.
  //
  // In the headless Marionette test environment there is no guarantee that the
  // SolidJS overlay (and its input) is actually mounted, so relying on the UI
  // mounting would silently skip this test. Instead, this test creates the
  // input manually and inserts it into the document before executing the
  // reserved rows — `executeCommand` passes `this.targetWindow === window`, so
  // the fn's DOM write lands on the manually-inserted input regardless of
  // whether the UI is mounted. The inserted input is removed in `finally` so
  // the test leaves the document as it found it.
  {
    name: "__reserved:s/__reserved:t mirror query into #command-palette-search input",
    async fn() {
      if (!getCommand("floorp-search-web", window)) {
        return; // floorp-search-web not registered in this environment — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      const doc = window.document;
      if (!doc) {
        return; // no document in this environment — skip
      }
      // The chrome/XUL document may not have `document.body`; append to the
      // root element instead. Drop any pre-existing element with the same id
      // first so getElementById resolves to the element we insert.
      const existing = doc.getElementById("command-palette-search");
      existing?.remove();
      const input = doc.createElement("input");
      input.id = "command-palette-search";
      const container = doc.documentElement ?? doc.body;
      if (!container) {
        return; // no insertion point in this environment — skip
      }
      container.appendChild(input);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        const reservedS = rows.find((r) => r.id === "__reserved:s");
        const reservedT = rows.find((r) => r.id === "__reserved:t");
        assert(
          reservedS !== undefined,
          "__reserved:s row should be in the @ list",
        );
        assert(
          reservedT !== undefined,
          "__reserved:t row should be in the @ list",
        );

        // The reserved fn sets `input.value` directly via DOM manipulation —
        // verify it actually landed, independent of any UI mounting.
        ctrl.executeCommand(reservedS);
        assertEquals(
          input.value,
          "@s ",
          "input.value should be '@s ' after executing __reserved:s",
        );

        ctrl.executeCommand(reservedT);
        assertEquals(
          input.value,
          "@t ",
          "input.value should be '@t ' after executing __reserved:t",
        );
      } finally {
        input.remove();
        restoreShortcutsPref(shortcutsPrefSnapshot);
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
  runTests("commandPaletteController.test.ts", [
    ...rawTests,
    ...shortcutTests,
    ...tabSearchTests,
    ...reservedListTests,
  ]);
}
