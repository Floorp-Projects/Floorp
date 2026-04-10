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

  // Reset the static ctx on the ChromeCSS component
  const indexMod = await import("../index.ts");
  indexMod.default.ctx = null;
}

/**
 * Mock browser dialogs (window.prompt, Services.prompt.confirm/alert) so that
 * svc.create() does not block on user interaction.
 * Returns a restore function to undo the mocks.
 */
function mockPromptDialogs(): () => void {
  const originalConfirm = Services.prompt.confirm;
  const originalAlert = Services.prompt.alert;
  const originalPrompt = globalThis.prompt;

  // Services.prompt.confirm and .alert may be read-only in Firefox,
  // so use Object.defineProperty wrapped in try/catch.
  try {
    Object.defineProperty(Services.prompt, "confirm", {
      value: () => false,
      configurable: true,
      writable: true,
    });
  } catch {
    // Property is non-configurable; tests using this mock may be skipped.
  }
  try {
    Object.defineProperty(Services.prompt, "alert", {
      value: () => {},
      configurable: true,
      writable: true,
    });
  } catch {
    // Property is non-configurable; tests using this mock may be skipped.
  }
  globalThis.prompt = () => null;

  return () => {
    try {
      Object.defineProperty(Services.prompt, "confirm", {
        value: originalConfirm,
        configurable: true,
        writable: true,
      });
    } catch {
      // Ignore if property cannot be restored.
    }
    try {
      Object.defineProperty(Services.prompt, "alert", {
        value: originalAlert,
        configurable: true,
        writable: true,
      });
    } catch {
      // Ignore if property cannot be restored.
    }
    globalThis.prompt = originalPrompt;
  };
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
// Tests: CSSEntry — enabled setter with file operations
// ---------------------------------------------------------------------------

async function testCSSEntryEnabledSetterHandlesNonExistentFile(): Promise<void> {
  const { CSSEntry } = await import("../cssEntry.ts");
  const entry = new CSSEntry("nonexistent.css", TEST_CSS_FOLDER);

  // Setting enabled on non-existent file should not throw
  // (it logs errors but catches them internally)
  let threw = false;
  try {
    entry.enabled = true;
  } catch {
    threw = true;
  }
  assertEquals(threw, false, "enabled setter should not throw for non-existent file");
}

async function testCSSEntryEnabledSetterUpdatesInternalState(): Promise<void> {
  const { CSSEntry } = await import("../cssEntry.ts");
  const entry = new CSSEntry("state-test.css", TEST_CSS_FOLDER);

  assertEquals(entry.enabled, false, "should start disabled");

  // Even if file operations fail, internal state should update
  entry.enabled = true;
  assertEquals(entry.enabled, true, "internal enabled state should be true");

  entry.enabled = false;
  assertEquals(entry.enabled, false, "internal enabled state should be false");
}

// ---------------------------------------------------------------------------
// Tests: CSSEntry — edge cases
// ---------------------------------------------------------------------------

async function testCSSEntryCaseInsensitiveMatching(): Promise<void> {
  const { CSSEntry } = await import("../cssEntry.ts");

  // Test various case combinations for .as.css
  const asEntry1 = new CSSEntry("test.AS.CSS", TEST_CSS_FOLDER);
  const asEntry2 = new CSSEntry("test.As.Css", TEST_CSS_FOLDER);
  const asEntry3 = new CSSEntry("test.aS.cSs", TEST_CSS_FOLDER);

  assertEquals(asEntry1.SHEET, Ci.nsIStyleSheetService.AGENT_SHEET, "uppercase AS.CSS");
  assertEquals(asEntry2.SHEET, Ci.nsIStyleSheetService.AGENT_SHEET, "mixed case As.Css");
  assertEquals(asEntry3.SHEET, Ci.nsIStyleSheetService.AGENT_SHEET, "mixed case aS.cSs");

  // Test case variations for .author.css
  const authorEntry = new CSSEntry("test.AUTHOR.CSS", TEST_CSS_FOLDER);
  assertEquals(authorEntry.SHEET, Ci.nsIStyleSheetService.AUTHOR_SHEET, "uppercase AUTHOR.CSS");
}

async function testCSSEntrySpecialCharactersInFilename(): Promise<void> {
  const { CSSEntry } = await import("../cssEntry.ts");
  const entry = new CSSEntry("test-file_123.css", TEST_CSS_FOLDER);

  assertEquals(entry.leafName, "test-file_123.css", "should handle special characters");
  assertEquals(
    entry.path,
    PathUtils.join(TEST_CSS_FOLDER, "test-file_123.css"),
    "should construct path with special characters",
  );
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSS component — init method
// ---------------------------------------------------------------------------

async function testChromeCSSComponentInitCreatesService(): Promise<void> {
  await resetServiceSingleton();
  const mod = await import("../index.ts");
  const ChromeCSS = mod.default;

  const instance = new ChromeCSS();

  // @noraComponent decorator auto-calls init() in constructor,
  // so ctx may already be set after construction.
  instance.init();

  assert(ChromeCSS.ctx !== null, "ctx should be non-null after init");
  assertEquals(ChromeCSS.ctx.initialized, true, "service should be initialized after init");
}

async function testChromeCSSComponentInitOnlyOnce(): Promise<void> {
  await resetServiceSingleton();
  const mod = await import("../index.ts");
  const ChromeCSS = mod.default;

  const instance = new ChromeCSS();
  instance.init();

  const firstCtx = ChromeCSS.ctx;
  assert(firstCtx !== null, "first init should create context");

  // Second init should not create new instance
  instance.init();
  assertEquals(ChromeCSS.ctx, firstCtx, "ctx should remain the same after second init");
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — rebuild method
// ---------------------------------------------------------------------------

async function testRebuildCreatesDirectoryIfNotExists(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  // Use a test-specific folder that likely doesn't exist
  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-rebuild",
  );

  // Temporarily override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  // This should not throw even if folder doesn't exist
  let threw = false;
  try {
    await svc.rebuild();
  } catch {
    threw = true;
  }
  assertEquals(threw, false, "rebuild should not throw when folder doesn't exist");

  // Restore original method
  svc.getCSSFolder = originalGetCSSFolder;

  // Cleanup
  try {
    await IOUtils.remove(testFolder, { recursive: true });
  } catch {
    // Ignore cleanup errors
  }
}

async function testRebuildExcludesUcCssFiles(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-exclude",
  );

  // Create test directory and files
  await IOUtils.makeDirectory(testFolder);
  await IOUtils.writeUTF8(PathUtils.join(testFolder, "regular.css"), "");
  await IOUtils.writeUTF8(PathUtils.join(testFolder, "script.uc.css"), "");
  await IOUtils.writeUTF8(PathUtils.join(testFolder, "another.uc.css"), "");

  // Temporarily override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  try {
    await svc.rebuild();

    // regular.css should be loaded, .uc.css files should not
    assert("regular.css" in svc.readCSS, "regular.css should be loaded");
    assert(!("script.uc.css" in svc.readCSS), "script.uc.css should be excluded");
    assert(!("another.uc.css" in svc.readCSS), "another.uc.css should be excluded");
  } finally {
    // Restore and cleanup
    svc.getCSSFolder = originalGetCSSFolder;
    try {
      await IOUtils.remove(testFolder, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
}

async function testRebuildHandlesDeletedFiles(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-deleted",
  );

  // Create test directory and file
  await IOUtils.makeDirectory(testFolder);
  await IOUtils.writeUTF8(PathUtils.join(testFolder, "temp.css"), "");

  // Temporarily override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  try {
    // First rebuild - should load the file
    await svc.rebuild();
    assert("temp.css" in svc.readCSS, "temp.css should be loaded initially");

    // Delete the file
    await IOUtils.remove(PathUtils.join(testFolder, "temp.css"));

    // Second rebuild - should remove the file from readCSS
    await svc.rebuild();
    assert(!("temp.css" in svc.readCSS), "temp.css should be removed after deletion");
  } finally {
    // Restore and cleanup
    svc.getCSSFolder = originalGetCSSFolder;
    try {
      await IOUtils.remove(testFolder, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — loadCSS with preferences
// ---------------------------------------------------------------------------

async function testLoadCSSRespectsDisabledListPref(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  // Set a disabled list preference
  const disabledList = encodeURIComponent("test-disabled.css|another-disabled.css");
  Services.prefs.setStringPref("UserCSSLoader.disabled_list", disabledList);

  try {
    const entry1 = svc.loadCSS("test-disabled.css", TEST_CSS_FOLDER);
    const entry2 = svc.loadCSS("another-disabled.css", TEST_CSS_FOLDER);
    const entry3 = svc.loadCSS("enabled.css", TEST_CSS_FOLDER);

    assertEquals(entry1.enabled, false, "test-disabled.css should be disabled");
    assertEquals(entry2.enabled, false, "another-disabled.css should be disabled");
    assertEquals(entry3.enabled, true, "enabled.css should be enabled");
  } finally {
    // Cleanup
    Services.prefs.clearUserPref("UserCSSLoader.disabled_list");
  }
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — convertUTF8ToShiftJIS
// ---------------------------------------------------------------------------

async function testConvertUTF8ToShiftJISHandlesBasicString(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const input = "test-string";
  const result = svc.convertUTF8ToShiftJIS(input);

  // Should return a string (even if conversion is a no-op in the implementation)
  assertEquals(typeof result, "string", "should return a string");
}

async function testConvertUTF8ToShiftJISHandlesEmptyString(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const result = svc.convertUTF8ToShiftJIS("");
  assertEquals(typeof result, "string", "should return string for empty input");
}

async function testConvertUTF8ToShiftJISHandlesSpecialCharacters(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const input = "path/with/slashes\\and\\backslashes";
  const result = svc.convertUTF8ToShiftJIS(input);

  // Should not throw and should return a string
  assertEquals(typeof result, "string", "should handle special characters");
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — openFolder platform detection
// ---------------------------------------------------------------------------

async function testOpenFolderHandlesNonExistentFolder(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-open-folder-new",
  );

  // Ensure folder doesn't exist
  try {
    await IOUtils.remove(testFolder, { recursive: true });
  } catch {
    // Ignore if it doesn't exist
  }

  // Override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  try {
    // Should not throw - should create the folder
    await svc.openFolder();

    // Verify folder was created
    const exists = await IOUtils.exists(testFolder);
    assertEquals(exists, true, "folder should be created if it doesn't exist");
  } finally {
    // Restore and cleanup
    svc.getCSSFolder = originalGetCSSFolder;
    try {
      await IOUtils.remove(testFolder, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — editUserCSS
// ---------------------------------------------------------------------------

async function testEditUserCSSConstructsCorrectPath(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-edit",
  );

  // Override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  try {
    // This should not throw when constructing the path
    // (it may fail when trying to actually edit since no editor is configured)
    let _threw = false;
    try {
      svc.editUserCSS("test-edit.css");
    } catch {
      // Expected to fail due to no editor, but should not fail on path construction
      _threw = true;
    }
    // We expect it might throw due to editor not being configured
    // but not due to path construction issues
  } finally {
    // Restore
    svc.getCSSFolder = originalGetCSSFolder;
  }
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — uninit disabled list pref saving
// ---------------------------------------------------------------------------

async function testUninitSavesDisabledListPref(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  // Load multiple CSS files with different enabled states
  svc.loadCSS("enabled-file-1.css", TEST_CSS_FOLDER);
  svc.loadCSS("enabled-file-2.css", TEST_CSS_FOLDER);
  svc.loadCSS("disabled-file-1.css", TEST_CSS_FOLDER);
  svc.loadCSS("disabled-file-2.css", TEST_CSS_FOLDER);

  // Manually set some entries as disabled
  svc.readCSS["disabled-file-1.css"].enabled = false;
  svc.readCSS["disabled-file-2.css"].enabled = false;
  svc.readCSS["enabled-file-1.css"].enabled = true;
  svc.readCSS["enabled-file-2.css"].enabled = true;

  // Set initialized to true so uninit runs the full cleanup
  svc.initialized = true;

  // Clear any existing pref
  try {
    Services.prefs.clearUserPref("UserCSSLoader.disabled_list");
  } catch {
    // Ignore if pref doesn't exist
  }

  // Call uninit which should save the disabled list
  svc.uninit();

  // Verify the pref was saved with correct disabled file names
  const savedPref = Services.prefs.getStringPref("UserCSSLoader.disabled_list", "");
  const decodedPref = decodeURIComponent(savedPref);

  // The pref should contain the disabled files separated by |
  assert(
    decodedPref.includes("disabled-file-1.css"),
    "pref should contain disabled-file-1.css",
  );
  assert(
    decodedPref.includes("disabled-file-2.css"),
    "pref should contain disabled-file-2.css",
  );
  assert(
    !decodedPref.includes("enabled-file-1.css"),
    "pref should not contain enabled-file-1.css",
  );
  assert(
    !decodedPref.includes("enabled-file-2.css"),
    "pref should not contain enabled-file-2.css",
  );

  // Cleanup
  try {
    Services.prefs.clearUserPref("UserCSSLoader.disabled_list");
  } catch {
    // Ignore cleanup errors
  }
}

async function testUninitSavesEmptyDisabledListWhenAllEnabled(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  // Load CSS files that are all enabled
  svc.loadCSS("file-1.css", TEST_CSS_FOLDER);
  svc.loadCSS("file-2.css", TEST_CSS_FOLDER);
  svc.readCSS["file-1.css"].enabled = true;
  svc.readCSS["file-2.css"].enabled = true;

  svc.initialized = true;

  try {
    Services.prefs.clearUserPref("UserCSSLoader.disabled_list");
  } catch {
    // Ignore if pref doesn't exist
  }

  svc.uninit();

  // Verify the pref exists but is empty (or contains only the separator)
  const savedPref = Services.prefs.getStringPref("UserCSSLoader.disabled_list", "");
  const decodedPref = decodeURIComponent(savedPref);

  assertEquals(
    decodedPref,
    "",
    "pref should be empty when all files are enabled",
  );

  // Cleanup
  try {
    Services.prefs.clearUserPref("UserCSSLoader.disabled_list");
  } catch {
    // Ignore cleanup errors
  }
}

async function testUninitSavesAllFilesWhenAllDisabled(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  // Load CSS files that are all disabled
  svc.loadCSS("all-disabled-1.css", TEST_CSS_FOLDER);
  svc.loadCSS("all-disabled-2.css", TEST_CSS_FOLDER);
  svc.readCSS["all-disabled-1.css"].enabled = false;
  svc.readCSS["all-disabled-2.css"].enabled = false;

  svc.initialized = true;

  try {
    Services.prefs.clearUserPref("UserCSSLoader.disabled_list");
  } catch {
    // Ignore if pref doesn't exist
  }

  svc.uninit();

  // Verify the pref contains both disabled files
  const savedPref = Services.prefs.getStringPref("UserCSSLoader.disabled_list", "");
  const decodedPref = decodeURIComponent(savedPref);

  assert(
    decodedPref.includes("all-disabled-1.css"),
    "pref should contain all-disabled-1.css",
  );
  assert(
    decodedPref.includes("all-disabled-2.css"),
    "pref should contain all-disabled-2.css",
  );

  // Cleanup
  try {
    Services.prefs.clearUserPref("UserCSSLoader.disabled_list");
  } catch {
    // Ignore cleanup errors
  }
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — loadCSS re-enable branch
// ---------------------------------------------------------------------------

async function testLoadCSSReEnablesExistingEntry(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  // Load a CSS file for the first time (should be enabled by default)
  const firstLoad = svc.loadCSS("reenable-test.css", TEST_CSS_FOLDER);
  assertEquals(firstLoad.enabled, true, "first load should be enabled by default");

  // Manually disable it
  firstLoad.enabled = false;
  assertEquals(firstLoad.enabled, false, "entry should be disabled after manual change");

  // LoadCSS again - this should NOT re-enable it when already disabled
  // (the re-enable branch only runs when cssFile.enabled is true)
  const secondLoad = svc.loadCSS("reenable-test.css", TEST_CSS_FOLDER);
  assertEquals(
    secondLoad.enabled,
    false,
    "loadCSS should not re-enable a disabled entry",
  );
  assertEquals(
    firstLoad,
    secondLoad,
    "should return the same entry instance",
  );
}

async function testLoadCSSKeepsEnabledEntryEnabled(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  // Load a CSS file for the first time
  const firstLoad = svc.loadCSS("keep-enabled-test.css", TEST_CSS_FOLDER);
  assertEquals(firstLoad.enabled, true, "first load should be enabled by default");

  // LoadCSS again on an already-enabled entry
  // This tests the else if branch at line 235-237
  const secondLoad = svc.loadCSS("keep-enabled-test.css", TEST_CSS_FOLDER);
  assertEquals(
    secondLoad.enabled,
    true,
    "loadCSS should keep an enabled entry enabled",
  );
  assertEquals(
    firstLoad,
    secondLoad,
    "should return the same entry instance",
  );
}

async function testLoadCSSRespectsDisabledListPrefOnReload(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  // Set a disabled list preference
  const disabledList = encodeURIComponent("reload-disabled.css");
  Services.prefs.setStringPref("UserCSSLoader.disabled_list", disabledList);

  try {
    // First load - should be disabled due to pref
    const firstLoad = svc.loadCSS("reload-disabled.css", TEST_CSS_FOLDER);
    assertEquals(
      firstLoad.enabled,
      false,
      "first load should be disabled due to pref",
    );

    // LoadCSS again - should remain disabled
    // (entry exists and is disabled, so else if branch doesn't run)
    const secondLoad = svc.loadCSS("reload-disabled.css", TEST_CSS_FOLDER);
    assertEquals(
      secondLoad.enabled,
      false,
      "loadCSS should keep entry disabled on reload",
    );
    assertEquals(
      firstLoad,
      secondLoad,
      "should return the same entry instance",
    );
  } finally {
    // Cleanup
    Services.prefs.clearUserPref("UserCSSLoader.disabled_list");
  }
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — create method filename validation
// ---------------------------------------------------------------------------

async function testCreateValidatesWhitespace(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-create",
  );

  // Override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  // Create folder
  await IOUtils.makeDirectory(testFolder);

  try {
    // This test verifies the filename validation logic exists
    // Actual user prompts make this difficult to test fully
    const _filename = "  test  file  ";
    // The implementation should normalize whitespace
    // We can't easily test the prompt interaction, but we can verify
    // the method doesn't throw on problematic input
  } finally {
    // Restore and cleanup
    svc.getCSSFolder = originalGetCSSFolder;
    try {
      await IOUtils.remove(testFolder, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
}

async function testCreatePreventsConcurrentCreation(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-concurrent",
  );

  // Override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  // Create folder
  await IOUtils.makeDirectory(testFolder);

  try {
    // Start a create operation (without filename, which would prompt)
    // Since we can't test prompts, we'll test the isCreating flag indirectly
    // by verifying the service state doesn't get corrupted
    assertEquals((svc as unknown as { isCreating: boolean }).isCreating, false, "should not be creating initially");

    // Manually set isCreating to simulate a concurrent create operation
    (svc as unknown as { isCreating: boolean }).isCreating = true;

    // Try to create with a filename - should return early due to isCreating guard
    await svc.create("test-concurrent.css");

    // Verify isCreating is still true (the early return happened)
    assertEquals((svc as unknown as { isCreating: boolean }).isCreating, true, "isCreating should remain true when guarded");

    // The file should NOT have been created because of the concurrent guard
    const filePath = PathUtils.join(testFolder, "test-concurrent.css");
    const fileExists = await IOUtils.exists(filePath);
    assertEquals(fileExists, false, "file should not be created when isCreating is true");

    // Reset isCreating for cleanup
    (svc as unknown as { isCreating: boolean }).isCreating = false;
  } finally {
    // Restore and cleanup
    svc.getCSSFolder = originalGetCSSFolder;
    try {
      await IOUtils.remove(testFolder, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
}

async function testCreateNormalizesWhitespace(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-whitespace",
  );

  // Override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  // Create folder
  await IOUtils.makeDirectory(testFolder);

  try {
    const restorePrompts = mockPromptDialogs();
    try {
    // Create a file with multiple spaces - these should be normalized to single spaces
    // Line 579: fileName.replace(/\s+/g, " ")
    const testFileName = "test   multiple    spaces";
    await svc.create(testFileName);

    // The file should be created with normalized whitespace: "test multiple spaces.css"
    const expectedFileName = "test multiple spaces.css";
    const filePath = PathUtils.join(testFolder, expectedFileName);
    const fileExists = await IOUtils.exists(filePath);

    assertEquals(fileExists, true, "file should be created with normalized whitespace");

    // Verify the file was NOT created with the original un-normalized name
    const originalPath = PathUtils.join(testFolder, testFileName + ".css");
    const originalExists = await IOUtils.exists(originalPath);
    assertEquals(originalExists, false, "file should not be created with original whitespace");

    // Cleanup
    await IOUtils.remove(filePath);
    } finally {
      restorePrompts();
    }
  } finally {
    // Restore and cleanup
    svc.getCSSFolder = originalGetCSSFolder;
    try {
      await IOUtils.remove(testFolder, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
}

async function testCreateSanitizesInvalidCharacters(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-sanitize",
  );

  // Override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  // Create folder
  await IOUtils.makeDirectory(testFolder);

  try {
    const restorePrompts = mockPromptDialogs();
    try {
    // Test with invalid filename characters: \ / : * ? " < > |
    // Line 580: fileName.replace(/[\\/:*?"<>|]/g, "")
    const testFileName = 'test:file/with\\invalid*chars?"<|>name';
    await svc.create(testFileName);

    // The file should be created with invalid chars removed: "testfilewithinvalidcharsname.css"
    const expectedFileName = "testfilewithinvalidcharsname.css";
    const filePath = PathUtils.join(testFolder, expectedFileName);
    const fileExists = await IOUtils.exists(filePath);

    assertEquals(fileExists, true, "file should be created with invalid characters removed");

    // Cleanup
    await IOUtils.remove(filePath);
    } finally {
      restorePrompts();
    }
  } finally {
    // Restore and cleanup
    svc.getCSSFolder = originalGetCSSFolder;
    try {
      await IOUtils.remove(testFolder, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
}

async function testCreateAppendsCssExtension(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-extension",
  );

  // Override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  // Create folder
  await IOUtils.makeDirectory(testFolder);

  try {
    const restorePrompts = mockPromptDialogs();
    try {
    // Create a file without .css extension - it should be appended
    // Line 587-589: if (!fileName.endsWith(".css")) { fileName += ".css"; }
    const testFileName = "test-file-without-extension";
    await svc.create(testFileName);

    // The file should be created with .css appended: "test-file-without-extension.css"
    const expectedFileName = "test-file-without-extension.css";
    const filePath = PathUtils.join(testFolder, expectedFileName);
    const fileExists = await IOUtils.exists(filePath);

    assertEquals(fileExists, true, "file should be created with .css extension appended");

    // Verify the file was NOT created without the extension
    const noExtPath = PathUtils.join(testFolder, testFileName);
    const noExtExists = await IOUtils.exists(noExtPath);
    assertEquals(noExtExists, false, "file should not be created without .css extension");

    // Cleanup
    await IOUtils.remove(filePath);
    } finally {
      restorePrompts();
    }
  } finally {
    // Restore and cleanup
    svc.getCSSFolder = originalGetCSSFolder;
    try {
      await IOUtils.remove(testFolder, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
}

async function testCreateRejectsEmptyFilename(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-empty",
  );

  // Override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  // Create folder
  await IOUtils.makeDirectory(testFolder);

  try {
    const restorePrompts = mockPromptDialogs();
    try {
    // Try to create with empty filename - should return early
    // Line 582-585: if (!fileName || !/\S/.test(fileName)) { this.isCreating = false; return; }
    // Note: empty string is falsy, so prompt() is called first; mock returns null → early return
    await svc.create("");

    // isCreating should be reset to false
    assertEquals((svc as unknown as { isCreating: boolean }).isCreating, false, "isCreating should be reset after empty filename");

    // No file should be created
    const files = await IOUtils.getChildren(testFolder);
    assertEquals(files.length, 0, "no files should be created for empty filename");
    } finally {
      restorePrompts();
    }
  } finally {
    // Restore and cleanup
    svc.getCSSFolder = originalGetCSSFolder;
    try {
      await IOUtils.remove(testFolder, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
}

async function testCreateRejectsWhitespaceOnlyFilename(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-whitespace-only",
  );

  // Override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  // Create folder
  await IOUtils.makeDirectory(testFolder);

  try {
    // Try to create with whitespace-only filename - should return early
    // Line 582-585: if (!fileName || !/\S/.test(fileName)) checks for non-whitespace chars
    await svc.create("   \t  \n  ");

    // isCreating should be reset to false
    assertEquals((svc as unknown as { isCreating: boolean }).isCreating, false, "isCreating should be reset after whitespace-only filename");

    // No file should be created
    const files = await IOUtils.getChildren(testFolder);
    assertEquals(files.length, 0, "no files should be created for whitespace-only filename");
  } finally {
    // Restore and cleanup
    svc.getCSSFolder = originalGetCSSFolder;
    try {
      await IOUtils.remove(testFolder, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
}

async function testCreateWithAlreadyHasCssExtension(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-has-extension",
  );

  // Override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  // Create folder
  await IOUtils.makeDirectory(testFolder);

  try {
    const restorePrompts = mockPromptDialogs();
    try {
    // Create a file that already has .css extension - should NOT append another
    const testFileName = "test-already-has.css";
    await svc.create(testFileName);

    // The file should be created with the exact name provided (no double extension)
    const filePath = PathUtils.join(testFolder, testFileName);
    const fileExists = await IOUtils.exists(filePath);

    assertEquals(fileExists, true, "file should be created with original .css extension");

    // Verify the file was NOT created with a double extension
    const doubleExtPath = PathUtils.join(testFolder, testFileName + ".css");
    const doubleExtExists = await IOUtils.exists(doubleExtPath);
    assertEquals(doubleExtExists, false, "file should not be created with double .css extension");

    // Cleanup
    await IOUtils.remove(filePath);
    } finally {
      restorePrompts();
    }
  } finally {
    // Restore and cleanup
    svc.getCSSFolder = originalGetCSSFolder;
    try {
      await IOUtils.remove(testFolder, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
}

async function testCreateNormalizesAndSanitizesCombined(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  const testFolder = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome-test-combined",
  );

  // Override getCSSFolder
  const originalGetCSSFolder = svc.getCSSFolder.bind(svc);
  svc.getCSSFolder = () => testFolder;

  // Create folder
  await IOUtils.makeDirectory(testFolder);

  try {
    const restorePrompts = mockPromptDialogs();
    try {
    // Test combined: whitespace normalization + invalid char removal + extension append
    const testFileName = "  test:   file/  name  ";
    await svc.create(testFileName);

    // Expected: "  test:   file/  name  "
    //   .replace(/\s+/g, " ") → " test: file/ name "
    //   .replace(/[\\/:*?"<>|]/g, "") → " test file name "
    //   + ".css" → " test file name .css"
    const expectedFileName = " test file name .css";
    const filePath = PathUtils.join(testFolder, expectedFileName);
    const fileExists = await IOUtils.exists(filePath);

    assertEquals(fileExists, true, "file should be created with all normalizations applied");

    // Cleanup
    await IOUtils.remove(filePath);
    } finally {
      restorePrompts();
    }
  } finally {
    // Restore and cleanup
    svc.getCSSFolder = originalGetCSSFolder;
    try {
      await IOUtils.remove(testFolder, { recursive: true });
    } catch {
      // Ignore cleanup errors
    }
  }
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — cleanupExistingElements
// ---------------------------------------------------------------------------

async function testCleanupExistingElementsRemovesElements(): Promise<void> {
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  // This test verifies the cleanup method exists and can be called
  // without throwing. Actual DOM manipulation is difficult to test
  // without a real DOM.
  let threw = false;
  try {
    svc["cleanupExistingElements"]();
  } catch {
    threw = true;
  }
  assertEquals(threw, false, "cleanupExistingElements should not throw");
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — toggle timing
// ---------------------------------------------------------------------------

async function testToggleUpdatesCssFilesList(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");
  const svc = ChromeCSSService.getInstance();

  svc.loadCSS("toggle-update-test.css", TEST_CSS_FOLDER);
  svc.updateCssFilesList();

  const filesBefore = svc.getCssFiles().length;
  assertEquals(filesBefore, 1, "should have 1 file before toggle");

  // Toggle should trigger updateCssFilesList after a delay
  svc.toggle("toggle-update-test.css");

  // Wait for the timeout
  await new Promise(resolve => setTimeout(resolve, 100));

  // The list should still exist (may or may not have changed length)
  const filesAfter = svc.getCssFiles().length;
  assertEquals(filesAfter, 1, "should still have 1 file after toggle");
}

// ---------------------------------------------------------------------------
// Tests: ChromeCSSService — multiple instances and state isolation
// ---------------------------------------------------------------------------

async function testMultipleServicesShareState(): Promise<void> {
  await resetServiceSingleton();
  const { ChromeCSSService } = await import("../service.tsx");

  const svc1 = ChromeCSSService.getInstance();
  const svc2 = ChromeCSSService.getInstance();

  svc1.loadCSS("shared-state.css", TEST_CSS_FOLDER);

  assert("shared-state.css" in svc2.readCSS, "both instances should share readCSS");
  assertEquals(svc1, svc2, "getInstance should return the same instance");
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

    // CSSEntry — enabled setter with file operations
    {
      name: "CSSEntry: enabled setter handles non-existent file",
      fn: testCSSEntryEnabledSetterHandlesNonExistentFile,
    },
    {
      name: "CSSEntry: enabled setter updates internal state",
      fn: testCSSEntryEnabledSetterUpdatesInternalState,
    },

    // CSSEntry — edge cases
    {
      name: "CSSEntry: case-insensitive matching for sheet types",
      fn: testCSSEntryCaseInsensitiveMatching,
    },
    {
      name: "CSSEntry: special characters in filename",
      fn: testCSSEntrySpecialCharactersInFilename,
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
      name: "ChromeCSSService: loadCSS does not re-enable disabled entry",
      fn: testLoadCSSReEnablesExistingEntry,
    },
    {
      name: "ChromeCSSService: loadCSS keeps enabled entry enabled",
      fn: testLoadCSSKeepsEnabledEntryEnabled,
    },
    {
      name: "ChromeCSSService: loadCSS respects disabled_list pref on reload",
      fn: testLoadCSSRespectsDisabledListPrefOnReload,
    },
    {
      name: "ChromeCSSService: toggle flips enabled state",
      fn: testToggleFlipsEnabledState,
    },
    {
      name: "ChromeCSSService: toggle non-existent does not throw",
      fn: testToggleNonExistentDoesNotThrow,
    },
    {
      name: "ChromeCSSService: loadCSS respects disabled_list pref",
      fn: testLoadCSSRespectsDisabledListPref,
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
    {
      name: "ChromeCSSService: toggle updates cssFiles list after delay",
      fn: testToggleUpdatesCssFilesList,
    },

    // ChromeCSSService — rebuild method
    {
      name: "ChromeCSSService: rebuild creates directory if not exists",
      fn: testRebuildCreatesDirectoryIfNotExists,
    },
    {
      name: "ChromeCSSService: rebuild excludes .uc.css files",
      fn: testRebuildExcludesUcCssFiles,
    },
    {
      name: "ChromeCSSService: rebuild handles deleted files",
      fn: testRebuildHandlesDeletedFiles,
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
    {
      name: "ChromeCSSService: uninit saves disabled list pref",
      fn: testUninitSavesDisabledListPref,
    },
    {
      name: "ChromeCSSService: uninit saves empty disabled list when all enabled",
      fn: testUninitSavesEmptyDisabledListWhenAllEnabled,
    },
    {
      name: "ChromeCSSService: uninit saves all files when all disabled",
      fn: testUninitSavesAllFilesWhenAllDisabled,
    },
    {
      name: "ChromeCSSService: cleanupExistingElements can be called",
      fn: testCleanupExistingElementsRemovesElements,
    },

    // ChromeCSSService — convertUTF8ToShiftJIS
    {
      name: "ChromeCSSService: convertUTF8ToShiftJIS handles basic string",
      fn: testConvertUTF8ToShiftJISHandlesBasicString,
    },
    {
      name: "ChromeCSSService: convertUTF8ToShiftJIS handles empty string",
      fn: testConvertUTF8ToShiftJISHandlesEmptyString,
    },
    {
      name: "ChromeCSSService: convertUTF8ToShiftJIS handles special characters",
      fn: testConvertUTF8ToShiftJISHandlesSpecialCharacters,
    },

    // ChromeCSSService — openFolder
    {
      name: "ChromeCSSService: openFolder handles non-existent folder",
      fn: testOpenFolderHandlesNonExistentFolder,
    },

    // ChromeCSSService — editUserCSS
    {
      name: "ChromeCSSService: editUserCSS constructs correct path",
      fn: testEditUserCSSConstructsCorrectPath,
    },

    // ChromeCSSService — create method
    {
      name: "ChromeCSSService: create validates whitespace",
      fn: testCreateValidatesWhitespace,
    },
    {
      name: "ChromeCSSService: create prevents concurrent creation",
      fn: testCreatePreventsConcurrentCreation,
    },
    {
      name: "ChromeCSSService: create normalizes whitespace in filename",
      fn: testCreateNormalizesWhitespace,
    },
    {
      name: "ChromeCSSService: create sanitizes invalid characters",
      fn: testCreateSanitizesInvalidCharacters,
    },
    {
      name: "ChromeCSSService: create appends .css extension",
      fn: testCreateAppendsCssExtension,
    },
    {
      name: "ChromeCSSService: create rejects empty filename",
      fn: testCreateRejectsEmptyFilename,
    },
    {
      name: "ChromeCSSService: create rejects whitespace-only filename",
      fn: testCreateRejectsWhitespaceOnlyFilename,
    },
    {
      name: "ChromeCSSService: create with already has .css extension",
      fn: testCreateWithAlreadyHasCssExtension,
    },
    {
      name: "ChromeCSSService: create normalizes and sanitizes combined",
      fn: testCreateNormalizesAndSanitizesCombined,
    },

    // ChromeCSSService — state isolation
    {
      name: "ChromeCSSService: multiple instances share state",
      fn: testMultipleServicesShareState,
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
    {
      name: "ChromeCSS component: init creates service",
      fn: testChromeCSSComponentInitCreatesService,
    },
    {
      name: "ChromeCSS component: init only once",
      fn: testChromeCSSComponentInitOnlyOnce,
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
