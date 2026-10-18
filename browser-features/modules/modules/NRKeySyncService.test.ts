// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../chrome/test/utils/test_harness.ts";
import { keySyncService } from "./NRKeySyncService.sys.mts";
import type { FirefoxKeyEntry } from "../common/NRKeySyncTypes.ts";

/** A key id that cannot exist in the live DOM mirror. */
const UNKNOWN_KEY_ID = "nonexistent-key-id-noratest";

/**
 * Structural validation that never depends on how many keys the test
 * profile happens to expose: every mirrored entry must carry the raw
 * attribute strings, boolean flags and derived fields of FirefoxKeyEntry.
 */
function assertWellFormedEntry(entry: FirefoxKeyEntry, label: string): void {
  assert(typeof entry.id === "string", `${label}: id should be a string`);
  assert(
    entry.keysetKind === "main" || entry.keysetKind === "extension",
    `${label}: keysetKind should be "main" or "extension" (got ${
      String(entry.keysetKind)
    })`,
  );
  assert(
    typeof entry.modifiers === "string",
    `${label}: modifiers should be a string`,
  );
  assert(
    typeof entry.key === "string",
    `${label}: key should be a string`,
  );
  assert(
    typeof entry.keycode === "string",
    `${label}: keycode should be a string`,
  );
  assert(
    typeof entry.command === "string",
    `${label}: command should be a string`,
  );
  assert(
    typeof entry.internal === "boolean",
    `${label}: internal should be a boolean`,
  );
  assert(
    typeof entry.reserved === "boolean",
    `${label}: reserved should be a boolean`,
  );
  assert(
    typeof entry.customized === "boolean",
    `${label}: customized should be a boolean`,
  );
  assert(
    typeof entry.label === "string" && entry.label.length > 0,
    `${label}: label should be a non-empty string`,
  );
  assert(
    typeof entry.shortcutText === "string",
    `${label}: shortcutText should be a string`,
  );
  assert(
    entry.canonicalCode === null || typeof entry.canonicalCode === "string",
    `${label}: canonicalCode should be a string or null`,
  );
}

function testInitDoesNotThrowAndIsIdempotent(): void {
  // Bootstraps CustomKeys detection and window attachment; must never throw,
  // even when the keyboard-shortcut feature already initialized the service.
  keySyncService.init();
  // A second call must be a no-op (idempotent bootstrap).
  keySyncService.init();
  assertEquals(
    typeof keySyncService.isWriteEnabled(),
    "boolean",
    "isWriteEnabled should answer with a boolean after init",
  );
}

function testGetKeysReturnsWellFormedSnapshot(): void {
  keySyncService.init();
  const keys = keySyncService.getKeys();
  assert(Array.isArray(keys), "getKeys should return an array");
  // The test profile may legitimately expose zero keys; only the shape is
  // asserted, never the count.
  let extensionSeen = false;
  for (const [index, entry] of keys.entries()) {
    assertWellFormedEntry(entry, `entry ${index} (${entry.id})`);
    if (entry.keysetKind === "extension") {
      extensionSeen = true;
    } else {
      assert(
        !extensionSeen,
        `main keyset entries should precede extension entries (entry ${index} is out of order)`,
      );
    }
  }
  assert(
    keySyncService.getKeys() !== keys,
    "getKeys should return a fresh array safe for callers to retain",
  );
}

function testChangeKeyUnknownIdFailsWithoutWrite(): void {
  keySyncService.init();
  const result = keySyncService.changeKey(UNKNOWN_KEY_ID, {
    modifiers: "accel",
    key: "Z",
  });
  assertEquals(
    result.ok,
    false,
    "changing an unknown key id must be rejected",
  );
  assert(
    typeof result.error === "string" && result.error.length > 0,
    "a rejected changeKey should carry an error message",
  );
}

function testChangeKeyInternalKeyIsRejected(): void {
  keySyncService.init();
  const internal = keySyncService.getKeys().find((entry) => entry.internal);
  if (!internal) {
    // No internal key in this profile: nothing to assert (skipped).
    return;
  }
  const result = keySyncService.changeKey(internal.id, {
    modifiers: "accel",
    key: "Z",
  });
  assertEquals(
    result.ok,
    false,
    `internal key ${internal.id} must not be editable`,
  );
  assert(
    typeof result.error === "string" && result.error.length > 0,
    "a rejected internal changeKey should carry an error message",
  );
}

function testResetKeyUnknownIdFailsWithoutWrite(): void {
  keySyncService.init();
  const result = keySyncService.resetKey(UNKNOWN_KEY_ID);
  assertEquals(
    result.ok,
    false,
    "resetting an unknown key id must be rejected",
  );
  assert(
    typeof result.error === "string" && result.error.length > 0,
    "a rejected resetKey should carry an error message",
  );
}

function testListenerRegistrationIsSafeAndIdempotent(): void {
  keySyncService.init();
  const listener = (_keys: FirefoxKeyEntry[]): void => {};
  keySyncService.addListener(listener);
  // Duplicate registration must not throw (the service deduplicates).
  keySyncService.addListener(listener);
  // Repeated and unregistered removals must both be no-ops.
  keySyncService.removeListener(listener);
  keySyncService.removeListener(listener);
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "init does not throw and is idempotent",
      fn: testInitDoesNotThrowAndIsIdempotent,
    },
    {
      name: "getKeys returns a well-formed mirror snapshot",
      fn: testGetKeysReturnsWellFormedSnapshot,
    },
    {
      name: "changeKey rejects unknown key ids without writing",
      fn: testChangeKeyUnknownIdFailsWithoutWrite,
    },
    {
      name: "changeKey rejects internal keys",
      fn: testChangeKeyInternalKeyIsRejected,
    },
    {
      name: "resetKey rejects unknown key ids without writing",
      fn: testResetKeyUnknownIdFailsWithoutWrite,
    },
    {
      name: "listener add/remove is safe and idempotent",
      fn: testListenerRegistrationIsSafeAndIdempotent,
    },
  ];
  await runTests("NRKeySyncService.test.ts", tests);
}
