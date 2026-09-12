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

function makeStepCommand(
  steps: CommandStep[],
  fn?: PaletteCommand["fn"],
): PaletteCommand {
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

// Controllers attach a window-capture keydown listener in their constructor.
// Track every instance so runAllTests can destroy them — leaked instances
// with isVisible()===true swallow later Escape keydowns in OTHER test files.
const liveControllers: CommandPaletteController[] = [];

function createController(): CommandPaletteController {
  const ctrl = new CommandPaletteController(window);
  liveControllers.push(ctrl);
  return ctrl;
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

/**
 * Returns only user-declared shortcut rows; reserved built-in rows
 * (`__reserved:s/t/b/h`) are pinned separately and excluded so length/order
 * assertions see only user-declared shortcuts.
 */
function userShortcutRows(commands: PaletteCommand[]): PaletteCommand[] {
  return commands.filter(
    (c) => c.category === "shortcut" && !c.id.startsWith("__reserved:"),
  );
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
        const rows = userShortcutRows(ctrl.state.filteredCommands());
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
        const rows = userShortcutRows(ctrl.state.filteredCommands());
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

  // --- "@s" alone (no trailing space) shows the reserved row, not the plain shortcut ---
  //
  // With the trailing-space check, "@s" exactly no longer commits to web-search
  // mode: it shows the reserved __reserved:s row (plus any user-defined
  // shortcuts starting with "@s") so typing continues to narrow the list.
  // "@s " (trailing space) is what commits to the plain floorp-search-web row.
  {
    name: "@s alone (no trailing space) shows the reserved __reserved:s row",
    async fn() {
      if (!getCommand("floorp-search-web", window)) {
        return; // floorp-search-web not registered in this environment — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([
        { prefix: "s", commandId: "floorp-search-web" },
      ]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@s");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assert(rows.length >= 1, "should have at least 1 shortcut row");
        assertEquals(
          rows[0].id,
          "__reserved:s",
          `"@s" alone should lead with the reserved row (got "${rows[0].id}")`,
        );
        // The user "s" entry is a reserved prefix, so it must NOT leak a second
        // plain row below the reserved one.
        assertEquals(
          rows.filter((r) => r.id.startsWith("__shortcut:s:")).length,
          0,
          "reserved-prefix user entry must not appear as a plain shortcut row",
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
    name: "@s alone lists the reserved row even with an empty shortcuts pref",
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
          "__reserved:s",
          "built-in @s should resolve to the reserved __reserved:s row",
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
          "__reserved:s",
          "built-in @s must beat a stale user 's' shortcut",
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@s" alone (no trailing space) also lists user shortcuts starting with "@s" ---
  //
  // The reserved branch does not early-commit to web-search mode: it appends
  // `buildShortcutCommands("s", "")`, so user-defined shortcuts whose prefix
  // starts with "s" (e.g. "@st") remain visible below the reserved row while
  // the user keeps typing to narrow the list (fzf-style).
  {
    name: "@s alone lists user shortcuts starting with @s (e.g. @st) below __reserved:s",
    async fn() {
      if (!getCommand("floorp-search-web", window)) {
        return; // floorp-search-web not registered in this environment — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([{ prefix: "st", commandId: KNOWN_ID }]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@s");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(
          rows.length,
          2,
          "expected __reserved:s + user '@st' shortcut",
        );
        assertEquals(
          rows[0].id,
          "__reserved:s",
          "the reserved row should be pinned to the top",
        );
        assertEquals(
          rows[1].id,
          `__shortcut:st:${KNOWN_ID}`,
          "user shortcut starting with @s should follow the reserved row",
        );
        assertEquals(
          ctrl.state.highlightQuery(),
          "@s",
          "highlightQuery should stay the full query while narrowing",
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@s " (trailing space) commits to the plain floorp-search-web row ---
  //
  // The trailing space is what commits "@s" to web-search mode: the plain
  // (args-less) `__shortcut:s:floorp-search-web` row appears, matching the
  // pre-change behavior for "@s " and "@s <query>".
  {
    name: "@s with a trailing space lists the plain floorp-search-web row",
    async fn() {
      if (!getCommand("floorp-search-web", window)) {
        return; // floorp-search-web not registered in this environment — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@s ");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(
          rows.length,
          1,
          "@s<space> should yield exactly one shortcut row",
        );
        assertEquals(
          rows[0].id,
          "__shortcut:s:floorp-search-web",
          "@s<space> should resolve to the plain floorp-search-web row",
        );
        assert(
          !rows[0].id.endsWith(":args"),
          `id "${rows[0].id}" should be the plain (args-less) shortcut row`,
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
  // --- "@t" alone (no trailing space) shows the reserved row, not tabs ---
  //
  // With the trailing-space check, "@t" exactly no longer commits to tab mode:
  // it shows the reserved __reserved:t row (plus any user-defined shortcuts
  // starting with "@t") so typing continues to narrow the list. "@t "
  // (trailing space) or "@t <query>" commits to the tab list below.
  {
    name: "@t alone (no trailing space) shows only the reserved __reserved:t row",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@t");
        await flushDebounce();
        const results = ctrl.state.filteredCommands();
        assertEquals(
          results.length,
          1,
          "@t alone should yield exactly the __reserved:t row (no tabs yet)",
        );
        assertEquals(
          results[0].id,
          "__reserved:t",
          "@t alone should show the reserved tab-search row",
        );
        assertEquals(
          results[0].category,
          "shortcut",
          "category should be shortcut",
        );
        assertEquals(
          ctrl.state.highlightQuery(),
          "@t",
          "highlightQuery should stay the full query while narrowing",
        );
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
          "@t",
          "@t alone should keep the full query as the highlight (narrowing list)",
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
      const gb = (window as unknown as { gBrowser: GBrowser }).gBrowser;
      if (!gb || gb.tabs.length < 2) return; // need a non-selected tab to switch to
      const originalTab = gb.selectedTab;
      const targetTab = gb.tabs.find((t) => t !== originalTab);
      assert(targetTab !== undefined, "a non-selected tab must exist");
      setShortcuts([]);
      try {
        const ctrl = createController();
        // Trailing space commits "@t " to tab-search mode; bare "@t" would only
        // show the reserved __reserved:t row with no tab commands to execute.
        ctrl.updateSearch("@t ");
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
      // "@t " (trailing space) commits to tab mode; bare "@t" would show only
      // the reserved row, which would not exercise the tab listing at all.
      setShortcuts([{ prefix: "t", commandId: KNOWN_ID }]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@t ");
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
// search, `__reserved:t` for tab search, `__reserved:b` for bookmark search,
// `__reserved:h` for history search) at the very top of the candidate list
// — above any user-defined shortcuts, and even when the shortcuts pref is
// empty. `__reserved:s` is only present when `floorp-search-web` is registered
// in the test window (same guard as the existing @s tests). Selecting a
// reserved row transitions into its mode by replacing the query (`@s ` /
// `@t ` / `@b ` / `@h `) WITHOUT hiding the palette: `mode` stays "command"
// and the palette stays visible, so the user lands directly in the next mode
// instead of re-opening the palette.
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
        // Reserved rows in declaration order: __reserved:s (only when
        // floorp-search-web is registered), then __reserved:t, __reserved:b,
        // __reserved:h.
        const reservedCount = hasSearchWeb ? 4 : 3;
        const expectedIds = hasSearchWeb
          ? ["__reserved:s", "__reserved:t", "__reserved:b", "__reserved:h"]
          : ["__reserved:t", "__reserved:b", "__reserved:h"];
        for (let i = 0; i < reservedCount; i++) {
          assert(
            filtered[i].id.startsWith("__reserved:"),
            `filtered[${i}] should be a reserved row (got "${filtered[i].id}")`,
          );
          assertEquals(
            filtered[i].id,
            expectedIds[i],
            `filtered[${i}] should be "${expectedIds[i]}"`,
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
      const expectedTabs = getTabCommands(window);
      if (expectedTabs.length === 0) return; // nothing to list — trivially passes
      setShortcuts([]);
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
      setShortcuts([]);
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

// ---------------------------------------------------------------------------
// @b / @h bookmark & history search tests
// ---------------------------------------------------------------------------
//
// "b" (@b = bookmark search) and "h" (@h = history search) are built-in
// reserved prefixes (see RESERVED_SHORTCUT_PREFIXES in config.ts), mirroring
// the @s/@t modes. Typing "@b" / "@h" exactly (no trailing space) shows the
// reserved __reserved:b / __reserved:h row plus any user-defined shortcuts
// starting with the prefix, so typing continues to narrow the list
// (fzf-style). "@b " / "@h " (trailing space) or "@b <query>" / "@h <query>"
// commits to the dedicated search mode: the result list is populated
// asynchronously from Places via the same debounce + stale-query guard as the
// dynamic bookmark/history suggestions — a 100ms (bookmark) / 200ms
// (history) timer, then the Places lookup, and results are only appended when
// the query hasn't changed in the meantime.
//
// The committed-mode result rows carry category "bookmark-suggestions" /
// "history-suggestions" (NOT "shortcut"), so these tests assert on the full
// `filteredCommands()` list, never `shortcutRows()`. Places is available in
// the browser test environment (see providers.test.ts), so the end-to-end
// tests insert a temporary bookmark / history visit to verify results
// deterministically; the insertion is skipped (test returns early) when
// Places cannot write in the current profile. The structural tests below
// never depend on the profile having data: an empty result list still
// verifies that the committed branch was entered (highlightQuery = args
// part) and that every row shown is a bookmark/history suggestion row.

/** Wait a fixed number of milliseconds. */
function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

/**
 * Wait long enough for the committed @b/@h mode's async search to complete:
 * 30ms updateSearch debounce + 100ms (bookmark) / 200ms (history) timer +
 * Places lookup + margin.
 */
function flushAsyncSearch(): Promise<void> {
  return sleep(800);
}

const bookmarkHistorySearchTests: TestCase[] = [
  // --- "@b" exactly shows the reserved row (no commit yet) ---
  {
    name: "@b alone (no trailing space) shows the reserved __reserved:b row",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@b");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(
          rows.length,
          1,
          "@b alone should yield exactly the __reserved:b row (no search yet)",
        );
        assertEquals(
          rows[0].id,
          "__reserved:b",
          "@b alone should show the reserved bookmark-search row",
        );
        assertEquals(
          ctrl.state.highlightQuery(),
          "@b",
          "highlightQuery should stay the full query while narrowing",
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@h" exactly shows the reserved row (no commit yet) ---
  {
    name: "@h alone (no trailing space) shows the reserved __reserved:h row",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@h");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        assertEquals(
          rows.length,
          1,
          "@h alone should yield exactly the __reserved:h row (no search yet)",
        );
        assertEquals(
          rows[0].id,
          "__reserved:h",
          "@h alone should show the reserved history-search row",
        );
        assertEquals(
          ctrl.state.highlightQuery(),
          "@h",
          "highlightQuery should stay the full query while narrowing",
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@b " (trailing space) commits to async bookmark search ---
  {
    name: "@b with a trailing space commits to async bookmark search (recent bookmarks)",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@b ");
        await flushDebounce();
        // Committed mode highlights only the (empty) args part — this proves
        // the dedicated @b branch was entered rather than the generic
        // shortcut path (which would highlight the full trimmed query).
        assertEquals(
          ctrl.state.highlightQuery(),
          "",
          "@b<space> should highlight the (empty) args part",
        );
        await flushAsyncSearch();
        // Empty profiles yield an empty list (trivially passes); any rows
        // present must be bookmark suggestions from the async search.
        for (const cmd of ctrl.state.filteredCommands()) {
          assertEquals(
            cmd.category,
            "bookmark-suggestions",
            `row "${cmd.id}" should be a bookmark suggestion`,
          );
          assert(
            cmd.id.startsWith("__bookmark__"),
            `row "${cmd.id}" should carry the __bookmark__ id prefix`,
          );
        }
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@b <query>" searches bookmarks by term ---
  {
    name: "@b <query> commits to async bookmark search by term",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@b test");
        await flushDebounce();
        assertEquals(
          ctrl.state.highlightQuery(),
          "test",
          "@b <query> should highlight only the args part",
        );
        await flushAsyncSearch();
        for (const cmd of ctrl.state.filteredCommands()) {
          assertEquals(
            cmd.category,
            "bookmark-suggestions",
            `row "${cmd.id}" should be a bookmark suggestion`,
          );
          assert(
            cmd.id.startsWith("__bookmark__"),
            `row "${cmd.id}" should carry the __bookmark__ id prefix`,
          );
        }
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@h " (trailing space) commits to async history search ---
  {
    name: "@h with a trailing space commits to async history search (recent visits)",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@h ");
        await flushDebounce();
        assertEquals(
          ctrl.state.highlightQuery(),
          "",
          "@h<space> should highlight the (empty) args part",
        );
        await flushAsyncSearch();
        for (const cmd of ctrl.state.filteredCommands()) {
          assertEquals(
            cmd.category,
            "history-suggestions",
            `row "${cmd.id}" should be a history suggestion`,
          );
          assert(
            cmd.id.startsWith("__history__"),
            `row "${cmd.id}" should carry the __history__ id prefix`,
          );
        }
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- "@h <query>" searches history by term ---
  {
    name: "@h <query> commits to async history search by term",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@h test");
        await flushDebounce();
        assertEquals(
          ctrl.state.highlightQuery(),
          "test",
          "@h <query> should highlight only the args part",
        );
        await flushAsyncSearch();
        for (const cmd of ctrl.state.filteredCommands()) {
          assertEquals(
            cmd.category,
            "history-suggestions",
            `row "${cmd.id}" should be a history suggestion`,
          );
          assert(
            cmd.id.startsWith("__history__"),
            `row "${cmd.id}" should carry the __history__ id prefix`,
          );
        }
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- @b end-to-end: a freshly inserted bookmark is findable via @b ---
  //
  // Inserts a temporary bookmark into Places (unique title + URL), then
  // verifies both committed paths surface it: "@b " (recent bookmarks) and
  // "@b <title>" (term search). The bookmark is removed in `finally`. If
  // Places cannot write in the current profile the insert throws and the
  // test returns early (result verification skipped, structural tests above
  // still cover the branch).
  {
    name: "@b finds a bookmark inserted into Places (end-to-end)",
    async fn() {
      const marker = `cp-bm-test-${Date.now()}`;
      const url = `https://command-palette-test-${Date.now()}.example/`;
      const mod = ChromeUtils.importESModule(
        "resource://gre/modules/PlacesUtils.sys.mjs",
      ) as unknown as {
        PlacesUtils: {
          bookmarks: {
            insert(info: {
              parentGuid: string;
              title: string;
              url: string;
            }): Promise<{ guid: string }>;
            remove(guid: string): Promise<void>;
            unfiledGuid: string;
          };
        };
      };
      let guid: string | null = null;
      try {
        const inserted = await mod.PlacesUtils.bookmarks.insert({
          parentGuid: mod.PlacesUtils.bookmarks.unfiledGuid,
          title: marker,
          url,
        });
        guid = inserted.guid;
      } catch (e) {
        console.error(
          "[command-palette] @b end-to-end insert skipped (Places not writable):",
          e,
        );
        return; // cannot verify results — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        // Recent-bookmarks path ("@b ").
        ctrl.updateSearch("@b ");
        await flushDebounce();
        await flushAsyncSearch();
        assert(
          ctrl.state.filteredCommands().some((c) => c.description === url),
          `"@b " should list the inserted bookmark (${url})`,
        );
        // Term-search path ("@b <title>").
        ctrl.updateSearch(`@b ${marker}`);
        await flushDebounce();
        await flushAsyncSearch();
        assert(
          ctrl.state.filteredCommands().some((c) => c.description === url),
          `"@b <title>" should surface the inserted bookmark (${url})`,
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
        if (guid !== null) {
          try {
            await mod.PlacesUtils.bookmarks.remove(guid);
          } catch (e) {
            console.error(
              "[command-palette] @b end-to-end cleanup failed:",
              e,
            );
          }
        }
      }
    },
  },

  // --- @h end-to-end: a freshly inserted visit is findable via @h ---
  //
  // Inserts a temporary history visit (unique URL, timestamp = now) and
  // verifies the committed "@h " path (recent visits, last 7 days, newest
  // first) surfaces it. The visit is removed in `finally`. If Places cannot
  // write in the current profile the insert throws and the test returns
  // early.
  {
    name: "@h finds a history visit inserted into Places (end-to-end)",
    async fn() {
      const url = `https://command-palette-history-test-${Date.now()}.example/`;
      const mod = ChromeUtils.importESModule(
        "resource://gre/modules/PlacesUtils.sys.mjs",
      ) as unknown as {
        PlacesUtils: {
          history: {
            insert(info: {
              url: string;
              visits: { transition: number; date: Date }[];
            }): Promise<unknown>;
            remove(url: string): Promise<void>;
          };
        };
      };
      try {
        await mod.PlacesUtils.history.insert({
          url,
          visits: [{ transition: 1, date: new Date() }],
        });
      } catch (e) {
        console.error(
          "[command-palette] @h end-to-end insert skipped (Places not writable):",
          e,
        );
        return; // cannot verify results — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@h ");
        await flushDebounce();
        await flushAsyncSearch();
        assert(
          ctrl.state.filteredCommands().some((c) => c.description === url),
          `"@h " should list the inserted visit (${url})`,
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
        try {
          await mod.PlacesUtils.history.remove(url);
        } catch (e) {
          console.error(
            "[command-palette] @h end-to-end cleanup failed:",
            e,
          );
        }
      }
    },
  },

  // --- Stale-query guard: an old query's results never override the latest ---
  //
  // The committed @b mode debounces (100ms) then asynchronously queries
  // Places; the in-flight lookup is guarded by `currentSearchQuery` so a
  // slow result for an outdated query is discarded. This test changes the
  // query while the first lookup may still be in flight (or right after it
  // resolved — either way doUpdateSearch clears/overrides the state) and
  // verifies the final list contains only results for the LATEST query.
  //
  // A category check alone would be false confidence: stale rows from the
  // superseded query are still "bookmark-suggestions", so a broken guard
  // would pass. A marker bookmark is therefore inserted into Places whose
  // title AND url contain ONLY the first query's term ("foo") — the first
  // lookup can return it, the second ("bar") never can. The final assertion
  // "marker absent from the list" then fails exactly when a stale first-query
  // result leaks through the guard. If Places cannot write in the current
  // profile the insert throws and the test returns early (the structural
  // assertions in the other @b tests still cover the branch).
  {
    name: "@b stale async results are discarded when the query changes",
    async fn() {
      const markerUrl = `https://stale-foo-${Date.now()}.example/`;
      const markerTitle = `cp-stale-foo-${Date.now()}`;
      const mod = ChromeUtils.importESModule(
        "resource://gre/modules/PlacesUtils.sys.mjs",
      ) as unknown as {
        PlacesUtils: {
          bookmarks: {
            insert(info: {
              parentGuid: string;
              title: string;
              url: string;
            }): Promise<{ guid: string }>;
            remove(guid: string): Promise<void>;
            unfiledGuid: string;
          };
        };
      };
      let guid: string | null = null;
      try {
        const inserted = await mod.PlacesUtils.bookmarks.insert({
          parentGuid: mod.PlacesUtils.bookmarks.unfiledGuid,
          title: markerTitle,
          url: markerUrl,
        });
        guid = inserted.guid;
      } catch (e) {
        console.error(
          "[command-palette] @b stale insert skipped (Places not writable):",
          e,
        );
        return; // cannot verify results — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        // First committed search: 30ms debounce + 100ms timer. Wait past the
        // timer so the Places lookup starts before we move on, but keep the
        // gap short so the first lookup is likely still in flight when the
        // second query lands (that is the stale-leak window the guard closes).
        ctrl.updateSearch("@b foo");
        await flushDebounce();
        await sleep(90);
        // New query while the first lookup is in flight.
        ctrl.updateSearch("@b bar");
        await flushDebounce();
        await flushAsyncSearch();
        assertEquals(
          ctrl.state.highlightQuery(),
          "bar",
          "highlight should reflect the latest args",
        );
        const finalList = ctrl.state.filteredCommands();
        // The marker matches ONLY "foo", so if the stale guard is broken the
        // first query's in-flight lookup appends it on top of the second
        // query's results — its presence here is exactly the leak to catch.
        assert(
          !finalList.some((c) => c.description === markerUrl),
          `stale marker bookmark (${markerUrl}) must not appear in the final list`,
        );
        // Any rows present must be bookmark suggestions for the latest query;
        // the committed mode clears the list on every query, so the final
        // list is exactly the second search's output (possibly empty).
        for (const cmd of finalList) {
          assertEquals(
            cmd.category,
            "bookmark-suggestions",
            `row "${cmd.id}" should be a bookmark suggestion`,
          );
        }
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
        if (guid !== null) {
          try {
            await mod.PlacesUtils.bookmarks.remove(guid);
          } catch (e) {
            console.error(
              "[command-palette] @b stale cleanup failed:",
              e,
            );
          }
        }
      }
    },
  },

  // --- Stale-query guard (fzf branch): deleting back to "@b" clears pending work ---
  //
  // A committed "@b foo" arms a 100ms timer; if the user then deletes back to
  // "@b" (no args, no trailing space), the fzf narrowing branch MUST clear
  // that timer AND currentSearchQuery. Without the clearing, the armed timer
  // fires after the branch already rendered the reserved row and the stale
  // "foo" results are appended on top of the narrowing list — the guard check
  // inside runSearch passes trivially in that case because currentSearchQuery
  // was never reset, so "trimmed === currentSearchQuery" still holds.
  //
  // Unlike the committed→committed race above, this leak is deterministic:
  // the timer is armed but un-fired when "@b" is typed (flushDebounce leaves
  // ~70ms of the 100ms timer), so a removed clear would always leak the
  // marker. The same marker pattern is used: title/url contain "foo" only.
  {
    name: "@b <query> deleted back to @b clears the pending search (no stale leak)",
    async fn() {
      const markerUrl = `https://stale-foo-${Date.now()}.example/`;
      const markerTitle = `cp-stale-foo-${Date.now()}`;
      const mod = ChromeUtils.importESModule(
        "resource://gre/modules/PlacesUtils.sys.mjs",
      ) as unknown as {
        PlacesUtils: {
          bookmarks: {
            insert(info: {
              parentGuid: string;
              title: string;
              url: string;
            }): Promise<{ guid: string }>;
            remove(guid: string): Promise<void>;
            unfiledGuid: string;
          };
        };
      };
      let guid: string | null = null;
      try {
        const inserted = await mod.PlacesUtils.bookmarks.insert({
          parentGuid: mod.PlacesUtils.bookmarks.unfiledGuid,
          title: markerTitle,
          url: markerUrl,
        });
        guid = inserted.guid;
      } catch (e) {
        console.error(
          "[command-palette] @b fzf-stale insert skipped (Places not writable):",
          e,
        );
        return; // cannot verify results — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        // Phase A: the committed search DOES surface the marker, proving the
        // marker actually matches "foo" (otherwise the absence assertion
        // below would be vacuous).
        ctrl.updateSearch("@b foo");
        await flushDebounce();
        await flushAsyncSearch();
        assert(
          ctrl.state.filteredCommands().some((c) => c.description === markerUrl),
          `"@b foo" should surface the marker bookmark (${markerUrl})`,
        );
        // Phase B: re-arm the committed search, then delete back to "@b"
        // while its 100ms timer is still pending (flushDebounce leaves the
        // timer armed for ~70ms).
        ctrl.updateSearch("@b foo");
        await flushDebounce();
        ctrl.updateSearch("@b");
        await flushDebounce();
        await flushAsyncSearch();
        assertEquals(
          ctrl.state.highlightQuery(),
          "@b",
          "deleting back to @b should enter the narrowing branch",
        );
        const rows = ctrl.state.filteredCommands();
        assertEquals(
          rows.length,
          1,
          "narrowing list should contain only the __reserved:b row",
        );
        assertEquals(
          rows[0].id,
          "__reserved:b",
          "the sole row should be the reserved bookmark-search row",
        );
        assert(
          !rows.some((c) => c.description === markerUrl),
          `stale marker bookmark (${markerUrl}) must not leak into the narrowing list`,
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
        if (guid !== null) {
          try {
            await mod.PlacesUtils.bookmarks.remove(guid);
          } catch (e) {
            console.error(
              "[command-palette] @b fzf-stale cleanup failed:",
              e,
            );
          }
        }
      }
    },
  },

  // --- Stale-query guard (@t/@s): replacing the query clears pending work ---
  //
  // The @b/@h fzf branch clears the pending bookmark/history search on entry,
  // but the @t/@s branches must do the same: a committed "@b foo" (or a normal
  // search for "foo") arms a 100ms timer whose freshness guard compares the
  // captured query against currentSearchQuery. If the user replaces the whole
  // query with "@t x" / "@s x" without the branches resetting
  // currentSearchQuery and clearing the timers, the stale guard passes
  // trivially ("@b foo" === currentSearchQuery) and the old bookmark results
  // are appended on top of the tab / web-search list.
  //
  // Same deterministic leak window and marker pattern as the fzf-stale test
  // above: flushDebounce leaves the armed timer un-fired (~70ms remaining),
  // and the marker's title/url contain ONLY the first query's term ("foo").
  {
    name: "@b <query> replaced by @t x clears the pending search (no stale leak into the tab list)",
    async fn() {
      const markerUrl = `https://stale-foo-${Date.now()}.example/`;
      const markerTitle = `cp-stale-foo-${Date.now()}`;
      const mod = ChromeUtils.importESModule(
        "resource://gre/modules/PlacesUtils.sys.mjs",
      ) as unknown as {
        PlacesUtils: {
          bookmarks: {
            insert(info: {
              parentGuid: string;
              title: string;
              url: string;
            }): Promise<{ guid: string }>;
            remove(guid: string): Promise<void>;
            unfiledGuid: string;
          };
        };
      };
      let guid: string | null = null;
      try {
        const inserted = await mod.PlacesUtils.bookmarks.insert({
          parentGuid: mod.PlacesUtils.bookmarks.unfiledGuid,
          title: markerTitle,
          url: markerUrl,
        });
        guid = inserted.guid;
      } catch (e) {
        console.error(
          "[command-palette] @t stale insert skipped (Places not writable):",
          e,
        );
        return; // cannot verify results — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        // Phase A: the committed @b search DOES surface the marker, proving
        // the marker actually matches "foo" (otherwise the absence assertion
        // below would be vacuous).
        ctrl.updateSearch("@b foo");
        await flushDebounce();
        await flushAsyncSearch();
        assert(
          ctrl.state.filteredCommands().some((c) => c.description === markerUrl),
          `"@b foo" should surface the marker bookmark (${markerUrl})`,
        );
        // Phase B: re-arm the committed search, then replace the whole query
        // with "@t x" while the 100ms timer is still pending (flushDebounce
        // leaves the timer armed for ~70ms).
        ctrl.updateSearch("@b foo");
        await flushDebounce();
        ctrl.updateSearch("@t x");
        await flushDebounce();
        await flushAsyncSearch();
        assertEquals(
          ctrl.state.highlightQuery(),
          "x",
          "highlight should reflect the @t args",
        );
        const rows = ctrl.state.filteredCommands();
        assert(
          !rows.some((c) => c.description === markerUrl),
          `stale marker bookmark (${markerUrl}) must not leak into the @t tab list`,
        );
        for (const cmd of rows) {
          assert(
            cmd.category !== "bookmark-suggestions",
            `row "${cmd.id}" must not be a stale bookmark suggestion in @t mode`,
          );
        }
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
        if (guid !== null) {
          try {
            await mod.PlacesUtils.bookmarks.remove(guid);
          } catch (e) {
            console.error(
              "[command-palette] @t stale cleanup failed:",
              e,
            );
          }
        }
      }
    },
  },

  {
    name: "@b <query> replaced by @s x clears the pending search (no stale leak into the web-search list)",
    async fn() {
      const markerUrl = `https://stale-foo-${Date.now()}.example/`;
      const markerTitle = `cp-stale-foo-${Date.now()}`;
      const mod = ChromeUtils.importESModule(
        "resource://gre/modules/PlacesUtils.sys.mjs",
      ) as unknown as {
        PlacesUtils: {
          bookmarks: {
            insert(info: {
              parentGuid: string;
              title: string;
              url: string;
            }): Promise<{ guid: string }>;
            remove(guid: string): Promise<void>;
            unfiledGuid: string;
          };
        };
      };
      let guid: string | null = null;
      try {
        const inserted = await mod.PlacesUtils.bookmarks.insert({
          parentGuid: mod.PlacesUtils.bookmarks.unfiledGuid,
          title: markerTitle,
          url: markerUrl,
        });
        guid = inserted.guid;
      } catch (e) {
        console.error(
          "[command-palette] @s stale insert skipped (Places not writable):",
          e,
        );
        return; // cannot verify results — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        // Phase A: prove the marker matches "foo" (see the @t variant above).
        ctrl.updateSearch("@b foo");
        await flushDebounce();
        await flushAsyncSearch();
        assert(
          ctrl.state.filteredCommands().some((c) => c.description === markerUrl),
          `"@b foo" should surface the marker bookmark (${markerUrl})`,
        );
        // Phase B: re-arm the committed search, then replace the whole query
        // with "@s x" while the 100ms timer is still pending.
        ctrl.updateSearch("@b foo");
        await flushDebounce();
        ctrl.updateSearch("@s x");
        await flushDebounce();
        await flushAsyncSearch();
        assertEquals(
          ctrl.state.highlightQuery(),
          "x",
          "highlight should reflect the @s args",
        );
        const rows = ctrl.state.filteredCommands();
        assert(
          !rows.some((c) => c.description === markerUrl),
          `stale marker bookmark (${markerUrl}) must not leak into the @s web-search list`,
        );
        for (const cmd of rows) {
          assert(
            cmd.category !== "bookmark-suggestions",
            `row "${cmd.id}" must not be a stale bookmark suggestion in @s mode`,
          );
        }
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
        if (guid !== null) {
          try {
            await mod.PlacesUtils.bookmarks.remove(guid);
          } catch (e) {
            console.error(
              "[command-palette] @s stale cleanup failed:",
              e,
            );
          }
        }
      }
    },
  },

  // --- Selecting __reserved:b transitions into @b mode without hiding the palette ---
  {
    name: "executing __reserved:b transitions to @b mode (palette stays open)",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        const reservedB = rows.find((r) => r.id === "__reserved:b");
        assert(
          reservedB !== undefined,
          "__reserved:b row should be in the @ list",
        );
        ctrl.state.setIsVisible(true);
        ctrl.executeCommand(reservedB);
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
          "@b ",
          "query should transition to '@b '",
        );
        // The reserved fn re-runs updateSearch (debounced) which arms the
        // async bookmark search — let it complete harmlessly.
        await flushDebounce();
        await flushAsyncSearch();
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- Selecting __reserved:h transitions into @h mode without hiding the palette ---
  {
    name: "executing __reserved:h transitions to @h mode (palette stays open)",
    async fn() {
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        ctrl.updateSearch("@");
        await flushDebounce();
        const rows = shortcutRows(ctrl.state.filteredCommands());
        const reservedH = rows.find((r) => r.id === "__reserved:h");
        assert(
          reservedH !== undefined,
          "__reserved:h row should be in the @ list",
        );
        ctrl.state.setIsVisible(true);
        ctrl.executeCommand(reservedH);
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
          "@h ",
          "query should transition to '@h '",
        );
        await flushDebounce();
        await flushAsyncSearch();
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
      }
    },
  },

  // --- hide/reopen while the deferred @b/@h Places lookup is in flight ---
  //
  // hidePalette() resets currentSearchQuery to "" AND clears the pending
  // 100ms/200ms search timers. To exercise the reset (not just the timer
  // clear) the search timer must have ALREADY FIRED and the Places lookup be
  // in flight when hidePalette() is called — only then does the deferred
  // `.then` callback's `query !== this.currentSearchQuery` guard decide the
  // outcome. Each test therefore:
  //   1. proves a marker row actually matches the term (non-vacuous absence),
  //   2. re-arms the committed search and waits past the 100ms/200ms timer so
  //      the Places lookup starts,
  //   3. hides the palette, re-shows it, and lets the in-flight lookup settle,
  //   4. asserts the stale result is not appended.
  // Without the `currentSearchQuery = ""` reset the re-typed query string
  // would still match and the stale marker would leak into the reopened list.
  {
    name: "hidePalette while the @b Places lookup is in flight drops the stale result",
    async fn() {
      const markerUrl = `https://stalemarker-${Date.now()}.example/`;
      const markerTitle = `cp-stalemarker-${Date.now()}`;
      const mod = ChromeUtils.importESModule(
        "resource://gre/modules/PlacesUtils.sys.mjs",
      ) as unknown as {
        PlacesUtils: {
          bookmarks: {
            insert(info: {
              parentGuid: string;
              title: string;
              url: string;
            }): Promise<{ guid: string }>;
            remove(guid: string): Promise<void>;
            unfiledGuid: string;
          };
        };
      };
      let guid: string | null = null;
      try {
        const inserted = await mod.PlacesUtils.bookmarks.insert({
          parentGuid: mod.PlacesUtils.bookmarks.unfiledGuid,
          title: markerTitle,
          url: markerUrl,
        });
        guid = inserted.guid;
      } catch (e) {
        console.error(
          "[command-palette] @b hide stale insert skipped (Places not writable):",
          e,
        );
        return; // cannot verify results — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        // Phase A: prove the marker matches "stalemarker", so the absence
        // assertion below is meaningful.
        ctrl.updateSearch("@b stalemarker");
        await flushDebounce();
        await flushAsyncSearch();
        assert(
          ctrl.state.filteredCommands().some((c) => c.description === markerUrl),
          `"@b stalemarker" should surface the marker bookmark (${markerUrl})`,
        );
        // Phase B: re-arm the committed search, then wait past the 100ms
        // search timer (30ms debounce in flushDebounce + 150ms margin) so the
        // Places lookup is IN FLIGHT — clearTimeout in hidePalette can no
        // longer cancel it and only the currentSearchQuery guard applies.
        ctrl.updateSearch("@b stalemarker");
        await flushDebounce();
        await sleep(150);
        ctrl.hidePalette();
        // Reopen the palette and let the in-flight lookup resolve.
        ctrl.state.setIsVisible(true);
        await flushAsyncSearch();
        const stale = ctrl.state.filteredCommands().filter((c) =>
          c.category === "bookmark-suggestions" ||
          c.id.startsWith("__bookmark__") ||
          c.description === markerUrl
        );
        assertEquals(
          stale.length,
          0,
          "stale bookmark results must not be applied after hidePalette()",
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
        if (guid !== null) {
          try {
            await mod.PlacesUtils.bookmarks.remove(guid);
          } catch (e) {
            console.error(
              "[command-palette] @b hide stale cleanup failed:",
              e,
            );
          }
        }
      }
    },
  },
  {
    name: "hidePalette while the @h Places lookup is in flight drops the stale result",
    async fn() {
      const markerUrl = `https://stalemarker-${Date.now()}.example/`;
      const mod = ChromeUtils.importESModule(
        "resource://gre/modules/PlacesUtils.sys.mjs",
      ) as unknown as {
        PlacesUtils: {
          history: {
            insert(info: {
              url: string;
              visits: { transition: number; date: Date }[];
            }): Promise<unknown>;
            remove(url: string): Promise<void>;
          };
        };
      };
      try {
        await mod.PlacesUtils.history.insert({
          url: markerUrl,
          visits: [{ transition: 1, date: new Date() }],
        });
      } catch (e) {
        console.error(
          "[command-palette] @h hide stale insert skipped (Places not writable):",
          e,
        );
        return; // cannot verify results — skip
      }
      const shortcutsPrefSnapshot = snapshotShortcutsPref();
      setShortcuts([]);
      try {
        const ctrl = createController();
        // Phase A: prove the marker matches "stalemarker".
        ctrl.updateSearch("@h stalemarker");
        await flushDebounce();
        await flushAsyncSearch();
        assert(
          ctrl.state.filteredCommands().some((c) => c.description === markerUrl),
          `"@h stalemarker" should surface the marker visit (${markerUrl})`,
        );
        // Phase B: re-arm, wait past the 200ms history search timer so the
        // Places lookup is in flight, then hide.
        ctrl.updateSearch("@h stalemarker");
        await flushDebounce();
        await sleep(250);
        ctrl.hidePalette();
        ctrl.state.setIsVisible(true);
        await flushAsyncSearch();
        const stale = ctrl.state.filteredCommands().filter((c) =>
          c.category === "history-suggestions" ||
          c.id.startsWith("__history__") ||
          c.description === markerUrl
        );
        assertEquals(
          stale.length,
          0,
          "stale history results must not be applied after hidePalette()",
        );
      } finally {
        restoreShortcutsPref(shortcutsPrefSnapshot);
        try {
          await mod.PlacesUtils.history.remove(markerUrl);
        } catch (e) {
          console.error(
            "[command-palette] @h hide stale cleanup failed:",
            e,
          );
        }
      }
    },
  },
];

function createDeferred<T>(): {
  promise: Promise<T>;
  resolve: (value: T) => void;
} {
  let resolve!: (value: T) => void;
  const promise = new Promise<T>((resolvePromise) => {
    resolve = resolvePromise;
  });
  return { promise, resolve };
}

const rawTests: TestCase[] = [
  // --- Controller instantiation ---
  {
    name: "controller constructs with window",
    fn() {
      const ctrl = createController();
      assert(ctrl !== null, "controller should be created");
      assertEquals(
        ctrl.state.mode(),
        "command",
        "initial mode should be command",
      );
      assertEquals(
        ctrl.state.isVisible(),
        false,
        "initial visibility should be false",
      );
    },
  },

  // --- enterInputMode via executeCommand ---
  {
    name: "executeCommand with steps enters input mode",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_NO_VALIDATE_2);
      assertEquals(ctrl.state.mode(), "input", "mode should be input");
      assertEquals(
        ctrl.state.activeCommand()?.id,
        "__test-step-command__",
        "active command should be set",
      );
      assertEquals(ctrl.state.currentStepIndex(), 0, "step index should be 0");
      // assertEquals compares objects by reference, so assert emptiness
      // structurally instead of against a fresh `{}`.
      assertEquals(Object.keys(ctrl.state.stepInputs()).length, 0, "step inputs should be empty");
      assertEquals(ctrl.state.stepError(), null, "step error should be null");
    },
  },

  // --- advanceStep progression ---
  {
    name: "advanceStep progresses through steps",
    async fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_NO_VALIDATE_2);
      ctrl.updateSearch("value1");
      // Non-empty queries are debounced; flush before advancing so the step
      // input captures "value1".
      await flushDebounce();
      ctrl.advanceStep();
      assertEquals(
        ctrl.state.currentStepIndex(),
        1,
        "should advance to step 1",
      );
      assertEquals(
        ctrl.state.stepInputs().step1,
        "value1",
        "step1 input should be saved",
      );
    },
  },
  {
    name: "advanceStep at last step executes fn with collected args",
    async fn() {
      capturedArgs = undefined;
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_CAPTURE_ARGS);
      ctrl.updateSearch("final value");
      // Non-empty queries are debounced; flush before advancing so the fn
      // receives "final value" as the collected arg.
      await flushDebounce();
      ctrl.advanceStep();
      assertEquals(
        (capturedArgs as unknown as Record<string, string>)?.input,
        "final value",
        "fn should receive collected args",
      );
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
      assertEquals(
        typeof ctrl.state.stepError(),
        "string",
        "should have error string",
      );
      assertEquals(ctrl.state.currentStepIndex(), 0, "should stay on step 0");
    },
  },
  {
    name: "advanceStep with validation pass clears error",
    async fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_VALIDATE);
      ctrl.updateSearch("   ");
      ctrl.advanceStep();
      assert(
        ctrl.state.stepError() !== null,
        "should have error after empty input",
      );
      ctrl.updateSearch("valid");
      // "valid" is non-empty so it is debounced; flush before advancing so
      // validation sees the new value and clears the error.
      await flushDebounce();
      ctrl.advanceStep();
      assertEquals(
        ctrl.state.stepError(),
        null,
        "error should be cleared after valid input",
      );
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
      assertEquals(
        ctrl.state.mode(),
        "command",
        "should return to command mode",
      );
      assertEquals(
        ctrl.state.activeCommand(),
        null,
        "active command should be null",
      );
    },
  },

  // --- loadStepChoices with static choices ---
  {
    name: "enterInputMode with choices populates filteredStepChoices",
    fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_CHOICES);
      assertEquals(
        ctrl.state.filteredStepChoices().length,
        3,
        "should have 3 choices",
      );
      assertEquals(
        ctrl.state.filteredStepChoices()[0].value,
        "a",
        "first choice should be Alpha",
      );
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
      assertEquals(
        ctrl.state.stepChoicesLoading(),
        false,
        "loading should be done",
      );
      assertEquals(
        ctrl.state.filteredStepChoices().length,
        2,
        "should have 2 loaded choices",
      );
      assertEquals(
        ctrl.state.filteredStepChoices()[0].value,
        "la",
        "first loaded choice value",
      );
    },
  },
  {
    name: "choicesLoader receives target window and saved inputs",
    async fn() {
      let loaderWindow: Window | undefined;
      let loaderArgs: Readonly<Record<string, string>> | undefined;
      const command = makeStepCommand([
        { id: "context", label: "Context", placeholder: "Type context" },
        {
          id: "loaded",
          label: "Loaded",
          placeholder: "Pick one",
          choicesLoader: (win, args) => {
            loaderWindow = win;
            loaderArgs = args;
            return Promise.resolve([{ label: "Choice", value: "choice" }]);
          },
        },
      ]);
      const ctrl = createController();

      ctrl.executeCommand(command);
      ctrl.updateSearch("workspace-a");
      ctrl.advanceStep();

      assertEquals(
        loaderWindow,
        window,
        "loader should receive controller target window",
      );
      assertEquals(
        loaderArgs?.context,
        "workspace-a",
        "loader should receive saved inputs",
      );
      await Promise.resolve();
      assertEquals(
        ctrl.state.filteredStepChoices()[0]?.value,
        "choice",
        "loader result should apply",
      );
    },
  },

  // --- conditional step traversal ---
  {
    name:
      "conditional steps are skipped forward and backward with input restored",
    fn() {
      let predicateWindow: Window | undefined;
      const command = makeStepCommand([
        {
          id: "destination",
          label: "Destination",
          placeholder: "Type destination",
        },
        {
          id: "container",
          label: "Container",
          placeholder: "Type container",
          shouldInclude: (args, win) => {
            predicateWindow = win;
            return args.destination !== "current-tab";
          },
        },
        { id: "url", label: "URL", placeholder: "Type URL" },
      ]);
      const ctrl = createController();

      ctrl.executeCommand(command);
      ctrl.updateSearch("current-tab");
      ctrl.advanceStep();

      assertEquals(
        ctrl.state.currentStepIndex(),
        2,
        "forward traversal should skip container",
      );
      assertEquals(
        predicateWindow,
        window,
        "predicate should receive controller target window",
      );
      assertEquals(
        ctrl.state.stepInputs().container,
        undefined,
        "skipped step should not have input",
      );
      const progress = ctrl.getStepProgress();
      assertEquals(
        progress.current,
        2,
        "URL should be the second visible step",
      );
      assertEquals(progress.total, 2, "only two steps should be visible");

      ctrl.goBackStep();
      assertEquals(
        ctrl.state.currentStepIndex(),
        0,
        "back traversal should skip container",
      );
      assertEquals(
        ctrl.state.query(),
        "current-tab",
        "previous input should be restored",
      );
    },
  },
  {
    name: "inputs from newly excluded steps are pruned before execution",
    fn() {
      let executedArgs: Record<string, string> | undefined;
      const command = makeStepCommand(
        [
          {
            id: "destination",
            label: "Destination",
            placeholder: "Type destination",
          },
          {
            id: "container",
            label: "Container",
            placeholder: "Type container",
            shouldInclude: (args) => args.destination !== "current-tab",
          },
          { id: "url", label: "URL", placeholder: "Type URL" },
        ],
        (_win, args) => {
          executedArgs = args;
        },
      );
      const ctrl = createController();

      ctrl.executeCommand(command);
      ctrl.updateSearch("new-tab");
      ctrl.advanceStep();
      ctrl.updateSearch("3");
      ctrl.advanceStep();
      ctrl.goBackStep();
      ctrl.goBackStep();

      ctrl.updateSearch("current-tab");
      ctrl.advanceStep();
      assertEquals(
        ctrl.state.currentStepIndex(),
        2,
        "container should now be skipped",
      );
      assertEquals(
        ctrl.state.stepInputs().container,
        undefined,
        "stale container input should be pruned",
      );

      ctrl.updateSearch("https://example.com");
      ctrl.advanceStep();
      assertEquals(
        executedArgs?.container,
        undefined,
        "excluded input must not reach command fn",
      );
      assertEquals(
        executedArgs?.destination,
        "current-tab",
        "updated destination should execute",
      );
    },
  },
  {
    name:
      "stale choices loader cannot overwrite another command at the same step index",
    async fn() {
      const stale = createDeferred<Array<{ label: string; value: string }>>();
      const staleCommand = makeStepCommand([
        {
          id: "shared-step-id",
          label: "Stale",
          placeholder: "Wait",
          choicesLoader: () => stale.promise,
        },
      ]);
      const currentCommand = makeStepCommand([
        {
          id: "shared-step-id",
          label: "Current",
          placeholder: "Wait",
          choicesLoader: () =>
            Promise.resolve([{ label: "Current", value: "current" }]),
        },
      ]);
      const ctrl = createController();

      ctrl.executeCommand(staleCommand);
      ctrl.executeCommand(currentCommand);
      await Promise.resolve();
      assertEquals(
        ctrl.state.filteredStepChoices()[0]?.value,
        "current",
        "current loader should apply",
      );

      stale.resolve([{ label: "Stale", value: "stale" }]);
      await Promise.resolve();
      assertEquals(
        ctrl.state.filteredStepChoices()[0]?.value,
        "current",
        "stale loader should be ignored",
      );
    },
  },

  // --- updateStepChoices filtering ---
  {
    name: "updateSearch filters choices in input mode",
    async fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_CHOICES);
      assertEquals(
        ctrl.state.filteredStepChoices().length,
        3,
        "should start with 3 choices",
      );
      ctrl.updateSearch("alp");
      // Non-empty queries are debounced; flush before asserting the filter.
      await flushDebounce();
      assertEquals(ctrl.state.filteredStepChoices().length, 1, "should filter to 1 choice");
      assertEquals(ctrl.state.filteredStepChoices()[0].value, "a", "filtered choice should be Alpha");
    },
  },
  {
    name: "empty query restores all choices",
    async fn() {
      const ctrl = createController();
      ctrl.executeCommand(STEP_COMMAND_WITH_CHOICES);
      ctrl.updateSearch("alp");
      // Non-empty queries are debounced; flush before asserting the filter.
      await flushDebounce();
      assertEquals(ctrl.state.filteredStepChoices().length, 1, "should be filtered");
      ctrl.updateSearch("");
      assertEquals(
        ctrl.state.filteredStepChoices().length,
        3,
        "should restore all choices",
      );
    },
  },
];

export async function runAllTests(): Promise<void> {
  try {
    await runTests("commandPaletteController.test.ts", [
      ...rawTests,
      ...shortcutTests,
      ...tabSearchTests,
      ...reservedListTests,
      ...bookmarkHistorySearchTests,
    ]);
  } finally {
    for (const ctrl of liveControllers.splice(0)) {
      ctrl.destroy();
    }
  }
}
