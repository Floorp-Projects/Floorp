// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// Dead and unknown CSS custom properties in Floorp's chrome CSS.
//
// Gecko renames chrome variables between releases (Gecko 152 turned
// `--toolbar-bgcolor` into `--toolbar-background-color`, see
// utils/gecko-152-var-aliases.css.ts). Nothing fails when that happens:
// a `var(--old)` read just falls back, and an override of `--old` just stops
// reaching Gecko's surfaces. This test lists
//
// 1. variables Floorp/Lepton read with `var()` that nothing defines: not
//    Floorp/Lepton, not Gecko's own chrome/toolkit CSS, not a `"--name"`
//    string in Gecko or Floorp JS (ThemeVariableMap, setProperty); and
// 2. variables Floorp/Lepton define that nothing reads: no `var()` in Gecko
//    or Floorp CSS and no `"--name"` string in Gecko or Floorp JS.
//
// Gecko's side is read from the Runtime the test browser runs (omni.ja when
// packaged, the unpacked chrome directories in a developer Runtime), so the
// result follows the pinned Runtime and needs no network. The live chrome
// window is deliberately not consulted: its variables depend on whichever
// design and theme the browser happens to have when the test runs.
//
// Both lists start from a reviewed baseline (css-variables-allowlist.ts);
// only names that are not in the baseline fail. Stale baseline entries are
// reported with console.warn.
//
// Known blind spot: a variable that Floorp/Lepton both sets and reads but
// Gecko stopped reading (the #3093 `--toolbar-bgcolor` case) is in neither
// list; Floorp's own reads keep it "alive".

import {
  ALLOWED_UNDEFINED_READS,
  ALLOWED_UNREAD_DEFS,
} from "./css-variables-allowlist.ts";
import {
  emptyUses,
  findVarProblems,
  type PlatformScan,
  scanCss,
  scanFloorpSources,
  scanPlatform,
  scanTs,
} from "./css-variables.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

let platformScan: PlatformScan | null = null;
async function platform(): Promise<PlatformScan> {
  platformScan ??= await scanPlatform();
  return platformScan;
}

function testScannerSelfCheck(): void {
  const floorp = emptyUses();
  scanCss(
    "/* --commented: 1; var(--commented-read) */\n" +
      ":root { --a: 1; --b: var(--c, var(--d)); }\n" +
      "@property --e { syntax: '*'; inherits: false; }\n" +
      "#x:hover { color: var(--gecko-def); }",
    "a.css",
    floorp,
  );
  scanTs(
    'el.style.setProperty("--f", "1");\n' +
      "const CSS = `#y { --g: 1; color: var(--h); }`;\n" +
      'el.style.width = "var(--i)";',
    "b.ts",
    floorp,
  );
  assertEquals(
    [...floorp.defs.keys()].sort().join(" "),
    "--a --b --e --g",
    "definitions must come from declarations and @property",
  );
  assertEquals(
    [...floorp.reads.keys()].sort().join(" "),
    "--c --d --gecko-def --h --i",
    "reads must come from var(), incl. nested fallbacks and TS strings",
  );
  assertEquals(floorp.defs.get("--a"), "a.css:2", "definitions carry a line");
  assertEquals(floorp.reads.get("--h"), "b.ts:2", "TS templates carry a line");
  const gecko = emptyUses();
  scanCss(
    ":root { --gecko-def: 1; --d: 2 } #z { color: var(--b) }",
    "g.css",
    gecko,
  );
  const found = findVarProblems(floorp, gecko);
  assertEquals(
    [...found.undefinedReads.keys()].sort().join(" "),
    "--c --h --i",
    "only reads with no definition anywhere are reported",
  );
  assertEquals(
    [...found.unreadDefs.keys()].sort().join(" "),
    "--a --e --g",
    "only definitions nobody reads are reported (--f is a mention)",
  );
}

async function testPlatformScan(): Promise<void> {
  const { uses, files, layout } = await platform();
  assert(files > 500, `expected Gecko chrome files (${layout}); got ${files}`);
  for (
    const name of ["--toolbar-background-color", "--panel-background-color"]
  ) {
    assert(uses.defs.has(name), `Gecko 159 must define ${name} (${layout})`);
    assert(uses.reads.has(name), `Gecko 159 must read ${name} (${layout})`);
  }
  assert(
    uses.mentions.has("--lwt-accent-color"),
    "ThemeVariableMap's --lwt-* names must be seen as JS mentions",
  );
}

function compare(
  found: Map<string, string>,
  allowed: Record<string, string>,
  what: string,
): void {
  const fresh = [...found].filter(([name]) => !(name in allowed));
  const stale = Object.keys(allowed).filter((name) => !found.has(name));
  if (stale.length) {
    console.warn(
      `[css-variables] ${what}: allowlist entries no longer needed: ${
        stale.join(", ")
      }`,
    );
  }
  assertEquals(
    fresh.length,
    0,
    `${what} (add a fix, or an entry with a reason to ` +
      `css-variables-allowlist.ts):\n  ${
        fresh.map(([name, where]) => `${name}  (${where})`).join("\n  ")
      }\n`,
  );
}

async function testNoNewUndefinedReads(): Promise<void> {
  const found = findVarProblems(
    scanFloorpSources(),
    (await platform()).uses,
  );
  compare(
    found.undefinedReads,
    ALLOWED_UNDEFINED_READS,
    "Floorp/Lepton read custom properties that nothing defines",
  );
}

async function testNoNewUnreadDefinitions(): Promise<void> {
  const found = findVarProblems(
    scanFloorpSources(),
    (await platform()).uses,
  );
  compare(
    found.unreadDefs,
    ALLOWED_UNREAD_DEFS,
    "Floorp/Lepton set custom properties that nothing reads",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "scanner self-check", fn: testScannerSelfCheck },
    { name: "Gecko chrome files are scanned", fn: testPlatformScan },
    { name: "no new undefined var() reads", fn: testNoNewUndefinedReads },
    { name: "no new unread definitions", fn: testNoNewUnreadDefinitions },
  ];
  await runTests("css-variables.test.ts", tests);
}

await runAllTests();
