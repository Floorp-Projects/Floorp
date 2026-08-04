// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";
import {
  getCommandPaletteSettings,
  saveCommandPaletteSettings,
} from "../../../src/app/command-palette/dataManager.ts";

/**
 * Unit tests for command-palette dataManager.
 *
 * These tests run inside the actual browser (colocated-env browser), so
 * `rpc.getBoolPref` / `rpc.setBoolPref` resolve to `directServicesFunctions`
 * in `src/lib/rpc/rpc.ts`, which delegate to `Services.prefs` directly.
 * We therefore drive `Services.prefs` to set up known state, invoke the
 * dataManager functions, and assert the observed behavior.
 *
 * The pref `floorp.commandPalette.enabled` has no default value defined in
 * the pref `.ini` files, so `clearUserPref` reliably makes `getPrefType`
 * return PREF_INVALID, which causes `rpc.getBoolPref` to resolve to `null`.
 */

const PREF = "floorp.commandPalette.enabled";

// `Services` is a Firefox global available in the browser test environment.
// deno-lint-ignore no-explicit-any
declare const Services: any;

/** Read the raw pref value as a boolean, or return null when not set. */
function readRawPref(): boolean | null {
  if (Services.prefs.getPrefType(PREF) !== Services.prefs.PREF_BOOL) {
    return null;
  }
  return Services.prefs.getBoolPref(PREF);
}

/** True when the pref is currently unset (PREF_INVALID). */
function isPrefUnset(): boolean {
  return Services.prefs.getPrefType(PREF) === Services.prefs.PREF_INVALID;
}

function testGetReturnsNullWhenPrefUnset(): Promise<void> {
  Services.prefs.clearUserPref(PREF);
  return getCommandPaletteSettings().then((result) => {
    assertEquals(result, null, "should return null when pref is not set");
    assert(isPrefUnset(), "pref should remain unset after get");
  });
}

function testGetReturnsEnabledTrue(): Promise<void> {
  Services.prefs.setBoolPref(PREF, true);
  try {
    return getCommandPaletteSettings().then((result) => {
      assert(result !== null, "result should not be null when pref is set");
      assertEquals(result!.enabled, true, "enabled should be true");
    });
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

function testGetReturnsEnabledFalse(): Promise<void> {
  Services.prefs.setBoolPref(PREF, false);
  try {
    return getCommandPaletteSettings().then((result) => {
      assert(result !== null, "result should not be null when pref is set");
      assertEquals(result!.enabled, false, "enabled should be false");
    });
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

function testGetReturnsNullOnRethrownPrefTypeMismatch(): Promise<void> {
  // Setting the pref as a STRING makes getPrefType !== PREF_BOOL, so the rpc
  // layer resolves to null and the dataManager forwards that null without
  // throwing — verifying the defensive guard.
  Services.prefs.setStringPref(PREF, "not-a-bool");
  try {
    return getCommandPaletteSettings().then((result) => {
      assertEquals(result, null, "should return null on pref type mismatch");
    });
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

async function testSaveEmptyObjectIsNoOp(): Promise<void> {
  Services.prefs.setBoolPref(PREF, true);
  try {
    await saveCommandPaletteSettings({});
    assertEquals(
      readRawPref(),
      true,
      "empty object should not change the pref value",
    );
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

async function testSaveSetsEnabledTrue(): Promise<void> {
  Services.prefs.setBoolPref(PREF, false);
  try {
    await saveCommandPaletteSettings({ enabled: true });
    assertEquals(readRawPref(), true, "save({enabled:true}) should set pref");
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

async function testSaveSetsEnabledFalse(): Promise<void> {
  Services.prefs.setBoolPref(PREF, true);
  try {
    await saveCommandPaletteSettings({ enabled: false });
    assertEquals(readRawPref(), false, "save({enabled:false}) should set pref");
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

async function testSaveCoercesToBoolean(): Promise<void> {
  // Boolean(undefined) === false; the dataManager wraps with Boolean() before
  // delegating to setBoolPref, so this must persist as a real boolean false.
  Services.prefs.setBoolPref(PREF, true);
  try {
    // deno-lint-ignore no-explicit-any
    await saveCommandPaletteSettings({ enabled: undefined } as any);
    assertEquals(
      readRawPref(),
      false,
      "save should coerce missing enabled to boolean false",
    );
  } finally {
    Services.prefs.clearUserPref(PREF);
  }
}

const tests: TestCase[] = [
  { name: "getCommandPaletteSettings returns null when pref is unset", fn: testGetReturnsNullWhenPrefUnset },
  { name: "getCommandPaletteSettings returns { enabled: true }", fn: testGetReturnsEnabledTrue },
  { name: "getCommandPaletteSettings returns { enabled: false }", fn: testGetReturnsEnabledFalse },
  { name: "getCommandPaletteSettings returns null on pref type mismatch", fn: testGetReturnsNullOnRethrownPrefTypeMismatch },
  { name: "saveCommandPaletteSettings({}) is a no-op", fn: testSaveEmptyObjectIsNoOp },
  { name: "saveCommandPaletteSettings({ enabled: true }) sets pref", fn: testSaveSetsEnabledTrue },
  { name: "saveCommandPaletteSettings({ enabled: false }) sets pref", fn: testSaveSetsEnabledFalse },
  { name: "saveCommandPaletteSettings coerces enabled to boolean", fn: testSaveCoercesToBoolean },
];

export async function runAllTests(): Promise<void> {
  const original = readRawPref();
  try {
    await runTests("dataManager.test.ts (command-palette)", tests);
  } finally {
    // Restore original pref state so the test suite is hermetic.
    if (original === null) {
      Services.prefs.clearUserPref(PREF);
    } else {
      Services.prefs.setBoolPref(PREF, original);
    }
  }
}
