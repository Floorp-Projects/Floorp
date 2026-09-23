// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";
import { classifyBundleRecovery } from "./bundleRecovery.ts";

const oldHash = "a".repeat(64);
const newHash = "b".repeat(64);

Deno.test("recovery recognizes every real atomic exchange interruption boundary", () => {
  assertEquals(
    classifyBundleRecovery(oldHash, newHash, null, oldHash, newHash),
    "staged",
  );
  assertEquals(
    classifyBundleRecovery(oldHash, null, newHash, oldHash, newHash),
    "ready-to-swap",
  );
  assertEquals(
    classifyBundleRecovery(newHash, null, oldHash, oldHash, newHash),
    "swapped",
  );
  assertEquals(
    classifyBundleRecovery(newHash, null, null, oldHash, newHash),
    "committed",
  );
  assertEquals(
    classifyBundleRecovery(oldHash, null, null, oldHash, newHash),
    "rolled-back",
  );
});

Deno.test("recovery refuses unrelated or ambiguous contents", () => {
  for (
    const [live, stage, backup] of [
      [null, newHash, oldHash],
      ["c".repeat(64), null, oldHash],
      [oldHash, newHash, newHash],
      [newHash, oldHash, oldHash],
      [oldHash, oldHash, null],
    ]
  ) {
    assertEquals(
      classifyBundleRecovery(live, stage, backup, oldHash, newHash),
      "conflict",
    );
  }
  assertEquals(
    classifyBundleRecovery(oldHash, newHash, null, oldHash, oldHash),
    "conflict",
  );
  assertEquals(
    classifyBundleRecovery(oldHash, newHash, null, "invalid", newHash),
    "conflict",
  );
});
