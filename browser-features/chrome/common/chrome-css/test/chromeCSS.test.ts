// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  assertNotEquals,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

// ---------------------------------------------------------------------------
// Module under test — imported lazily so globals are available first
// ---------------------------------------------------------------------------

// CSSEntry and ChromeCSSService are imported inside test functions to ensure
// browser globals (Cc, Ci, Services, etc.) are set up before module evaluation.

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Temporary folder path for CSSEntry tests (does not need to exist for construction) */
const TEST_CSS_FOLDER = PathUtils.join(
  Services.dirsvc.get("ProfD", Ci.nsIFile).path,
  "chrome-test-css",
);

/**
 * Reset the ChromeCSSService singleton between tests.
 * Accesses the module-level `instance` variable via module re-import.
 */
async function resetServiceSingleton(): Promise<void> {
  const mod = await import("../service.tsx");
  // The module exports ChromeCSSService but instance is module-scoped.
  // We call uninit() to reset state if initialized.
  const svc = mod.ChromeCSSService.getInstance();
  if (svc.initialized) {
    svc.uninit();
  }
  // uninit() does not clear readCSS, so we reset it manually
  // to prevent state leakage between tests.
  svc.readCSS = {};
}

// ---------------------------------------------------------------------------
// Tests: CSSEntry — Sheet type detection
// ---------------------------------------------------------------------------

async function testCSSEntryAgentSheetForXulPrefix(): Promise<void> {
  const { CSSEntry } = await import("../cssEntry.ts");
  const entry = new CSSEntry("xul-custom.css", TEST_CSS_FOLDER);
  assertEquals(
    entry.SHEET,
    Ci.nsIStyleSheetService.AGENT_SHEET,
    "xul- prefix should map to AGENT_SHEET",
  );
}

async function testCSSEntryAgentSheetForAsCss(): Promise<void> {
  const { CSSEntry } = await import("../cssEntry.ts");
  const entry = new CSSEntry("theme.as.css", TEST_CSS_FOLDER);
  assertEquals(
    entry.SHEET,
    Ci.nsIStyleSheetService.AGENT_SHEET,
    ".as.css suffix should map to AGENT_SHEET",
  );
}

async function testCSSEntryAuthorSheet(): Promise<void> {
  const { CSSEntry } = await import("../cssEntry.ts");
  const entry = new CSSEntry("override.author.css", TEST_CSS_FOLDER);
  assertEquals(
    entry.SHEET,
    Ci.nsIStyleSheetService.AUTHOR_SHEET,
    ".author.css suffix should map to AUTHOR_SHEET",
  );
}

async function testCSSEntryUserSheet(): Promise<void> {
  const { CSSEntry } = await import("../cssEntry.ts");
  const entry = new CSSEntry("my-style.css", TEST_CSS_FOLDER);
  assertEquals(
    entry.SHEET,
    Ci.nsIStyleSheetService.USER_SHEET,
    "regular .css should map to USER_SHEET",
  );
}

async function testCSSEntryUserSheetForUcCss(): Promise<void> {
  const { CSSEntry } = await import("../cssEntry.ts");
  // .uc.css is excluded in rebuild() but CSSEntry itself still creates it as USER_SHEET
  const entry = new CSSEntry("script.uc.css", TEST_CSS_FOLDER);
  assertEquals(
    entry.SHEET,
    Ci.nsIStyleSheetService.USER_SHEET,
    ".uc.css should map to USER_SHEET",
  );
}

// ---------------------------------------------------------------------------
// Tests: CSSEntry — Properties
// ---------------------------------------------------------------------------

async function testCSSEntryProperties(): Promise<void> {
  const { CSSEntry } = await import("../cssEntry.ts");
  const entry = new CSSEntry("test-file.css", TEST_CSS_FOLDER);
  assertEquals(
    entry.leafName,
    "test-file.css",
    "leafName should match filename",
  );
  assertEquals(
    entry.path,
    PathUtils.join(TEST_CSS_FOLDER, "test-file.css"),
    "path should be joined with folder",
  );
  assertEquals(
    entry.lastModifiedTime,
    1,
    "lastModifiedTime should default to 1",
  );
  assertEquals(entry.enabled, false, "enabled should default to false");
}

async function testCSSEntryEnabledSetter(): Promise<void> {
  const { CSSEntry } = await import("../cssEntry.ts");
  const entry = new CSSEntry("enable-test.css", TEST_CSS_FOLDER);
  assertEquals(entry.enabled, false, "should start disabled");

  // Setting enabled to false when already false should not throw
  entry.enabled = false;
  assertEquals(entry.enabled, false, "should remain false");
}

async function testCSSEntryMultipleSheetTypes(): Promise<void> {
  const { CSSEntry } = await import("../cssEntry.ts");

  const names = [
    "xul-tabs.css",
    "xul-navbar.as.css",
    "content.author.css",
    "user-style.css",
    "mixed.AS.CSS", // case-insensitive
  ];

  const entry0 = new CSSEntry(names[0], TEST_CSS_FOLDER);
  const entry1 = new CSSEntry(names[1], TEST_CSS_FOLDER);
  const entry2 = new CSSEntry(names[2], TEST_CSS_FOLDER);
  const entry3 = new CSSEntry(names[3], TEST_CSS_FOLDER);
  const entry4 = new CSSEntry(names[4], TEST_CSS_FOLDER);

  assertEquals(
    entry0.SHEET,
    Ci.nsIStyleSheetService.AGENT_SHEET,
    `${names[0]} → AGENT`,
  );
  assertEquals(
    entry1.SHEET,
    Ci.nsIStyleSheetService.AGENT_SHEET,
    `${names[1]} → AGENT`,
  );
  assertEquals(
    entry2.SHEET,
    Ci.nsIStyleSheetService.AUTHOR_SHEET,
    `${names[2]} → AUTHOR`,
  );
  assertEquals(
    entry3.SHEET,
    Ci.nsIStyleSheetService.USER_SHEET,
    `${names[3]} → USER`,
  );
  // .AS.CSS is case-insensitive due to /i flag on regex
  assertEquals(
    entry4.SHEET,
    Ci.nsIStyleSheetService.AGENT_SHEET,
    `${names[4]} → AGENT (case insensitive)`,
  );
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — Singleton
// ---------------------------------------------------------------------------

async function testGetInstanceReturnsObject(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const instance = ChromeCSSService.getInstance();
  assert(instance !== null, "getInstance should return non-null");
  assert(typeof instance === "object", "getInstance should return an object");
}

async function testGetInstanceReturnsSameInstance(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const a = ChromeCSSService.getInstance();
  const b = ChromeCSSService.getInstance();
  assertEquals(a, b, "getInstance should return the same instance");
}

async function testServiceNotInitializedByDefault(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();
  // Service may or may not be initialized depending on test order,
  // but after reset it should not be
  assertEquals(
    svc.initialized,
    false,
    "service should not be initialized after reset",
  );
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — Sheet class name mapping
// ---------------------------------------------------------------------------

async function testGetSheetClassNameAgent(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();
  const { CSSEntry } = await import("../cssEntry.ts");
  const entry = new CSSEntry("xul-test.css", TEST_CSS_FOLDER);
  assertEquals(
    svc.getSheetClassName(entry),
    "AGENT_SHEET",
    "xul- prefixed entry should return AGENT_SHEET name",
  );
}

async function testGetSheetClassNameAuthor(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();
  const { CSSEntry } = await import("../cssEntry.ts");
  const entry = new CSSEntry("style.author.css", TEST_CSS_FOLDER);
  assertEquals(
    svc.getSheetClassName(entry),
    "AUTHOR_SHEET",
    ".author.css entry should return AUTHOR_SHEET name",
  );
}

async function testGetSheetClassNameUser(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();
  const { CSSEntry } = await import("../cssEntry.ts");
  const entry = new CSSEntry("custom.css", TEST_CSS_FOLDER);
  assertEquals(
    svc.getSheetClassName(entry),
    "USER_SHEET",
    "regular .css entry should return USER_SHEET name",
  );
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — CSS folder path
// ---------------------------------------------------------------------------

async function testGetCSSFolderReturnsString(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();
  const folder = svc.getCSSFolder();
  assert(typeof folder === "string", "getCSSFolder should return a string");
  assert(folder.length > 0, "getCSSFolder should return non-empty string");
}

async function testGetCSSFolderContainsChromeSegment(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();
  const folder = svc.getCSSFolder();
  // Default path should contain "chrome" and "CSS" segments
  assert(
    folder.includes("chrome") || folder.includes("CSS"),
    "CSS folder path should contain chrome/CSS segments",
  );
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — readCSS and toggle state management
// ---------------------------------------------------------------------------

async function testReadCSSEmptyByDefault(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();
  assertEquals(
    Object.keys(svc.readCSS).length,
    0,
    "readCSS should be empty after reset",
  );
}

async function testLoadCSSCreatesEntry(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const entry = svc.loadCSS("test-load.css", TEST_CSS_FOLDER);

  assert(entry !== null, "loadCSS should return an entry");
  assertEquals(
    entry.leafName,
    "test-load.css",
    "entry should have correct leafName",
  );
  assert("test-load.css" in svc.readCSS, "entry should be stored in readCSS");
}

async function testLoadCSSReturnsSameEntryForSameName(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const first = svc.loadCSS("duplicate.css", TEST_CSS_FOLDER);
  const second = svc.loadCSS("duplicate.css", TEST_CSS_FOLDER);

  assertEquals(first, second, "loadCSS should return same entry for same name");
}

async function testToggleFlipsEnabledState(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  svc.loadCSS("toggle-test.css", TEST_CSS_FOLDER);
  const before = svc.readCSS["toggle-test.css"].enabled;

  svc.toggle("toggle-test.css");
  const after = svc.readCSS["toggle-test.css"].enabled;

  assertEquals(after, !before, "toggle should flip enabled state");
}

async function testToggleNonExistentDoesNotThrow(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  // toggle() returns early silently when the file doesn't exist in readCSS
  let threw = false;
  try {
    svc.toggle("nonexistent.css");
  } catch {
    threw = true;
  }
  assertEquals(threw, false, "toggle for non-existent file should not throw");
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — updateCssFilesList
// ---------------------------------------------------------------------------

async function testUpdateCssFilesListReflectsReadCSS(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  svc.loadCSS("file-a.css", TEST_CSS_FOLDER);
  svc.loadCSS("file-b.css", TEST_CSS_FOLDER);
  svc.loadCSS("file-c.css", TEST_CSS_FOLDER);

  svc.updateCssFilesList();
  const files = svc.getCssFiles();

  assertEquals(files.length, 3, "should have 3 CSS files");
  const names = files.map((f) => f.name).sort();
  assertEquals(names[0], "file-a.css", "first file should be file-a.css");
  assertEquals(names[1], "file-b.css", "second file should be file-b.css");
  assertEquals(names[2], "file-c.css", "third file should be file-c.css");
}

async function testUpdateCssFilesListEntryReference(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const entry = svc.loadCSS("ref-test.css", TEST_CSS_FOLDER);
  svc.updateCssFilesList();
  const files = svc.getCssFiles();

  assertEquals(files.length, 1, "should have 1 file");
  assertEquals(
    files[0].entry,
    entry,
    "entry reference should match readCSS entry",
  );
  assertEquals(files[0].name, "ref-test.css", "name should match");
}

async function testUpdateCssFilesListEmptyAfterReset(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  svc.loadCSS("temp.css", TEST_CSS_FOLDER);
  svc.updateCssFilesList();
  assert(svc.getCssFiles().length > 0, "should have files before reset");

  // uninit() only runs cleanup when initialized === true
  svc.initialized = true;
  svc.uninit();
  assertEquals(svc.getCssFiles().length, 0, "should be empty after uninit");
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — uninit cleanup
// ---------------------------------------------------------------------------

async function testUninitResetsInitialized(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  // Even without full init, uninit should set initialized to false
  svc.uninit();
  assertEquals(
    svc.initialized,
    false,
    "uninit should set initialized to false",
  );
}

async function testUninitClearsCssFilesButPreservesReadCSS(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  svc.loadCSS("cleanup-test.css", TEST_CSS_FOLDER);
  assert("cleanup-test.css" in svc.readCSS, "entry should exist before uninit");

  // uninit() only runs cleanup when initialized === true
  svc.initialized = true;
  svc.uninit();
  // uninit clears cssFiles signal but does NOT clear readCSS — entries persist
  assertEquals(
    svc.getCssFiles().length,
    0,
    "cssFiles should be empty after uninit",
  );
  assert(
    "cleanup-test.css" in svc.readCSS,
    "readCSS entries persist across uninit (rebuild reuses them)",
  );
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSS component (index.ts)
// ---------------------------------------------------------------------------

async function testChromeCSSComponentHasInitMethod(): Promise<void> {
  const mod = await import("../index.ts");
  const ChromeCSS = mod.default;
  assert(
    typeof ChromeCSS.prototype.init === "function",
    "ChromeCSS should have init method",
  );
}

async function testChromeCSSComponentHasStaticCtx(): Promise<void> {
  const mod = await import("../index.ts");
  const ChromeCSS = mod.default;
  assert("ctx" in ChromeCSS, "ChromeCSS should have static ctx property");
}

// ---------------------------------------------------------------------------
// Tests: Sheet type constants
// ---------------------------------------------------------------------------

async function testSheetTypeConstants(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  assertEquals(
    svc.AGENT_SHEET,
    Ci.nsIStyleSheetService.AGENT_SHEET,
    "AGENT_SHEET constant",
  );
  assertEquals(
    svc.USER_SHEET,
    Ci.nsIStyleSheetService.USER_SHEET,
    "USER_SHEET constant",
  );
  assertEquals(
    svc.AUTHOR_SHEET,
    Ci.nsIStyleSheetService.AUTHOR_SHEET,
    "AUTHOR_SHEET constant",
  );
}

async function testSheetTypeConstantsAreDistinct(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  assertNotEquals(
    svc.AGENT_SHEET,
    svc.USER_SHEET,
    "AGENT and USER should differ",
  );
  assertNotEquals(
    svc.AGENT_SHEET,
    svc.AUTHOR_SHEET,
    "AGENT and AUTHOR should differ",
  );
  assertNotEquals(
    svc.USER_SHEET,
    svc.AUTHOR_SHEET,
    "USER and AUTHOR should differ",
  );
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    // CSSEntry — Sheet type detection
    {
      name: "CSSEntry: xul- prefix → AGENT_SHEET",
      fn: testCSSEntryAgentSheetForXulPrefix,
    },
    {
      name: "CSSEntry: .as.css suffix → AGENT_SHEET",
      fn: testCSSEntryAgentSheetForAsCss,
    },
    {
      name: "CSSEntry: .author.css suffix → AUTHOR_SHEET",
      fn: testCSSEntryAuthorSheet,
    },
    {
      name: "CSSEntry: regular .css → USER_SHEET",
      fn: testCSSEntryUserSheet,
    },
    {
      name: "CSSEntry: .uc.css → USER_SHEET",
      fn: testCSSEntryUserSheetForUcCss,
    },

    // CSSEntry — Properties
    {
      name: "CSSEntry: constructor sets properties correctly",
      fn: testCSSEntryProperties,
    },
    {
      name: "CSSEntry: enabled defaults to false",
      fn: testCSSEntryEnabledSetter,
    },
    {
      name: "CSSEntry: multiple sheet types detected correctly",
      fn: testCSSEntryMultipleSheetTypes,
    },

    // ChromeCSSService — Singleton
    {
      name: "ChromeCSSService: getInstance returns object",
      fn: testGetInstanceReturnsObject,
    },
    {
      name: "ChromeCSSService: getInstance returns same instance",
      fn: testGetInstanceReturnsSameInstance,
    },
    {
      name: "ChromeCSSService: not initialized after reset",
      fn: testServiceNotInitializedByDefault,
    },

    // ChromeCSSService — Sheet class name
    {
      name: "ChromeCSSService: getSheetClassName AGENT",
      fn: testGetSheetClassNameAgent,
    },
    {
      name: "ChromeCSSService: getSheetClassName AUTHOR",
      fn: testGetSheetClassNameAuthor,
    },
    {
      name: "ChromeCSSService: getSheetClassName USER",
      fn: testGetSheetClassNameUser,
    },

    // ChromeCSSService — CSS folder
    {
      name: "ChromeCSSService: getCSSFolder returns string",
      fn: testGetCSSFolderReturnsString,
    },
    {
      name: "ChromeCSSService: getCSSFolder contains chrome/CSS",
      fn: testGetCSSFolderContainsChromeSegment,
    },

    // ChromeCSSService — State management
    {
      name: "ChromeCSSService: readCSS empty after reset",
      fn: testReadCSSEmptyByDefault,
    },
    {
      name: "ChromeCSSService: loadCSS creates entry",
      fn: testLoadCSSCreatesEntry,
    },
    {
      name: "ChromeCSSService: loadCSS returns same entry for same name",
      fn: testLoadCSSReturnsSameEntryForSameName,
    },
    {
      name: "ChromeCSSService: toggle flips enabled state",
      fn: testToggleFlipsEnabledState,
    },
    {
      name: "ChromeCSSService: toggle non-existent does not throw",
      fn: testToggleNonExistentDoesNotThrow,
    },

    // ChromeCSSService — updateCssFilesList
    {
      name: "ChromeCSSService: updateCssFilesList reflects readCSS",
      fn: testUpdateCssFilesListReflectsReadCSS,
    },
    {
      name: "ChromeCSSService: updateCssFilesList entry reference",
      fn: testUpdateCssFilesListEntryReference,
    },
    {
      name: "ChromeCSSService: updateCssFilesList empty after reset",
      fn: testUpdateCssFilesListEmptyAfterReset,
    },

    // ChromeCSSService — uninit cleanup
    {
      name: "ChromeCSSService: uninit resets initialized",
      fn: testUninitResetsInitialized,
    },
    {
      name: "ChromeCSSService: uninit clears cssFiles, preserves readCSS",
      fn: testUninitClearsCssFilesButPreservesReadCSS,
    },

    // ChromeCSS component (index.ts)
    {
      name: "ChromeCSS component: has init method",
      fn: testChromeCSSComponentHasInitMethod,
    },
    {
      name: "ChromeCSS component: has static ctx",
      fn: testChromeCSSComponentHasStaticCtx,
    },

    // Sheet type constants
    {
      name: "ChromeCSSService: sheet type constants match Gecko",
      fn: testSheetTypeConstants,
    },
    {
      name: "ChromeCSSService: sheet type constants are distinct",
      fn: testSheetTypeConstantsAreDistinct,
    },
  ];

  const { runTests } = await import("../../../test/utils/test_harness.ts");
  await runTests("chromeCSS.test.ts", tests);
}
